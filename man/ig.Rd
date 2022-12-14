% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/distributions.R
\name{ig}
\alias{ig}
\alias{dig}
\alias{rig}
\alias{pig}
\alias{qig}
\title{The Inverse-Gaussian (IG) Distribution}
\usage{
dig(x, a, b, log = FALSE)

rig(n, a, b, seed = 0)

pig(q, a, b, lower.tail = TRUE, log.p = FALSE)

qig(p, a, b, lower.tail = TRUE, log.p = FALSE)
}
\arguments{
\item{x, q}{vector of quantiles.}

\item{a, b}{parameters \code{a} and \code{b}. Must be positive.}

\item{log, log.p}{logical; if \code{TRUE}, probabilities/densities \eqn{p} are
returned as \eqn{log(p)}.}

\item{n, }{number of observations.}

\item{seed}{Seed for the random generation.}

\item{lower.tail}{logical; if \code{TRUE}, probabilities are \eqn{P[X\leq x]},
otherwise, \eqn{P[X>x]}.}

\item{p}{vector of probabilities.}
}
\value{
dig gives the density, pig gives the distribution function,
qig gives the quantile function, and rig generates random deviates.

Invalid arguments will result in return value NaN, with a warning.

The length of the result is determined by \code{n} for rig.
}
\description{
Density, distribution function, quantile function and
random generation for the inverse-Gaussian distribution
 with parameters \code{a} and \code{b}.
}
\details{
The inverse-Gaussian distribution has density given
by
 \deqn{f(x; a, b) = \frac{\sqrt{b}}{\sqrt{2\pi x^3}}\exp(
 -\frac{a}{2}x -\frac{b}{2x} + \sqrt{ab}),}
 where \eqn{x>0} and \eqn{a,b>0}. In this parameterization,
 \eqn{E(X) = \sqrt{b}/\sqrt{a}}. See Tweedie (1957a, 1957b) for
 further details.
}
\examples{
rig(100, a = 1, b = 1)
pig(0.4, a = 1, b = 1)
qig(0.8, a = 1, b = 1)
plot(function(x){dig(x, a = 1, b = 1)}, main =
"Inverse-Gaussian density", ylab = "Probability density",
xlim = c(0,10))
}
\references{
Tweedie, M. C. K. (1957a). "Statistical Properties of Inverse Gaussian Distributions I". Annals of Mathematical Statistics. 28 (2): 362–377. \doi{10.1214/aoms/1177706964}

Tweedie, M. C. K. (1957b). "Statistical Properties of Inverse Gaussian Distributions II". Annals of Mathematical Statistics. 28 (3): 696–705. \doi{10.1214/aoms/1177706881}
}
\seealso{
\code{\link{dnig}}, \code{\link{dgig}}, \code{\link{digam}}
}
