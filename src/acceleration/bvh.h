#pragma once
#include "core/geometry.h"

struct AABB { Vec3 lo,hi;
    bool intersect(const Ray&,double,double) const;
};
struct BVH {
    struct Node { AABB box;
        int left=-1,right=-1;
        std::size_t object=0;
    };
    std::vector<Node> nodes;
    void build(const Scene&,double time_open=0,double time_close=0);
    bool intersect(const Scene&,const Ray&,double,double,HitRecord&) const;
};
