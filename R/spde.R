#' Create spde model object
#'
#' @param alpha
#' @param mesh mesh argument
#' @param fem.mesh.matrices specify the FEM matrices
#' @param d indicating the dimension of mesh (together with fem.mesh.matrices)
#' @param B.tau bases for tau
#' @param B.kappa bases for kappa
#' @param theta.init
#'
#' @return a list (n, C (diagonal), G, B.tau, B.kappa) for constructing operator
#' @export
#'
#' @examples

ngme.spde.matern <- function(
    alpha = 2,
    var="nig",
    mesh = NULL,
    fem.mesh.matrices = NULL,
    d = NULL,
    B.tau = matrix(c(0,1,0), 1, 3),
    B.kappa = matrix(c(0,0,1), 1, 3),
    theta.init = c(1, 1)
    )
{
  if (ncol(B.tau) != ncol(B.kappa)) stop ("B.tau and B.kappa should have same number of columns")
  if (is.null(mesh) && is.null(fem.mesh.matrices)) stop("At least specify mesh or matrices")
  if (alpha - round(alpha) != 0) {
    stop("alpha should be integer")
  }

  # supply mesh
  if (!is.null(mesh)) {
    n <- mesh$n
    d <- get_inla_mesh_dimension(mesh)
    if (d == 1) {
      fem <- INLA::inla.mesh.1d.fem(mesh)
      C <- fem$c1
      G <- fem$g1
    } else {
      fem <- INLA::inla.mesh.fem(mesh, order = alpha)
      C <- fem$c0 # diag
      G <- fem$g1

      # Q <- function(kappa, tau) {
      #   ans <- kappa^(2 * alpha) * fem[["c0"]]
      #   for (i in 2:(alpha + 1)) {
      #     ans <- ans + choose(alpha, i - 1) * kappa^(2 * (alpha - i + 1)) * fem[[paste0("g", i - 1)]]
      #   }
      #   ans * tau
      # }
      # K <- function(kappa, tau) {
      #   if (alpha %% 2 == 0) {
      #     m <- alpha / 2
      #     ans <- kappa^(2 * m) * fem[["c0"]]
      #     for (i in 2:(m + 1)) {
      #       ans <- ans + choose(alpha, i - 1) * kappa^(2 * (alpha - i + 1)) * fem[[paste0("g", i - 1)]]
      #     }
      #     c0_sqrt_inv <- fem[["c0"]]
      #     c0_sqrt_inv@x <- 1 / sqrt(c0_sqrt_inv@x)
      #     ans * tau * c0_sqrt_inv
      #   } else {
      #     return(chol(Q(kappa, tau)))
      #   }
      # }
    }

    n <- mesh$n
    spde.spec <- list(
      # general
      n_params = ncol(B.kappa)-1,
      init_operator = theta.init,
      var="nig",

      # spde
      operator_in = list(
        alpha = alpha,
        n_params = length(theta.init),
        n = n,
        C = as(C, "dgCMatrix"),
        G = as(G, "dgCMatrix"),
        B.tau = B.tau,
        B.kappa = B.kappa,
        init_operator = theta.init,
        use_num_dK = FALSE
      )
    )
  }

  # create precision matrix
  class(spde.spec) <- "ngme.spde"

  return(spde.spec)
}


#' @name get_inla_mesh_dimension
#' @title Get the dimension of an INLA mesh
#' @description Get the dimension of an INLA mesh
#' @param inla_mesh An INLA mesh
#' @return The dimension of an INLA mesh.
#' @noRd
#'
get_inla_mesh_dimension <- function(inla_mesh) {
  cond1 <- inherits(inla_mesh, "inla.mesh.1d")
  cond2 <- inherits(inla_mesh, "inla.mesh")
  stopifnot(cond1 || cond2)
  if (inla_mesh$manifold == "R1") {
    d <- 1
  } else if (inla_mesh$manifold == "R2") {
    d <- 2
  } else {
    stop("The mesh should be from a flat manifold.")
  }
  return(d)
}


# # test
# mesh_size = 5
# range = 0.2
# sigma = 1
# nu=0.8
# kappa = sqrt(8*nu)/range
# mesh_grid = inla.mesh.lattice(x=seq(from=0,to=1, length=mesh_size),y=seq(from=0,to=1,length=mesh_size))
# mesh_grid_2d = inla.mesh.create(lattice = mesh_grid, extend = FALSE, refine = FALSE)
# mesh_2d = mesh_grid_2d
# obs_coords = mesh_2d$loc[,c(1,2)]
# nob = nrow(obs_coords)
# fem = inla.mesh.fem(mesh_2d, order = 2)
# C = fem$c0
# G = fem$g1
# A = inla.spde.make.A(mesh_2d, loc = obs_coords)
#
# o = ngme.spde.matern(alpha=2, mesh=mesh_2d)
# o$Q(1, 1)
#
# o$Q(1,1)