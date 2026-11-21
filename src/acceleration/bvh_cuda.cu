#include "acceleration/bvh_cuda.h"
#include "acceleration/intersection_fp32.cuh"
#include <algorithm>
#include <chrono>
#include <climits>
#include <cfloat>
#include <cmath>
#include <stdexcept>
#include <string>

namespace {
void check(cudaError_t result, const char* operation) {
    if (result != cudaSuccess) throw std::runtime_error(std::string(operation) + ": " + cudaGetErrorString(result));
}
template<class T> struct DeviceBuffer {
    T* ptr = nullptr;
    void allocate(std::size_t count) {
        if (count) check(cudaMalloc(&ptr, count*sizeof(T)), "cudaMalloc");
    }
    void upload(const std::vector<T>& values) {
        allocate(values.size());
        if (ptr) check(cudaMemcpy(ptr, values.data(), values.size()*sizeof(T), cudaMemcpyHostToDevice), "upload scene");
    }
    ~DeviceBuffer() { if (ptr) cudaFree(ptr); }
};
struct Event {
    cudaEvent_t handle{};
    Event() { check(cudaEventCreate(&handle), "create event"); }
    ~Event() { cudaEventDestroy(handle); }
};
// The host keeps the CPU renderer's FP64 interface. Only the packed FP32
// representation crosses PCIe. Rebase large world coordinates before narrowing.
float directed_float(double x, bool down) {
    if (x == INFINITY || x == -INFINITY) return static_cast<float>(x);
    if (x > FLT_MAX) return down ? FLT_MAX : INFINITY;
    if (x < -FLT_MAX) return down ? -INFINITY : -FLT_MAX;
    const float f = static_cast<float>(x);
    if (down && f > x) return std::nextafter(f, -INFINITY);
    if (!down && f < x) return std::nextafter(f, INFINITY);
    return f;
}
fp32::V pack(Vec3 v) {
    return {static_cast<float>(v.x),static_cast<float>(v.y),static_cast<float>(v.z)};
}
fp32::V pack_geometry(Vec3 v) {
    if (!finite(v) || std::max({std::abs(v.x),std::abs(v.y),std::abs(v.z)}) > FLT_MAX)
        throw std::range_error("GPU geometry exceeds the FP32 coordinate range");
    return pack(v);
}
__global__ void intersect_rays(const fp32::Node* nodes, int count_nodes,
                               const fp32::Triangle* triangles, const fp32::Sphere* spheres,
                               const fp32::Ray* queries, fp32::Hit* hits, int count) {
    const int i = blockIdx.x*blockDim.x + threadIdx.x;
    if (i >= count) return;
    const fp32::Ray ray = queries[i];
    fp32::Hit best;
    hits[i] = best;
    if (!(ray.minimum <= ray.maximum) || !isfinite(ray.time)) return;
    fp32::SlabAxis axes[3];
    #pragma unroll
    for (int k=0; k<3; ++k) {
        const float o = fp32::axis(ray.origin,k), d = fp32::axis(ray.direction,k);
        if (!isfinite(o) || !isfinite(d)) return;
        axes[k] = {o,d,d == 0 ? 0 : __frcp_rd(fabsf(d)),d == 0 ? 0 : __frcp_ru(fabsf(d))};
    }
    if (ray.direction.x == 0 && ray.direction.y == 0 && ray.direction.z == 0) return;
    const auto projection = fp32::projection(ray.direction);
    float maximum = ray.maximum;
    int cursor = 0;
    while (cursor < count_nodes) {
        const auto& node = nodes[cursor];
        if (!fp32::box_hit(node,axes,ray.minimum,maximum)) {
            cursor = node.end;
            continue;
        }
        if (node.object >= 0) {
            fp32::Hit h;
            const bool found = node.shape >= 0
                ? fp32::triangle_hit(triangles[node.shape],ray,projection,maximum,h)
                : fp32::sphere_hit(spheres[~node.shape],ray,maximum,h);
            if (found && (best.object < 0 || h.t < maximum || (h.t == maximum && node.object < best.object))) {
                h.object = node.object;
                best = h;
                maximum = h.t;
            }
        }
        ++cursor;
    }
    hits[i] = best;
}
}

struct CudaBVH::Storage {
    Vec3 origin{};
    DeviceBuffer<fp32::Node> nodes;
    DeviceBuffer<fp32::Triangle> triangles;
    DeviceBuffer<fp32::Sphere> spheres;
    DeviceBuffer<fp32::Ray> queries;
    DeviceBuffer<fp32::Hit> hits;
    std::vector<fp32::Ray> host_queries;
    std::vector<fp32::Hit> host_hits;
    Event begin, end;
    int node_count = 0;
    Statistics timing;
};

CudaBVH::CudaBVH(const BVH& bvh, const Scene& scene) : storage_(std::make_unique<Storage>()) {
    if (bvh.nodes.size() > INT_MAX) throw std::length_error("GPU BVH exceeds 32-bit node indices");
    auto& gpu = *storage_;
    if (!bvh.nodes.empty()) {
        const AABB& b = bvh.nodes.front().box;
        gpu.origin = {std::clamp(0.0,b.lo.x,b.hi.x), std::clamp(0.0,b.lo.y,b.hi.y),
                      std::clamp(0.0,b.lo.z,b.hi.z)};
    }
    std::vector<fp32::Node> nodes(bvh.nodes.size());
    std::vector<fp32::Triangle> triangles;
    std::vector<fp32::Sphere> spheres;
    // Refit the same DFS topology to enclose the rounded leaves.
    for (std::size_t end = nodes.size(); end > 0; --end) {
        const auto index = end-1;
        const auto& src = bvh.nodes[index];
        auto& n = nodes[index];
        n.end = int(src.subtree_end);
        n.object = -1;
        if (!src.is_leaf()) {
            const auto& left = nodes[index+1];
            const auto& right = nodes[left.end];
            for (int k=0;k<3;++k) {
                n.lo[k] = std::min(left.lo[k],right.lo[k]);
                n.hi[k] = std::max(left.hi[k],right.hi[k]);
            }
            continue;
        }
        n.object = int(src.object);
        const auto& shape = scene.objects[src.object];
        Vec3 position, velocity;
        double radius = 0;
        if (const auto* t = std::get_if<Triangle>(&shape)) {
            n.shape = int(triangles.size());
            position = t->a-gpu.origin;
            velocity = t->velocity;
            triangles.push_back({pack_geometry(position),pack_geometry(t->b-gpu.origin),
                                 pack_geometry(t->c-gpu.origin),pack_geometry(velocity)});
        } else {
            const auto& s = std::get<Sphere>(shape);
            position = s.center-gpu.origin;
            velocity = s.velocity;
            radius = s.radius;
            if (!(radius >= 0 && radius <= FLT_MAX)) throw std::range_error("GPU sphere radius exceeds FP32");
            n.shape = ~int(spheres.size());
            spheres.push_back({pack_geometry(position),pack_geometry(velocity),static_cast<float>(radius)});
        }
        const Vec3 lo = src.box.lo-gpu.origin, hi = src.box.hi-gpu.origin;
        const double low[]{lo.x,lo.y,lo.z}, high[]{hi.x,hi.y,hi.z};
        const double at_zero[]{position.x,position.y,position.z}, speed[]{velocity.x,velocity.y,velocity.z};
        for (int k=0;k<3;++k) {
            // A moving vertex rounds its base, velocity, shutter time and translated sum.
            const double scale = std::max(std::abs(low[k]),std::abs(high[k])) + std::abs(at_zero[k]) + radius;
            const double pad = speed[k] != 0 || radius != 0 ? 8*FLT_EPSILON*scale + 8*FLT_MIN : 0;
            n.lo[k] = directed_float(low[k]-pad,true);
            n.hi[k] = directed_float(high[k]+pad,false);
        }
    }
    gpu.node_count = int(nodes.size());
    gpu.nodes.upload(nodes);
    gpu.triangles.upload(triangles);
    gpu.spheres.upload(spheres);
    gpu.queries.allocate(capacity);
    gpu.hits.allocate(capacity);
    gpu.host_queries.resize(capacity);
    gpu.host_hits.resize(capacity);
}
CudaBVH::~CudaBVH() = default;
CudaBVH::Statistics CudaBVH::stats() const { return storage_->timing; }
void CudaBVH::intersect(const RayQuery* queries, GeometryHit* hits, int count) {
    if (count < 0 || count > capacity) throw std::length_error("Invalid GPU batch size");
    if (!count) return;
    auto& g = *storage_;
    const auto start = std::chrono::steady_clock::now();
    for (int i=0;i<count;++i) {
        const RayQuery& r = queries[i];
        // Inward rounding preserves the caller's closed interval. In particular,
        // nextafter(previous_t,+inf) excludes that hit after conversion to float.
        g.host_queries[i] = {pack(r.origin-g.origin),pack(r.direction),static_cast<float>(r.time),
                             directed_float(r.minimum,false),directed_float(r.maximum,true)};
    }
    check(cudaMemcpy(g.queries.ptr, g.host_queries.data(), count*sizeof(fp32::Ray), cudaMemcpyHostToDevice), "upload rays");
    check(cudaEventRecord(g.begin.handle), "record start");
    intersect_rays<<<(count+255)/256,256>>>(g.nodes.ptr, g.node_count, g.triangles.ptr, g.spheres.ptr,
                                             g.queries.ptr, g.hits.ptr, count);
    check(cudaGetLastError(), "launch intersect_rays");
    check(cudaEventRecord(g.end.handle), "record end");
    check(cudaMemcpy(g.host_hits.data(), g.hits.ptr, count*sizeof(fp32::Hit), cudaMemcpyDeviceToHost), "download hits");
    for (int i=0;i<count;++i) {
        const auto& h = g.host_hits[i];
        hits[i] = {{h.t,h.u,h.v},h.object};
    }
    float ms = 0;
    check(cudaEventElapsedTime(&ms, g.begin.handle, g.end.handle), "time intersect_rays");
    ++g.timing.batches;
    g.timing.queries += count;
    g.timing.kernel_ms += ms;
    g.timing.batch_ms += std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
}
