#include "render/path_integrator.h"
#include "render/direct_lighting.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

bool advance_path(const RenderScene& scene, PathState& path, Sampler& rng, int depth, SamplingMode mode,
                  Wavelength band, bool hit_surface, const HitRecord& hit,
                  std::vector<DirectLightSample>* pending, Vec3& direct_throughput) {
    constexpr double infinity = std::numeric_limits<double>::infinity();
    auto& [ray, result, throughput, previous_position, previous_pdf, previous_delta, bounce, alpha_crossings] = path;
    const auto segment = scene.medium().sample_segment(ray, hit_surface ? hit.t : infinity, rng);
    result = result + multiply(throughput, segment.radiance);
    if (segment.collision) {
        if (bounce == depth || scene.medium().albedo == 0) return false;
        // Free-flight sampling already accounts for extinction on the incoming segment.
        throughput = throughput * scene.medium().albedo;
        const Vec3 position = *segment.collision;
        if (pending) direct_throughput = throughput;
        const Vec3 direct = medium_direct_lighting(scene, position, ray.time, rng, mode, pending);
        result = result + multiply(throughput, direct);
        ray = {position, sample_isotropic_direction(rng), ray.time, ray.cone.advanced(position - ray.origin)};
        previous_delta = false;
        previous_pdf = isotropic_phase_pdf;
        previous_position = position;
    } else if (!hit_surface) {
        const auto background = scene.lights().escape(ray.direction);
        const double weight = emission_weight(mode, previous_delta, previous_pdf,
                                              background.pdf, background.sampled_directly);
        result = result + multiply(throughput, background.radiance) * weight;
        return false;
    } else {
        const auto& material = scene.material(hit);
        const double alpha = material.alpha(hit.uv);
        if (alpha < 1 && (alpha == 0 || rng.next() >= alpha)) {
            if (++alpha_crossings > 4096) throw std::runtime_error("Alpha crossing limit exceeded");
            const Vec3 next_origin = ray.at(std::nextafter(hit.t, infinity)) + ray.direction * 1e-7;
            ray.cone = ray.cone.advanced(next_origin - ray.origin);
            ray.origin = next_origin;
            return true;
        }

        if (material.is_emitter()) {
            if (hit.front_face) {
                const double light_pdf = scene.lights().emitter_pdf(hit.light_id, previous_position, hit.position);
                const double weight = emission_weight(mode, previous_delta, previous_pdf,
                                                      light_pdf, hit.light_id >= 0);
                result = result + multiply(throughput, material.color) * weight;
            }
            return false;
        }
        if (bounce == depth) return false;

        if (pending) direct_throughput = throughput;
        const Vec3 direct = direct_lighting(scene, ray, hit, material, rng, mode, pending);
        result = result + multiply(throughput, direct);
        const auto sampled = sample_bsdf(material, ray, hit, rng, band);
        if (sampled.pdf <= 0) return false;
        throughput = multiply(throughput, sampled.weight);
        previous_delta = sampled.delta;
        previous_pdf = sampled.pdf;
        previous_position = hit.position;
        const RayCone cone = ray.cone.advanced(ray.direction * hit.t);
        ray = spawn(hit, sampled.direction);
        ray.cone = cone;
    }
    if (!finite(throughput) || !finite(result) || !finite(ray.direction))
        throw std::runtime_error("Nonfinite path state");

    if (bounce >= 3) {
        const double survive = std::clamp(std::max({throughput.x, throughput.y, throughput.z}), 0.05, 0.95);
        if (rng.next() >= survive) return false;
        throughput = throughput * (1 / survive);
    }
    ++bounce;
    return true;
}

namespace {
Vec3 trace_path(const RenderScene& scene, Ray ray, Sampler& rng, int depth, SamplingMode mode, Wavelength band) {
    PathState path{ray};
    Vec3 unused{};
    HitRecord hit;
    do {
        const bool found = scene.intersect(path.ray, 0, std::numeric_limits<double>::infinity(), hit);
        if (!advance_path(scene, path, rng, depth, mode, band, found, hit, nullptr, unused)) break;
    } while (path.bounce <= depth);
    return path.result;
}
}

Vec3 integrate(const RenderScene& scene, Ray ray, Sampler& rng, int depth, SamplingMode mode) {
    if (!scene.uses_dispersion()) return trace_path(scene, ray, rng, depth, mode, Wavelength::green);
    // Use the same camera ray and random sequence so achromatic paths agree across RGB.
    Sampler red_rng = rng, blue_rng = rng;
    const double red = trace_path(scene, ray, red_rng, depth, mode, Wavelength::red).x;
    const double green = trace_path(scene, ray, rng, depth, mode, Wavelength::green).y;
    const double blue = trace_path(scene, ray, blue_rng, depth, mode, Wavelength::blue).z;
    return {red, green, blue};
}
