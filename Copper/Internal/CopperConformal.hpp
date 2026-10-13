// Geometry for the conformal PEC treatment of z-extruded metal prisms (CopperOperator's
// applyConformalPEC()). The scheme is Dey and Mittra's locally conformal FDTD ("A locally conformal
// finite-difference time-domain (FDTD) algorithm for modeling three-dimensional perfectly
// conducting objects", IEEE MGWL 7(9), 1997), in the finite-integration form CST calls the Perfect
// Boundary Approximation: a Yee edge partly inside the metal carries the voltage of its open part
// only, so its capacitance is that of the open length; a face partly inside the metal carries flux
// through its open area only, so its inductance is that of the open area. Both need only how much
// of each mesh edge and face a prism's cross-section covers, which is what this file computes.
#pragma once

#include <vector>

namespace copper::conformal {

/// A simple polygon's vertices, in order (either winding), in native CSX drawing units.
struct Polygon {
    std::vector<double> x, y;
};

/// Length of the segment from `a` to `b` along x at y = `fixed` (`alongX`), or along y at x =
/// `fixed`, that lies inside `polygon` (even-odd rule).
double insideLength(const Polygon& polygon, bool alongX, double fixed, double a, double b);

/// Area of the rectangle [x0, x1] x [y0, y1] that lies inside `polygon`.
double insideArea(const Polygon& polygon, double x0, double x1, double y0, double y1);

/// Railton, Paul and Dumanli's modified material parameters for a thin wire of radius `radius`
/// running along a line of nodes ("The treatment of thin wire and coaxial structures in lossless
/// and lossy media in FDTD by the modification of assigned material parameters", IEEE TEMC 48(4),
/// 2006, eqs. 6 and 11): the factor on the permeability of the H edge circling the wire beside a
/// radial edge of length `along`, whose transverse cell width is `across`. The radial edge's
/// permittivity takes the reciprocal. Only meaningful for `radius` < `along`.
double thinWirePermeabilityFactor(double radius, double along, double across);

} // namespace copper::conformal
