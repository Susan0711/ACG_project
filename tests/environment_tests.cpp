#include "render.h"
#include <iostream>
#include <stdexcept>
void check(bool condition,const char* label) { if(!condition) throw std::runtime_error(label); }
int main() {
    try {
        Environment env;env.width=8;env.height=4;env.pixels.assign(32,{2,2,2});env.prepare();
        Sampler rng{81};double average_y=0,sum=0;
        for(double p:env.probabilities) sum+=p;
        check(std::abs(sum-1)<1e-12,"environment normalization");
        for(int i=0;i<50000;++i) {
            double a=rng.next(),b=rng.next(),c=rng.next();auto s=env.sample(a,b,c);
            check(std::abs(s.pdf-1/(4*pi))<1e-12,"constant environment solid angle PDF");
            check(std::abs(s.pdf-env.pdf(s.direction))<1e-12,"sample/query PDF consistency");
            average_y+=s.direction.y;
        }
        check(std::abs(average_y/50000)<.01,"uniform sphere moment");
        env.pixels.assign(32,{});env.prepare();check(std::isfinite(env.pdf({0,1,0})),"black pole fallback");
        env.pixels[8]={20,0,0};env.prepare();
        check(env.evaluate({1,.5,.01}).x==20,"latlong +X mapping");
        env.rotation=pi/2;check(env.evaluate({-.01,.5,1}).x==20,"environment rotation");
        check(std::isfinite(env.pdf({0,-1,0})),"south pole PDF");
        Environment seam;seam.width=4;seam.height=2;
        for(int i=0;i<8;++i) seam.pixels.push_back({double(i+1),0,0});
        seam.prepare();
        check(seam.evaluate({1,.5,1e-6}).x==1 && seam.evaluate({1,.5,-1e-6}).x==4,"azimuth seam wraps");
        double original_pdf=seam.pdf({1,.5,1e-6});seam.intensity=2;
        check(seam.evaluate({1,.5,1e-6}).x==2 && seam.pdf({1,.5,1e-6})==original_pdf,"intensity scales radiance not normalized PDF");
        Environment varied;varied.width=8;varied.height=4;
        for(int i=0;i<32;++i) varied.pixels.push_back({double(i+1),double(i+1),double(i+1)});
        varied.prepare();std::vector<int> counts(32);
        for(int i=0;i<50000;++i) {
            double a=rng.next(),b=rng.next(),c=rng.next();auto sample=varied.sample(a,b,c);
            double phi=std::atan2(sample.direction.z,sample.direction.x)/(2*pi);phi-=std::floor(phi);
            int x=std::min(7,static_cast<int>(phi*8));
            int y=std::min(3,static_cast<int>(std::acos(sample.direction.y)/pi*4));
            ++counts[y*8+x];
            check(std::abs(sample.pdf-varied.pdf(sample.direction))<1e-12,"nonuniform sample/query PDF");
            check(sample.radiance.x==varied.evaluate(sample.direction).x,"sample and escape share same HDR map");
        }
        for(int i=0;i<32;++i) check(std::abs(counts[i]/50000.0-varied.probabilities[i])<.006,"luminance solid-angle histogram");
        check(power_heuristic(2,3)+power_heuristic(3,2)==1,"MIS complementary weights");
        check(power_heuristic(1,0)==1 && power_heuristic(0,1)==0,"zero competing PDF");
        World w;w.hdr=std::make_shared<Environment>();w.hdr->width=8;w.hdr->height=4;
        w.hdr->pixels.assign(32,{2,2,2});w.hdr->prepare();
        w.materials={{Kind::mirror,{1,1,1},1.5}};w.scene.objects.push_back(Sphere{{0,0,0},1,0});w.bvh.build(w.scene);
        for(auto mode:{SamplingMode::bsdf,SamplingMode::nee,SamplingMode::mis}) {
            Sampler r{1};auto value=integrate(w,{{0,0,3},{0,0,-1}},r,4,true,true,mode);
            check(value.x==2,"delta mirror environment full weight");
            value=integrate(w,{{0,0,3},{0,1,0}},r,4,true,true,mode);
            check(value.x==2,"primary environment full weight");
        }
        w.materials[0].kind=Kind::glass;
        Sampler glass_rng{4};
        auto glass_mis=integrate(w,{{0,0,3},{0,0,-1}},glass_rng,16,true,true,SamplingMode::mis);
        Sampler glass_bsdf_rng{4};
        auto glass_bsdf=integrate(w,{{0,0,3},{0,0,-1}},glass_bsdf_rng,16,true,true,SamplingMode::bsdf);
        check(finite(glass_mis) && glass_mis.x>0 && glass_mis.x==glass_bsdf.x,"delta glass keeps full environment weight");
        World plane;
        plane.hdr=w.hdr;
        plane.materials={{Kind::diffuse,{.6,.6,.6},1.5}};
        plane.scene.objects.push_back(Triangle{{-100,-100,0},{100,-100,0},{0,100,0},0});
        plane.bvh.build(plane.scene);
        for(auto mode:{SamplingMode::bsdf,SamplingMode::nee,SamplingMode::mis}) {
            Sampler r{75};double total=0;
            for(int i=0;i<30000;++i) total+=integrate(plane,{{0,0,1},{0,0,-1}},r,1,true,true,mode).x;
            check(std::abs(total/30000-1.2)<.025,"constant HDR diffuse energy across modes");
        }
        World points=plane;
        points.hdr.reset();
        points.points={{{0,0,2},{4,4,4}},{{0,0,2},{4,4,4}}};
        Sampler point_rng{9};
        auto point_value=integrate(points,{{0,0,1},{0,0,-1}},point_rng,1,true,true,SamplingMode::mis);
        check(std::abs(point_value.x-1.2/pi)<1e-12,"two delta lights enumerated with probability one");
        auto point_bsdf=integrate(points,{{0,0,1},{0,0,-1}},point_rng,1,true,true,SamplingMode::bsdf);
        check(point_bsdf.x==0,"pure BSDF cannot hit point light");
        World unregistered;
        unregistered.materials={{Kind::diffuse,{1,1,1},1.5},{Kind::emissive,{2,2,2},1.5}};
        unregistered.scene.objects.push_back(Sphere{{0,0,0},1,0});
        unregistered.scene.objects.push_back(Triangle{{-100,-100,3},{0,100,3},{100,-100,3},1});
        unregistered.bvh.build(unregistered.scene);
        Sampler nr{1},mr{1};
        double ns=0,ms=0;
        for(int i=0;i<1000;++i) {
            ns+=integrate(unregistered,{{0,0,2},{0,0,-1}},nr,3,true,true,SamplingMode::nee).x;
            ms+=integrate(unregistered,{{0,0,2},{0,0,-1}},mr,3,true,true,SamplingMode::mis).x;
        }
        check(ns==ms && ms>1000,"unregistered emitter keeps MIS hit contribution");
        w=make_sampling_scene();w.hdr=std::make_shared<Environment>();w.hdr->load("assets/studio-hdr.pfm");
        double peak=0;for(Vec3 p:w.hdr->pixels) peak=std::max(peak,p.x);
        check(peak>10,"HDR values retained above display range");
        w.bvh.build(w.scene);
        Film a(20,12),b(20,12);RenderSettings settings{8,8,1,9,true,true,SamplingMode::mis};
        render(w,a,settings);settings.threads=4;render(w,b,settings);
        for(std::size_t i=0;i<a.pixels.size();++i) check(a.pixels[i].x==b.pixels[i].x&&a.pixels[i].y==b.pixels[i].y&&a.pixels[i].z==b.pixels[i].z,"HDR MIS thread invariance");
        std::cout<<"PASS HDR mapping, distribution, black poles, MIS and delta/thread checks\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
