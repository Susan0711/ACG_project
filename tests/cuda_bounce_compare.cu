// Replay a frozen camera batch through one production C3 wave and C2's host
// integrator. An optional scene argument writes per-path measurements as CSV.
#include "render/device_storage.cuh"
#include "render/wavefront_stages.cuh"
#include "render/direct_lighting.h"
#include "scene_init.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>

namespace {
template<class T> std::vector<T> download(const wf::Buffer<T>& b,int n) {
    std::vector<T> v(n);
    wf::check(cudaMemcpy(v.data(),b.ptr,n*sizeof(T),cudaMemcpyDeviceToHost),"download probe");
    return v;
}
Vec3 unpack(wf::V v) { return {v.x,v.y,v.z}; }
wf::V pack(Vec3 v) { return {float(v.x),float(v.y),float(v.z)}; }
double difference(Vec3 a,Vec3 b) {
    return std::max({std::abs(a.x-b.x),std::abs(a.y-b.y),std::abs(a.z-b.z)});
}
void compare(const char* file,const char* csv) {
    RenderScene scene(load_scene_file(file),Acceleration::bvh,BVHSplit::sah);
    CudaBVH gpu(scene.bvh(),scene.geometry(),scene.materials());
    CudaWavefront renderer(scene,gpu);
    auto s=renderer.device_scene();
    s.spectral=false; // Replay the green band and observe the path before slot reuse.
    const int w=48,h=32,n=w*h,blocks=(n+255)/256;
    const wf::Frame frame{w,h,1,16,1,SamplingMode::mis,s.mipmaps?2*s.camera.scale/h:0};
    std::vector<wf::Path> input(n);
    std::vector<RayQuery> queries(n);
    std::vector<fp32::Ray> packed(n);
    std::vector<GeometryHit> c2_hits(n);
    std::vector<int> indices(n);
    std::iota(indices.begin(),indices.end(),0);
    Film film(w,h);
    const Vec3 origin=gpu.coordinate_origin();
    for(int i=0;i<n;++i) {
        auto rng=pixel_sampler(1,i,0);
        const auto r=camera_sample(scene,film,i%w,i/w,frame.spread,rng);
        packed[i]={pack(r.origin-origin),pack(r.direction),float(r.time),0,INFINITY};
        auto& p=input[i];
        p.ray=packed[i]; p.rng.state=rng.state; p.sample=i; p.band=1;
        queries[i]={unpack(p.ray.origin)+origin,unpack(p.ray.direction),p.ray.time,0,INFINITY};
    }
    gpu.intersect(queries.data(),c2_hits.data(),n);
    wf::Buffer<fp32::Ray> rays,shadow(n*std::max(1,s.light_count()));
    wf::Buffer<wf::Path> paths;
    wf::Buffer<fp32::Hit> hits(n);
    wf::Buffer<wf::Vertex> vertices(n);
    wf::Buffer<wf::Counts> counts(1);
    wf::Buffer<wf::ShadowTerm> terms(n*std::max(1,s.light_count()));
    wf::Buffer<float> visibility(n*std::max(1,s.light_count()));
    wf::Buffer<int> ids,next(n),free_ids(CudaBVH::capacity);
    wf::Buffer<wf::V> samples(n);
    rays.upload(packed); paths.upload(input); ids.upload(indices);
    wf::check(cudaMemset(counts.ptr,0,sizeof(wf::Counts)),"clear probe counts");
    gpu.intersect_device(rays.ptr,hits.ptr,n);
    wf::classify<<<blocks,256>>>(s,frame,ids.ptr,paths.ptr,hits.ptr,vertices.ptr,n,counts.ptr);
    if(s.light_count()) {
        wf::light_samples<<<blocks,256>>>(s,frame,ids.ptr,paths.ptr,vertices.ptr,0,s.light_count(),n,shadow.ptr,terms.ptr);
        gpu.shadow_device(shadow.ptr,visibility.ptr,s.light_count()*n);
        wf::resolve_shadows<<<blocks,256>>>(vertices.ptr,terms.ptr,visibility.ptr,s.light_count(),n,counts.ptr);
    }
    wf::finish_paths<<<blocks,256>>>(s,frame,0,paths.ptr,vertices.ptr,ids.ptr,n,next.ptr,free_ids.ptr,counts.ptr,samples.ptr);
    const auto count=download(counts,1)[0];
    if(count.error || count.live+count.dead!=n) throw std::runtime_error("Invalid probe counts");
    const auto live_ids=download(next,count.live);
    std::vector<bool> alive(n);
    for(int id:live_ids) alive[id]=true;
    const auto out=download(paths,n);
    const auto geom=download(hits,n);
    const auto event=download(vertices,n);
    const auto light_rays=download(shadow,s.light_count()*n);
    const auto trans=download(visibility,s.light_count()*n);
    std::ofstream log;
    if(csv) {
        log.open(csv); log.exceptions(std::ios::failbit|std::ios::badbit);
        log<<"path,object,uv_error,visibility_error,shadow_direction_error,next_direction_error,radiance_error,rng_match,alive_match\n"<<std::setprecision(12);
    }
    int state_mismatches=0;
    double worst_color=0,worst_dir=0,worst_vis=0;
    // Batch all reference shadow queries so the diagnostic itself is inexpensive.
    std::vector<RayQuery> shadows;
    std::vector<int> shadow_start(n+1);
    std::vector<std::vector<DirectLightSample>> pending(n);
    std::vector<PathState> reference(n);
    std::vector<Vec3> weights(n);
    std::vector<bool> c2_alive(n),same_rng(n);
    std::vector<double> uv_error(n);
    for(int i=0;i<n;++i) {
        const auto q=queries[i];
        auto& p=reference[i]; p.ray={q.origin,q.direction,q.time,{0,frame.spread}};
        const auto g=c2_hits[i];
        if(g.object!=geom[i].object || (g.object>=0 && g.parameters.t!=geom[i].t))
            throw std::runtime_error("Frozen closest hit changed");
        HitRecord hit;
        if(g.object>=0) {
            const auto& shape=scene.geometry().objects[g.object];
            std::visit([&](const auto& primitive){ primitive.finish_hit(p.ray,g.parameters,hit); },shape);
            hit.object_index=g.object;
            if(const auto* t=std::get_if<Triangle>(&shape))
                hit.position=t->a+(t->b-t->a)*g.parameters.u+(t->c-t->a)*g.parameters.v+t->velocity*q.time;
            else {
                const auto& sphere=std::get<Sphere>(shape);
                hit.position=sphere.center_at(q.time)+hit.outward_normal*sphere.radius;
            }
            scene.apply_surface_maps(p.ray,hit);
            if(event[i].kind==wf::EventKind::surface)
                uv_error[i]=std::max(std::abs(event[i].hit.uv.x-hit.uv.u),std::abs(event[i].hit.uv.y-hit.uv.v));
        }
        Sampler rng{input[i].rng.state};
        c2_alive[i]=advance_path(scene,p,rng,16,SamplingMode::mis,Wavelength::green,g.object>=0,hit,&pending[i],weights[i]);
        same_rng[i]=rng.state==out[i].rng.state;
        shadow_start[i]=int(shadows.size());
        for(const auto& l:pending[i]) {
            const auto& seg=l.segment;
            shadows.push_back({seg.ray.origin,seg.ray.direction,seg.ray.time,0,seg.distance-1e-5});
        }
    }
    shadow_start[n]=int(shadows.size());
    std::vector<float> c2_trans(shadows.size());
    for(int i=0;i<int(shadows.size());i+=CudaBVH::capacity)
        gpu.intersect_batch(shadows.data()+i,nullptr,0,c2_trans.data()+i,
                            std::min(CudaBVH::capacity,int(shadows.size())-i));
    for(int i=0;i<n;++i) {
        Vec3 direct{};
        double vis_error=0,shadow_dir_error=0;
        int matched=0, emitted=0;
        for(int j=0;j<s.light_count();++j) {
            const int k=j*n+i;
            if(light_rays[k].maximum<light_rays[k].minimum) continue;
            ++emitted;
            if(matched>=int(pending[i].size())) continue;
            const auto& ref=pending[i][matched].segment.ray;
            shadow_dir_error=std::max(shadow_dir_error,difference(ref.direction,unpack(light_rays[k].direction)));
            vis_error=std::max(vis_error,std::abs(double(c2_trans[shadow_start[i]+matched])-trans[k]));
            ++matched;
        }
        for(size_t j=0;j<pending[i].size();++j) {
            const auto& l=pending[i][j];
            const double t=c2_trans[shadow_start[i]+j]*scene.medium().transmittance(l.segment.ray,l.segment.distance);
            direct=direct+l.contribution(t);
        }
        const double color=difference(reference[i].result+multiply(weights[i],direct),unpack(out[i].result));
        const double dir=alive[i] && c2_alive[i]?difference(reference[i].ray.direction,unpack(out[i].ray.direction)):0;
        worst_color=std::max(worst_color,color); worst_dir=std::max(worst_dir,dir); worst_vis=std::max(worst_vis,vis_error);
        state_mismatches+=(!same_rng[i] || alive[i]!=c2_alive[i] || emitted!=int(pending[i].size()));
        if(csv) log<<i<<','<<geom[i].object<<','<<uv_error[i]<<','<<vis_error<<','<<shadow_dir_error<<','<<dir<<','<<color<<','<<same_rng[i]<<','<<(alive[i]==c2_alive[i])<<'\n';
    }
    std::cout<<"paths="<<n<<" shadow_queries="<<shadows.size()<<" state_mismatches="<<state_mismatches
             <<" max_next_dir_error="<<worst_dir<<" max_visibility_error="<<worst_vis<<" max_radiance_error="<<worst_color<<'\n';
    if(!csv && (state_mismatches || worst_color>1e-4 || worst_dir>1e-4 || worst_vis>1e-5))
        throw std::runtime_error("Single-bounce replay differs");
}
}
int main(int argc,char** argv) {
    try { compare(argc>1?argv[1]:"scene/volume_alpha_shadow.json",argc>2?argv[2]:nullptr); }
    catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
