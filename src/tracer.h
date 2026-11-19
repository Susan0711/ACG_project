#pragma once
#include "geometry.h"
#include <algorithm>
#include <cstdint>
#include <string>
#include <memory>
#include "assets.h"
#include "environment.h"
constexpr double pi=3.14159265358979323846;
inline Vec3 multiply(Vec3 a, Vec3 b) { return {a.x*b.x,a.y*b.y,a.z*b.z};
}
inline bool finite(Vec3 v) { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);
}
struct Sampler {
    std::uint64_t state;
    double next() { state+=0x9e3779b97f4a7c15ULL;
        auto z=state;
        z=(z^(z>>30))*0xbf58476d1ce4e5b9ULL;
        z=(z^(z>>27))*0x94d049bb133111ebULL;
        return double((z^(z>>31))>>11)*0x1.0p-53;
    }
};
struct Camera {
    Vec3 origin{0,0,3.4}, target{0,0,-1};
    double fov=40;
    double lens_radius=0, focus_distance=4;
    double shutter_open=0, shutter_close=0;
    void validate() const;
    Ray sample(double u,double v,double aspect,Sampler&) const;
    Ray generate(double u,double v,double aspect) const;
};
struct Film {
    int width,height;
    std::vector<Vec3> pixels;
    Film(int w,int h):width(w),height(h),pixels(static_cast<std::size_t>(w)*h){}
    void write(const std::string&,double exposure=1) const;
    void write_linear(const std::string&) const;
};
enum class Kind { diffuse, mirror, glass, emissive };
struct Material { Kind kind=Kind::diffuse;
    Vec3 color{0.75,0.75,0.75};
    double ior=1.5;
    std::shared_ptr<Texture> texture{};
    double opacity=1;
    std::shared_ptr<Texture> opacity_map{};
    double alpha(UV uv) const;
    Vec3 albedo(UV uv) const { return texture ? texture->value(uv) : color;
    } };
struct BSDFSample { Vec3 direction,weight;
    double pdf;
    bool delta;
};
double fresnel(double cosine,double eta_i,double eta_t);
BSDFSample sample_bsdf(const Material&,const Ray&,const HitRecord&,Sampler&);
Ray spawn(const HitRecord&,Vec3 direction);
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
struct PointLight { Vec3 position,intensity;
};
struct AreaLight { Vec3 origin,u,v,radiance;
};
struct World {
    Vec3 environment{};
    std::shared_ptr<Environment> hdr{};
    void add_area(AreaLight light);
    Scene scene;
    std::vector<Material> materials;
    std::vector<PointLight> points;
    std::vector<AreaLight> areas;
    BVH bvh;
    Camera camera;
    bool intersect(const Ray& r,double lo,double hi,HitRecord& h,bool accelerated) const { return accelerated?bvh.intersect(scene,r,lo,hi,h):scene.intersect(r,lo,hi,h);
    }
};
double shadow_transmittance(const World&,Ray,double distance,bool accelerated);
World make_effect_scene(const std::string& name,const std::string& root="assets");
World make_world(bool point,bool build=true);
World make_forest(bool gray=false,const std::string& root="assets",bool build=true);
World make_forest_v03(bool gray,const std::string& root,bool detail=false);
enum class SamplingMode { bsdf, nee, mis };
double power_heuristic(double a,double b);
Vec3 integrate(const World&,Ray,Sampler&,int depth,bool direct,bool accelerated,SamplingMode mode=SamplingMode::nee);
World make_sampling_scene();
