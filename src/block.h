/*
BlockModel
*/

#ifndef NGME_BLOCK_H
#define NGME_BLOCK_H

#include <Rcpp.h>
#include <RcppEigen.h>
#include <Eigen/Sparse>

#include <string>
#include <vector>
#include <iostream>

#include "include/timer.h"
#include "include/solver.h"
#include "include/MatrixAlgebra.h"
#include "model.h"
#include "latent.h"

#include "latents/ar1.h"
#include "latents/matern.h"

using Eigen::SparseMatrix;

const int latent_para = 4;

class BlockModel : public Model {
protected:

// n_meshs   = row(A1) + ... + row(An)
// n_paras = 4 * n_latent + 1
    // general_in
    MatrixXd X;
    VectorXd Y; 
    int n_meshs;
    string family;
    
    VectorXd beta;
    double sigma_eps;
    
    int n_latent;
    
    int n_obs;
    int n_paras, n_feff, n_merr; 

    // controls 
    int n_gibbs;
    bool opt_fix_effect, kill_var;
    double kill_power, threshold, termination;

    SparseMatrix<double> A, K, dK, d2K;

    // debug
    bool debug, fixW, fixSV, fixSigEps;
    
    // optimize related
    VectorXd stepsizes, gradients;
    int counting {0};
    VectorXd indicate_threshold, steps_to_threshold;

    // No initializer
    std::vector<Latent*> latents;
    cholesky_solver chol_Q, chol_QQ;
    SparseLU<SparseMatrix<double> > LU_K;

    VectorXd trueW, trueSV;
    
public:
    BlockModel() {}

    BlockModel(Rcpp::List gen_list,
               Rcpp::List inits,
               Rcpp::List latents_in,
               Rcpp::List control_list,
            //    Rcpp::List control_list
               Rcpp::List debug_list
               ) : 
    X             ( Rcpp::as<MatrixXd>   (gen_list["X"]) ),
    Y             ( Rcpp::as<VectorXd>   (gen_list["Y"]) ), 
    n_meshs        ( Rcpp::as<int>        (gen_list["n_meshs"]) ),
    family        ( Rcpp::as<string>     (gen_list["family"]) ),
    
    beta          ( Rcpp::as<VectorXd>   (inits["beta"]) ),
    sigma_eps     ( Rcpp::as<double>     (inits["sigma_eps"]) ), 
    
    n_latent      ( latents_in.size()), 
    
    n_obs         ( Y.size()),
    n_paras       ( n_latent * latent_para + 1), // change
    n_feff        ( beta.size()),
    n_merr        ( 1 ),
    
    n_gibbs       ( Rcpp::as<int>     (control_list["gibbs_sample"]) ),
    opt_fix_effect( Rcpp::as<bool>    (control_list["opt_fix_effect"]) ),
    kill_var      ( Rcpp::as<bool>    (control_list["kill_var"]) ),
    kill_power    ( Rcpp::as<double>  (control_list["kill_power"]) ), 
    threshold     ( Rcpp::as<double>  (control_list["threshold"]) ), 
    termination   ( Rcpp::as<double>  (control_list["termination"]) ), 

    A             ( n_obs, n_meshs), 
    K             ( n_meshs, n_meshs), 
    dK            ( n_meshs, n_meshs),
    d2K           ( n_meshs, n_meshs),

    debug         ( Rcpp::as<bool> (debug_list["debug"]) ),
    fixW          ( Rcpp::as<bool> (debug_list["fixW"]) ),
    fixSV         ( Rcpp::as<bool> (debug_list["fixSV"])),
    fixSigEps     ( Rcpp::as<bool> (debug_list["fixSigEps"]))
    {
        const int burnin = control_list["burnin"];
        const double stepsize = control_list["stepsize"];
        
        if (fixW)      trueW = Rcpp::as<VectorXd>   (debug_list["trueW"]);
        if (fixSV)     trueSV = Rcpp::as<VectorXd>  (debug_list["trueSV"]);
        
        // Init each latent model
        for (int i=0; i < n_latent; ++i) {
            Rcpp::List latent_in = Rcpp::as<Rcpp::List> (latents_in[i]);

            // construct acoording to models
            string type = latent_in["type"];
            if (type == "ar1") {
                latents.push_back(new AR(latent_in) );
            } 
            // else if (type == "matern") {
            //     latents.push_back(new Matern(latent_in));
            // }
        }
        
        /* Fixed effects */
        if (opt_fix_effect) {
            int n_beta = beta.size();
            n_paras = n_latent * latent_para + 1 + n_beta;
        }


        /* Init variables: h, A */
        int n = 0;
        for (std::vector<Latent*>::iterator it = latents.begin(); it != latents.end(); it++) {
            setSparseBlock(&A,   0, n, (*it)->getA());            
            n += (*it)->getSize();
        }
        assemble();

if (debug) std::cout << "Assemble complete." << std::endl;        

            VectorXd inv_SV = VectorXd::Constant(n_meshs, 1).cwiseQuotient(getSV());
            SparseMatrix<double> Q = K.transpose() * inv_SV.asDiagonal() * K;
            SparseMatrix<double> QQ = Q + pow(sigma_eps, -2) * A.transpose() * A;
        
        chol_Q.analyze(Q);
        chol_QQ.analyze(QQ);
        LU_K.analyzePattern(K);

        // optimizer related
        stepsizes = VectorXd::Constant(n_paras, stepsize);
        steps_to_threshold = VectorXd::Constant(n_paras, 0);
        indicate_threshold = VectorXd::Constant(n_paras, 0);

        burn_in(burnin);
    }

    /* Gibbs Sampler */
    void burn_in(int iterations) {
        for (int i=0; i < iterations; i++) {
            sampleW_VY();
            sampleV_WY();
        }
    }

    void sampleW_VY();
    void sampleW_V();

    void sampleV_WY() {
        for (unsigned i=0; i < n_latent; i++) {
            (*latents[i]).sample_cond_V();
        }
    }
    void setW(const VectorXd&);
    
    /* Optimizer related */
    VectorXd             get_parameter() const;
    VectorXd             get_stepsizes() const;
    void                 set_parameter(const VectorXd&);
    VectorXd             grad();
    SparseMatrix<double> precond() const;
    
    void                 examine_gradient();

    /* Aseemble */
    void assemble() {
        int n = 0;
        for (std::vector<Latent*>::iterator it = latents.begin(); it != latents.end(); it++) {
            setSparseBlock(&K,   n, n, (*it)->getK());      
            setSparseBlock(&dK,  n, n, (*it)->get_dK());   
            setSparseBlock(&d2K, n, n, (*it)->get_d2K()); 
            
            n += (*it)->getSize();
        }
    }

    // return mean = mu*(V-h)
    VectorXd getMean() const {
        VectorXd mean (n_meshs);
        int pos = 0;
        for (std::vector<Latent*>::const_iterator it = latents.begin(); it != latents.end(); it++) {
            int size = (*it)->getSize();
            mean.segment(pos, size) = (*it)->getMean();
            pos += size;
        }
        return mean;
    }

    // return sigma * V
    VectorXd getSV() const {
        VectorXd SV (n_meshs);
        int pos = 0;
        for (std::vector<Latent*>::const_iterator it = latents.begin(); it != latents.end(); it++) {
            int size = (*it)->getSize();
            SV.segment(pos, size) = (*it)->getSV();
            pos += size;
        }
        
        // debug
        if (fixSV) {
            SV = trueSV;
        }
        return SV;
    }

    VectorXd getW() const {
        VectorXd W (n_meshs);
        int pos = 0;
        for (std::vector<Latent*>::const_iterator it = latents.begin(); it != latents.end(); it++) {
            int size = (*it)->getSize();
            W.segment(pos, size) = (*it)->getW();
            pos += size;
        }
        return W;
    }

    VectorXd getPrevW() const {
        VectorXd W (n_meshs);
        int pos = 0;
        for (std::vector<Latent*>::const_iterator it = latents.begin(); it != latents.end(); it++) {
            int size = (*it)->getSize();
            W.segment(pos, size) = (*it)->getPrevW();
            pos += size;
        }
        return W;
    }

    double get_theta_sigma_eps() const {
        return log(sigma_eps);
    }

    // measurement error
    double grad_theta_sigma_eps() const {
        double g = 0;
        if (family=="normal") {

            VectorXd tmp = Y - A * getW() - X * beta;
            double norm2 =  tmp.dot(tmp);
            
            VectorXd tmp2 = Y - A * getPrevW() - X * beta;
            double prevnorm2 = tmp2.dot(tmp2);

            // g = -(1.0 / sigma_eps) * n_obs + pow(sigma_eps, -3) * norm2;
            double gTimesSigmaEps = -(1.0) * n_obs + pow(sigma_eps, -2) * norm2;
            double prevgTimesSigmaEps = -(1.0) * n_obs + pow(sigma_eps, -2) * prevnorm2;
            
            double hess = -2.0 * n_obs * pow(sigma_eps, -2);
            // double hess = 1.0 * n_obs * pow(sigma_eps, -2)  - 3 * pow(sigma_eps, -4) * norm2;

            g = gTimesSigmaEps / (hess * pow(sigma_eps, 2) + prevgTimesSigmaEps); 
            
            // g = - gTimesSigmaEps / (2 * n_obs);

            if (fixSigEps)  g=0;
        }

        return g;
    }

    void set_theta_sgima_eps(double theta) {
        sigma_eps = exp(theta);
    }

    // fixed effects
    VectorXd grad_beta() {
        VectorXd inv_SV = VectorXd::Constant(n_meshs, 1).cwiseQuotient(getSV());
        VectorXd grads = X.transpose() * (Y - X*beta - A*getW());
        
        // SparseMatrix<double> Q = K.transpose() * inv_SV.asDiagonal() * K;
        // SparseMatrix<double> QQ = Q + pow(sigma_eps, -2) * A.transpose() * A;

        // LU_K.factorize(K);
        // chol_Q.compute(Q);

        // VectorXd mean = getMean(); // mean = mu*(V-h)
        // VectorXd b = LU_K.solve(mean);
        
        // VectorXd b_tilde = QQ * chol_Q.solve(b) + pow(sigma_eps, -2) * A.transpose() * (Y-X*beta);
        // VectorXd grads = X.transpose() * (Y - X*beta - A*b_tilde);

        MatrixXd hess = X.transpose() * X;
        grads = hess.ldlt().solve(grads);

// std::cout << "grads of beta=" << -grads << std::endl;        
        return -grads;
    }

    // return estimates
    Rcpp::List get_estimates() const {
        Rcpp::List latents_estimates;
        
        for (int i=0; i < n_latent; i++) {
            latents_estimates.push_back((*latents[i]).get_estimates());
        }

        return Rcpp::List::create(
            Rcpp::Named("m_err")        = sigma_eps,
            Rcpp::Named("f_eff")        = beta,
            Rcpp::Named("latents_est")  = latents_estimates
        );
    }
};

// ---- inherited functions ------
/* the way structuring the parameter 
    latents[1].get_parameter 
        ...
    sigma_eps
    beta (fixed effects)
*/

inline VectorXd BlockModel::get_parameter() const {
    VectorXd thetas (n_paras);
    int pos = 0;
    for (std::vector<Latent*>::const_iterator it = latents.begin(); it != latents.end(); it++) {
        VectorXd theta = (*it)->get_parameter();
        thetas.segment(pos, theta.size()) = theta;
        pos += theta.size();
    }
    
    // sigma_eps
    thetas(n_paras-1) = get_theta_sigma_eps();
    
    // fixed effects
    if (opt_fix_effect) {
        int n_beta = beta.size();
        thetas.segment(n_paras - n_beta-1, n_beta) = beta;
    }

    return thetas;
}

inline VectorXd BlockModel::grad() {
    VectorXd avg_gradient = VectorXd::Zero(n_paras);
    
long long time_compute_g = 0;
long long time_sample_w = 0;

    for (int i=0; i < n_gibbs; i++) {
        
        // stack grad
        VectorXd gradient = VectorXd::Zero(n_paras);
        
auto timer_computeg = std::chrono::steady_clock::now();
        // get grad for each latent
        int pos = 0;
        for (std::vector<Latent*>::const_iterator it = latents.begin(); it != latents.end(); it++) {
            int theta_len = (*it)->get_n_params();
            gradient.segment(pos, theta_len) = (*it)->get_grad();
            pos += theta_len;
        }
time_compute_g += since(timer_computeg).count();

        // fixed effects
        if (opt_fix_effect) {
            int n_beta = beta.size();
            gradient.segment(n_paras - n_beta-1, n_beta) = grad_beta();
        }
        
        // sigma_eps 
        gradient(n_paras-1) = grad_theta_sigma_eps();

        avg_gradient += gradient;

        // gibbs sampling
        sampleV_WY(); 
auto timer_sampleW = std::chrono::steady_clock::now();
        sampleW_VY();
time_sample_w += since(timer_sampleW).count();
    }

if (debug) {
std::cout << "avg time for compute grad (ms): " << time_compute_g / n_gibbs << std::endl;   
std::cout << "avg time for sampling W(ms): " << time_sample_w / n_gibbs << std::endl;   
}

    avg_gradient = (1.0/n_gibbs) * avg_gradient;
    gradients = avg_gradient;
    
    // EXAMINE the gradient to change the stepsize
    if (kill_var) examine_gradient();

    return gradients;
}

inline void BlockModel::set_parameter(const VectorXd& Theta) {
    int pos = 0;
    for (std::vector<Latent*>::iterator it = latents.begin(); it != latents.end(); it++) {
        int theta_len = (*it)->get_n_params();
        VectorXd theta = Theta.segment(pos, theta_len);
        (*it)->set_parameter(theta);
        pos += theta_len;
    }
    // sigma_eps
    set_theta_sgima_eps(Theta(n_paras-1));
    
    // fixed effects
    if (opt_fix_effect) {
        int n_beta = beta.size();
        beta = Theta.segment(n_paras - n_beta-1, n_beta);
    }

    assemble(); //update K,dK,d2K after
// std::cout << "Theta=" << Theta <<std::endl;
}


// provide stepsize
inline void BlockModel::examine_gradient() {
    
    // examine if the gradient under the threshold
    for (int i=0; i < n_paras; i++) {
        if (abs(gradients(i)) < threshold) {
            indicate_threshold(i) = 1;
        }      // mark if under threshold
        if (!indicate_threshold(i)) steps_to_threshold(i) = counting; // counting up
    }
    
    counting += 1;
    stepsizes = (VectorXd::Constant(n_paras, counting) - steps_to_threshold).cwiseInverse().array().pow(kill_power);

    // finish opt fo latents
    for (int i=0; i < n_latent; i++) {
        for (int j=0; j < latent_para; j++) {
            int index = latent_para*i + j;
            
            // if (counting - steps_to_threshold(index) > 100) 
            if (abs(gradients(index)) < termination) 
                latents[i]->finishOpt(j);
        }
    }

    // stop opt feff
    // for (int i=0; i < beta.size(); i++) {
    //     int index = latent_para * n_latent + i;
    // }
    // stop opt merr
    // ...

if (debug) {
    std::cout << "steps=" << steps_to_threshold <<std::endl;
    std::cout << "gradients=" << gradients <<std::endl;
    std::cout << "stepsizes=" << stepsizes <<std::endl;
}
}


inline VectorXd BlockModel::get_stepsizes() const {
    return stepsizes;
}


// Not Implemented
inline SparseMatrix<double> BlockModel::precond() const {
    SparseMatrix<double> precond;
    std::cout << "Not implemented \n";
    throw;
    return precond;
}


#endif