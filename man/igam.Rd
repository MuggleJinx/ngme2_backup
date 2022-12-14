% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/distributions.R
\name{igam}
\alias{igam}
\alias{digam}
\alias{rigam}
\alias{pigam}
\alias{qigam}
\title{The Inverse-Gamma (IGam) Distribution}
\usage{
digam(x, a, b, log = FALSE)

rigam(n, a, b)

pigam(q, a, b, lower.tail = TRUE, log.p = FALSE)

qigam(p, a, b, lower.tail = TRUE, log.p = FALSE)
}
\arguments{
\item{x, q}{vector of quantiles.}

\item{a, b}{parameters \code{a} and \code{b}. Must be positive.}

\item{log, log.p}{logical; if \code{TRUE}, probabilities/densities \eqn{p} are
returned as \eqn{log(p)}.}

\item{n, }{number of observations.}

\item{lower.tail}{logical; if \code{TRUE}, probabilities are \eqn{P[X\leq x]},
otherwise, \eqn{P[X>x]}.}

\item{p}{vector of probabilities.}
}
\value{
digam gives the density, pigam gives the distribution function,
qigam gives the quantile function, and rigam generates random deviates.

Invalid arguments will result in return value NaN, with a warning.

The length of the result is determined by \code{n} for rig.
}
\description{
Density, distribution function, quantile function and
random generation for the inverse-Gamma distribution
 with parameters \code{a} and \code{b}.
}
\details{
The inverse-Gamma distribution has density given
by
 \deqn{f(x; a, b) = \frac{b^a}{\Gamma(a)}x^{a-1}\exp(
 -\frac{b}{x}),}
 where \eqn{x>0} and \eqn{a,b>0}.
}
\examples{
rigam(100, a = 1, b = 1)
pigam(0.4, a = 1, b = 1)
qigam(0.8, a = 1, b = 1)
plot(function(x){digam(x, a = 1, b = 1)}, main =
"Inverse-Gamma density", ylab = "Probability density",
xlim = c(0,10))
}
\seealso{
\code{\link{dnig}}, \code{\link{dgig}}
}
