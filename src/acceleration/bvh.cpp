#include "acceleration/bvh.h"
#include <algorithm>
#include <array>
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

    constexpr int bucket_count = 12;

    struct Bucket {
        AABB box{};
        std::size_t count = 0;

        void add(const AABB& other, std::size_t amount = 1) {
            if (amount == 0) return;
            box = count == 0 ? other : merge(box, other);
            count += amount;
        }

        double weighted_area() const {
            const Vec3 d = box.hi - box.lo;
            return count * 2.0 * (d.x * d.y + d.y * d.z + d.z * d.x);
        }
    };

    int bucket_index(double center, double low, double extent) {
        const double fraction = (center - low) / extent;
        return std::min(bucket_count - 1, static_cast<int>(fraction * bucket_count));
    }

    std::size_t split_sah(std::vector<std::size_t>& ids, std::size_t begin, std::size_t end,
                          const std::vector<AABB>& boxes, const std::vector<Vec3>& centers,
                          const AABB& parent) {
        // With two primitives there is only one possible grouping.
        if (end - begin == 2) return split_median(ids, begin, end, boxes, parent);

        AABB center_bounds{centers[ids[begin]], centers[ids[begin]]};
        for (auto i = begin + 1; i < end; ++i) {
            const Vec3 c = centers[ids[i]];
            center_bounds = merge(center_bounds, {c, c});
        }

        double best_cost = std::numeric_limits<double>::infinity();
        int best_axis = -1, best_boundary = -1;
        const Vec3 extent = center_bounds.hi - center_bounds.lo;
        for (int dim = 0; dim < 3; ++dim) {
            const double width = axis(extent, dim), low = axis(center_bounds.lo, dim);
            if (!(width > 0) || !std::isfinite(width)) continue;

            std::array<Bucket, bucket_count> buckets{};
            for (auto i = begin; i < end; ++i) {
                const auto id = ids[i];
                buckets[bucket_index(axis(centers[id], dim), low, width)].add(boxes[id]);
            }

            // right_of[b] covers buckets (b, last], left covers [0, b].
            std::array<Bucket, bucket_count - 1> right_of{};
            Bucket right, left;
            for (int b = bucket_count - 1; b > 0; --b) {
                right.add(buckets[b].box, buckets[b].count);
                right_of[b - 1] = right;
            }
            for (int b = 0; b < bucket_count - 1; ++b) {
                left.add(buckets[b].box, buckets[b].count);
                if (left.count == 0 || right_of[b].count == 0) continue;
                const double cost = left.weighted_area() + right_of[b].weighted_area();
                if (cost < best_cost) {
                    best_cost = cost;
                    best_axis = dim;
                    best_boundary = b;
                }
            }
        }
        if (best_axis < 0) return split_median(ids, begin, end, boxes, parent);
        const double low = axis(center_bounds.lo, best_axis), width = axis(extent, best_axis);
        auto middle = std::stable_partition(ids.begin() + begin, ids.begin() + end, [&](std::size_t id) {
            return bucket_index(axis(centers[id], best_axis), low, width) <= best_boundary;
        });
        return static_cast<std::size_t>(middle - ids.begin());
    }
}
bool AABB::intersect(const Ray& r,double lo,double hi) const {
    return clip_interval(r, lo, hi);
}
bool AABB::clip_interval(const Ray& r,double& lo,double& hi) const {
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
void BVH::build(const Scene& scene,double time_open,double time_close,BVHSplit split) {
    std::vector<Node> built;
    std::vector<AABB> boxes;
    boxes.reserve(scene.objects.size());
    for (const auto& object : scene.objects) {
        boxes.push_back(std::visit([&](const auto& shape) { return bounds(shape, time_open, time_close); }, object));
    }
    std::vector<Vec3> centers;
    if (split == BVHSplit::sah) {
        centers.reserve(boxes.size());
        for (const AABB& box : boxes) centers.push_back(box.lo * 0.5 + box.hi * 0.5);
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
            const auto mid = split == BVHSplit::sah
                ? split_sah(ids, begin, end, boxes, centers, box)
                : split_median(ids, begin, end, boxes, box);
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
bool BVH::intersect(const Scene& scene,const Ray& ray,double lo,double hi,HitRecord& hit,BVHTraversalStats* stats) const {
    bool found=false;
    auto visit=[&](auto&& self,int index)->void { const auto& n=nodes[index];
        if (stats) ++stats->box_tests;
        if(!n.box.intersect(ray,lo,hi))return;
        if(n.left<0) { HitRecord h;
            if (stats) ++stats->primitive_tests;
            if(std::visit([&](const auto& s){return s.intersect(ray,lo,hi,h);},scene.objects[n.object])&&(!found||h.t<hi||(h.t==hi&&n.object<hit.object_index))) {found=true;
                hi=h.t;
                hit=h;
                hit.object_index=n.object;
            } }
        else {self(self,n.left);
            self(self,n.right);
        }
    };
    if(!nodes.empty())visit(visit,0);
    return found;
}
