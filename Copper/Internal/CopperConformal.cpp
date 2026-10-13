#include "CopperConformal.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace copper::conformal {

double insideLength(const Polygon& polygon, bool alongX, double fixed, double a, double b) {
    const std::vector<double>& u = alongX ? polygon.x : polygon.y;
    const std::vector<double>& v = alongX ? polygon.y : polygon.x;
    const std::size_t n = u.size();
    if (n < 3) return 0.0;
    std::vector<double> crossings;
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
        // Half-open in v, so a vertex on the line is counted once and an edge along it not at all.
        if ((v[j] <= fixed) == (v[i] <= fixed)) continue;
        const double t = (fixed - v[j]) / (v[i] - v[j]);
        crossings.push_back(u[j] + t * (u[i] - u[j]));
    }
    std::sort(crossings.begin(), crossings.end());
    const double lo = std::min(a, b);
    const double hi = std::max(a, b);
    double length = 0.0;
    for (std::size_t i = 0; i + 1 < crossings.size(); i += 2) {
        length += std::max(0.0, std::min(hi, crossings[i + 1]) - std::max(lo, crossings[i]));
    }
    return length;
}

double insideArea(const Polygon& polygon, double x0, double x1, double y0, double y1) {
    // Sutherland-Hodgman against the rectangle's four sides, then the shoelace formula. The clip
    // window is convex, so this is exact for any simple polygon.
    std::vector<std::pair<double, double>> points;
    for (std::size_t i = 0; i < polygon.x.size(); ++i) points.emplace_back(polygon.x[i], polygon.y[i]);
    const auto clip = [&](auto inside, auto intersect) {
        std::vector<std::pair<double, double>> out;
        for (std::size_t i = 0; i < points.size(); ++i) {
            const auto& current = points[i];
            const auto& previous = points[(i + points.size() - 1) % points.size()];
            const bool currentIn = inside(current);
            if (currentIn != inside(previous)) out.push_back(intersect(previous, current));
            if (currentIn) out.push_back(current);
        }
        points = std::move(out);
    };
    const auto onX = [](double edge) {
        return [edge](const auto& p, const auto& q) {
            const double t = (edge - p.first) / (q.first - p.first);
            return std::pair{edge, p.second + t * (q.second - p.second)};
        };
    };
    const auto onY = [](double edge) {
        return [edge](const auto& p, const auto& q) {
            const double t = (edge - p.second) / (q.second - p.second);
            return std::pair{p.first + t * (q.first - p.first), edge};
        };
    };
    const double xLo = std::min(x0, x1), xHi = std::max(x0, x1), yLo = std::min(y0, y1), yHi = std::max(y0, y1);
    clip([&](const auto& p) { return p.first >= xLo; }, onX(xLo));
    if (!points.empty()) clip([&](const auto& p) { return p.first <= xHi; }, onX(xHi));
    if (!points.empty()) clip([&](const auto& p) { return p.second >= yLo; }, onY(yLo));
    if (!points.empty()) clip([&](const auto& p) { return p.second <= yHi; }, onY(yHi));
    double twiceArea = 0.0;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const auto& p = points[i];
        const auto& q = points[(i + 1) % points.size()];
        twiceArea += p.first * q.second - q.first * p.second;
    }
    return std::abs(twiceArea) / 2.0;
}

double thinWirePermeabilityFactor(double radius, double along, double across) {
    return std::log(along / radius) / (2.0 * std::atan(across / along)) * across / along;
}

} // namespace copper::conformal
