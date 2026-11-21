#pragma once
#include "render/device_scene.cuh"

namespace wf {

enum class EventKind { ended, transparent, surface, medium };
struct Frame { int width,height,spp,depth; uint64_t seed; SamplingMode mode; float spread; };
struct Path {
    fp32::Ray ray;
    V result{}, throughput{1,1,1}, previous_position{};
    Random rng;
    float cone=0, advanced=0, previous_pdf=0;
    int sample,band=0,bounce=0,crossings=0;
    bool previous_delta=true;
};
struct Vertex {
    Surface hit;
    BSDFMaterial material;
    EventKind kind;
    V direct;
};
struct ShadowTerm { V weighted; };
struct Counts { int live,dead,error; };
struct SampleAddress { int x,y,sample; };

__global__ void classify(SceneView,Frame,const int*,Path*,const fp32::Hit*,Vertex*,int,Counts*);
__global__ void light_samples(SceneView,Frame,const int*,Path*,const Vertex*,int,int,int,fp32::Ray*,ShadowTerm*);
__global__ void resolve_shadows(Vertex*,const ShadowTerm*,const float*,int,int,Counts*);
__global__ void finish_paths(SceneView,Frame,uint64_t,Path*,const Vertex*,const int*,int,int*,int*,Counts*,V*);
} // namespace wf
