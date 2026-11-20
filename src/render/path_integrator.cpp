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
        if (!scene.intersect(ray, 0, infinity, hit)) {
            const auto background = scene.lights().escape(ray.direction);
            const double weight = emission_weight(mode, previous_delta, previous_pdf,
                                                  background.pdf, background.sampled_directly);
            result = result + multiply(throughput, background.radiance) * weight;
            break;
        }

        const auto& material = scene.material(hit);
        const double alpha = material.alpha(hit.uv);
        if (alpha < 1 && (alpha == 0 || rng.next() >= alpha)) {
            if (++alpha_crossings > 4096) throw std::runtime_error("Alpha crossing limit exceeded");
            ray.origin = ray.at(std::nextafter(hit.t, infinity)) + ray.direction * 1e-7;
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
        throughput = multiply(throughput, sampled.weight);
        previous_delta = sampled.delta;
        previous_pdf = sampled.pdf;
        previous_position = hit.position;
        if (!finite(throughput) || !finite(result) || !finite(sampled.direction))
            throw std::runtime_error("Nonfinite path state");

        if (bounce >= 3) {
            const double survive = std::clamp(std::max({throughput.x, throughput.y, throughput.z}), 0.05, 0.95);
            if (rng.next() >= survive) break;
            throughput = throughput * (1 / survive);
        }
        ray = spawn(hit, sampled.direction);
        ++bounce;
    }
    return result;
}
