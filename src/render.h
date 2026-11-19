#pragma once
#include "tracer.h"
struct RenderSettings {
    int spp=64, depth=16, threads=1;
    std::uint64_t seed=1;
    bool direct=true, accelerated=true;
    SamplingMode mode=SamplingMode::nee;
};
void render(const World& world, Film& film, const RenderSettings& settings);
