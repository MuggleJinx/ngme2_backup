/*
BlockModel
*/
// #define EIGEN_USE_MKL_ALL

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
#include "var.h"
#include "latent.h"

#include "latents/ar1.h"
#include "latents/matern.h"
#include "latents/matern_ns.h"

using Eigen::SparseMatrix;

const int latent_para = 4;

class BlockModel : public Model {
protected:
// n_meshs   = row(A1) + ... + row(An)
    // general
    MatrixXd X;
    VectorXd Y; 
    int n_meshs;
    string family;
    
    VectorXd beta, theta_merr;
        double sigma_eps;
    
    int n_latent;
    
    int n_obs;
    int n_params, n_la_params, n_feff, n_merr; 

    // controls 
    int n_gibbs;
    bool opt_fix_effect, kill_var;
    double kill_power, threshold, termination;

    SparseMatrix<double> A, K;      // not used: dK, d2K; 

    // debug
    bool debug, fix_W, fixSV, fix_merr;
    
    // optimize related
    VectorXd stepsizes, gradients;
    int counting {0};
    VectorXd indicate_threshold, steps_to_threshold;

    // No initializer
    std::vector<Latent*> latents;
    cholesky_solver chol_Q, chol_QQ;
    SparseLU<SparseMatrix<double> > LU_K;

    VectorXd fixedW;
    
public:
    // BlockModel() {}

    BlockModel(
        Rcpp::List general_in,
        Rcpp::List latents_in,
        Rcpp::List noise_in,
        Rcpp::List control_list,
        Rcpp::List debug_list
    );

    /* Gibbs Sampler */
    void burn_in(int iterations) {
        for (int i=0; i < iterations; i++) {
            sampleW_VY();
            sampleV_WY();
        }
    }

    void sampleW_VY();

    void sampleV_WY() {
if (debug) std::cout << "Start sampling V" << std::endl;        
        for (unsigned i=0; i < n_latent; i++) {
            (*latents[i]).sample_cond_V();
        }
if (debug) std::cout << "Finish sampling V" << std::endl;        
    }
    void setW(const VectorXd&);
    
    /* Optimizer related */
    VectorXd             get_parameter() const;
    VectorXd             get_stepsizes() const;
    void                 set_parameter(const VectorXd&);
    VectorXd             grad();
    SparseMatrix<double> precond() const;
    
    void                 examine_gradient();

    /* Noise */
    void sampleV_XY();

    /* Aseemble */
    void assemble() {
        int n = 0;
        for (std::vector<Latent*>::iterator it = latents.begin(); it != latents.end(); it++) {
            setSparseBlock(&K,   n, n, (*it)->getK());      
            // setSparseBlock(&dK,  n, n, (*it)->get_dK());   
            // setSparseBlock(&d2K, n, n, (*it)->get_d2K()); 
            
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

    // --------- Measurement error related ------------

    VectorXd get_theta_merr() const {
        VectorXd theta_merr = VectorXd::Zero(n_merr);
        
        if (family=="normal") {
            theta_merr(0) = log(sigma_eps);
        } else if (family=="nig") {
            // to-do
        }

        return theta_merr;
    }

    VectorXd grad_theta_merr() const {
        VectorXd grad = VectorXd::Zero(n_merr);
        
        if (!fix_merr) {
            if (family=="normal") {
                double g = 0;
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
                grad(0) = g;
            } 
            else if (family=="nig") {
                // to-do
            }
        }

        return grad;
    }

    void set_theta_merr(VectorXd theta_merr) {
        if (family=="normal") {
            sigma_eps = exp(theta_merr(0));
        } else if (family=="nig") {
            // to-do
        }
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

    // return output
    Rcpp::List output() const {
        Rcpp::List latents_estimates;
        
        for (int i=0; i < n_latent; i++) {
            latents_estimates.push_back((*latents[i]).output());
        }

        return Rcpp::List::create(
            Rcpp::Named("mesurement.noise")     = get_theta_merr(),
            Rcpp::Named("fixed.effects")        = beta,
            Rcpp::Named("block.W")              = getW(),
            Rcpp::Named("latent.model")         = latents_estimates
        );
    }
};

// ---- inherited functions ------
/* the way structuring the parameter 
    latents[1].get_parameter 
        ...
    beta (fixed effects)
    measurement noise
*/

// provide stepsize
inline void BlockModel::examine_gradient() {
    
    // examine if the gradient under the threshold
    for (int i=0; i < n_params; i++) {
        if (abs(gradients(i)) < threshold) {
            indicate_threshold(i) = 1;
        }      // mark if under threshold
        if (!indicate_threshold(i)) steps_to_threshold(i) = counting; // counting up
    }
    
    counting += 1;
    stepsizes = (VectorXd::Constant(n_params, counting) - steps_to_threshold).cwiseInverse().array().pow(kill_power);

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


// adding new class 
// class block gaussian


#endif