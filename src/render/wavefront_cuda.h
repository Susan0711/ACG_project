#pragma once
#include "render/wavefront.h"
#include <array>

namespace wf { struct SceneView; }

struct DeviceRenderStats {
    uint64_t rounds=0, refills=0, main_queries=0, shadow_slots=0, control_bytes=0, film_bytes=0;
    int peak_paths=0;
    // Milliseconds inside CUDA stages. Timing enabled only for a profiling run.
    std::array<double,6> stage_ms{};
};
class CudaWavefront {
public:
    CudaWavefront(const RenderScene&,const CudaBVH&);
    ~CudaWavefront();
    wf::SceneView device_scene() const;
    DeviceRenderStats render(Film&,const RenderSettings&,int threshold,bool profile=false);
private:
    struct Storage;
    std::unique_ptr<Storage> storage_;
};
