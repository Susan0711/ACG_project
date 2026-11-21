#include "materials/material_reader.h"
#include "render_scene.h"
#include "scene_init.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
bool close(Vec3 a, Vec3 b, double eps=1e-10) {
    return finite(a) && finite(b) && (a-b).dot(a-b) < eps*eps;
}

void parameters() {
    using Json = nlohmann::json;
    Json entry{{"type","coated"},{"color",{0.8,0.3,0.1}}};
    const auto m = read_material(entry, ".");
    check(m.kind == Kind::coated && m.has_continuous_bsdf() && m.coat_ior == 1.5
          && m.coat_weight == 1 && m.coat_thickness == 0, "Coated defaults");
    for (const auto* field : {"coat_weight", "coat_roughness", "coat_ior", "coat_thickness"}) {
        auto bad = entry; bad[field] = -0.1;
        bool rejected = false;
        try { read_material(bad, "."); } catch (const std::exception&) { rejected = true; }
        check(rejected, "Reject negative coat parameters");
    }
    for (const auto* field : {"coat_weight", "coat_roughness"}) {
        auto bad = entry; bad[field] = 1.1;
        bool rejected = false;
        try { read_material(bad, "."); } catch (const std::exception&) { rejected = true; }
        check(rejected, "Bound coat weights and roughness");
    }
    entry["coat_absorption"] = {-1,0,0};
    bool rejected = false;
    try { read_material(entry, "."); } catch (const std::exception&) { rejected = true; }
    check(rejected, "Reject negative absorption");
    auto scene = load_scene_file("scene/coated_material.json");
    check(scene.materials.size() >= 4, "Load the comparison scene");
}

void model() {
    Material m; m.kind=Kind::coated; m.color={0.8,0.3,0.1}; m.roughness=0.65;
    const Ray ray{{0,0,1},{0,0,-1}};
    HitRecord h; h.set(ray,1,{0,0,1},0);
    Material base=m; base.kind=Kind::principled;
    m.coat_weight=0;
    Sampler a{9}, b{9};
    for(int i=0;i<256;++i) {
        const auto x=sample_bsdf(m,ray,h,a), y=sample_bsdf(base,ray,h,b);
        check(close(x.direction,y.direction) && close(x.weight,y.weight) && x.pdf==y.pdf,
              "Disabled coat preserves base samples and RNG");
    }
    check(a.state==b.state, "Disabled coat RNG state");
    m.coat_weight=1; m.coat_roughness=0.35;
    const Vec3 light=Vec3{0.4,0.2,1}.normalized();
    const auto clear=evaluate_bsdf(m,ray,h,light);
    m.coat_thickness=1; m.coat_absorption={0,0.5,1};
    const auto tinted=evaluate_bsdf(m,ray,h,light);
    check(tinted.value.x==clear.value.x && tinted.value.y<clear.value.y
          && tinted.value.z<clear.value.z && tinted.pdf==clear.pdf,
          "Absorption attenuates substrate by channel without changing sampling density");
    Material opaque=m;
    opaque.coat_absorption={1000,1000,1000};
    Material black=opaque;
    black.color={}; black.metallic=1;
    const auto top=evaluate_bsdf(opaque,ray,h,light).value;
    check(top.x>0 && close(top,evaluate_bsdf(black,ray,h,light).value),
          "Opaque film removes substrate but preserves the top-interface reflection");
    const Ray reverse{{}, light * -1};
    check(close(tinted.value,evaluate_bsdf(m,reverse,h,{0,0,1}).value), "Layer reciprocity");
    m.coat_ior=1; m.coat_absorption={};
    check(close(evaluate_bsdf(m,ray,h,light).value,evaluate_bsdf(base,ray,h,light).value),
          "Index-matched transparent layer has no reflection or attenuation");
    m.coat_ior=1.5;
    for(double weight : {0.0,0.3,1.0}) for(double rough : {0.0,0.2,1.0}) {
        m.coat_weight=weight; m.coat_roughness=rough;
        for(double cosine : {1.0,0.5,1e-5}) {
            const Ray incident{{},{std::sqrt(1-cosine*cosine),0,-cosine}};
            Sampler rng{17};
            for(int i=0;i<2048;++i) {
                const auto s=sample_bsdf(m,incident,h,rng);
                const auto e=evaluate_bsdf(m,incident,h,s.direction);
                check(finite(s.weight) && std::isfinite(s.pdf) && s.pdf>=0 && s.pdf==e.pdf,
                      "Finite mixture samples and matching query PDF");
                if(s.pdf>0) check(close(s.weight,e.value*(h.normal.dot(s.direction)/s.pdf),1e-8),
                                  "Full layered BSDF over full mixture PDF");
            }
        }
    }
}

void transport() {
    World w;
    auto env=std::make_shared<Environment>(); env->width=4; env->height=2;
    env->pixels.assign(8,{1,1,1}); env->prepare(); w.lights.hdr=env;
    Material m; m.kind=Kind::coated; m.color={0.8,0.8,0.8}; m.roughness=0.6;
    m.coat_roughness=0.4;
    w.materials={m};
    w.scene.objects.push_back(Triangle{{-100,-100,0},{100,-100,0},{0,100,0},0});
    const RenderScene scene(w); const Ray ray{{0,0,1},{0,0,-1}};
    HitRecord h; check(scene.intersect(ray,0,10,h),"Hit furnace plane");
    // Independent midpoint integration of f*cos over the hemisphere.
    double integral=0;
    constexpr int rows=1024, columns=128;
    for(int y=0;y<rows;++y) for(int x=0;x<columns;++x) {
        const double c=(y+0.5)/rows, phi=2*pi*(x+0.5)/columns, s=std::sqrt(1-c*c);
        integral+=evaluate_bsdf(m,ray,h,{s*std::cos(phi),s*std::sin(phi),c}).value.x*c;
    }
    integral*=2*pi/(rows*columns);
    check(integral>0 && integral<=1.001,"White-furnace energy at the tested normal view");
    for(auto mode : {SamplingMode::bsdf,SamplingMode::nee,SamplingMode::mis}) {
        Sampler rng{41}; double mean=0;
        constexpr int count=65536;
        for(int i=0;i<count;++i) mean+=integrate(scene,ray,rng,1,mode).x/count;
        std::cout << "mode=" << static_cast<int>(mode) << " mean=" << mean << " quadrature=" << integral << '\n';
        check(std::abs(mean-integral)<0.025,"BSDF/NEE/MIS agree with independent hemisphere integration");
    }
    Film one(12,8),many(12,8); RenderSettings settings;
    settings.spp=8; settings.mode=SamplingMode::mis;
    render(scene,one,settings);settings.threads=4;render(scene,many,settings);
    for(std::size_t i=0;i<one.pixels.size();++i)
        check(close(one.pixels[i],many.pixels[i],1e-15),"Coated render thread invariance");
}
}
int main() {
    try { parameters(); model(); transport(); }
    catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
