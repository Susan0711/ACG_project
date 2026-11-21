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
    bool clip_interval(const Ray&, double& enter, double& exit) const;
};
struct BVH {
    struct Node {
        AABB box;
        // Nodes are in preorder. This is the first index after this subtree.
        std::size_t subtree_end = 0;
        std::size_t object = HitRecord::no_object;
        bool is_leaf() const { return object != HitRecord::no_object; }
    };
    std::vector<Node> nodes;
    void build(const Scene&, double time_open=0, double time_close=0, BVHSplit split=BVHSplit::median);
    bool intersect(const Scene&, const Ray&, double, double, HitRecord&, BVHTraversalStats* stats=nullptr) const;
};
