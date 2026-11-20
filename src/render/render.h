#pragma once
#include "render/film.h"
#include "render/path_integrator.h"
struct RenderSettings {
    static constexpr int max_threads = 256;
    int spp=64, depth=16, threads=1;
    std::uint64_t seed=1;
    SamplingMode mode=SamplingMode::nee;
};
void render(const RenderScene& scene, Film& film, const RenderSettings& settings);
