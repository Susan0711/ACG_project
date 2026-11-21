#pragma once
#include "acceleration/bvh.h"
#include <memory>
struct Material;
namespace fp32 { struct Ray; struct Hit; struct GeometryView; }

struct RayQuery {
    Vec3 origin, direction;
    double time, minimum, maximum;
};
struct GeometryHit {
    HitParameters parameters{};
    int object = -1;
};

class CudaBVH {
public:
    static constexpr int capacity = 32768;
    CudaBVH(const BVH&, const Scene&);
    CudaBVH(const BVH&, const Scene&, const std::vector<Material>&);
    ~CudaBVH();
    void intersect(const RayQuery*, GeometryHit*, int count);
    // Queries are packed as closest-hit rays followed by complete shadow segments.
    void intersect_batch(const RayQuery*, GeometryHit*, int main_count,
                         float* surface_transmittance, int shadow_count);
    struct Statistics {
        std::uint64_t batches = 0, queries = 0;
        double kernel_ms = 0, batch_ms = 0;
    };
    Statistics stats() const;
    Vec3 coordinate_origin() const;
    fp32::GeometryView device_geometry() const;
    void intersect_device(const fp32::Ray*, fp32::Hit*, int count) const;
    void shadow_device(const fp32::Ray*, float*, int count) const;
private:
    struct Storage;
    std::unique_ptr<Storage> storage_;
};
