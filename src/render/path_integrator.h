#pragma once
#include "render/render.h"

enum class SamplingMode { bsdf, nee, mis };
Vec3 integrate(const RenderScene&, Ray, Sampler&, int depth, SamplingMode mode = SamplingMode::nee);

struct DirectLightSample;
struct PathState {
    Ray ray;
    Vec3 result{}, throughput{1,1,1}, previous_position{};
    double previous_pdf = 0;
    bool previous_delta = true;
    int bounce = 0, alpha_crossings = 0;
};

// Advance one intersection. The batch renderer resolves pending direct light before the next vertex.
bool advance_path(const RenderScene&, PathState&, Sampler&, int depth, SamplingMode, Wavelength,
                  bool hit_surface, const HitRecord&, std::vector<DirectLightSample>* pending,
                  Vec3& direct_throughput);
