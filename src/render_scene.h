#pragma once
#include "render/film.h"
#include "render/path_integrator.h"
#include <cstdint>

struct RenderSettings {
    static constexpr int max_threads = 256;
    int spp=64, depth=16, threads=1;
    std::uint64_t seed=1;
    SamplingMode mode=SamplingMode::nee;
};

Sampler pixel_sampler(std::uint64_t seed, std::size_t pixel, int sample);
Ray camera_sample(const RenderScene&, const Film&, int x, int y, double spread, Sampler&);

// Render one frame into the supplied film using a shared, prepared scene.
void render(const RenderScene& scene, Film& film, const RenderSettings& settings);
