#ifndef NGME_OPERATOR_H
#define NGME_OPERATOR_H

#include <Rcpp.h>
#include <RcppEigen.h>
#include <Eigen/Dense>
#include <Eigen/Sparse>
#include <cassert>

using Eigen::SparseMatrix;
using Eigen::VectorXd;

class Operator {
protected:
    double kappa; // parameter
    SparseMatrix<double, 0, int> K, dK, d2K;
public:
    Operator(): kappa(1) {};
    Operator(double kappa): kappa(kappa) {};

    // update matrices when kappa is updated
    virtual void update()=0;

    const double getKappa() const {return kappa; }
    void         setKappa(const double kappa) {this->kappa = kappa; update();}

    // getter for K, dK, d2K
    SparseMatrix<double, 0, int>& getK()    {return K;}
    SparseMatrix<double, 0, int>& get_dK()  {return dK;}
    SparseMatrix<double, 0, int>& get_d2K() {return d2K;}
};


// fit for AR and Matern 
class GC : public Operator {
private:
    SparseMatrix<double, 0, int> G, C;

public:
    GC(Rcpp::List ope_in) {
        kappa = ope_in["a_init"];
        G = Rcpp::as< SparseMatrix<double,0,int> > (ope_in["G"]);
        C = Rcpp::as< SparseMatrix<double,0,int> > (ope_in["C"]);
        
        K =  kappa * C + G;
        dK = C;
        d2K = 0 * C;
    }

    void update() {
        K = kappa * C + G;
    }

};

#endif