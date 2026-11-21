#pragma once
#include "acceleration/bvh.h"
#include "world.h"
#include <optional>

enum class Acceleration { linear, bvh };

// Own the geometry and its index together for the lifetime of a render.
class RenderScene {
public:
    explicit RenderScene(World world, Acceleration backend = Acceleration::bvh, BVHSplit split = BVHSplit::median);
    const Camera& camera() const { return world_.camera; }
    const Lighting& lights() const { return world_.lights; }
    const HomogeneousMedium& medium() const { return world_.medium; }
    const Material& material(const HitRecord& hit) const { return world_.materials.at(hit.material_id); }
    bool intersect(const Ray&, double minimum, double maximum, HitRecord&) const;
    double visibility(const HitRecord&, const LightSample&) const;
    double shadow_transmittance(Ray, double t_max) const;
    double visibility_to_light(const Ray& shadow, double distance) const;

private:
    bool intersect_geometry(const Ray&, double minimum, double maximum, HitRecord&) const;
    double surface_transmittance(const Ray&, double t_max) const;
    World world_;
    std::optional<BVH> bvh_;
    bool has_normal_maps_;
};
