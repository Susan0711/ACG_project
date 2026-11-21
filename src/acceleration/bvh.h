#pragma once
#include "core/geometry.h"
#include <cstdint>

enum class BVHSplit { median, sah };

struct BVHTraversalStats {
    std::uint64_t box_tests = 0;
    std::uint64_t primitive_tests = 0;
};

struct AABB { Vec3 lo,hi;
    bool intersect(const Ray&,double,double) const;
};
struct BVH {
    struct Node { AABB box;
        int left=-1,right=-1;
        std::size_t object=0;
    };
    std::vector<Node> nodes;
    void build(const Scene&, double time_open=0, double time_close=0, BVHSplit split=BVHSplit::median);
    bool intersect(const Scene&, const Ray&, double, double, HitRecord&, BVHTraversalStats* stats=nullptr) const;
};
