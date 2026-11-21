#include "render/medium.h"
#include <algorithm>
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

double Medium::density_at(Vec3 p) const {
    if (p.x < bounds.lo.x || p.x > bounds.hi.x || p.y < bounds.lo.y || p.y > bounds.hi.y
        || p.z < bounds.lo.z || p.z > bounds.hi.z) return 0;
    if (profile == DensityProfile::homogeneous) return density;
    const double height = std::clamp((p.y - bounds.lo.y) / (bounds.hi.y - bounds.lo.y), 0.0, 1.0);
    return density * (1 - height) + density_top * height;
}

namespace {
// Evaluate only height here: slab clipping can place an endpoint a few ULPs
// beyond a box face, which must not turn its limiting density into vacuum.
double endpoint_density(const Medium& medium, const Ray& ray, double t) {
    if (medium.profile == DensityProfile::homogeneous) return medium.density;
    const double h = std::clamp((ray.at(t).y - medium.bounds.lo.y)
                               / (medium.bounds.hi.y - medium.bounds.lo.y), 0.0, 1.0);
    return medium.density * (1 - h) + medium.density_top * h;
}
}

double Medium::optical_depth(const Ray& ray, double t_max) const {
    if (density == 0 && (profile == DensityProfile::homogeneous || density_top == 0)) return 0;
    const Segment segment = fog_segment(bounds, ray, t_max);
    if (segment.length == 0) return 0;
    const double a = endpoint_density(*this, ray, segment.enter);
    const double b = endpoint_density(*this, ray, segment.enter + segment.length / segment.speed);
    return (a * 0.5 + b * 0.5) * segment.length;
}

double Medium::transmittance(const Ray& ray, double t_max) const {
    return std::exp(-optical_depth(ray, t_max));
}

MediumSegmentSample Medium::sample_segment(const Ray& ray, double t_max, Sampler& rng) const {
    MediumSegmentSample sample;
    if (density == 0 && (profile == DensityProfile::homogeneous || density_top == 0)) return sample;
    const Segment segment = fog_segment(bounds, ray, t_max);
    if (segment.length == 0) return sample;
    const double a = endpoint_density(*this, ray, segment.enter);
    const double b = endpoint_density(*this, ray, segment.enter + segment.length / segment.speed);
    const double tau = (a * 0.5 + b * 0.5) * segment.length;
    if (tau == 0) return sample;
    // Integrate the full clipped segment; free flight below samples only the scattering term.
    if (emission.x != 0 || emission.y != 0 || emission.z != 0)
        sample.radiance = emission * (-std::expm1(-tau));
    const double sampled_tau = -std::log1p(-rng.next());
    if (a == b) {
        // Preserve the homogeneous free-flight calculation and its random stream.
        const double free_path = sampled_tau / a;
        if (free_path < segment.length)
            sample.collision = ray.at(segment.enter + free_path / segment.speed);
    } else if (sampled_tau < tau) {
        // Invert a*s + (b-a)*s*s/(2*L). Scale by the maximum density
        // and rationalize the quadratic root to avoid cancellation near a=b.
        const double scale = std::max(a, b);
        const double start = a / scale, change = b / scale - start;
        const double q = (sampled_tau / scale) / segment.length;
        const double denominator = start + std::sqrt(std::max(0.0, start * start + 2 * change * q));
        const double fraction = sampled_tau == 0 ? 0 : std::clamp(2 * q / denominator, 0.0, 1.0);
        sample.collision = ray.at(segment.enter + fraction * segment.length / segment.speed);
    }
    return sample;
}

Vec3 sample_isotropic_direction(Sampler& rng) {
    const double y = 1 - 2 * rng.next();
    const double azimuth = 2 * pi * rng.next();
    const double radius = std::sqrt((1 - y) * (1 + y));
    return {radius * std::cos(azimuth), y, radius * std::sin(azimuth)};
}
