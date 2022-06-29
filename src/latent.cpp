// grad_theta_mu
// grad_theta_sigma
// function_K
// function_kappa
// numerical_grad

#include "latent.h"

Latent::Latent(Rcpp::List latent_in) : 
      debug         (Rcpp::as< bool >        (latent_in["debug"])),
      n_mesh        (Rcpp::as< int >         (latent_in["n_mesh"])),
    
      B_mu          (Rcpp::as< MatrixXd >    (latent_in["B_mu"])),
      B_sigma       (Rcpp::as< MatrixXd >    (latent_in["B_sigma"])),
      
      n_mu          (B_mu.cols()),
      n_sigma       (B_sigma.cols()),

      trace         (0),
      trace_eps     (0),
      eps           (0.001), 
      
      W             (n_mesh),
      prevW         (n_mesh),
      h             (Rcpp::as< VectorXd >                     (latent_in["h"])),
      A             (Rcpp::as< SparseMatrix<double,0,int> >   (latent_in["A"]))
    {
if (debug) std::cout << "constructor of latent" << std::endl;
        // general input
            string var_type = Rcpp::as<string>     (latent_in["var_type"]);
        
        Rcpp::List control_f = Rcpp::as<Rcpp::List> (latent_in["control_f"]);
            fix_flag[0]   = Rcpp::as<int>        (control_f["fix_operator"]);
            fix_flag[1]   = Rcpp::as<int>        (control_f["fix_mu"]);
            fix_flag[2]   = Rcpp::as<int>        (control_f["fix_sigma"]);
            fix_flag[3]   = Rcpp::as<int>        (control_f["fix_noise"]);

            use_precond  = Rcpp::as<bool>        (control_f["use_precond"] );
            numer_grad   = Rcpp::as<bool>        (control_f["numer_grad"]) ;
            eps          = Rcpp::as<double>      (control_f["eps"]) ;
            
        // starting values
        Rcpp::List start = Rcpp::as<Rcpp::List> (latent_in["start"]);
            theta_mu = Rcpp::as< VectorXd >    (start["theta_mu"]);
                set_theta_mu(theta_mu);
            theta_sigma = Rcpp::as< VectorXd >    (start["theta_sigma"]);
                set_theta_sigma(theta_sigma);
            
        Rcpp::List ope_in = Rcpp::as<Rcpp::List> (latent_in["operator_in"]);
            n_ope = ope_in["n_params"];
            n_noise = 1;
            n_params = n_ope + n_mu + n_sigma + n_noise;

        
        double theta_noise = Rcpp::as< double > (start["theta_noise"]);                
        // construct noise
        Rcpp::List var_in = Rcpp::as<Rcpp::List> (latent_in["var_in"]);
        if (var_type == "nig") {
            var = new ind_IG(theta_noise, n_mesh, h);
        } else if (var_type == "normal") {
            var = new normal(n_mesh, h);
            // fix mu to be 0
            fix_flag[1] = 1;
        }
        
        // set V
        if (start["V"] != R_NilValue) {
            VectorXd V = Rcpp::as< VectorXd >    (start["V"]);
            var->setV(V);
        }

if (debug) std::cout << "finish constructor of latent" << std::endl;
}

VectorXd Latent::grad_theta_mu() {
if (debug) std::cout << "Start mu gradient"<< std::endl;   
    VectorXd result(n_mu);

    SparseMatrix<double> K = getK();
    VectorXd V = getV();
    VectorXd inv_V = V.cwiseInverse();
    VectorXd prevV = getPrevV();
    VectorXd prev_inv_V = prevV.cwiseInverse();
    
    // if (n_mu == 1 && n_sigma == 1) {
    //     // stationary case
    //     double grad = pow(sigma(0),-2) * (V-h).cwiseProduct(inv_V).dot(K*W - mu(0) * (V-h));
    //     double hess = -pow(sigma(0),-2) * (prevV-h).cwiseProduct(prev_inv_V).dot(prevV-h);

    //     result(0) = grad / hess;
    // }
    // else {
        VectorXd grad (n_mu);
        for (int l=0; l < n_mu; l++) {
            grad(l) = (V-h).cwiseProduct( B_mu.col(l).cwiseQuotient(getSV()) ).dot(K*W - mu.cwiseProduct(V-h));
        }

        result = - 1.0 / n_mesh * grad;
    // }

if (debug) {
// std::cout << "grad of mu=" << grad <<std::endl;
// std::cout << "hess of mu=" << hess <<std::endl;
}
    return result;
}


// return the gradient wrt. theta, theta=log(sigma)
inline VectorXd Latent::grad_theta_sigma() {
    SparseMatrix<double> K = getK();
    VectorXd V = getV();
    VectorXd prevV = getPrevV();

    VectorXd result(n_sigma);

    if (n_sigma == 1) {
        // stationary case
        // double msq = (K*W - mu(0)*(V-h)).cwiseProduct(V.cwiseInverse()).dot(K*W - mu(0)*(V-h));
        if (debug) std::cout << "Using stationary sigma"<< std::endl;   
        double msq = (K*W - mu.cwiseProduct(V-h)).array().pow(2).matrix().dot(V.cwiseInverse());
        double grad = - n_mesh / sigma(0) + pow(sigma(0), -3) * msq;

        // hessian using prevous V
        // double msq2 = (K*prevW - mu(0)*(prevV-h)).cwiseProduct(prevV.cwiseInverse()).dot(K*prevW - mu(0)*(prevV-h));
        double msq2 = (K*prevW - mu.cwiseProduct(prevV-h)).array().pow(2).matrix().dot(prevV.cwiseInverse());
        double hess = n_mesh / pow(sigma(0), 2) - 3 * pow(sigma(0), -4) * msq2;
        
        // grad. wrt theta
        result(0) =  grad / (hess * sigma(0) + grad);
       // result(0) = -1.0 / n_mesh * grad * sigma(0);
    } else {

         if (debug) std::cout << "Using non-stationary sigma"<< std::endl;  

        // double msq = (K*W - mu.cwiseProduct(V-h)).cwiseProduct(V.cwiseInverse()).dot(K*W - mu(0)*(V-h));
        // VectorXd vsq = (K*W - mu.cwiseProduct(V-h)).array().pow(2);
        VectorXd vsq = (K*W - mu.cwiseProduct(V-h)).array().pow(2).matrix().cwiseProduct(V.cwiseInverse());
        VectorXd grad (n_sigma);
        // for (int l=0; l < n_sigma; l++) {
        //     VectorXd tmp1 = vsq.cwiseProduct(sigma.array().pow(-2).matrix()) - VectorXd::Constant(n_mesh, 1);
        //     VectorXd tmp2 = B_sigma.col(l).cwiseProduct(tmp1);
        //     grad(l) = tmp2.sum();
        // }

        // vector manner
        VectorXd tmp1 = vsq.cwiseProduct(sigma.array().pow(-2).matrix()) - VectorXd::Constant(n_mesh, 1);
        grad = B_sigma.transpose() * tmp1;

        VectorXd prev_vsq = (K*prevW - mu.cwiseProduct(prevV-h)).array().pow(2).matrix().cwiseProduct(prevV.cwiseInverse());
        MatrixXd hess (n_sigma, n_sigma);
        VectorXd tmp3 = -2*prev_vsq.cwiseProduct(sigma.array().pow(-2).matrix());

        hess = B_sigma.transpose() * tmp3.asDiagonal() * B_sigma;

        result = - 1.0 / n_mesh * grad;
        // result = hess.llt().solve(grad);
    }

    return result;
}

// function_K(params += ( 0,0,eps,0,0) )
double Latent::function_K(VectorXd parameter) {
    assert(parameter.size()==ope->get_n_params());
    SparseMatrix<double> K = ope->getK(parameter);

    VectorXd V = getV();
    VectorXd SV = getSV();

    SparseMatrix<double> Q = K.transpose() * SV.cwiseInverse().asDiagonal() * K;
    VectorXd tmp = K * W - mu.cwiseProduct(V-h);

    double l;
    if (!symmetricK) {
        solver_Q.compute(Q);
        l = 0.5 * solver_Q.logdet() 
               - 0.5 * tmp.cwiseProduct(SV.cwiseInverse()).dot(tmp);
    } else {
        chol_solver_K.compute(K);
        l = chol_solver_K.logdet()
            - 0.5 * tmp.cwiseProduct(SV.cwiseInverse()).dot(tmp);
    }


// auto timer_computeg2 = std::chrono::steady_clock::now();
// time_compute_g2 += since(timer_computeg2).count();

    return l;
}

// only for stationary case, delete later
// W|V ~ N(K^-1 mu(V-h), sigma^2 K-1 diag(V) K-T)
double Latent::function_kappa(double eps) {
    SparseMatrix<double> K = ope->getK(0, eps);

    VectorXd V = getV();
    VectorXd SV = getSV();

    SparseMatrix<double> Q = K.transpose() * SV.cwiseInverse().asDiagonal() * K;
    
    solver_Q.compute(Q);
    
    VectorXd tmp = K * W - mu.cwiseProduct(V-h);

    double l = 0.5 * solver_Q.logdet() 
               - 0.5 * tmp.cwiseProduct(SV.cwiseInverse()).dot(tmp);
                // - 0.5 * (prevW-mean).transpose() * Q * (prevW-mean);

    return l;
}


// numerical gradient for K parameters
VectorXd Latent::numerical_grad() {
std::cout << "start numerical gradient" <<std::endl;
    int n_ope = ope->get_n_params();
    VectorXd params = ope->get_parameter();
    double val = function_K(params);

    VectorXd grad (n_ope);
    for (int i=0; i < n_ope; i++) {
        VectorXd params_add_eps = params;
            params_add_eps(i) += eps;
        double val_add_eps = function_K(params_add_eps);
        double num_g = (val_add_eps - val) / eps;
        
        if (!use_precond) {
            grad(i) = - num_g / n_mesh;
            // grad(i) = - num_g;
        } else {
            VectorXd params_minus_eps = params;
                params_minus_eps(i) -= eps;
            double val_minus_eps = function_K(params_minus_eps);
            double num_hess = (val_minus_eps + val_add_eps - 2*val) / pow(eps, 2);
            grad(i) = num_g / num_hess;
        }
    } 
    // return grad;
    return grad;
}