// Generated by using Rcpp::compileAttributes() -> do not edit by hand
// Generator token: 10BE3573-1514-4C36-9D1C-5A225CD40393

#include <RcppEigen.h>
#include <Rcpp.h>

using namespace Rcpp;

#ifdef RCPP_USE_GLOBAL_ROSTREAM
Rcpp::Rostream<true>&  Rcpp::Rcout = Rcpp::Rcpp_cout_get();
Rcpp::Rostream<false>& Rcpp::Rcerr = Rcpp::Rcpp_cerr_get();
#endif

// predict_cpp
Rcpp::List predict_cpp(Rcpp::List in_list);
RcppExport SEXP _ngme2_predict_cpp(SEXP in_listSEXP) {
BEGIN_RCPP
    Rcpp::RObject rcpp_result_gen;
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< Rcpp::List >::type in_list(in_listSEXP);
    rcpp_result_gen = Rcpp::wrap(predict_cpp(in_list));
    return rcpp_result_gen;
END_RCPP
}
// test_init
Rcpp::List test_init(Rcpp::List in_list);
RcppExport SEXP _ngme2_test_init(SEXP in_listSEXP) {
BEGIN_RCPP
    Rcpp::RObject rcpp_result_gen;
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< Rcpp::List >::type in_list(in_listSEXP);
    rcpp_result_gen = Rcpp::wrap(test_init(in_list));
    return rcpp_result_gen;
END_RCPP
}
// rGIG_cpp
Eigen::VectorXd rGIG_cpp(Eigen::VectorXd p, Eigen::VectorXd a, Eigen::VectorXd b, unsigned long seed);
RcppExport SEXP _ngme2_rGIG_cpp(SEXP pSEXP, SEXP aSEXP, SEXP bSEXP, SEXP seedSEXP) {
BEGIN_RCPP
    Rcpp::RObject rcpp_result_gen;
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< Eigen::VectorXd >::type p(pSEXP);
    Rcpp::traits::input_parameter< Eigen::VectorXd >::type a(aSEXP);
    Rcpp::traits::input_parameter< Eigen::VectorXd >::type b(bSEXP);
    Rcpp::traits::input_parameter< unsigned long >::type seed(seedSEXP);
    rcpp_result_gen = Rcpp::wrap(rGIG_cpp(p, a, b, seed));
    return rcpp_result_gen;
END_RCPP
}

RcppExport SEXP run_testthat_tests(SEXP);

static const R_CallMethodDef CallEntries[] = {
    {"_ngme2_predict_cpp", (DL_FUNC) &_ngme2_predict_cpp, 1},
    {"_ngme2_test_init", (DL_FUNC) &_ngme2_test_init, 1},
    {"_ngme2_rGIG_cpp", (DL_FUNC) &_ngme2_rGIG_cpp, 4},
    {"run_testthat_tests", (DL_FUNC) &run_testthat_tests, 1},
    {NULL, NULL, 0}
};

RcppExport void R_init_ngme2(DllInfo *dll) {
    R_registerRoutines(dll, NULL, CallEntries, NULL, NULL);
    R_useDynamicSymbols(dll, FALSE);
}
