# This file contains ngme noise specifications (NIG and Normal)

#' ngme noise specification
#'
#' Function for specifying ngme noise.
#' Please use noise_type for simpler specification.
#' Use ngme_noise_types() to check all the available types.
#'
#' @param noise_type    type of noise, "nig", "normal"
#' @param n             number of noise (= nrow(B_mu) = nrow(B_sigma))
#' @param V             value for V
#' @param B_mu          Basis matrix for mu (if non-stationary)
#' @param theta_mu      specify a non-stationary noise using theta_mu
#' @param B_sigma       Basis matrix for sigma (if non-stationary)
#' @param theta_sigma   specify a non-stationary noise using theta_sigma
#' @param theta_V       value for theta_V, theta_V = nu > 0
#' @param fix_theta_mu    fix the parameter of theta_mu
#' @param fix_theta_sigma  fix the parameter of theta_sigma
#' @param fix_theta_V   fix the parameter of theta_V
#' @param fix_V         fix the sampling of V
#' @param ...       additional arguments
#'
#' @return a list of specification of noise
#' @export
#'
ngme_noise <- function(
  noise_type,
  theta_mu        = 0,
  theta_sigma     = 0,
  theta_V         = 1,
  n               = 1,
  V               = NULL,
  B_mu            = NULL,
  B_sigma         = NULL,
  fix_theta_mu    = FALSE,
  fix_theta_sigma = FALSE,
  fix_theta_V     = FALSE,
  fix_V           = FALSE,
  ...
) {
  # check input
  stopifnot("Unkown noise type. Please check ngme_noise_types()" =
    noise_type %in% ngme_noise_types())
  stopifnot("ngme_noise: theta_V should be positive" = theta_V > 0)

  # check B_mu and B_sigma
  if (!is.null(B_mu))
    stopifnot("Make sure n == nrow(B_mu)"
      = n == 1 || nrow(B_mu) == 1 || n == nrow(B_mu))
  else
    B_mu <- as.matrix(1)
  if (!is.null(B_sigma))
    stopifnot("Make sure n == nrow(B_sigma)"
      = n == 1 || nrow(B_sigma) == 1 || n == nrow(B_sigma))
  else
    B_sigma <- as.matrix(1)
  if (n == 1) n <- max(nrow(B_mu), nrow(B_sigma)) # change default

  if (!is.matrix(B_mu))
    stop("Please input B_mu as a matrix to use non-stationary mu")
  if (!is.matrix(B_sigma))
    stop("Please input B_sigma as a matrix to use non-stationary sigma")
  if (ncol(B_mu) != length(theta_mu))
    stop("Please make sure ncol(B_mu) == length(theta_mu).")
  if (ncol(B_sigma) != length(theta_sigma))
    stop("Please make sure ncol(B_sigma) == length(theta_sigma).")

  # auto-complete (make sure nrow(B_sigma) == nrow(B_mu) for n=1 case)
  if (nrow(B_mu) == 1 && nrow(B_sigma) != 1)
    B_mu <- matrix(rep(B_mu, n), nrow = n, byrow = TRUE)
  else if (nrow(B_mu) != 1 && nrow(B_sigma) == 1)
    B_sigma <- matrix(rep(B_sigma, n), nrow = n, byrow = TRUE)

  structure(
    list(
      n_noise         = n,  # this is same as V_size
      noise_type      = noise_type,
      theta_V         = theta_V,
      V               = V,
      theta_mu        = theta_mu,
      theta_sigma     = theta_sigma,
      B_mu            = B_mu,
      B_sigma         = B_sigma,
      n_theta_mu      = length(theta_mu),
      n_theta_sigma   = length(theta_sigma),
      n_theta_V       = 1,
      fix_theta_mu    = fix_theta_mu,
      fix_theta_sigma = fix_theta_sigma,
      fix_theta_V     = fix_theta_V,
      fix_V           = fix_V,
      n_params        = switch(noise_type,
        "normal" = length(theta_sigma),
        "nig"    = length(c(theta_mu, theta_sigma, 1))
      ) # parameter to estimate
    ),
    class = "ngme_noise"
  )
}

#' Specify a normal noise
#'
#' @param sd            standard deviation
#' @param theta_sigma  specify a non-stationary noise using theta_sigma
#' @param B_sigma      Basis matrix for sigma (if non-stationary)
#' @param n             number of noise (= nrow(B_mu) = nrow(B_sigma))
#' @param ...       additional arguments
#'
#' @return ngme_noise object
#' @export
#'
#' @examples
#' noise_normal(n = 10, sd = 2)
noise_normal <- function(
  sd = NULL,
  theta_sigma = NULL,
  B_sigma = matrix(1, 1, 1),
  n = nrow(B_sigma),
  ...
) {
  if (!is.null(sd) && !is.null(theta_sigma))
    stop("Please only use sd or theta_sigma as input")

  # both are null, use default value
  if (is.null(sd) && is.null(theta_sigma)) {
    theta_sigma <- 0
  }

  if (!is.null(sd)) {
    stopifnot(
      "sd is a double" = is.double(sd),
      "sd should be positive" = sd > 0
    )

    theta_sigma <- log(sd)
  }

  stopifnot("Make sure ncol of B_sigma = length of theta_signa"
    = ncol(B_sigma) == length(theta_sigma))

  ngme_noise(
    noise_type = "normal",
    theta_sigma = theta_sigma,
    B_sigma = B_sigma,
    n = n,
    ...
  )
}

#' Specify a nig noise (normal inverse Gaussian)
#'
#' The parameterization can be found in ...
#'
#' @param mu      mu parameter (stationary)
#' @param sigma   sigma parameter (stationary)
#' @param nu      nu
#' @param V             value for V
#' @param n             number of noise (= nrow(B_mu) = nrow(B_sigma))
#' @param theta_mu     specify a non-stationary noise using theta_mu
#' @param theta_sigma  specify a non-stationary noise using theta_sigma
#' @param B_mu         Basis matrix for mu (if non-stationary)
#' @param B_sigma      Basis matrix for sigma (if non-stationary)
#' @param ...       additional arguments
#'
#' @return ngme_noise object
#' @export
#'
#' @examples
#' noise_nig(mu = 1, sigma = 2, nu = 1, n=10)
noise_nig <- function(
  mu            = NULL,
  sigma         = NULL,
  nu            = NULL,
  n             = 1,
  V             = NULL,
  theta_mu      = NULL,
  theta_sigma   = NULL,
  B_mu          = matrix(1),
  B_sigma       = matrix(1),
  ...
) {
  # if nothing, then fill with default
  stopifnot("Please use theta_mu for non-stationary mu." = length(mu) < 2)
  if (is.null(mu) && is.null(theta_mu)) theta_mu <- 0
  if (is.null(sigma) && is.null(theta_sigma)) theta_sigma <- 0
  if (is.null(nu)) theta_V <- 1 else theta_V <- nu

  if (!is.null(nu) && nu <= 0) stop("ngme_nosie: nu should be positive.")
  if (!is.null(sigma) && sigma <= 0) stop("ngme_nosie: sigma should be positive.")

  if (!is.null(mu))     theta_mu <- mu
  if (!is.null(sigma))  theta_sigma <- log(sigma)

  ngme_noise(
    noise_type = "nig",
    theta_mu = theta_mu,
    theta_sigma = theta_sigma,
    theta_V = theta_V,
    V = V,
    B_mu = B_mu,
    B_sigma = B_sigma,
    n = n,
    ...
  )
}

# update noise
update_noise <- function(noise, n = NULL, new_noise = NULL) {
  if (!is.null(n)) {
    stopifnot("n should be integer" = is.numeric(n))
    B_mu <- noise$B_mu
    noise$B_mu <- matrix(data = rep(B_mu, n / nrow(B_mu)), nrow = n)

    B_sigma <- noise$B_sigma
    noise$B_sigma <- matrix(data = rep(B_sigma, n / nrow(B_sigma)), nrow = n)
    noise$n_noise <- n

    noise <- do.call(ngme_noise, noise)
  } else if (!is.null(new_noise)) {
    # update with another noise
    if (noise$noise_type == "nig") {
      noise$theta_mu    <- new_noise$theta_mu
      noise$theta_sigma <- new_noise$theta_sigma
      noise$theta_V     <- new_noise$theta_V
      noise$V           <- new_noise$V
    } else if (noise$noise_type == "normal") {
      noise$theta_sigma <- new_noise$theta_sigma
    }
  }
  noise
}

#' Create ngme noise with a list
#' @param x a list
#'
#' @return a list of specification for ngme
create_noise <- function(x) {
  do.call(ngme_noise, x)
}

#' Print ngme noise
#'
#' @param x noise object
#' @param padding number of white space padding in front
#' @param ... ...
#'
#' @return a list (noise specifications)
#' @export
print.ngme_noise <- function(x, padding = 0, ...) {
  noise <- x
  pad_space <- paste(rep(" ", padding), collapse = "")
  pad_add4_space <- paste(rep(" ", padding + 4), collapse = "")

  if (is.null(noise)) {
    cat(pad_space); cat("Noise type - "); cat("NULL"); cat("\n")
  } else {
    cat(pad_space); cat("Noise type - "); cat(noise$noise_type); cat("\n")

    cat(pad_space); cat("Noise parameters: \n")
    params <- with(noise, {
      switch(noise_type,
        "normal" = paste0(pad_add4_space, ngme_format("sigma", theta_sigma)),
        "nig"    = paste0(pad_add4_space, ngme_format("mu", theta_mu),
                    "\n", pad_add4_space, ngme_format("sigma", theta_sigma),
                    "\n", pad_add4_space, ngme_format("nu", theta_V)),
        stop("unknown noise type")
      )
    })
    cat(params)
  }
  cat("\n")
  invisible(noise)
}