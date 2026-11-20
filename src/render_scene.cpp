#include "render_scene.h"
#include <limits>
#include <stdexcept>
#include <utility>

RenderScene::RenderScene(World world, Acceleration backend) : world_(std::move(world)) {
    const Camera& camera = world_.camera;
    camera.validate();
    if (backend == Acceleration::bvh) {
        bvh_.emplace().build(world_.scene, camera.shutter_open, camera.shutter_close);
    }
}

bool RenderScene::intersect(const Ray& ray, double minimum, double maximum, HitRecord& hit) const {
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
        distance -= 1e-5;
    }
    return shadow_transmittance(shadow, distance);
}

double RenderScene::shadow_transmittance(Ray ray, double distance) const {
    double transmission = 1, minimum = 0;
    for (int crossing = 0; crossing < 4096; ++crossing) {
        HitRecord hit;
        if (!intersect(ray, minimum, distance, hit)) return transmission;
        transmission *= 1 - material(hit).alpha(hit.uv);
        if (transmission == 0) return 0;
        minimum = std::nextafter(hit.t, std::numeric_limits<double>::infinity());
    }
    throw std::runtime_error("Shadow crossing limit exceeded");
}
