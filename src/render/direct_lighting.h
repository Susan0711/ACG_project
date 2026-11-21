#pragma once
#include "render/path_integrator.h"

Vec3 direct_lighting(const RenderScene& scene, const Ray& incident, const HitRecord& hit,
                     const Material& material, Sampler& rng, SamplingMode mode);
Vec3 medium_direct_lighting(const RenderScene&, Vec3 position, double time, Sampler&, SamplingMode);

// Weight of emission reached by following the path to a light or the environment.
double emission_weight(SamplingMode mode, bool previous_delta, double scatter_pdf,
                       double light_pdf, bool sampled_by_lights);
double power_heuristic(double sample_pdf, double other_pdf);
