#include "render/medium.h"
#include <cmath>

namespace {
struct Segment {
    double enter = 0, speed = 0, length = 0;
};

Segment fog_segment(const AABB& bounds, const Ray& ray, double t_max) {
    double enter = 0, exit = t_max;
    if (!bounds.clip_interval(ray, enter, exit) || exit <= enter) return {};
    const double speed = std::sqrt(ray.direction.dot(ray.direction));
    if (speed == 0) return {};
    return {enter, speed, (exit - enter) * speed};
}
}

double HomogeneousMedium::transmittance(const Ray& ray, double t_max) const {
    if (density == 0) return 1;
    return std::exp(-density * fog_segment(bounds, ray, t_max).length);
}

std::optional<Vec3> HomogeneousMedium::sample_collision(const Ray& ray, double t_max, Sampler& rng) const {
    if (density == 0) return std::nullopt;
    const Segment segment = fog_segment(bounds, ray, t_max);
    if (segment.length == 0) return std::nullopt;
    const double free_path = -std::log1p(-rng.next()) / density;
    if (free_path >= segment.length) return std::nullopt;
    return ray.at(segment.enter + free_path / segment.speed);
}

Vec3 sample_isotropic_direction(Sampler& rng) {
    const double y = 1 - 2 * rng.next();
    const double azimuth = 2 * pi * rng.next();
    const double radius = std::sqrt((1 - y) * (1 + y));
    return {radius * std::cos(azimuth), y, radius * std::sin(azimuth)};
}
