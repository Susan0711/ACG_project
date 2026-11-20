#include "render/path_integrator.h"
#include <iostream>
#include <stdexcept>
void check(bool ok,const char* name) {
    if(!ok) throw std::runtime_error(name);
}
World layers(double alpha,int count,bool opaque=false) {
    World w;
    Material m; m.opacity=alpha;
    w.materials={m,Material{}};
    for(int i=0;i<count;++i)
        w.scene.objects.push_back(Triangle{{-10,-10,-double(i)},{10,-10,-double(i)},{0,10,-double(i)},0});
    if(opaque) w.scene.objects.push_back(Triangle{{-10,-10,-5},{10,-10,-5},{0,10,-5},1});
    return w;
}
int main() {
    try {
        Camera c;
        Sampler rng{12}; auto before=rng.state;
        Ray a=c.generate(.3,.6,1),b=c.sample(.3,.6,1,rng);
        check(a.direction.x==b.direction.x && a.origin.x==b.origin.x && before==rng.state,"zero lens consumes no RNG");
        c.lens_radius=.3; c.focus_distance=5;
        Vec3 forward=(c.target-c.origin).normalized();
        Vec3 expected=a.origin+a.direction*(5/a.direction.dot(forward));
        double radius2=0;
        for(int i=0;i<10000;++i) {
            b=c.sample(.3,.6,1,rng);
            Vec3 p=b.origin+b.direction*((5-(b.origin-c.origin).dot(forward))/b.direction.dot(forward));
            check((p-expected).dot(p-expected)<1e-20,"focal plane convergence");
            Vec3 delta=b.origin-c.origin; radius2+=delta.dot(delta);
        }
        check(std::abs(radius2/10000-.045)<.002,"uniform lens disk radial moment");
        for(int which=0;which<3;++which) {
            Camera bad; if(which==0)bad.fov=180; if(which==1)bad.focus_distance=0; if(which==2)bad.target=bad.origin+Vec3{0,1,0};
            bool rejected=false; try {bad.validate();} catch(...) {rejected=true;}
            check(rejected,"invalid camera rejected");
        }
        Scene scene{{Sphere{{0,0,0},1,0,{2,0,0}}}};
        BVH bvh; bvh.build(scene,-1,2);
        for(int i=0;i<=100;++i) {
            double time=-1+i*.03;
            Ray ray{{2*time,0,3},{0,0,-1},time}; HitRecord h,k;
            check(scene.intersect(ray,0,10,h) && bvh.intersect(scene,ray,0,10,k) && h.t==k.t && h.t==2,"motion BVH interval");
            check(spawn(h,{0,0,1}).time==time,"spawn time propagation");
        }
        c.shutter_open=2;c.shutter_close=3;
        for(int i=0;i<100;++i) {b=c.sample(.5,.5,1,rng);check(b.time>=2 && b.time<3,"shutter sampling");}
        Ray shadow{{0,0,2},{0,0,-1},.73};
        for (auto backend : {Acceleration::linear, Acceleration::bvh}) {
            check(RenderScene(layers(0,2), backend).shadow_transmittance(shadow,10)==1,"zero alpha");
            check(RenderScene(layers(1,1), backend).shadow_transmittance(shadow,10)==0,"opaque alpha");
            check(RenderScene(layers(.5,1), backend).shadow_transmittance(shadow,10)==.5,"half alpha");
            check(RenderScene(layers(.5,2), backend).shadow_transmittance(shadow,10)==.25,"two half layers");
            check(RenderScene(layers(.5,2,true), backend).shadow_transmittance(shadow,10)==0,"opaque blocker after layers");
        }
        World w=layers(.5,2); w.lights.background={1,1,1};w.materials[0].color={0,0,0};
        RenderScene sheets(w);
        double sum=0;
        for(int i=0;i<20000;++i) sum+=integrate(sheets,shadow,rng,1,SamplingMode::bsdf).x;
        check(std::abs(sum/20000-.25)<.015,"stochastic alpha not double weighted, no depth consumption");
        World timed=layers(0,1);
        timed.materials[1].kind=Kind::emissive;
        timed.materials[1].color={3,3,3};
        timed.scene.objects.push_back(Sphere{{-2,0,-2},.5,1,{2,0,0}});
        timed.camera.shutter_close=1;
        RenderScene moving_emitter(timed);
        Sampler timed_rng{42};
        Vec3 emitted=integrate(moving_emitter,{{0,0,2},{0,0,-1},1},timed_rng,1);
        check(emitted.x==3,"transparent crossing retains time and emission eligibility");
        timed.materials[1].kind=Kind::diffuse;
        RenderScene blocker(timed);
        check(blocker.shadow_transmittance({{0,0,2},{0,0,-1},1},8)==0,"shadow inherits moving blocker time");
        check(blocker.shadow_transmittance({{0,0,2},{0,0,-1},0},8)==1,"moving blocker absent at other time");
        Material forbidden; forbidden.kind=Kind::emissive; forbidden.opacity=.5;
        bool rejected=false; try {forbidden.alpha({});} catch(...) {rejected=true;}
        check(rejected,"emitter alpha rejected");
        Texture data;data.load("assets/test_colors.ppm",false);
        check(std::abs(data.value({.25,.25}).x-128.0/255)<1e-12,"opacity linear data");
        std::cout<<"PASS camera, disk, motion BVH, time, alpha boundaries/layers/statistics\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
