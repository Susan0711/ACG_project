#include "acceleration/bvh_cuda.h"
#include "core/sampling.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

static void check_narrow_boxes() {
    // At 2^27 several coordinates inside this triangle round to the same float.
    const double x = std::ldexp(1.0, 27);
    Scene scene{{Triangle{{x+1,0,-4},{x+3,0,-4},{x+1,2,-4}},
                 Triangle{{x+1,0,-6},{x+3,0,-6},{x+1,2,-6}}}};
    BVH tree;
    tree.build(scene, 0, 1, BVHSplit::sah);
    CudaBVH gpu(tree, scene);
    const double inf = std::numeric_limits<double>::infinity();
    const std::vector<RayQuery> rays{
        {{x+2,1,0}, {0,0,-1}, 0,4,4},                // closed interval, triangle edge
        {{x+2,1,0}, {-0.0,0,-1}, 0,std::nextafter(4.,inf),6},
        {{std::nextafter(x+1,-inf),0,0}, {0,0,-1}, 0,0,inf},
        {{x+1.25,0.25,0}, {1e-40,0,-1}, 0,0,inf},    // nonzero FP32 subnormal, reciprocal overflows
        {{x+5,0.5,0}, {-1,0,-1}, 0,0,inf},
        {{x+2,0.5,0}, {0,0,1}, 0,-6,-4}
    };
    std::vector<GeometryHit> hits(rays.size());
    gpu.intersect(rays.data(), hits.data(), int(rays.size()));
    for (std::size_t i = 0; i < rays.size(); ++i) {
        const auto& q = rays[i];
        HitRecord hit;
        const bool found = scene.intersect({q.origin,q.direction,q.time}, q.minimum,q.maximum,hit);
        if (found != (hits[i].object >= 0) || (found && (hits[i].parameters.t != hit.t ||
                std::size_t(hits[i].object) != hit.object_index)))
            throw std::runtime_error("Narrow-box closest-hit mismatch on ray " + std::to_string(i));
    }
}

static void check_shadow_continuation() {
    const float next = std::nextafter(4.0f, std::numeric_limits<float>::infinity());
    Scene scene{{Triangle{{0,0,4},{1,0,4},{0,1,4}},Triangle{{0,0,next},{1,0,next},{0,1,next}}}};
    BVH tree;
    tree.build(scene,0,1,BVHSplit::sah);
    CudaBVH gpu(tree,scene);
    const double inf = std::numeric_limits<double>::infinity();
    RayQuery ray{{0.25,0.25,0},{0,0,1},0,0,inf};
    GeometryHit hit;
    for (int id=0; id<2; ++id) {
        gpu.intersect(&ray,&hit,1);
        if (hit.object != id) throw std::runtime_error("Shadow skipped or repeated a one-ULP layer");
        ray.minimum = std::nextafter(hit.parameters.t,inf);
    }
    gpu.intersect(&ray,&hit,1);
    if (hit.object >= 0) throw std::runtime_error("Shadow did not clear the final layer");
}

int main() {
    try {
        check_narrow_boxes();
        check_shadow_continuation();
        Scene scene{{Sphere{{0,0,-2}, 1, 0, {0.7,0,0}},
                     Triangle{{-2,-2,-4},{2,-2,-4},{0,2,-4}},
                     Triangle{{-2,-2,-4},{2,-2,-4},{0,2,-4}},
                     Sphere{{0,0,-2}, 1, 0, {0.7,0,0}}}};
        BVH tree;
        tree.build(scene, 0, 1, BVHSplit::sah);
        CudaBVH gpu(tree, scene);
        constexpr double fp32_error = std::numeric_limits<float>::epsilon();
        const double inf = std::numeric_limits<double>::infinity();
        std::vector<RayQuery> q = {
            {{0,0,0},{0,0,-1},0,1,1}, {{0,0,0},{0,0,-1},0,1.1,3},
            {{0,0,0},{0,0,-1},0,3.1,4}, {{0,0,0},{0,0,-1},0,4.1,inf},
            {{0,0,0},{0,0,-1},0,2,1}, {{0,0,0},{1,0,0},1,0,inf}};
        Sampler rng{42};
        for (int i=0; i<10000; ++i) {
            const Vec3 direction{rng.next()-0.5, rng.next()-0.5, -1};
            const double time = rng.next();
            q.push_back({{0,0,0},direction,time,0,inf});
            q.push_back({{0,0,-3},direction,time,1e-5,2*rng.next()});
        }
        std::vector<GeometryHit> hits(q.size());
        gpu.intersect(q.data(), hits.data(), int(q.size()));
        for (std::size_t i=0; i<q.size(); ++i) {
            const Ray ray{q[i].origin,q[i].direction,q[i].time};
            HitRecord reference;
            const bool found = scene.intersect(ray,q[i].minimum,q[i].maximum,reference);
            const auto& gpu_hit = hits[i];
            if (found != (gpu_hit.object >= 0) || (found && (std::size_t(gpu_hit.object) != reference.object_index ||
                std::abs(gpu_hit.parameters.t-reference.t) > 64*fp32_error*std::max(1.0,std::abs(reference.t)))))
                throw std::runtime_error("GPU closest-hit mismatch on ray " + std::to_string(i));
            if (found) {
                HitRecord hit;
                std::visit([&](const auto& shape){shape.finish_hit(ray,gpu_hit.parameters,hit);}, scene.objects[gpu_hit.object]);
                const double delta_u = std::abs(hit.uv.u-reference.uv.u);
                if (std::min(delta_u,std::abs(1-delta_u)) > 64*fp32_error ||
                    std::abs(hit.uv.v-reference.uv.v) > 64*fp32_error)
                    throw std::runtime_error("GPU surface coordinates differ");
            }
        }
        std::cout << q.size() << " matching GPU/linear queries\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
