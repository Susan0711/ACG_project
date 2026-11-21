#pragma once
#include "render/render.h"

enum class SamplingMode { bsdf, nee, mis };
Vec3 integrate(const RenderScene&, Ray, Sampler&, int depth, SamplingMode mode = SamplingMode::nee);
