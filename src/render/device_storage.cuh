#pragma once
#include "render/device_scene.cuh"
#include "render/wavefront_cuda.h"
#include <stdexcept>
#include <string>

namespace wf {
inline void check(cudaError_t result,const char* operation) {
    if (result!=cudaSuccess) throw std::runtime_error(std::string(operation)+": "+cudaGetErrorString(result));
}
template<class T> class Buffer {
public:
    T* ptr=nullptr;
    Buffer()=default;
    explicit Buffer(size_t n) { allocate(n); }
    Buffer(const Buffer&)=delete;
    Buffer& operator=(const Buffer&)=delete;
    ~Buffer() { if (ptr) cudaFree(ptr); }
    void allocate(size_t n) { if (n) check(cudaMalloc(&ptr,n*sizeof(T)),"allocate CUDA buffer"); }
    void upload(const std::vector<T>& v) {
        allocate(v.size());
        if (!v.empty()) check(cudaMemcpy(ptr,v.data(),v.size()*sizeof(T),cudaMemcpyHostToDevice),"upload shading data");
    }
};
}
struct CudaWavefront::Storage {
    explicit Storage(const CudaBVH& tree) : bvh(tree) {}
    const CudaBVH& bvh;
    wf::SceneView view{};
    wf::Buffer<wf::Object> objects;
    wf::Buffer<wf::MaterialData> materials;
    wf::Buffer<wf::V> normals,pixels;
    wf::Buffer<wf::Map> maps;
    wf::Buffer<wf::Level> levels;
    wf::Buffer<wf::Point> points;
    wf::Buffer<wf::Area> areas;
    wf::Buffer<wf::EnvironmentCell> environment;
};
