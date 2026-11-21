#pragma once
#include "core/vec3.h"
#include <cstddef>
#include <limits>

// Width is the pixel footprint diameter perpendicular to the ray, in world units.
struct RayCone {
    double width=0, spread=0;
    RayCone advanced(Vec3 displacement) const {
        return {spread > 0 ? width + spread * std::sqrt(displacement.dot(displacement)) : width, spread};
    }
};

struct Ray {
    Vec3 origin, direction;
    double time=0;
    RayCone cone{};
    Vec3 at(double t) const { return origin + direction*t; }
};

struct HitRecord {
    UV uv;
    int light_id=-1;
    double t{};
    double time=0;
    Vec3 position{};
    Vec3 outward_normal{};  // Geometric normal for sidedness and ray offsets.
    Vec3 normal{};          // Shading normal oriented towards the incident ray's origin.
    bool front_face{};
    int material_id{};
    // Filled by scene-level queries after choosing the closest primitive.
    static constexpr std::size_t no_object = std::numeric_limits<std::size_t>::max();
    std::size_t object_index = no_object;
    double albedo_lod = 0;
    void set(const Ray& ray, double parameter, Vec3 outward, int material) {
        object_index = no_object;
        albedo_lod = 0;
        time = ray.time;
        t = parameter; position = ray.at(t); outward_normal = outward;
        front_face = ray.direction.dot(outward) < 0;
        normal = front_face ? outward : outward*(-1);
        material_id = material;
    }
};

Ray spawn(const HitRecord&, Vec3 direction);
