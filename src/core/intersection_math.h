#pragma once
#include "core/vec3.h"

// FP64 reference geometry for the CPU renderer.
struct TriangleGeometry {
    Vec3 a, edge1, edge2, velocity;
};
struct SphereGeometry {
    Vec3 center, velocity;
    double radius;
};
struct HitParameters {
    double t = 0, u = 0, v = 0;
};

inline bool closest_triangle(const TriangleGeometry& g, Vec3 origin, Vec3 dir, double time,
                                    double lo, double hi, HitParameters& hit) {
    const Vec3 normal = g.edge1.cross(g.edge2);
    const double scale = ::sqrt(normal.dot(normal) * dir.dot(dir));
    const Vec3 p = dir.cross(g.edge2);
    const double det = g.edge1.dot(p);
    if (!(scale > 0) || ::fabs(det) <= 1e-12 * scale) return false;
    const Vec3 offset = origin - (g.a + g.velocity * time);
    const double u = offset.dot(p) / det;
    const Vec3 q = offset.cross(g.edge1);
    const double v = dir.dot(q) / det;
    if (u < 0 || v < 0 || u + v > 1) return false;
    const double t = g.edge2.dot(q) / det;
    if (t < lo || t > hi) return false;
    hit = {t, u, v};
    return true;
}

inline bool closest_sphere(const SphereGeometry& g, Vec3 origin, Vec3 dir, double time,
                                  double lo, double hi, HitParameters& hit) {
    const Vec3 offset = origin - (g.center + g.velocity * time);
    const double a = dir.dot(dir), b = offset.dot(dir);
    if (!(a > 0) || !(g.radius > 0)) return false;
    const double d = b*b - a*(offset.dot(offset) - g.radius*g.radius);
    if (d < 0) return false;
    double t = (-b - ::sqrt(d)) / a;
    if (t < lo || t > hi) t = (-b + ::sqrt(d)) / a;
    if (t < lo || t > hi) return false;
    hit = {t, 0, 0};
    return true;
}
