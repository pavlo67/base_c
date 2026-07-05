#ifndef MATHLIB_H
#define MATHLIB_H

struct Vec2D {
    double x = 0.0;
    double y = 0.0;
};

double dot(const Vec2D& a, const Vec2D& b);
double len(const Vec2D& v);
Vec2D normalized(Vec2D v, double eps = 1e-9);
double angleToHorizontalRad(const Vec2D& v);

#endif //MATHLIB_H
