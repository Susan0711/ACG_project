#include "render/path_integrator.h"
#include "render/direct_lighting.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

Vec3 integrate(const RenderScene& scene, Ray ray, Sampler& rng, int depth, SamplingMode mode) {
    constexpr double infinity = std::numeric_limits<double>::infinity();
    Vec3 result{}, throughput{1, 1, 1};
    Vec3 previous_position{};
    double previous_pdf = 0;
    bool previous_delta = true;  // A camera ray has no competing direct-light sample.
    int alpha_crossings = 0;

    for (int bounce = 0; bounce <= depth;) {
        HitRecord hit;
        const bool hit_surface = scene.intersect(ray, 0, infinity, hit);
        const auto segment = scene.medium().sample_segment(ray, hit_surface ? hit.t : infinity, rng);
        result = result + multiply(throughput, segment.radiance);
        if (segment.collision) {
            if (bounce == depth || scene.medium().albedo == 0) break;
            // Free-flight sampling already accounts for extinction on the incoming segment.
            throughput = throughput * scene.medium().albedo;
            const Vec3 position = *segment.collision;
            const Vec3 direct = medium_direct_lighting(scene, position, ray.time, rng, mode);
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
            break;
        } else {
            const auto& material = scene.material(hit);
            const double alpha = material.alpha(hit.uv);
            if (alpha < 1 && (alpha == 0 || rng.next() >= alpha)) {
                if (++alpha_crossings > 4096) throw std::runtime_error("Alpha crossing limit exceeded");
                const Vec3 next_origin = ray.at(std::nextafter(hit.t, infinity)) + ray.direction * 1e-7;
                ray.cone = ray.cone.advanced(next_origin - ray.origin);
                ray.origin = next_origin;
                continue;
            }

            if (material.is_emitter()) {
                if (hit.front_face) {
                    const double light_pdf = scene.lights().emitter_pdf(hit.light_id, previous_position, hit.position);
                    const double weight = emission_weight(mode, previous_delta, previous_pdf,
                                                          light_pdf, hit.light_id >= 0);
                    result = result + multiply(throughput, material.color) * weight;
                }
                break;
            }
            if (bounce == depth) break;

            const Vec3 direct = direct_lighting(scene, ray, hit, material, rng, mode);
            result = result + multiply(throughput, direct);
            const auto sampled = sample_bsdf(material, ray, hit, rng);
            if (sampled.pdf <= 0) break;
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
            if (rng.next() >= survive) break;
            throughput = throughput * (1 / survive);
        }
        ++bounce;
    }
    return result;
}
