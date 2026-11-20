#include "acceleration/bvh.h"
#include <algorithm>
#include <numeric>
#include <limits>
namespace {
    double axis(Vec3 v,int a) { return a==0?v.x:a==1?v.y:v.z;
    }
    AABB merge(AABB a,AABB b) { return {{std::min(a.lo.x,b.lo.x),std::min(a.lo.y,b.lo.y),std::min(a.lo.z,b.lo.z)},{std::max(a.hi.x,b.hi.x),std::max(a.hi.y,b.hi.y),std::max(a.hi.z,b.hi.z)}};
    }
    AABB bounds(const Sphere& s,double open,double close) { Vec3 r{s.radius,s.radius,s.radius};
        return merge({s.center_at(open)-r,s.center_at(open)+r},{s.center_at(close)-r,s.center_at(close)+r});
    }
    AABB bounds(const Triangle& t, double open, double close) {
        const AABB base = merge({t.a,t.a}, merge({t.b,t.b}, {t.c,t.c}));
        const Vec3 start = t.velocity * open, end = t.velocity * close;
        const AABB swept = merge({base.lo + start, base.hi + start},
                                 {base.lo + end, base.hi + end});
        const Vec3 padding{1e-9,1e-9,1e-9};
        return {swept.lo - padding, swept.hi + padding};
    }
    std::size_t split_median(std::vector<std::size_t>& ids, std::size_t begin, std::size_t end,
                             const std::vector<AABB>& boxes, const AABB& parent) {
        const Vec3 extent = parent.hi - parent.lo;
        int longest = extent.x > extent.y ? 0 : 1;
        if (extent.z > axis(extent, longest)) longest = 2;
        auto center = [&](std::size_t id) { return axis(boxes[id].lo + boxes[id].hi, longest); };
        std::stable_sort(ids.begin() + begin, ids.begin() + end,
                         [&](std::size_t a, std::size_t b) { return center(a) < center(b); });
        return begin + (end - begin) / 2;
    }
}
bool AABB::intersect(const Ray& r,double lo,double hi) const {
    if (!finite(r.origin)||!finite(r.direction)||std::isnan(lo)||std::isnan(hi)||lo>hi) return false;
    for(int a=0;a<3;++a) { double o=axis(r.origin,a),d=axis(r.direction,a),mn=axis(this->lo,a),mx=axis(this->hi,a);
        if(d==0) { if(o<mn||o>mx)return false;
            continue;
        }
        double t0=(mn-o)/d,t1=(mx-o)/d;
        if(t0>t1)std::swap(t0,t1);
        lo=std::max(lo,t0);
        hi=std::min(hi,t1);
        if(lo>hi)return false;
    } return true;
}
void BVH::build(const Scene& scene,double time_open,double time_close) {
    std::vector<Node> built;
    std::vector<AABB> boxes;
    boxes.reserve(scene.objects.size());
    for (const auto& object : scene.objects) {
        boxes.push_back(std::visit([&](const auto& shape) { return bounds(shape, time_open, time_close); }, object));
    }
    std::vector<std::size_t> ids(boxes.size());
    std::iota(ids.begin(), ids.end(), std::size_t{0});
    auto build_range = [&](auto&& self, std::size_t begin, std::size_t end) -> int {
        const int index = static_cast<int>(built.size());
        AABB box = boxes[ids[begin]];
        for (auto i = begin + 1; i < end; ++i) box = merge(box, boxes[ids[i]]);
        built.push_back({box});
        if (end - begin == 1) {
            built[index].object = ids[begin];
        } else {
            const auto mid = split_median(ids, begin, end, boxes, box);
            const int left = self(self, begin, mid);
            const int right = self(self, mid, end);
            built[index].left = left;
            built[index].right = right;
        }
        return index;
    };
    if (!ids.empty()) build_range(build_range, 0, ids.size());
    nodes.swap(built);
}
bool BVH::intersect(const Scene& scene,const Ray& ray,double lo,double hi,HitRecord& hit) const {
    bool found=false;
    std::size_t best=std::numeric_limits<std::size_t>::max();
    auto visit=[&](auto&& self,int index)->void { const auto& n=nodes[index];
        if(!n.box.intersect(ray,lo,hi))return;
        if(n.left<0) { HitRecord h;
            if(std::visit([&](const auto& s){return s.intersect(ray,lo,hi,h);},scene.objects[n.object])&&(!found||h.t<hi||(h.t==hi&&n.object<best))) {found=true;
                hi=h.t;
                hit=h;
                best=n.object;
            } }
        else {self(self,n.left);
            self(self,n.right);
        }
    };
    if(!nodes.empty())visit(visit,0);
    return found;
}
