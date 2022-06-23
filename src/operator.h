/*
    Operator class is for :
        1. store K_parameter, 
        2. compute K, dK.
*/

#ifndef NGME_OPERATOR_H
#define NGME_OPERATOR_H

#include <Rcpp.h>
#include <RcppEigen.h>
#include <Eigen/Dense>
#include <Eigen/Sparse>
#include <cassert>

using Eigen::SparseMatrix;
using Eigen::VectorXd;
using Eigen::MatrixXd;

class Operator {
protected:
    VectorXd parameter_K;
    int n_params; // how many parameters
    bool use_num_dK {false};

    SparseMatrix<double, 0, int> K, dK, d2K;
public:
    Operator(Rcpp::List ope_in)
    {
std::cout << "constructor of Operator" << std::endl;
        n_params = Rcpp::as<int> (ope_in["n_params"]);
        use_num_dK = Rcpp::as<bool> (ope_in["use_num_dK"]);
    };

    int get_n_params() const {return parameter_K.size(); }
    virtual VectorXd get_parameter() const {return parameter_K; } // kappa
    virtual void set_parameter(VectorXd parameter_K) {this->parameter_K = parameter_K;}

    // getter for K, dK, d2K
    SparseMatrix<double, 0, int>& getK()    {return K;}
    SparseMatrix<double, 0, int>& get_dK()  {return dK;}
    SparseMatrix<double, 0, int>& get_d2K() {return d2K;}

    // get K/dK using different parameter
    virtual SparseMatrix<double, 0, int> getK(VectorXd) const=0;
    virtual SparseMatrix<double, 0, int> get_dK(VectorXd) const=0;

    // param(pos) += eps;  getK(param);
    SparseMatrix<double, 0, int> getK(int pos, double eps) {
        VectorXd tmp = parameter_K;
        tmp(pos) += eps;
        return getK(tmp);
    }

    SparseMatrix<double, 0, int> get_dK(int pos, double eps) {
        VectorXd tmp = parameter_K;
        tmp(pos) += eps;
        return get_dK(tmp);
    }
    
};

#endif