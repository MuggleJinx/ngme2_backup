#ifndef NGME_OPT_H
#define NGME_OPT_H

#include <Rcpp.h>
#include <RcppEigen.h>
#include <Eigen/Dense>
#include "model.h"

class Optimizer
{
private:
public:
    Rcpp::List sgd(Model& model,
                double stepsize,
                double eps,
                bool precondioner,
                int iterations);

    // provide model.get_stepsizes()
     Eigen::VectorXd sgd(
            Model& model,
            double eps,
            int iterations,
            double max_relative_step,
            double max_absolute_step);
};

#endif
