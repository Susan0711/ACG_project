#pragma once
#include "acceleration/shadow_fp32.cuh"
#include "render/path_integrator.h"
#include <math_constants.h>

namespace wf {
using V = fp32::V;
constexpr float pi = 3.14159265358979323846f;
__device__ inline float clamp(float x,float lo,float hi) { return fminf(hi,fmaxf(x,lo)); }
__device__ inline V multiply(V a,V b) { return {a.x*b.x,a.y*b.y,a.z*b.z}; }
__device__ inline bool finite(V v) { return isfinite(v.x) && isfinite(v.y) && isfinite(v.z); }
struct Random {
    uint64_t state;
    __device__ static uint64_t mix(uint64_t x) {
        x=(x^(x>>30))*0xbf58476d1ce4e5b9ULL;
        x=(x^(x>>27))*0x94d049bb133111ebULL;
        return x^(x>>31);
    }
    __device__ uint64_t next53() {
        state += 0x9e3779b97f4a7c15ULL;
        return mix(state)>>11;
    }
    __device__ float next() { return fminf(0x1.fffffep-1f,__ull2float_rn(next53())*0x1p-53f); }
};
struct MaterialData {
    Kind kind;
    V color, coat_absorption;
    float ior, cauchy_b, metallic, roughness, coat_weight, coat_roughness, coat_ior, coat_thickness;
    int toon_bands, texture, normal_map, roughness_map;
};
struct Object { int material, light, normals; bool has_uv; fp32::TriangleUV uv; };
struct CameraData { V origin,forward,right,up; float scale,lens,focus,open,close; };
struct Atmosphere { V lo,hi,emission; float bottom,top,albedo; };
struct Point { V position,intensity; };
struct Area { V origin,u,v,radiance,normal; float area; };
struct EnvironmentCell { V radiance; float pdf; uint64_t cdf53; };
struct Level { size_t offset; int width,height; };
struct Map { Texture::Type type; Texture::Filter filter; V color; int first,last; };
struct SceneView {
    fp32::GeometryView geo;
    const Object* objects;
    const V* normals;
    const MaterialData* materials;
    const Map* maps;
    const Level* levels;
    const V* pixels;
    CameraData camera;
    Atmosphere fog;
    const Point* points;
    const Area* areas;
    const EnvironmentCell* environment;
    int points_count, areas_count, env_width, env_height;
    float rotation;
    V background;
    bool mipmaps, spectral;
    __host__ __device__ int light_count() const { return points_count+areas_count+(env_width>0); }
};
struct Surface {
    V position,normal,outward_normal;
    float2 uv;
    float albedo_lod;
    bool front_face;
};
struct BSDFMaterial : MaterialData {
    V base;
    __device__ V albedo(float2,float) const { return base; }
    __device__ float roughness_at(float2) const { return roughness; }
    __device__ bool has_continuous_bsdf() const {
        return kind==Kind::diffuse || kind==Kind::principled || kind==Kind::coated;
    }
    __device__ float ior_at(Wavelength band) const;
};
struct Evaluation { V value; float pdf; };
struct Scatter { V direction,weight; float pdf; bool delta; };

__device__ inline V image_level(const SceneView& s,const Level& l,float2 uv,bool nearest) {
    const float u=uv.x-floorf(uv.x), v=uv.y-floorf(uv.y);
    const V* pixels=s.pixels+l.offset;
    const int w=l.width,h=l.height;
    if(nearest) return pixels[(h-1-min(h-1,int(v*h)))*w+min(w-1,int(u*w))];
    const float x=u*w-0.5f, y=(1-v)*h-0.5f;
    const int ix=int(floorf(x)),iy=int(floorf(y));
    const float fx=x-ix,fy=y-iy;
    const int x0=(ix+w)%w,x1=(ix+1)%w,y0=((iy+h)%h)*w,y1=((iy+1)%h)*w;
    return (pixels[y0+x0]*(1-fx)+pixels[y0+x1]*fx)*(1-fy)
         + (pixels[y1+x0]*(1-fx)+pixels[y1+x1]*fx)*fy;
}
__device__ inline V sample_map(const SceneView& s,int id,float2 uv,float lod=0) {
    const auto& m=s.maps[id];
    if(m.type==Texture::Type::constant) return m.color;
    if(m.type==Texture::Type::procedural) return m.color*(0.7f+0.3f*sinf(uv.x*37+sinf(uv.y*19)));
    lod=clamp(lod,0,float(m.last));
    const int level=int(lod);
    const V a=image_level(s,s.levels[m.first+level],uv,m.filter==Texture::Filter::nearest);
    const float f=lod-level;
    return f==0 ? a : a*(1-f)+image_level(s,s.levels[m.first+level+1],uv,false)*f;
}
} // namespace wf
