#include "render_scene.h"
#include "scene_init.h"
#include <iostream>
#include <stdexcept>

namespace {
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
bool close(double a, double b) { return std::abs(a-b)<1e-10; }
Medium height_fog(double bottom, double top) {
    Medium m{{{-1,0,-1},{1,4,1}},bottom,0};
    m.profile=DensityProfile::height_linear; m.density_top=top;
    return m;
}

void analytic() {
    const Medium m=height_fog(0.6,0.1);
    check(close(m.density_at({0,0,0}),0.6) && close(m.density_at({0,4,0}),0.1)
          && close(m.density_at({0,2,0}),0.35) && m.density_at({2,2,0})==0,"Height density field");
    const Ray up{{0,-1,0},{0,2,0}}, down{{0,5,0},{0,-1,0}};
    check(close(m.optical_depth(up,3),1.4) && close(m.optical_depth(down,6),1.4),
          "Upward and downward full-column optical depths");
    check(close(m.optical_depth(up,1.5),0.95),"Clip a partial column to a finite endpoint");
    check(close(m.optical_depth({{0,2,0},{0,0,3}},1),0.35),"Horizontal ray uses local density and world distance");
    const Ray diagonal{{-1,0,0},{0.5,1,0}};
    check(close(m.optical_depth(diagonal,4),1.4*std::sqrt(1.25)),"Oblique column length");
    check(m.transmittance({{3,0,0},{0,1,0}},4)==1,"Missed box remains vacuum");

    Medium uniform=m; uniform.profile=DensityProfile::homogeneous; uniform.density=0.3;
    Medium equal=height_fog(0.3,0.3);
    Sampler a{7},b{7};
    for(int i=0;i<2048;++i) {
        const auto x=uniform.sample_segment(up,3,a),y=equal.sample_segment(up,3,b);
        check(bool(x.collision)==bool(y.collision),"Equal endpoints reproduce uniform collision decisions");
        if(x.collision) check(x.collision->y==y.collision->y,"Equal endpoints preserve uniform samples");
    }
    check(a.state==b.state && uniform.transmittance(up,3)==equal.transmittance(up,3),"Uniform degeneration");
}

void distribution() {
    for(const auto values : {UV{0.6,0.1},UV{0,0.6},UV{0.6,0},UV{0.6,0.600000000001}}) {
        const Medium m=height_fog(values.u,values.v);
        for(bool upward : {true,false}) {
            const Ray ray=upward ? Ray{{0,0,0},{0,2,0}} : Ray{{0,4,0},{0,-2,0}};
            Sampler rng{987}; int first_half=0, collided=0;
            constexpr int count=32768;
            for(int i=0;i<count;++i) {
                Sampler reference=rng;
                const double tau=-std::log1p(-reference.next());
                const auto s=m.sample_segment(ray,2,rng);
                check(bool(s.collision)==(tau<m.optical_depth(ray,2)),"Collision probability is based on optical depth");
                if(s.collision) {
                    const double t=std::abs(s.collision->y-ray.origin.y)/2;
                    check(t>=0 && t<=2 && close(m.optical_depth(ray,t),tau),"Inverse optical-depth sample");
                    ++collided; if(t<=1) ++first_half;
                }
            }
            const double expected=-std::expm1(-m.optical_depth(ray,2));
            check(std::abs(double(collided)/count-expected)<0.012
                  && std::abs(double(first_half)/count+std::expm1(-m.optical_depth(ray,1)))<0.012,
                  "Collision CDF matches analytic attenuation");
        }
    }
    Medium vacuum=height_fog(0,0);Sampler rng{3};const auto before=rng.state;
    check(!vacuum.sample_segment({{0,0,0},{0,1,0}},4,rng).collision && rng.state==before,"Vacuum preserves RNG");
}

void transport() {
    World w;w.medium=height_fog(0.6,0.1);w.lights.background={1,1,1};
    const Ray ray{{0,-1,0},{0,2,0}};
    RenderScene scene(w);Sampler rng{42};double mean=0;
    constexpr int count=32768;
    for(int i=0;i<count;++i) mean+=integrate(scene,ray,rng,0).x/count;
    check(std::abs(mean-std::exp(-1.4))<0.012,"Absorbing camera paths match analytic transmission");
    check(close(scene.shadow_transmittance(ray,3),std::exp(-1.4)),"Shadows query the same height medium");
    w.materials.emplace_back().opacity=0.5;
    w.scene.objects.push_back(Triangle{{-1,2,-1},{0,2,1},{1,2,-1},0});
    check(close(RenderScene(w).shadow_transmittance(ray,3),0.5*std::exp(-1.4)),"Surface alpha combines with height fog");
    w.scene.objects.clear();w.lights.background={};w.medium.emission={0.8,0.2,0.1};
    const RenderScene emitter(w);
    for(int i=0;i<64;++i) check(close(integrate(emitter,ray,rng,1).x,0.8*(-std::expm1(-1.4))),
                               "Emission integrates optical depth, including collision paths");
    w.medium.emission={};w.medium.albedo=0.9;w.lights.points.push_back({{0,3,0.5},{2,2,2}});
    const RenderScene scatter(w);
    for(auto mode : {SamplingMode::nee,SamplingMode::mis}) {
        double total=0;for(int i=0;i<256;++i) total+=integrate(scatter,ray,rng,1,mode).x;
        check(std::isfinite(total) && total>0,"Height fog scatters point light");
    }
    World demo=load_scene_file("scene/height_fog.json");
    check(demo.medium.profile==DensityProfile::height_linear && demo.medium.density_top==0.02,"Load height profile JSON");
    const RenderScene display(std::move(demo));Film one(24,16),many(24,16);RenderSettings settings;
    settings.spp=8;settings.mode=SamplingMode::mis;
    render(display,one,settings);settings.threads=4;render(display,many,settings);
    for(std::size_t i=0;i<one.pixels.size();++i)
        check(one.pixels[i].x==many.pixels[i].x && one.pixels[i].y==many.pixels[i].y
              && one.pixels[i].z==many.pixels[i].z,"Height fog thread invariance");
    std::cout<<"height fog: tau=1.4, expected T="<<std::exp(-1.4)<<", sampled T="<<mean<<'\n';
}
}
int main() {
    try {analytic();distribution();transport();}
    catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
