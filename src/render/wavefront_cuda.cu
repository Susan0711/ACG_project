#include "render/device_storage.cuh"
#include "render/wavefront_stages.cuh"
#include "render/shading_fp32.cuh"
#include <algorithm>
#include <limits>

namespace wf {
constexpr int capacity=CudaBVH::capacity, window_size=8*capacity, light_group=4;
__device__ SampleAddress address(const Frame& f,uint64_t ordinal) {
    const uint64_t row_stride=uint64_t(f.width)*16*f.spp;
    const int y0=int(ordinal/row_stride)*16;
    ordinal%=row_stride;
    const int h=min(16,f.height-y0);
    const uint64_t tile_stride=uint64_t(h)*16*f.spp;
    const int x0=int(ordinal/tile_stride)*16;
    ordinal%=tile_stride;
    const int w=min(16,f.width-x0),pixel=int(ordinal%(w*h));
    return {x0+pixel%w,y0+pixel/w,int(ordinal/(w*h))};
}
__device__ Path camera_path(const SceneView& s,Frame f,uint64_t base,int sample,int band) {
    Path p{};
    p.sample=sample; p.band=band;
    const auto a=address(f,base+sample);
    const uint64_t pixel=uint64_t(a.y)*f.width+a.x;
    p.rng.state=Random::mix(f.seed ^ Random::mix(pixel+0x9e3779b97f4a7c15ULL) ^ Random::mix(uint64_t(a.sample)+0xd1b54a32d192ed03ULL));
    const auto c=s.camera;
    const float u=(a.x+p.rng.next())/f.width,v=(a.y+p.rng.next())/f.height;
    V o=c.origin;
    V d=(c.forward+c.right*((2*u-1)*(float(f.width)/f.height)*c.scale)+c.up*((1-2*v)*c.scale)).normalized();
    if(c.lens>0) {
        const V focal=d*(c.focus/d.dot(c.forward));
        const float r=c.lens*sqrtf(p.rng.next()),angle=2*pi*p.rng.next();
        const V offset=c.right*(r*cosf(angle))+c.up*(r*sinf(angle));
        o=o+offset; d=(focal-offset).normalized();
    }
    float time=c.open;
    if(c.close>c.open) time+=(c.close-c.open)*p.rng.next();
    p.ray={o,d,time,0,CUDART_INF_F};
    return p;
}
__global__ void initialize_free(int* free_ids) {
    const int i=blockIdx.x*blockDim.x+threadIdx.x;
    if(i<capacity) free_ids[i]=i;
}
__global__ void refill(SceneView scene,Frame frame,uint64_t base,int sample,int live,int add,
                       const int* free_ids,int* ids,Path* paths) {
    const int i=blockIdx.x*blockDim.x+threadIdx.x;
    if(i>=add) return;
    const int slot=free_ids[capacity-live-i-1];
    ids[live+i]=slot;
    paths[slot]=camera_path(scene,frame,base,sample+i,0);
}
__global__ void gather(const int* ids,const Path* paths,fp32::Ray* rays,int n) {
    const int i=blockIdx.x*blockDim.x+threadIdx.x;
    if(i<n) rays[i]=paths[ids[i]].ray;
}
__global__ void classify(SceneView s,Frame f,const int* ids,Path* paths,const fp32::Hit* hits,
                         Vertex* vertices,int n,Counts* counts) {
    const int i=blockIdx.x*blockDim.x+threadIdx.x;
    if(i>=n) return;
    auto& p=paths[ids[i]];
    auto& v=vertices[i];
    v.kind=EventKind::ended; v.direct={};
    const auto hit=hits[i];
    const auto query=p.ray;
    auto local=query;
    local.origin=fp32::translate(query.origin,query.direction,p.advanced);
    const float distance=hit.object<0 ? CUDART_INF_F : fmaxf(0,hit.t-p.advanced);
    const auto fog=fog_segment(s.fog,local,distance);
    if(fog.tau>0) p.result=p.result+multiply(p.throughput,s.fog.emission)*(-expm1f(-fog.tau));
    const float collision=collision_distance(fog,p.rng);
    const float speed=sqrtf(query.direction.dot(query.direction));
    if(isfinite(collision)) {
        if(p.bounce==f.depth || s.fog.albedo==0) return;
        p.cone+=f.spread*collision*speed;
        p.throughput=p.throughput*s.fog.albedo;
        v.hit.position=fp32::translate(local.origin,local.direction,collision);
        v.kind=EventKind::medium;
        return;
    }
    if(hit.object<0) {
        const auto env=background(s,query.direction);
        const float w=emission_weight(f.mode,p.previous_delta,s.env_width>0,p.previous_pdf,env.pdf);
        p.result=p.result+multiply(p.throughput,env.radiance)*w;
        return;
    }
    p.cone+=f.spread*distance*speed;
    const float alpha=fp32::surface_alpha(s.geo.shadow,s.geo.spheres,query,hit);
    if(!(alpha>=0 && alpha<=1)) { atomicExch(&counts->error,1); return; }
    if(alpha<1 && (alpha==0 || p.rng.next()>=alpha)) {
        if(++p.crossings>4096) { atomicExch(&counts->error,2); return; }
        p.advanced=hit.t+1e-7f;
        p.ray.minimum=fmaxf(p.advanced,nextafterf(hit.t,CUDART_INF_F));
        p.cone+=f.spread*1e-7f*speed;
        v.kind=EventKind::transparent;
        return;
    }
    const auto& o=s.objects[hit.object];
    v.hit=finish_surface(s,query,hit,p.cone);
    v.material=resolve_material(s,o.material,v.hit);
    if(v.material.kind==Kind::emissive) {
        if(v.hit.front_face) {
            const float pdf=o.light<0?0:area_pdf(s.areas[o.light],p.previous_position,v.hit.position);
            const float w=emission_weight(f.mode,p.previous_delta,o.light>=0,p.previous_pdf,pdf);
            p.result=p.result+multiply(p.throughput,v.material.color)*w;
        }
        return;
    }
    if(p.bounce<f.depth) v.kind=EventKind::surface;
}
// A path owns its RNG. Generate its light samples in source order, even when a
// scene needs more lights than fit in one scratch buffer.
__global__ void light_samples(SceneView s,Frame f,const int* ids,Path* paths,const Vertex* vertices,
                              int first,int lights,int n,fp32::Ray* rays,ShadowTerm* terms) {
    const int i=blockIdx.x*blockDim.x+threadIdx.x;
    if(i>=n) return;
    const auto& v=vertices[i];
    auto& p=paths[ids[i]];
    for(int j=0;j<lights;++j) {
        const int k=j*n+i;
        rays[k]={{},{},0,1,0};
        terms[k]={};
        const bool volume=v.kind==EventKind::medium;
        if(!volume && (v.kind!=EventKind::surface || !v.material.has_continuous_bsdf())) continue;
        const auto l=sample_light(s,first+j,v.hit.position,p.rng);
        if(l.pdf<=0) continue;
        Evaluation eval{{1,1,1},1/(4*pi)};
        float cosine=eval.pdf;
        if(!volume) {
            eval=evaluate_bsdf(v.material,p.ray,v.hit,l.direction);
            cosine=fabsf(v.hit.normal.dot(l.direction));
            if(cosine==0 || (eval.value.x==0 && eval.value.y==0 && eval.value.z==0)) continue;
        }
        const float w=f.mode==SamplingMode::mis && !l.delta ? mis_weight(l.pdf,eval.pdf):1;
        float distance=l.distance;
        V d=l.direction,o=v.hit.position;
        if(!volume) {
            o=spawn_origin(o,v.hit.outward_normal,d);
            if(isfinite(distance)) {
                const V delta=d*distance+(v.hit.position-o);
                distance=sqrtf(delta.dot(delta));
                if(distance<=1e-5f) continue;
                d=delta*(1/distance);
            }
        }
        const float limit=isfinite(distance)?fminf(distance-1e-5f,nextafterf(distance,-CUDART_INF_F)):distance;
        rays[k]={o,d,p.ray.time,0,limit};
        const float medium=expf(-fog_segment(s.fog,rays[k],distance).tau);
        terms[k].weighted=multiply(eval.value,l.radiance)*(cosine*w*medium/l.pdf);
    }
}
__global__ void resolve_shadows(Vertex* vertices,const ShadowTerm* terms,const float* transmission,
                                int lights,int n,Counts* counts) {
    const int i=blockIdx.x*blockDim.x+threadIdx.x;
    if(i>=n) return;
    V sum=vertices[i].direct;
    for(int j=0;j<lights;++j) {
        const int k=j*n+i;
        if(!(transmission[k]>=0 && transmission[k]<=1)) { atomicExch(&counts->error,3); return; }
        sum=sum+terms[k].weighted*transmission[k];
    }
    vertices[i].direct=sum;
}
__device__ bool advance(Path& p,const Vertex& v) {
    if(v.kind==EventKind::ended) return false;
    if(v.kind==EventKind::transparent) return true;
    p.result=p.result+multiply(p.throughput,v.direct);
    V direction,origin=v.hit.position;
    if(v.kind==EventKind::medium) {
        const float y=1-2*p.rng.next(),phi=2*pi*p.rng.next(),r=sqrtf((1-y)*(1+y));
        direction={r*cosf(phi),y,r*sinf(phi)};
        p.previous_pdf=1/(4*pi); p.previous_delta=false;
    } else {
        const Wavelength bands[]={Wavelength::red,Wavelength::green,Wavelength::blue};
        const auto scatter=sample_bsdf(v.material,p.ray,v.hit,p.rng,bands[p.band]);
        if(scatter.pdf<=0) return false;
        direction=scatter.direction;
        origin=spawn_origin(origin,v.hit.outward_normal,direction);
        p.throughput=multiply(p.throughput,scatter.weight);
        p.previous_pdf=scatter.pdf; p.previous_delta=scatter.delta;
    }
    p.previous_position=v.hit.position; p.advanced=0; p.ray={origin,direction,p.ray.time,0,CUDART_INF_F};
    if(p.bounce>=3) {
        const float survive=clamp(fmaxf(p.throughput.x,fmaxf(p.throughput.y,p.throughput.z)),0.05f,0.95f);
        if(p.rng.next()>=survive) return false;
        p.throughput=p.throughput*(1/survive);
    }
    ++p.bounce; return true;
}
__global__ void finish_paths(SceneView s,Frame f,uint64_t base,Path* paths,const Vertex* vertices,
                             const int* ids,int n,int* next,int* free_ids,Counts* counts,V* samples) {
    const int i=blockIdx.x*blockDim.x+threadIdx.x;
    if(i>=n) return;
    const int id=ids[i];
    auto& p=paths[id];
    bool alive=advance(p,vertices[i]);
    if(!finite(p.result) || !finite(p.throughput) || !finite(p.ray.direction)) {
        atomicExch(&counts->error,4); return;
    }
    if(!alive) {
        auto& rgb=samples[p.sample];
        if(!s.spectral) rgb=p.result;
        else {
            if(p.band==0) rgb.x=p.result.x;
            if(p.band==1) rgb.y=p.result.y;
            if(p.band==2) rgb.z=p.result.z;
            if(p.band<2) { p=camera_path(s,f,base,p.sample,p.band+1); alive=true; }
        }
    }
    if(alive) next[atomicAdd(&counts->live,1)]=id;
    else free_ids[capacity-n+atomicAdd(&counts->dead,1)]=id;
}
__global__ void accumulate(Frame f,uint64_t base,int size,const V* samples,V* film) {
    const int pixel=blockIdx.x*blockDim.x+threadIdx.x;
    if(pixel>=f.width*f.height) return;
    const int x=pixel%f.width,y=pixel/f.width,x0=(x/16)*16,y0=(y/16)*16;
    const int w=min(16,f.width-x0),h=min(16,f.height-y0),area=w*h;
    const uint64_t first=(uint64_t(y0)*f.width+uint64_t(x0)*h)*f.spp+(y-y0)*w+x-x0;
    const uint64_t end=base+size;
    if(first>=end || first+uint64_t(f.spp-1)*area<base) return;
    uint64_t k=first;
    if(k<base) k+=((base-k+area-1)/area)*area;
    const uint64_t last=first+uint64_t(f.spp)*area;
    V color=film[pixel];
    for(;k<end && k<last;k+=area) color=color+samples[k-base];
    film[pixel]=color;
}
__global__ void normalize(V* film,int n,int spp) {
    const int i=blockIdx.x*blockDim.x+threadIdx.x;
    if(i<n) film[i]=film[i]*(1.f/spp);
}
struct Timer {
    cudaEvent_t begin{},end{};
    bool enabled;
    DeviceRenderStats& stats;
    Timer(bool p,DeviceRenderStats& s):enabled(p),stats(s) {
        if(enabled) { check(cudaEventCreate(&begin),"create start event"); check(cudaEventCreate(&end),"create end event"); }
    }
    ~Timer() { if(begin) cudaEventDestroy(begin); if(end) cudaEventDestroy(end); }
    void start() { if(enabled) check(cudaEventRecord(begin),"record stage start"); }
    void stop(int stage) {
        check(cudaGetLastError(),"launch wavefront stage");
        if(enabled) {
            check(cudaEventRecord(end),"record stage end"); check(cudaEventSynchronize(end),"wait for stage");
            float ms; check(cudaEventElapsedTime(&ms,begin,end),"measure stage"); stats.stage_ms[stage]+=ms;
        }
    }
};
inline int blocks(int n) { return (n+255)/256; }
} // namespace wf

DeviceRenderStats CudaWavefront::render(Film& film,const RenderSettings& settings,int threshold,bool profile) {
    using namespace wf;
    if(threshold<0 || threshold>capacity || settings.spp<1 || settings.depth<1 || settings.threads<1 ||
       settings.threads>RenderSettings::max_threads) throw std::invalid_argument("Invalid CUDA render settings");
    const auto s=storage_->view;
    const Frame f{film.width,film.height,settings.spp,settings.depth,settings.seed,settings.mode,
                  s.mipmaps?2*s.camera.scale/film.height:0};
    const int pixel_count=f.width*f.height;
    const uint64_t total=uint64_t(pixel_count)*f.spp;
    Buffer<Path> paths(capacity);
    Buffer<int> ids_a(capacity),ids_b(capacity),free_ids(capacity);
    Buffer<fp32::Ray> queries(capacity*light_group);
    Buffer<fp32::Hit> hits(capacity);
    Buffer<Vertex> vertices(capacity);
    Buffer<ShadowTerm> terms(capacity*light_group);
    Buffer<float> transmission(capacity*light_group);
    Buffer<V> samples(std::min<uint64_t>(window_size,total)),pixels(pixel_count);
    Buffer<Counts> counts(1);
    DeviceRenderStats stats;
    Timer timer(profile,stats);
    check(cudaMemset(pixels.ptr,0,pixel_count*sizeof(V)),"clear film");
    initialize_free<<<blocks(capacity),256>>>(free_ids.ptr);
    int* ids=ids_a.ptr; int* next=ids_b.ptr;
    for(uint64_t base=0;base<total;base+=window_size) {
        const int size=int(std::min<uint64_t>(window_size,total-base));
        int started=0,n=0;
        while(started<size || n) {
            timer.start();
            if((!n || n<threshold) && started<size) {
                const int add=std::min(capacity-n,size-started);
                refill<<<blocks(add),256>>>(s,f,base,started,n,add,free_ids.ptr,ids,paths.ptr);
                n+=add; started+=add; ++stats.refills;
                stats.peak_paths=std::max(stats.peak_paths,n);
            }
            check(cudaMemsetAsync(counts.ptr,0,sizeof(Counts)),"reset path counts");
            gather<<<blocks(n),256>>>(ids,paths.ptr,queries.ptr,n);
            timer.stop(0);
            timer.start();
            storage_->bvh.intersect_device(queries.ptr,hits.ptr,n);
            timer.stop(1);
            timer.start();
            classify<<<blocks(n),256>>>(s,f,ids,paths.ptr,hits.ptr,vertices.ptr,n,counts.ptr);
            timer.stop(2);
            const int lights=f.mode==SamplingMode::bsdf?0:s.light_count();
            for(int first=0;first<lights;first+=light_group) {
                const int m=std::min(light_group,lights-first);
                timer.start();
                light_samples<<<blocks(n),256>>>(s,f,ids,paths.ptr,vertices.ptr,first,m,n,queries.ptr,terms.ptr);
                timer.stop(2);
                timer.start();
                storage_->bvh.shadow_device(queries.ptr,transmission.ptr,m*n);
                timer.stop(3);
                timer.start();
                resolve_shadows<<<blocks(n),256>>>(vertices.ptr,terms.ptr,transmission.ptr,m,n,counts.ptr);
                timer.stop(4); stats.shadow_slots+=uint64_t(m)*n;
            }
            timer.start();
            finish_paths<<<blocks(n),256>>>(s,f,base,paths.ptr,vertices.ptr,ids,n,next,free_ids.ptr,counts.ptr,samples.ptr);
            timer.stop(4);
            Counts host{};
            check(cudaMemcpy(&host,counts.ptr,sizeof(host),cudaMemcpyDeviceToHost),"read queue counts");
            if(host.error || host.live+host.dead!=n) throw std::runtime_error("CUDA path validation failed: "+std::to_string(host.error));
            stats.control_bytes+=sizeof(host); stats.main_queries+=n; ++stats.rounds;
            std::swap(ids,next); n=host.live;
        }
        timer.start();
        accumulate<<<blocks(pixel_count),256>>>(f,base,size,samples.ptr,pixels.ptr);
        timer.stop(5);
    }
    timer.start();
    normalize<<<blocks(pixel_count),256>>>(pixels.ptr,pixel_count,f.spp);
    timer.stop(5);
    std::vector<V> rgb(pixel_count);
    stats.film_bytes=rgb.size()*sizeof(V);
    check(cudaMemcpy(rgb.data(),pixels.ptr,stats.film_bytes,cudaMemcpyDeviceToHost),"download linear film");
    for(int i=0;i<pixel_count;++i) film.pixels[i]={rgb[i].x,rgb[i].y,rgb[i].z};
    return stats;
}
