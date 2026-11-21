#pragma once
#include "core/ray.h"
#include <array>
#include <optional>
#include <variant>
#include <vector>

struct UVGradients { Vec3 du, dv; };  // World-space surface gradients of U and V.

struct Sphere {
    Vec3 center;
    double radius;
    int material_id = 0;
    Vec3 velocity{};
    Vec3 center_at(double time) const { return center + velocity*time; }
    bool intersect(const Ray&, double t_min, double t_max, HitRecord&) const;
    Vec3 mapped_normal(const HitRecord&, Vec3 tangent_space_normal) const;
    std::optional<UVGradients> uv_gradients(const HitRecord&) const;
};
struct Triangle {
    Vec3 a, b, c;
    int material_id = 0;
    UV ta{0,0}, tb{1,0}, tc{0,1};
    int light_id=-1;
    Vec3 velocity{};  // Uniform world-space translation per second; vertices are stored at time zero.
    std::optional<std::array<Vec3, 3>> corner_normals{};
    bool has_complete_uvs = true;
    bool intersect(const Ray&, double t_min, double t_max, HitRecord&) const;
    Vec3 mapped_normal(const HitRecord&, Vec3 tangent_space_normal) const;
    std::optional<UVGradients> uv_gradients(const HitRecord&) const;
};
struct Scene {
    std::vector<std::variant<Sphere, Triangle>> objects;
    bool intersect(const Ray&, double t_min, double t_max, HitRecord&) const;
};
