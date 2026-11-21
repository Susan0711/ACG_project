#pragma once
#include "render/path_integrator.h"

struct DirectLightSample {
    ShadowSegment segment;
    Vec3 radiance;
    double cosine, weight, pdf;
    Vec3 contribution(double visibility) const { return radiance * (cosine * visibility * weight / pdf); }
};

Vec3 direct_lighting(const RenderScene& scene, const Ray& incident, const HitRecord& hit,
                     const Material& material, Sampler& rng, SamplingMode mode,
                     std::vector<DirectLightSample>* pending = nullptr);
Vec3 medium_direct_lighting(const RenderScene&, Vec3 position, double time, Sampler&, SamplingMode,
                            std::vector<DirectLightSample>* pending = nullptr);

// Weight of emission reached by following the path to a light or the environment.
double emission_weight(SamplingMode mode, bool previous_delta, double scatter_pdf,
                       double light_pdf, bool sampled_by_lights);
double power_heuristic(double sample_pdf, double other_pdf);
