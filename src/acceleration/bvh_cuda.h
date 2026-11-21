#pragma once
#include "acceleration/bvh.h"
#include <memory>

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
    ~CudaBVH();
    void intersect(const RayQuery*, GeometryHit*, int count);
    struct Statistics {
        std::uint64_t batches = 0, queries = 0;
        double kernel_ms = 0, batch_ms = 0;
    };
    Statistics stats() const;
private:
    struct Storage;
    std::unique_ptr<Storage> storage_;
};
