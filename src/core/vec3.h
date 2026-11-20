#pragma once
#include <cmath>

struct Vec3 {
    double x, y, z;
    Vec3 operator+(Vec3 b) const { return {x+b.x, y+b.y, z+b.z}; }
    Vec3 operator-(Vec3 b) const { return {x-b.x, y-b.y, z-b.z}; }
    Vec3 operator*(double s) const { return {x*s, y*s, z*s}; }
    Vec3 cross(Vec3 b) const { return {y*b.z-z*b.y, z*b.x-x*b.z, x*b.y-y*b.x}; }
    double dot(Vec3 b) const { return x*b.x + y*b.y + z*b.z; }
    Vec3 normalized() const { const double length = std::sqrt(dot(*this)); return length > 0 ? *this * (1.0 / length) : Vec3{0,0,0}; }
};
inline constexpr double pi = 3.14159265358979323846;
inline Vec3 multiply(Vec3 a, Vec3 b) { return {a.x*b.x,a.y*b.y,a.z*b.z}; }
inline bool finite(Vec3 v) { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z); }

struct UV { double u=0, v=0; };
