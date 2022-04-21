#ifndef NGME_LATANT_H
#define NGME_LATANT_H

#include <string>
#include <iostream>
#include <cmath>
#include <Rcpp.h>
#include <RcppEigen.h>
#include <Eigen/Dense>

#include "include/timer.h"
#include "include/solver.h"
#include "operator.h"
#include "var.h"

using Eigen::SparseMatrix;
using Eigen::VectorXd;

class Latent {
protected:
    int n_reg, n_paras {4}; //regressors, parameters
    
    // indicate which parameter to optimize
    bool opt_mu {false}, opt_sigma {false}, opt_kappa {false}, opt_var {false}, 
        use_precond {false}, numer_grad {false};


    double mu, sigma, trace, trace_eps, eps;
    VectorXd W, prevW, h;
    SparseMatrix<double,0,int> A;
    
    Operator *ope;
    Var *var;

    // solver
    lu_sparse_solver solver_K;
    cholesky_solver  solver_Q; // Q = KT diag(1/SV) K

public:
    Latent(Rcpp::List latent_in) 
    : n_reg   ( Rcpp::as< unsigned > (latent_in["n_reg"]) ),
      
      mu        (0),
      sigma     (1),
      trace     (0),
      trace_eps (0),
      eps       (0.001), 
      
      W       (n_reg),
      prevW   (n_reg),
      h       (VectorXd::Constant(n_reg, 1)),
      A       (Rcpp::as< SparseMatrix<double,0,int> > (latent_in["A"]))
    {
        // Init opt. flag
        opt_mu      = Rcpp::as<bool>        (latent_in["opt_mu"]);
        opt_sigma   = Rcpp::as<bool>        (latent_in["opt_sigma"]);
        opt_kappa   = Rcpp::as<bool>        (latent_in["opt_kappa"]);
        opt_var     = Rcpp::as<bool>        (latent_in["opt_var"]);
        use_precond = Rcpp::as<bool>        (latent_in["use_precond"]);
        numer_grad  = Rcpp::as<bool>        (latent_in["numer_grad"]);

        // Init var
        Rcpp::List var_in = Rcpp::as<Rcpp::List> (latent_in["var_in"]);
        string type       = Rcpp::as<string>     (var_in["type"]);
        // Set initial values
        Rcpp::List init_value = Rcpp::as<Rcpp::List> (latent_in["init_value"]);
        mu           = Rcpp::as<double>  (init_value["mu"]);
        sigma        = Rcpp::as<double>  (init_value["sigma"]);
        double nu    = Rcpp::as<double>  (init_value["nu"]);
        if (type == "ind_IG") {
            var = new ind_IG(n_reg, nu);
        }
    }
    ~Latent() {}

    /*  1 Model itself   */
    unsigned getSize() const                  {return n_reg; } 
    unsigned getThetaSize() const             {return n_paras; } 
    SparseMatrix<double, 0, int>& getA()      {return A; }
    
    const VectorXd& getW()  const             {return W; }
    void            setW(const VectorXd& W)   { prevW = this->W; this->W = W; }

    VectorXd getMean() const { return mu * (getV() - h); }

    /*  2 Variance component   */
    VectorXd getSV() const { VectorXd V=getV(); return (V*pow(sigma,2)); }
    const VectorXd& getV()     const { return var->getV(); }
    const VectorXd& getPrevV() const { return var->getPrevV(); }
    virtual void sample_cond_V()=0;

    /*  3 Operator component   */
    SparseMatrix<double, 0, int>& getK()    { return ope->getK(); }
    SparseMatrix<double, 0, int>& get_dK()  { return ope->get_dK(); }
    SparseMatrix<double, 0, int>& get_d2K() { return ope->get_d2K(); }

    // Paramter kappa
    double getKappa() const       {return ope->getKappa(); } 
    void   setKappa(double kappa) {
        ope->setKappa(kappa);
        
        if (!numer_grad) compute_trace(); 
    } 

    /* 4 for optimizer */
    const VectorXd getTheta() const;
    const VectorXd getGrad();
    void           setTheta(const VectorXd&);

    // Parameter: kappa
    virtual double get_theta_kappa() const=0;
    virtual void   set_theta_kappa(double v)=0;
    virtual double grad_theta_kappa()=0;

    virtual double function_kappa(double kappa);    
    
    void compute_trace() {
        SparseMatrix<double> K = getK();
        SparseMatrix<double> dK = get_dK();
// compute trace
        solver_K.computeKKT(K);

// auto timer_trace = std::chrono::steady_clock::now();
        SparseMatrix<double> M = dK.transpose() * K;
        trace = solver_K.trace(M);
// std::cout << "time for the trace (ms): " << since(timer_trace).count() << std::endl;   

        // update trace_eps if using hessian
        if ((!numer_grad) && (use_precond)) {
            SparseMatrix<double> K = ope->getK(eps);
            SparseMatrix<double> dK = ope->get_dK(eps);
            SparseMatrix<double> M = dK.transpose() * K;
            trace_eps = solver_K.trace(M);
        }
    };
    
    // Parameter: nu
    virtual double get_theta_var() const   { return var->get_theta_var(); }
    virtual void   set_theta_var(double v) { var->set_theta_var(v); }
    virtual double grad_theta_var()        { 
        return var->grad_theta_var();
    }

    // Parameter: sigma
    virtual double get_theta_sigma() const        { return log(sigma); }
    virtual void   set_theta_sigma(double theta)  { this->sigma = exp(theta); }
    virtual double grad_theta_sigma();

    // Parameter: mu
    double get_mu() const     {return mu;} 
    void   set_mu(double mu) {this->mu = mu;} 
    virtual double grad_mu();
};

/*    Optimizer related    */
inline const VectorXd Latent::getTheta() const {
    VectorXd theta (n_paras);

    theta(0) = get_theta_kappa();
    theta(1) = get_mu();         
    theta(2) = get_theta_sigma();
    theta(3) = get_theta_var();  
    
    return theta;
}

inline const VectorXd Latent::getGrad() {
    VectorXd grad (n_paras);
auto grad1 = std::chrono::steady_clock::now();
    if (opt_kappa) grad(0) = grad_theta_kappa();         else grad(0) = 0;
    if (opt_mu)    grad(1) = grad_mu();                  else grad(1) = 0;
    if (opt_sigma) grad(2) = grad_theta_sigma();         else grad(2) = 0;
    if (opt_var)   grad(3) = grad_theta_var();           else grad(3) = 0;

std::cout << "grad_kappa (ms): " << since(grad1).count() << std::endl;   
    return grad;
}

inline void Latent::setTheta(const VectorXd& theta) {
    if (opt_kappa) set_theta_kappa(theta(0)); 
    if (opt_mu)    set_mu(theta(1)); 
    if (opt_sigma) set_theta_sigma(theta(2)); 
    if (opt_var)   set_theta_var(theta(3)); 
}

// sigma>0 -> theta=log(sigma)
// return the gradient wrt. theta, theta=log(sigma)
inline double Latent::grad_theta_sigma() {
    SparseMatrix<double> K = getK();
    VectorXd V = getV();
    VectorXd prevV = getPrevV();

    double msq = (K*W - mu*(V-h)).cwiseProduct(V.cwiseInverse()).dot(K*W - mu*(V-h));
    double msq2 = (K*W - mu*(prevV-h)).cwiseProduct(prevV.cwiseInverse()).dot(K*W - mu*(prevV-h));

    double grad = - n_reg / sigma + pow(sigma, -3) * msq;

    // hessian using prevous V
    double hess = n_reg / pow(sigma, 2) - 3 * pow(sigma, -4) * msq2;
    
    // grad. wrt theta
std::cout << "******* grad of sigma is: " << grad / (hess * sigma + grad) * n_reg << std::endl;   

    return grad / (hess * sigma + grad);
}


inline double Latent::grad_mu() {
    SparseMatrix<double> K = getK();
    VectorXd V = getV();
    VectorXd inv_V = V.cwiseInverse();
    
    VectorXd prevV = getPrevV();
    VectorXd prev_inv_V = prevV.cwiseInverse();

    // double hess_mu = -(Vmh).transpose() * inv_SV.asDiagonal() * Vmh;  // get previous V
    // double g = (Vmh).transpose() * inv_SV.asDiagonal() * (K*W - mu*Vmh);
    double hess = -pow(sigma,-2) * (prevV-h).cwiseProduct(prev_inv_V).dot(prevV-h);
    double grad = pow(sigma,-2) * (V-h).cwiseProduct(inv_V).dot(K*W - mu*(V-h));

    return grad / hess;
}

// W|V ~ N(K^-1 mu(V-h), sigma^2 K-1 diag(V) K-T)
inline double Latent::function_kappa(double eps) {
    SparseMatrix<double> K = ope->getK(eps);

    VectorXd V = getV();
    VectorXd SV = getSV();

    SparseMatrix<double> Q = K.transpose() * SV.cwiseInverse().asDiagonal() * K;
    
    solver_Q.compute(Q);
    
    VectorXd tmp = K * prevW - mu*(V-h);

    double l = 0.5 * solver_Q.logdet() 
               - 0.5 * tmp.cwiseProduct(SV.cwiseInverse()).dot(tmp);
                // - 0.5 * (prevW-mean).transpose() * Q * (prevW-mean);

    return l;
}


#endif