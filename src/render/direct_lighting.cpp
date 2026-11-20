#include "render/direct_lighting.h"
#include <cmath>

double power_heuristic(double sample_pdf, double other_pdf) {
    if (sample_pdf <= 0) return 0;
    if (other_pdf <= 0) return 1;
    const double ratio = other_pdf / sample_pdf;
    return 1 / (1 + ratio * ratio);
}

double emission_weight(SamplingMode mode, bool previous_delta, double bsdf_pdf,
                       double light_pdf, bool sampled_by_lights) {
    if (mode == SamplingMode::bsdf || previous_delta || !sampled_by_lights) return 1;
    if (mode == SamplingMode::nee) return 0;
    return power_heuristic(bsdf_pdf, light_pdf);
}

Vec3 direct_lighting(const RenderScene& scene, const Ray& incident, const HitRecord& hit,
                     const Material& material, Sampler& rng, SamplingMode mode) {
    if (mode == SamplingMode::bsdf || !material.has_continuous_bsdf()) return {};

    Vec3 result{};
    scene.lights().sample_each(hit.position, rng, [&](const LightSample& light) {
        if (light.pdf <= 0) return;
        const auto bsdf = evaluate_bsdf(material, incident, hit, light.direction);
        const double cosine = std::abs(hit.normal.dot(light.direction));
        if (cosine == 0 || (bsdf.value.x == 0 && bsdf.value.y == 0 && bsdf.value.z == 0)) return;

        const double visibility = scene.visibility(hit, light);
        const double weight = mode == SamplingMode::mis && !light.delta
            ? power_heuristic(light.pdf, bsdf.pdf) : 1;
        result = result + multiply(bsdf.value, light.radiance) * (cosine * visibility * weight / light.pdf);
    });
    return result;
}
