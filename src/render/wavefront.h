#pragma once
#include "acceleration/bvh_cuda.h"
#include "render_scene.h"

struct QueueStats {
    std::uint64_t refills = 0;
    int peak_paths = 0;
};
QueueStats render_wavefront(const RenderScene&, Film&, const RenderSettings&, CudaBVH&, int refill_threshold);
