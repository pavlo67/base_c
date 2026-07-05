#include "mathlib.h"

#include <cmath>

double dot(const Vec2D& a, const Vec2D& b) {
    return a.x * b.x + a.y * b.y;
}

double len(const Vec2D& v) {
    return std::sqrt(dot(v, v));
}

Vec2D normalized(Vec2D v, double eps) {
    const double l = len(v);
    if (l <= eps) return {1.0, 0.0};
    v.x /= l;
    v.y /= l;
    if (v.x < 0.0 || (std::abs(v.x) <= eps && v.y < 0.0)) {
        v.x = -v.x;
        v.y = -v.y;
    }
    return v;
}

double angleToHorizontalRad(const Vec2D& v) {
    const double ax = std::abs(v.x);
    const double ay = std::abs(v.y);
    return std::atan2(ay, ax);
}
