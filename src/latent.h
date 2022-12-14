/*
    latent class:
    basically, providing get, grad, set (theta_K, ...)
        1. theta_K is unbounded
        2. theta_mu
        3. theta_sigma
        4. theta_V
*/

#ifndef NGME_LATANT_H
#define NGME_LATANT_H

// #include<Eigen/IterativeLinearSolvers>
#include <string>
#include <iostream>
#include <vector>
#include <cmath>
#include <Rcpp.h>
#include <RcppEigen.h>
#include <Eigen/SparseLU>
#include <Eigen/Dense>
#include <random>
#include <cmath>

#include "include/timer.h"
#include "include/solver.h"
#include "var.h"

using std::exp;
using std::log;
using std::pow;
using Eigen::SparseMatrix;
using Eigen::MatrixXd;
using Eigen::VectorXd;
using std::vector;

enum Latent_fix_flag {
    latent_fix_theta_K,
    latent_fix_W,
    latent_fix_theta_mu,
    latent_fix_theta_sigma
};
const int LATENT_FIX_FLAG_SIZE = 4;

class Latent {
protected:
    std::mt19937 latent_rng;
    string model_type, noise_type;
    bool debug;
    int W_size, V_size, n_params, n_var {1}; // n_params=n_theta_K + n_theta_mu + n_theta_sigma + n_var

    // operator K related
    VectorXd theta_K;

    bool use_num_dK {false};
    SparseMatrix<double, 0, int> K, dK, d2K;
    int n_theta_K;

    bool fix_flag[LATENT_FIX_FLAG_SIZE] {0};

    bool use_precond {false}, numer_grad {false};
    bool symmetricK {false};

    // mu and sigma
    MatrixXd B_mu, B_sigma;
    VectorXd theta_mu, theta_sigma;
    VectorXd mu, sigma;
    int n_theta_mu, n_theta_sigma;

    // eps for numerical gradient.
    double trace, trace_eps, eps;

    VectorXd W, prevW, h;
    SparseMatrix<double,0,int> A;

    Var var;

    // solver
    cholesky_solver chol_solver_K;
    lu_sparse_solver lu_solver_K;
    bool use_iter_solver {false};
    iterative_solver CG_solver_K;

    cholesky_solver solver_Q; // Q = KT diag(1/SV) K

    // record trajectory
    vector<vector<double>> theta_K_traj;
    vector<vector<double>> theta_mu_traj;
    vector<vector<double>> theta_sigma_traj;
    vector<double>   theta_V_traj;
public:
    Latent(const Rcpp::List&, unsigned long seed);
    virtual ~Latent() {}

    // change of variable
    virtual VectorXd bound_to_unbound_K(const VectorXd&) const {return theta_K;}
    virtual VectorXd unbound_to_bound_K(const VectorXd&) const {return theta_K;}
    virtual void update_each_iter()=0;

    /*  1 Model itself   */
    int get_W_size() const                  {return W_size; }
    int get_V_size() const                  {return V_size; }
    int get_n_params() const                {return n_params; }
    SparseMatrix<double, 0, int>& getA()    {return A; }

    const VectorXd& getW()  const           {return W; }
    void            setW(const VectorXd& newW) {
        if (!fix_flag[latent_fix_W]) {
            prevW = W; W = newW;
        }
    }
    const VectorXd& getPrevW()  const       {return prevW; }
    void setPrevW(const VectorXd& W) { prevW = W; }

    VectorXd getMean() const { return mu.cwiseProduct(getV()-h); }

    /*  2 Variance component   */
    const VectorXd& getV()     const { return var.getV(); }
    const VectorXd& getPrevV() const { return var.getPrevV(); }
    void setPrevV(const VectorXd& V) { var.setPrevV(V); }

    VectorXd getSV() const {
        VectorXd V = getV();
        return (sigma.array().pow(2).matrix().cwiseProduct(V));
    }
    VectorXd getPrevSV() const {
        VectorXd prevV = getPrevV();
        return (sigma.array().pow(2).matrix().cwiseProduct(prevV));
    }

    void sample_V() {
        var.sample_V();
    }

    void sample_cond_V() {
        VectorXd tmp = (K * W + mu.cwiseProduct(h));
        VectorXd a_inc_vec = mu.cwiseQuotient(sigma).array().pow(2);
        VectorXd b_inc_vec = tmp.cwiseQuotient(sigma).array().pow(2);
        var.sample_cond_V(a_inc_vec, b_inc_vec);
    }

    /*  3 Operator component   */
    SparseMatrix<double, 0, int>& getK()    { return K; }

    // get K/dK using different parameter
    virtual SparseMatrix<double, 0, int> getK(const VectorXd&) const=0;
    virtual SparseMatrix<double, 0, int> get_dK(int, const VectorXd&) const=0;

    // variants of getK and get_dK
    // get_dK wrt. paramter_K[i]
    SparseMatrix<double, 0, int> get_dK_by_index(int index) const {
        return get_dK(index, theta_K);
    }

    // param(pos) += eps;  getK(param);
    SparseMatrix<double, 0, int> getK_by_eps(int pos, double eps) {
        VectorXd tmp = theta_K;
        tmp(pos) = tmp(pos) + eps;
        return getK(tmp);
    }

    SparseMatrix<double, 0, int> get_dK_by_eps(int index, int pos, double eps) {
        VectorXd tmp = theta_K;
        tmp(pos) = tmp(pos) + eps;
        return get_dK(index, tmp);
    }

    /* 4 for optimizer */
    const VectorXd get_parameter() const;
    const VectorXd get_grad();
    void           set_parameter(const VectorXd&);
    void           finishOpt(int i) {fix_flag[i] = 0; }


    // deprecated
    // virtual double function_kappa(double eps);
    virtual double function_K(VectorXd& parameter);

    // used for general case
    virtual double function_K(SparseMatrix<double>& K);
    virtual VectorXd numerical_grad(); // given eps

    // update the trace value
    void compute_trace() {
        if (W_size != V_size) return;

// compute trace
// auto timer_trace = std::chrono::steady_clock::now();

        SparseMatrix<double> K = getK(theta_K);
        SparseMatrix<double> dK = get_dK_by_index(0);
        if (!use_iter_solver) {
            if (!symmetricK) {
                lu_solver_K.computeKTK(K);
                trace = lu_solver_K.trace(dK);
            } else {
                chol_solver_K.compute(K);
                trace = chol_solver_K.trace(dK);
            }
        }
// Rcpp::Rcout << "eps K  in 1= " << K << std::endl;
// Rcpp::Rcout << "eps dK in 1= " << dK << std::endl;
// Rcpp::Rcout << "trace  in 1= " << trace << std::endl;
        // else {
        //     if (!symmetricK) {
        //         // BiCG solver
        //         throw("Not implemented yet");
        //     } else {
        //         SparseMatrix<double, RowMajor> KK = K;
        //         CG_solver_K.compute(K);
        //         trace = CG_solver_K.trace(M);
        //     }
        // }

// Rcpp::Rcout << "trace ====== " << trace << std::endl;
// Rcpp::Rcout << "time for the trace (ms): " << since(timer_trace).count() << std::endl;

        // update trace_eps if using hessian
        if ((!numer_grad) && (use_precond)) {
            SparseMatrix<double> K = getK_by_eps(0, eps);
            SparseMatrix<double> dK = get_dK_by_eps(0, 0, eps);
            SparseMatrix<double> M = dK;

            if (!use_iter_solver) {
                if (!symmetricK) {
                    lu_solver_K.computeKTK(K);
                    trace_eps = lu_solver_K.trace(M);
                } else {
                    chol_solver_K.compute(K);
                    trace_eps = chol_solver_K.trace(M);
                }
            }
// Rcpp::Rcout << "eps K  in 2 = " << K << std::endl;
// Rcpp::Rcout << "eps dK in 2 = " << dK << std::endl;
// Rcpp::Rcout << "trace_eps in 2 = " << trace_eps << std::endl;
            // else {
            //     if (!symmetricK) {
            //         // BiCG solver
            //         throw("Not implemented yet");
            //     } else {
            //         CG_solver_K.compute(K);
            //         trace_eps = CG_solver_K.trace(M);
            //     }
            // }
        }
    };

    virtual VectorXd grad_theta_K() { return numerical_grad(); }
    virtual VectorXd grad_theta_mu();
    virtual VectorXd grad_theta_sigma();

    virtual void   set_theta_var(double v) { var.set_theta_var(v); }

    // Output
    // virtual Rcpp::List get_estimates() const=0;
    Rcpp::List output() const;

    void record_traj() {
        for (int i=0; i < theta_K.size(); i++)
            theta_K_traj[i].push_back(theta_K(i));
        for (int i=0; i < theta_mu.size(); i++)
            theta_mu_traj[i].push_back(theta_mu(i));
        for (int i=0; i < theta_sigma.size(); i++)
            theta_sigma_traj[i].push_back(theta_sigma(i));
        theta_V_traj.push_back(var.get_theta_V());
    }
};

/*    Optimizer related    */
inline const VectorXd Latent::get_parameter() const {
// if (debug) Rcpp::Rcout << "Start latent get parameter"<< std::endl;

    VectorXd parameter (n_params);
        parameter.segment(0, n_theta_K)                         = theta_K;
        parameter.segment(n_theta_K, n_theta_mu)                = theta_mu;
        parameter.segment(n_theta_K+n_theta_mu, n_theta_sigma)  = theta_sigma;
        parameter(n_theta_K+n_theta_mu+n_theta_sigma)           = var.get_unbound_theta_V();

// if (debug) Rcpp::Rcout << "parameter= " << parameter << std::endl;
// if (debug) Rcpp::Rcout << "End latent get parameter"<< std::endl;
    return parameter;
}

inline const VectorXd Latent::get_grad() {
// if (debug) Rcpp::Rcout << "Start latent gradient"<< std::endl;
    VectorXd grad (n_params);

// auto grad1 = std::chrono::steady_clock::now();
    if (!fix_flag[latent_fix_theta_K])     grad.segment(0, n_theta_K)                        = grad_theta_K();         else grad.segment(0, n_theta_K) = VectorXd::Constant(n_theta_K, 0);
    if (!fix_flag[latent_fix_theta_mu])    grad.segment(n_theta_K, n_theta_mu)               = grad_theta_mu();        else grad.segment(n_theta_K, n_theta_mu) = VectorXd::Constant(n_theta_mu, 0);
    if (!fix_flag[latent_fix_theta_sigma]) grad.segment(n_theta_K+n_theta_mu, n_theta_sigma) = grad_theta_sigma();     else grad.segment(n_theta_K+n_theta_mu, n_theta_sigma) = VectorXd::Constant(n_theta_sigma, 0);
    grad(n_theta_K+n_theta_mu+n_theta_sigma)  = var.grad_theta_var();

// DEBUG: checking grads
if (debug) {
    // Rcpp::Rcout << "gradient= " << grad << std::endl;
    // Rcpp::Rcout << "one latent gradient time " << since(grad1).count() << std::endl;
}
    return grad;
}

inline void Latent::set_parameter(const VectorXd& theta) {
// if (debug) Rcpp::Rcout << "Start latent set parameter"<< std::endl;
    theta_K  = theta.segment(0, n_theta_K);
    theta_mu = theta.segment(n_theta_K, n_theta_mu);
    theta_sigma = theta.segment(n_theta_K+n_theta_mu, n_theta_sigma);
    var.set_theta_var   (theta(n_theta_K+n_theta_mu+n_theta_sigma));

    // update
    mu = (B_mu * theta_mu);
    sigma = (B_sigma * theta_sigma).array().exp();
    update_each_iter();

    // record
    record_traj();
}

// subclasses
class AR : public Latent {
private:
    SparseMatrix<double, 0, int> G, C;
public:
    AR(Rcpp::List& model_list, unsigned long seed);

    SparseMatrix<double> getK(const VectorXd& alpha) const;
    SparseMatrix<double> get_dK(int index, const VectorXd& alpha) const;
    VectorXd grad_theta_K();
    void update_each_iter();
    void update_num_dK();
    double th2a(double th) const {return (-1 + 2*exp(th) / (1+exp(th)));}
    double a2th(double k) const {return (log((-1-k)/(-1+k)));}

    VectorXd unbound_to_bound_K(const VectorXd& theta_K) const {
        VectorXd alpha (1);
        alpha(0) = th2a(theta_K(0));
        return alpha;
    }
    VectorXd bound_to_unbound_K(const VectorXd& alpha) const {
        VectorXd theta_K (1);
        theta_K(0) = a2th(alpha(0));
        return theta_K;
    }
    // Rcpp::List get_estimates() const {
    //     return Rcpp::List::create(
    //         Rcpp::Named("alpha")        = theta_K(0),
    //         Rcpp::Named("theta.mu")     = theta_mu,
    //         Rcpp::Named("theta.sigma")  = theta_sigma,
    //         Rcpp::Named("theta.noise")  = var.get_theta_V()
    //     );
    // }
};


class Matern : public Latent {
private:
    SparseMatrix<double, 0, int> G, C;
    int alpha;
    VectorXd Cdiag;
public:
    Matern(Rcpp::List& model_list, unsigned long seed);
    SparseMatrix<double> getK(const VectorXd& alpha) const;
    SparseMatrix<double> get_dK(int index, const VectorXd& alpha) const;
    VectorXd grad_theta_K();
    void update_each_iter();
    void update_num_dK();
    double th2k(double th) const {return exp(th);}
    double k2th(double k) const {return log(k);}

    VectorXd unbound_to_bound_K(const VectorXd& theta_K) const {
        VectorXd kappa (1);
        kappa(0) = th2k(theta_K(0));
        return kappa;
    }
    VectorXd bound_to_unbound_K(const VectorXd& kappa) const {
        VectorXd theta_K (1);
        theta_K(0) = k2th(kappa(0));
        return theta_K;
    }
    // Rcpp::List get_estimates() const {
    //     return Rcpp::List::create(
    //         Rcpp::Named("kappa")        = theta_K(0),
    //         Rcpp::Named("theta.mu")     = theta_mu,
    //         Rcpp::Named("theta.sigma")  = theta_sigma,
    //         Rcpp::Named("theta.noise")  = var.get_theta_V()
    //     );
    // }
};

class Matern_ns : public Latent {
private:
    SparseMatrix<double, 0, int> G, C;
    int alpha;
    MatrixXd Bkappa;
    VectorXd Cdiag;
public:
    Matern_ns(Rcpp::List& model_list, unsigned long seed);
    SparseMatrix<double> getK(const VectorXd& alpha) const;
    SparseMatrix<double> get_dK(int index, const VectorXd& alpha) const;
    VectorXd grad_theta_K();
    void update_each_iter();

    // Rcpp::List get_estimates() const {
    //     return Rcpp::List::create(
    //         Rcpp::Named("theta.kappa") = theta_K,
    //         Rcpp::Named("theta.mu")    = theta_mu,
    //         Rcpp::Named("theta.sigma") = theta_sigma,
    //         Rcpp::Named("theta.noise") = var.get_theta_V()
    //     );
    // }
};

#endif