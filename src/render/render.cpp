#include "render/render.h"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

RenderScene::RenderScene(World world, Acceleration backend, BVHSplit split) : world_(std::move(world)) {
    const Camera& camera = world_.camera;
    camera.validate();
    has_normal_maps_ = std::any_of(world_.materials.begin(), world_.materials.end(),
                                   [](const Material& m) { return bool(m.normal_map); });
    if (backend == Acceleration::bvh) {
        bvh_.emplace().build(world_.scene, camera.shutter_open, camera.shutter_close, split);
    }
}

bool RenderScene::intersect(const Ray& ray, double minimum, double maximum, HitRecord& hit) const {
    if (!intersect_geometry(ray, minimum, maximum, hit)) return false;
    if (has_normal_maps_) {
        const auto& map = material(hit).normal_map;
        if (map) {
            const Vec3 local = map->value(hit.uv) * 2 - Vec3{1, 1, 1};
            const Vec3 mapped = std::visit([&](const auto& shape) { return shape.mapped_normal(hit, local); },
                                           world_.scene.objects.at(hit.object_index));
            if (mapped.dot(ray.direction) < 0) hit.normal = mapped;
        }
    }
    return true;
}

bool RenderScene::intersect_geometry(const Ray& ray, double minimum, double maximum, HitRecord& hit) const {
    // Motion bounds only cover the shutter interval used to build this index.
    if (bvh_ && ray.time >= world_.camera.shutter_open && ray.time <= world_.camera.shutter_close)
        return bvh_->intersect(world_.scene, ray, minimum, maximum, hit);
    return world_.scene.intersect(ray, minimum, maximum, hit);
}

double RenderScene::visibility(const HitRecord& hit, const LightSample& light) const {
    Ray shadow = spawn(hit, light.direction);
    double distance = light.distance;
    if (std::isfinite(distance)) {
        // Keep the sampled endpoint fixed when offsetting the ray off the surface.
        const Vec3 target = hit.position + light.direction * distance;
        const Vec3 offset = target - shadow.origin;
        distance = std::sqrt(offset.dot(offset));
        if (distance <= 1e-5) return 0;
        shadow.direction = offset * (1 / distance);
    }
    return visibility_to_light(shadow, distance);
}

double RenderScene::visibility_to_light(const Ray& shadow, double distance) const {
    // Exclude the sampled light surface from blockers, but attenuate the full segment.
    return medium().transmittance(shadow, distance) * surface_transmittance(shadow, distance - 1e-5);
}

double RenderScene::shadow_transmittance(Ray ray, double t_max) const {
    return medium().transmittance(ray, t_max) * surface_transmittance(ray, t_max);
}

double RenderScene::surface_transmittance(const Ray& ray, double distance) const {
    double transmission = 1, minimum = 0;
    for (int crossing = 0; crossing < 4096; ++crossing) {
        HitRecord hit;
        if (!intersect_geometry(ray, minimum, distance, hit)) return transmission;
        transmission *= 1 - material(hit).alpha(hit.uv);
        if (transmission == 0) return 0;
        minimum = std::nextafter(hit.t, std::numeric_limits<double>::infinity());
    }
    throw std::runtime_error("Shadow crossing limit exceeded");
}
