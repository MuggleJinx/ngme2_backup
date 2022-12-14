#' Generate control specifications for ngme
#'
#' @param burnin          burn-in iterations
#' @param iterations      optimizing terations
#' @param gibbs_sample    number of gibbs sampels
#' @param stepsize        stepsize
#' @param estimation      estimating the parameters
#'
#' @param n_parallel_chain number of parallel chains
#' @param stop_points     number of stop points for convergence check
#' @param exchange_VW     exchange last V and W in each chian
#' @param n_slope_check   number of stop points for regression
#' @param std_lim         maximum allowed standard deviation
#' @param trend_lim       maximum allowed slope
#' @param print_check_info print the convergence information
#'
#' @param opt_beta        logical, optimize fixed effect
#' @param fix_beta        logical, fix fixed effect
#'
#' @param max_relative_step   max relative step allowed in 1 iteration
#' @param max_absolute_step   max absolute step allowed in 1 iteration
#'
#' @param reduce_var      logical, reduce variace
#' @param reduce_power    numerical the power of reduce level
#' @param threshold       till when start to reduce the variance
#' @param window_size     numerical, length of window for final estimates
#'
#' @return list of control variables
#' @export
ngme_control <- function(
  burnin            = 100,
  iterations        = 100,
  gibbs_sample      = 5,
  stepsize          = 1,
  estimation        = TRUE,

  # parallel options
  n_parallel_chain  = 2,
  stop_points       = 10,
  exchange_VW       = TRUE,
  n_slope_check     = 3,
  std_lim           = 0.1,
  trend_lim         = 0.05,
  print_check_info  = TRUE,

  # opt options
  opt_beta          = TRUE,
  fix_beta          = FALSE,

  max_relative_step = 0.1,
  max_absolute_step = 0.5,

  # reduce variance after conv. check
  reduce_var        = FALSE,
  reduce_power      = 0.75,
  threshold         = 1e-5,
  window_size       = 1
) {
  if ((reduce_power <= 0.5) || (reduce_power > 1)) {
    stop("reduceVar should be in (0.5,1]")
  }

  if (stop_points > iterations) stop_points <- iterations

  control <- list(
    burnin            = burnin,
    iterations        = iterations,
    gibbs_sample      = gibbs_sample,
    stepsize          = stepsize,
    estimation        = estimation,
    n_parallel_chain  = n_parallel_chain,
    stop_points       = stop_points,
    exchange_VW       = exchange_VW,
    n_slope_check     = n_slope_check, # how many on regression check
    std_lim           = std_lim,
    trend_lim         = trend_lim,

    opt_beta          = opt_beta,
    fix_beta          = fix_beta,
    print_check_info  = print_check_info,

    # variance reduction
    max_relative_step = max_relative_step,
    max_absolute_step = max_absolute_step,
    reduce_var        = reduce_var,
    reduce_power      = reduce_power,
    threshold         = threshold,
    window_size       = window_size
  )

  class(control) <- "ngme_control"
  control
}

#' Generate control specifications for f function
#'
#' @param numer_grad    whether to use numerical gradient
#' @param use_precond   whether to use preconditioner
#' @param use_num_hess  whether to use numerical hessian
#' @param eps           eps for numerical gradient
#'
#' @return list of control variables
#' @export
ngme_control_f <- function(
  numer_grad    = FALSE,
  use_precond   = FALSE,
  use_num_hess  = TRUE,
  eps           = 0.01
  # use_iter_solver = FALSE
  ) {

  control <- list(
    numer_grad    = numer_grad,
    use_precond   = use_precond,
    use_num_hess  = use_num_hess,
    eps           = eps,
    use_iter_solver = FALSE
  )

  class(control) <- "ngme_control_f"
  control
}