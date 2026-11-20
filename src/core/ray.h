#pragma once
#include "core/vec3.h"

struct Ray {
    Vec3 origin, direction;
    double time=0;
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
    void set(const Ray& ray, double parameter, Vec3 outward, int material) {
        time = ray.time;
        t = parameter; position = ray.at(t); outward_normal = outward;
        front_face = ray.direction.dot(outward) < 0;
        normal = front_face ? outward : outward*(-1);
        material_id = material;
    }
};

Ray spawn(const HitRecord&, Vec3 direction);
