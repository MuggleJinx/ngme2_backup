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

#include "include/solver.h"
#include "include/MatrixAlgebra.h"
#include "model.h"
#include "latent.h"

#include "latents/ar1.h"

using Eigen::SparseMatrix;

class BlockModel : public Model {
protected:

/* config */
    unsigned n_gibbs;

// n_regs   = row(A1) + ... + row(An)
// n_paras = total paras need to optimize
    unsigned n_obs, n_latent, n_regs, n_paras;
    std::vector<Latent*> latents;

    double sigma_eps;
    
    VectorXd Y;

    SparseMatrix<double> A, K, dK, d2K;

    cholesky_solver chol_Q;
public:
    BlockModel() {}
    // main constructor
    BlockModel(const VectorXd Y, 
               const int n_paras,
               const int n_regs,
               Rcpp::List latents_in) 
    : n_gibbs(20),
      n_obs(Y.size()),
      n_latent(latents_in.size()), 
      n_regs(n_regs),
      n_paras(n_paras),
      sigma_eps(1),
      Y(Y), 
      A(n_obs, n_regs), 
      K(n_regs, n_regs), 
      dK(n_regs, n_regs),
      d2K(n_regs, n_regs)
    {
    // Init each latent model
    for (int i=0; i < n_latent; ++i) {
        Rcpp::List latent_in = Rcpp::as<Rcpp::List> (latents_in[i]);

        // construct acoording to models
        string type = latent_in["type"];
        if (type == "ar1") {
            latents.push_back(new AR(latent_in) );
        }
    }
    
    /* Init variables: h, A */
    int n = 0;
    for (std::vector<Latent*>::iterator it = latents.begin(); it != latents.end(); it++) {
        setSparseBlock(&A,   0, n, (*it)->getA());            
        n += (*it)->getSize();
    }
    assemble();
std::cout << "after assemble" << std::endl;    
        VectorXd inv_SV = VectorXd::Constant(n_regs, 1).cwiseQuotient(getSV());
std::cout << "invsv=" << inv_SV << std::endl;    
        SparseMatrix<double> QQ = K.transpose() * inv_SV.asDiagonal() * K + pow(sigma_eps, 2) * A.transpose() * A;
    chol_Q.analyze(QQ);
std::cout << "before sample" << std::endl;    
    sampleW_VY();
std::cout << "after sample" << std::endl;
    }

    void sampleW_VY();
    void sampleV_WY() {
        for (unsigned i=0; i < n_latent; i++) {
            (*latents[i]).sample_cond_V();
        }
    }

    void setW(const VectorXd&);

    VectorXd             get_parameter() const;
    void                 set_parameter(const VectorXd&);
    VectorXd             grad();
    SparseMatrix<double> precond() const;

    void assemble() {
        int n = 0;
        for (std::vector<Latent*>::iterator it = latents.begin(); it != latents.end(); it++) {
            setSparseBlock(&K,   n, n, (*it)->getK());      
            setSparseBlock(&dK,  n, n, (*it)->get_dK());   
            setSparseBlock(&d2K, n, n, (*it)->get_d2K()); 
            
            n += (*it)->getSize();
        }
    }

    // return mean
    VectorXd getMean() const {
        VectorXd mean (n_regs);
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
        VectorXd SV (n_regs);
        int pos = 0;
        for (std::vector<Latent*>::const_iterator it = latents.begin(); it != latents.end(); it++) {
            int size = (*it)->getSize();
// std::cout << "latentSV=" <<  latentSV << std::endl;    
            SV.segment(pos, size) = (*it)->getSV();
// std::cout << "sv after segment=" <<  SV << std::endl;    
            pos += size;
        }
// std::cout << "SV=" << SV << std::endl;    
        return SV;
    }

    VectorXd getW() const {
        VectorXd W (n_regs);
        int pos = 0;
        for (std::vector<Latent*>::const_iterator it = latents.begin(); it != latents.end(); it++) {
            int size = (*it)->getSize();
            W.segment(pos, size) = (*it)->getW();
            pos += size;
        }
        return W;
    }


// FOR TESTING
Rcpp::List testResult() {
Rcpp::List res;
A.makeCompressed();
K.makeCompressed();
dK.makeCompressed();
d2K.makeCompressed();
res["A"] = A;
res["K"] = K;
res["dK"] = dK;
res["d2K"] = d2K;
res["V"] = getSV();
res["W"] = getW();
return res;
}

};

// ---- inherited functions ------

inline VectorXd BlockModel::get_parameter() const {
    VectorXd thetas (n_paras);
    int pos = 0;
    for (std::vector<Latent*>::const_iterator it = latents.begin(); it != latents.end(); it++) {
        VectorXd theta = (*it)->getTheta();
        thetas.segment(pos, theta.size()) = theta;
        pos += theta.size();
    }
std::cout << "getTheta=" << thetas <<std::endl;
    return thetas;
}

inline VectorXd BlockModel::grad() {
    VectorXd avg_gradient (n_paras);
        avg_gradient = VectorXd::Constant(n_paras, 0);
    
    for (int i=0; i < n_gibbs; i++) {
        
        // stack grad
        VectorXd gradient (n_paras); 
            gradient = VectorXd::Constant(n_paras, 0);
        
        int pos = 0;
        for (std::vector<Latent*>::const_iterator it = latents.begin(); it != latents.end(); it++) {
            int theta_len = (*it)->getThetaSize();
            gradient.segment(pos, theta_len) = (*it)->getGrad();
            pos += theta_len;
        }
// std::cout << "1grad=" << gradient << std::endl;
        avg_gradient += gradient;
        sampleV_WY(); 
        sampleW_VY();
    }
    avg_gradient = (1.0/n_gibbs) * avg_gradient;

std::cout << "grad=" << avg_gradient <<std::endl;
    return avg_gradient;
}

inline void BlockModel::set_parameter(const VectorXd& Theta) {
    int pos = 0;
    for (std::vector<Latent*>::iterator it = latents.begin(); it != latents.end(); it++) {
        int theta_len = (*it)->getThetaSize();
        VectorXd theta = Theta.segment(pos, theta_len);
        (*it)->setTheta(theta);
        pos += theta_len;
    }
    assemble(); //update K,dK,d2K after
std::cout << "Theta=" << Theta <<std::endl;
}


inline SparseMatrix<double> BlockModel::precond() const {
    SparseMatrix<double> precond;

    return precond;
}



#endif