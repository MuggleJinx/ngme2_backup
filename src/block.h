/*
BlockModel
*/
#ifndef NGME_BLOCK_H
#define NGME_BLOCK_H

// #define EIGEN_USE_MKL_ALL
#include <Rcpp.h>
#include <RcppEigen.h>
#include <Eigen/Sparse>

#include <string>
#include <vector>
#include <iostream>
#include <random>
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
using Eigen::MatrixXd;

const int BLOCK_FIX_FLAG_SIZE = 6;

enum Block_fix_flag {
    block_fix_beta, block_fix_mu, block_fix_sigma, 
    block_fix_var, block_fix_V
};

class BlockModel : public Model {
protected:
// n_meshs = row(A1) + ... + row(An)
    // general
    unsigned long seed;
    MatrixXd X;
    VectorXd Y; 
    int n_meshs;
    string family;

    // Fixed effects and Measurement noise
    VectorXd beta;
    MatrixXd B_mu;
    VectorXd noise_mu, theta_mu;
    int n_theta_mu;

    MatrixXd B_sigma;
    VectorXd noise_sigma, theta_sigma;
    int n_theta_sigma;

    Var *var;

    int n_latent; // how mnay latent model
    int n_obs; // how many observation
    int n_params, n_la_params, n_feff, n_merr;  // number of total params, la params, ...

    // fix estimation
    bool fix_flag[BLOCK_FIX_FLAG_SIZE] {0};

    // controls
    int n_gibbs;
    bool opt_beta, kill_var;
    double kill_power, threshold, termination;

    SparseMatrix<double> A, K;      // not used: dK, d2K; 

    // debug
    bool debug, fix_merr;
    bool fixblockV;
    
    // optimize related
    VectorXd stepsizes, gradients;
    int counting {0};
    VectorXd indicate_threshold, steps_to_threshold;

    // No initializer
    std::vector<Latent*> latents;
    cholesky_solver chol_Q, chol_QQ;
    SparseLU<SparseMatrix<double> > LU_K;

    VectorXd fixedW;
    std::mt19937 rng;
public:
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
            sample_cond_block_V();
        }
if (debug) std::cout << "Finish burn in period." << std::endl;
    }

    void sampleW_VY();
    void sampleV_WY() {
        for (unsigned i=0; i < n_latent; i++) {
            (*latents[i]).sample_cond_V();
        }
    }
    void setW(const VectorXd&);
    
    /* Optimizer related */
    VectorXd             get_parameter() const;
    VectorXd             get_stepsizes() const {return stepsizes;}
    void                 set_parameter(const VectorXd&);
    VectorXd             grad();
    SparseMatrix<double> precond() const;
    
    void                 examine_gradient();
    void                 sampleW_V();

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

    VectorXd getV() const {
        VectorXd V (n_meshs);
        int pos = 0;
        for (std::vector<Latent*>::const_iterator it = latents.begin(); it != latents.end(); it++) {
            int size = (*it)->getSize();
            V.segment(pos, size) = (*it)->getV();
            pos += size;
        }
        
        return V;
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

    VectorXd get_residual() const {
        return Y - A * getW() - X * beta - (-VectorXd::Ones(n_obs) + var->getV()).cwiseProduct(noise_mu);
    }

    void sample_cond_block_V() {
        if (fix_flag[block_fix_V]) return;

        if (family == "nig") {
            VectorXd residual = get_residual();
            VectorXd a_inc_vec = noise_mu.cwiseQuotient(noise_sigma).array().pow(2);
            VectorXd b_inc_vec = (residual + (-VectorXd::Ones(n_obs) + var->getV()).cwiseProduct(noise_mu)).cwiseQuotient(noise_sigma).array().pow(2);
            var->sample_cond_V(a_inc_vec, b_inc_vec);
        }
    }

    // --------- Fixed effects and Measurement error  ------------
    VectorXd grad_beta();
    
    VectorXd get_theta_merr() const;
    VectorXd grad_theta_mu();
    VectorXd grad_theta_sigma();
    VectorXd grad_theta_merr();
    void set_theta_merr(const VectorXd& theta_merr);

    // return output
    Rcpp::List output() const;
};

// ---- inherited functions ------
/* the way structuring the parameter 
    latents[1].get_parameter 
        ...
    beta (fixed effects)
    measurement noise
*/

// Not Implemented
inline SparseMatrix<double> BlockModel::precond() const {
    SparseMatrix<double> precond;
    std::cout << "Not implemented \n";
    throw;
    return precond;
}

// VectorXd BlockModel::grad_block() const {
//     // beta
//     // noise_mu
//     // noise_sigma
//     // noise_var
// }

#endif