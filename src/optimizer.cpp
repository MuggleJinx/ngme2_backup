#include "optimizer.h"


Rcpp::List Optimizer::sgd( Model& model,
                double stepsize, 
                double eps,
                bool precondioner,
                int iterations) {

    vector<VectorXd> x_traj;
    vector<VectorXd> grad_traj;

    int count = 0;
    VectorXd x = model.get_parameter();

    bool terminate = false;

    while (!terminate)
    {
        count += 1;
        
        VectorXd grad = model.grad();
        
        if (precondioner) {
            MatrixXd cond = model.precond();

            // update x <- x - stepsize * H^-1 * grad(x)
            x = x - stepsize * cond.selfadjointView<Eigen::Upper>().llt().solve(grad);
//            x = x - cond.selfadjointView<Eigen::Upper>().llt().solve(grad);
        } else {
            x = x - stepsize * grad;
        }

        // record x and grad
        x_traj.push_back(x);
        grad_traj.push_back(grad);

        model.set_parameter(x);

        // to-do: criteria of eps
        if ((grad.norm() <= pow(10, -6)) || (count > iterations))
            terminate = true;

    }
    return Rcpp::List::create(Rcpp::Named("grad_traj") = grad_traj,
                              Rcpp::Named("x_traj") = x_traj);
}

