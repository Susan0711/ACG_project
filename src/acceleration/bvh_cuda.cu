#include "acceleration/bvh_cuda.h"
#include "acceleration/intersection_fp32.cuh"
#include "acceleration/shadow_fp32.cuh"
#include "materials/material.h"
#include <math_constants.h>
#include <algorithm>
#include <chrono>
#include <climits>
#include <cfloat>
#include <cmath>
#include <stdexcept>
#include <string>
#include <unordered_map>

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
__device__ bool prepare_ray(const fp32::Ray& ray, fp32::SlabAxis axes[3]) {
    if (!(ray.minimum <= ray.maximum) || !isfinite(ray.time)) return false;
    #pragma unroll
    for (int k=0; k<3; ++k) {
        const float o = fp32::axis(ray.origin,k), d = fp32::axis(ray.direction,k);
        if (!isfinite(o) || !isfinite(d)) return false;
        axes[k] = {o,d,d == 0 ? 0 : __frcp_rd(fabsf(d)),d == 0 ? 0 : __frcp_ru(fabsf(d))};
    }
    return ray.direction.x != 0 || ray.direction.y != 0 || ray.direction.z != 0;
}
struct DeviceBVH {
    const fp32::Node* nodes;
    int count;
    const fp32::Triangle* triangles;
    const fp32::Sphere* spheres;
};
fp32::TexelAxis texel_axis(double a, double b, double c, int period) {
    const double origin = (a-std::floor(a))*period;
    const double du = (b-a)*period, dv = (c-a)*period;
    const float hi = float(origin), u = float(du), v = float(dv);
    return {hi,u,v,float(origin-hi),float(du-u),float(dv-v)};
}
__device__ fp32::Hit closest(DeviceBVH tree, const fp32::Ray& ray,
                             const fp32::SlabAxis axes[3], fp32::Projection projection) {
    fp32::Hit best;
    float maximum = ray.maximum;
    int cursor = 0;
    while (cursor < tree.count) {
        const auto& node = tree.nodes[cursor];
        if (!fp32::box_hit(node,axes,ray.minimum,maximum)) {
            cursor = node.end;
            continue;
        }
        if (node.object >= 0) {
            fp32::Hit h;
            const bool found = node.shape >= 0
                ? fp32::triangle_hit(tree.triangles[node.shape],ray,projection,maximum,h)
                : fp32::sphere_hit(tree.spheres[~node.shape],ray,maximum,h);
            if (found && (best.object < 0 || h.t < maximum || (h.t == maximum && node.object < best.object))) {
                h.object = node.object;
                best = h;
                maximum = h.t;
            }
        }
        ++cursor;
    }
    return best;
}
__global__ void intersect_rays(DeviceBVH tree, const fp32::Ray* queries, fp32::Hit* hits, int count) {
    const int i = blockIdx.x*blockDim.x + threadIdx.x;
    if (i >= count) return;
    const auto ray = queries[i];
    fp32::SlabAxis axes[3];
    hits[i] = prepare_ray(ray,axes) ? closest(tree,ray,axes,fp32::projection(ray.direction)) : fp32::Hit{};
}
// Keep the nearest few distinct layers in one DFS pass. Equal FP32 distances
// select the lowest object ID, exactly as in closest(). A full list narrows
// the traversal interval without losing any hit before its last entry.
struct ShadowLayers {
    static constexpr int capacity = 8;
    fp32::Hit hits[capacity];
    int size = 0;
    float maximum;
    __device__ void insert(fp32::Hit hit, bool opaque) {
        int i = 0;
        while (i < size && hits[i].t < hit.t) ++i;
        if (i < size && hits[i].t == hit.t) {
            if (hits[i].object <= hit.object) return;
        } else {
            if (i == capacity) return;
            if (size < capacity) ++size;
            for (int j=size-1; j>i; --j) hits[j] = hits[j-1];
        }
        hits[i] = hit;
        if (opaque) {
            size = i+1;
            maximum = hit.t;
        } else if (size == capacity) {
            maximum = hits[size-1].t;
        }
    }
};
__device__ void collect_layers(DeviceBVH tree, fp32::ShadowData data, const fp32::Ray& ray,
                               const fp32::SlabAxis axes[3], fp32::Projection projection, ShadowLayers& layers) {
    for (int cursor=0; cursor<tree.count;) {
        const auto& node = tree.nodes[cursor];
        if (!fp32::box_hit(node,axes,ray.minimum,layers.maximum)) {
            cursor = node.end;
            continue;
        }
        ++cursor;
        if (node.object < 0) continue;
        fp32::Hit hit;
        const bool found = node.shape >= 0
            ? fp32::triangle_hit(tree.triangles[node.shape],ray,projection,layers.maximum,hit)
            : fp32::sphere_hit(tree.spheres[~node.shape],ray,layers.maximum,hit);
        if (!found) continue;
        hit.object = node.object;
        const bool opaque = data.surfaces[hit.object].material < 0;
        layers.insert(hit,opaque);
        if (node.shape < 0 && hit.t < layers.maximum) {
            auto exit_ray = ray;
            exit_ray.minimum = nextafterf(hit.t,CUDART_INF_F);
            if (fp32::sphere_hit(tree.spheres[~node.shape],exit_ray,layers.maximum,hit))
                layers.insert(hit,opaque);
        }
    }
}
__global__ void shadow_segments(DeviceBVH tree, fp32::ShadowData data, const fp32::Ray* queries,
                                float* results, int count) {
    const int i = blockIdx.x*blockDim.x + threadIdx.x;
    if (i >= count) return;
    auto ray = queries[i];
    float transmission = 1.0f;
    fp32::SlabAxis axes[3];
    if (prepare_ray(ray,axes)) {
        const auto projection = fp32::projection(ray.direction);
        int crossings = 0;
        while (ray.minimum <= ray.maximum) {
            ShadowLayers layers;
            layers.maximum = ray.maximum;
            collect_layers(tree,data,ray,axes,projection,layers);
            for (int j=0; j<layers.size; ++j) {
                const float alpha = fp32::surface_alpha(data,tree.spheres,ray,layers.hits[j]);
                if (!(alpha >= 0 && alpha <= 1)) { transmission = -2; break; }
                transmission *= 1-alpha;
                if (transmission == 0) break;
                if (++crossings >= 4096) { transmission = -1; break; }
            }
            if (transmission <= 0 || layers.maximum == ray.maximum) break;
            ray.minimum = nextafterf(layers.maximum,CUDART_INF_F);
        }
    }
    results[i] = transmission;
}
}

struct CudaBVH::Storage {
    Vec3 origin{};
    DeviceBuffer<fp32::Node> nodes;
    DeviceBuffer<fp32::Triangle> triangles;
    DeviceBuffer<fp32::Sphere> spheres;
    DeviceBuffer<fp32::Ray> queries;
    DeviceBuffer<fp32::Hit> hits;
    DeviceBuffer<fp32::ShadowSurface> surfaces;
    DeviceBuffer<fp32::TriangleUV> uvs;
    DeviceBuffer<fp32::NearestUV> nearest_uvs;
    DeviceBuffer<fp32::AlphaMaterial> materials;
    DeviceBuffer<fp32::AlphaTexture> textures;
    DeviceBuffer<float> texels, transmission;
    std::vector<fp32::Ray> host_queries;
    std::vector<fp32::Hit> host_hits;
    Event begin, end;
    int node_count = 0;
    Statistics timing;
};

CudaBVH::CudaBVH(const BVH& bvh, const Scene& scene) : CudaBVH(bvh,scene,{}) {}
CudaBVH::CudaBVH(const BVH& bvh, const Scene& scene, const std::vector<Material>& materials)
    : storage_(std::make_unique<Storage>()) {
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
    std::vector<fp32::ShadowSurface> surfaces(materials.empty() ? 0 : scene.objects.size());
    std::vector<fp32::TriangleUV> uvs;
    std::vector<fp32::NearestUV> nearest_uvs;
    std::vector<fp32::AlphaTexture> textures;
    std::vector<fp32::AlphaMaterial> alpha_materials;
    std::vector<float> texels;
    std::unordered_map<const Texture*,int> texture_indices;
    for (const auto& material : materials) {
        if (material.is_emitter() && (material.opacity != 1 || material.opacity_map))
            throw std::invalid_argument("Emissive alpha is unsupported");
        int texture = -1;
        if (const auto* t = material.opacity_map.get()) {
            const auto entry = texture_indices.emplace(t,int(textures.size()));
            texture = entry.first->second;
            if (entry.second) {
                using Filter = fp32::AlphaFilter;
                Filter filter = Filter::constant;
                if (t->type == Texture::Type::procedural) filter = Filter::procedural;
                if (t->type == Texture::Type::image)
                    filter = t->filter() == Texture::Filter::nearest ? Filter::nearest : Filter::bilinear;
                textures.push_back({filter,float(t->color.x),t->width,t->height,texels.size()});
                if (t->type == Texture::Type::image) {
                    if (t->width <= 0 || t->height <= 0 || t->pixels.size() != size_t(t->width)*t->height)
                        throw std::invalid_argument("Invalid opacity image dimensions");
                    for (const auto& p : t->pixels) texels.push_back(float(p.x));
                }
            }
        }
        alpha_materials.push_back({float(material.opacity),texture});
    }
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
        if (!materials.empty()) {
            const int material = std::visit([](const auto& s){ return s.material_id; },shape);
            const auto& m = materials.at(material);
            auto& surface = surfaces[src.object];
            surface = {material,n.shape};
            if (!m.opacity_map && m.opacity == 1) surface.material = -1;
            if (n.shape >= 0 && m.opacity_map && m.opacity_map->type != Texture::Type::constant) {
                const auto& t = std::get<Triangle>(shape);
                const auto& texture = *m.opacity_map;
                if (texture.type == Texture::Type::image && texture.filter() == Texture::Filter::nearest) {
                    surface.uv_index = int(nearest_uvs.size());
                    nearest_uvs.push_back({texel_axis(t.ta.u,t.tb.u,t.tc.u,texture.width),
                                           texel_axis(t.ta.v,t.tb.v,t.tc.v,texture.height)});
                } else {
                    surface.uv_index = int(uvs.size());
                    uvs.push_back({{float(t.ta.u),float(t.ta.v)},{float(t.tb.u),float(t.tb.v)},
                                   {float(t.tc.u),float(t.tc.v)}});
                }
            }
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
    gpu.surfaces.upload(surfaces);
    gpu.uvs.upload(uvs);
    gpu.nearest_uvs.upload(nearest_uvs);
    gpu.materials.upload(alpha_materials);
    gpu.textures.upload(textures);
    gpu.texels.upload(texels);
    gpu.transmission.allocate(capacity);
    gpu.queries.allocate(capacity);
    gpu.hits.allocate(capacity);
    gpu.host_queries.resize(capacity);
    gpu.host_hits.resize(capacity);
}
CudaBVH::~CudaBVH() = default;
CudaBVH::Statistics CudaBVH::stats() const { return storage_->timing; }
void CudaBVH::intersect(const RayQuery* queries, GeometryHit* hits, int count) {
    intersect_batch(queries,hits,count,nullptr,0);
}
void CudaBVH::intersect_batch(const RayQuery* queries, GeometryHit* hits, int main_count,
                              float* transmission, int shadow_count) {
    if (main_count < 0 || shadow_count < 0 || main_count > capacity-shadow_count)
        throw std::length_error("Invalid GPU batch size");
    const int count = main_count + shadow_count;
    if (!count) return;
    auto& g = *storage_;
    if ((main_count && !hits) || (shadow_count && !transmission) || !queries)
        throw std::invalid_argument("Missing GPU batch buffer");
    if (shadow_count && g.node_count && !g.surfaces.ptr)
        throw std::logic_error("Shadow queries require scene materials");
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
    const DeviceBVH tree{g.nodes.ptr,g.node_count,g.triangles.ptr,g.spheres.ptr};
    if (main_count) {
        intersect_rays<<<(main_count+255)/256,256>>>(tree,g.queries.ptr,g.hits.ptr,main_count);
        check(cudaGetLastError(), "launch intersect_rays");
    }
    if (shadow_count) {
        const fp32::ShadowData data{g.surfaces.ptr,g.uvs.ptr,g.nearest_uvs.ptr,g.materials.ptr,g.textures.ptr,g.texels.ptr};
        const auto* rays = g.queries.ptr+main_count;
        auto* result = g.transmission.ptr;
        const int blocks = (shadow_count+255)/256;
        shadow_segments<<<blocks,256>>>(tree,data,rays,result,shadow_count);
        check(cudaGetLastError(), "launch shadow_segments");
    }
    check(cudaEventRecord(g.end.handle), "record end");
    if (shadow_count) {
        check(cudaMemcpy(transmission,g.transmission.ptr,shadow_count*sizeof(float),cudaMemcpyDeviceToHost), "download visibility");
        for (int i=0;i<shadow_count;++i) {
            if (transmission[i] == -1) throw std::runtime_error("Shadow crossing limit exceeded");
            if (transmission[i] == -2) throw std::runtime_error("Opacity must be linear coverage in [0,1]");
        }
    }
    if (main_count)
        check(cudaMemcpy(g.host_hits.data(), g.hits.ptr, main_count*sizeof(fp32::Hit), cudaMemcpyDeviceToHost), "download hits");
    for (int i=0;i<main_count;++i) {
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
