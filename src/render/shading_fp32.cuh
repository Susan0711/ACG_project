#pragma once
#include "render/bsdf_fp32.cuh"

namespace wf {
struct FogSegment { float enter,speed,length,a,b,tau; };
__device__ inline float density(const Atmosphere& fog,float y) {
    if (fog.bottom==fog.top) return fog.bottom;
    const float h=clamp((y-fog.lo.y)/(fog.hi.y-fog.lo.y),0,1);
    return fog.bottom*(1-h)+fog.top*h;
}
__device__ inline FogSegment fog_segment(const Atmosphere& fog,const fp32::Ray& ray,float maximum) {
    if (fog.top==0 && fog.bottom==0) return {};
    float enter=0,exit=maximum;
    for(int k=0;k<3;++k) {
        const float o=fp32::axis(ray.origin,k),d=fp32::axis(ray.direction,k);
        const float lo=fp32::axis(fog.lo,k),hi=fp32::axis(fog.hi,k);
        if (d==0) { if(o<lo || o>hi) return {}; }
        else {
            const float u=(lo-o)/d,v=(hi-o)/d;
            enter=fmaxf(enter,fminf(u,v)); exit=fminf(exit,fmaxf(u,v));
        }
    }
    if(exit<=enter) return {};
    const float speed=sqrtf(ray.direction.dot(ray.direction)),length=(exit-enter)*speed;
    const float a=density(fog,fmaf(enter,ray.direction.y,ray.origin.y));
    const float b=density(fog,fmaf(exit,ray.direction.y,ray.origin.y));
    return {enter,speed,length,a,b,(a*0.5f+b*0.5f)*length};
}
__device__ inline float collision_distance(FogSegment s,Random& rng) {
    if(s.tau<=0) return CUDART_INF_F;
    const float sample=-log1pf(-rng.next());
    if(sample>=s.tau) return CUDART_INF_F;
    if(s.a==s.b) return s.enter+(sample/s.a)/s.speed;
    const float scale=fmaxf(s.a,s.b), a=s.a/scale,q=(sample/scale)/s.length;
    const float denom=a+sqrtf(fmaxf(0,a*a+2*(s.b/scale-a)*q));
    return s.enter+(sample==0?0:clamp(2*q/denom,0,1))*s.length/s.speed;
}
__device__ inline V spawn_origin(V p,V normal,V direction) {
    const V offset=normal*(normal.dot(direction)>=0 ? 1e-5f : -1e-5f);
    V q=p+offset;
    // Keep the spawn offset representable in large local coordinate systems.
    if(q.x==p.x && offset.x!=0) q.x=nextafterf(p.x,copysignf(CUDART_INF_F,offset.x));
    if(q.y==p.y && offset.y!=0) q.y=nextafterf(p.y,copysignf(CUDART_INF_F,offset.y));
    if(q.z==p.z && offset.z!=0) q.z=nextafterf(p.z,copysignf(CUDART_INF_F,offset.z));
    return q;
}
__device__ inline Surface finish_surface(const SceneView& s,fp32::Ray ray,fp32::Hit hit,float footprint) {
    const auto& o=s.objects[hit.object];
    const int shape=s.geo.shape_index[hit.object];
    const auto& m=s.materials[o.material];
    Surface h{};
    V du{},dv{},tangent{},smooth{};
    float hand=0;
    if(shape>=0) {
        const auto& t=s.geo.triangles[shape];
        const V e1=t.b-t.a,e2=t.c-t.a;
        h.position=fp32::translate(fp32::translate(fp32::translate(t.a,e1,hit.u),e2,hit.v),t.velocity,ray.time);
        h.outward_normal=e1.cross(e2).normalized();
        const float2 a=o.uv.a,b=o.uv.b,c=o.uv.c;
        const float ub=b.x-a.x,uc=c.x-a.x,vb=b.y-a.y,vc=c.y-a.y;
        h.uv={fmaf(hit.v,uc,fmaf(hit.u,ub,a.x)),fmaf(hit.v,vc,fmaf(hit.u,vb,a.y))};
        if(o.normals>=0) {
            const V* n=s.normals+o.normals;
            smooth=(n[0]*(1-hit.u-hit.v)+n[1]*hit.u+n[2]*hit.v).normalized();
        }
        if(o.has_uv && m.texture>=0 && s.maps[m.texture].last) {
            const V gb=e2.cross(h.outward_normal),gc=h.outward_normal.cross(e1);
            const float area=e1.dot(gb);
            if(area!=0) { du=(gb*ub+gc*uc)*(1/area); dv=(gb*vb+gc*vc)*(1/area); }
        }
        const float det=ub*vc-uc*vb;
        if(o.has_uv && m.normal_map>=0 && fabsf(det)>1e-12f*(fabsf(ub*vc)+fabsf(uc*vb))) {
            hand=copysignf(1,det); tangent=(e1*vc-e2*vb)*hand;
        }
    } else {
        const auto& sphere=s.geo.spheres[~shape];
        const V center=fp32::translate(sphere.center,sphere.velocity,ray.time);
        const V n=fp32::translate(ray.origin-center,ray.direction,hit.t).normalized();
        h.position=fp32::translate(center,n,sphere.radius);
        h.outward_normal=n;
        h.uv={0.5f+atan2f(n.z,n.x)/(2*pi),0.5f+asinf(clamp(n.y,-1,1))/pi};
        tangent={-n.z,0,n.x}; hand=-1;
        const float hor=n.x*n.x+n.z*n.z;
        if(hor>1e-20f) {
            du=tangent*(1/(2*pi*sphere.radius*hor));
            dv=(V{0,1,0}-n*n.y)*(1/(pi*sphere.radius*sqrtf(hor)));
        } else if(m.texture>=0) h.albedo_lod=s.maps[m.texture].last;
    }
    const V n=h.outward_normal;
    h.front_face=n.dot(ray.direction)<0;
    h.normal=h.front_face?n:n*(-1);
    if(smooth.dot(h.normal)<0) smooth=smooth*(-1);
    if(smooth.dot(ray.direction)<0) h.normal=smooth;
    if(m.texture>=0 && s.maps[m.texture].last && footprint>0) {
        const auto& tex=s.maps[m.texture]; const auto& l=s.levels[tex.first];
        const V d=ray.direction.normalized(); const float cosine=d.dot(n);
        if(fabsf(cosine)<1e-12f) h.albedo_lod=tex.last;
        else {
            const V u=(du-n*(du.dot(d)/cosine))*l.width,v=(dv-n*(dv.dot(d)/cosine))*l.height;
            const float uu=u.dot(u), vv=v.dot(v);
            const float span=footprint*sqrtf(0.5f*(uu+vv+hypotf(uu-vv,2*u.dot(v))));
            h.albedo_lod=fmaxf(h.albedo_lod,clamp(log2f(fmaxf(1,span)),0,float(tex.last)));
        }
    }
    if(m.normal_map>=0 && hand!=0) {
        const V normal=h.front_face?h.normal:h.normal*(-1);
        const V t=(tangent-normal*normal.dot(tangent)).normalized();
        const V local=sample_map(s,m.normal_map,h.uv)*2-V{1,1,1};
        const V mapped=(t*local.x+normal.cross(t)*(hand*local.y)+normal*local.z).normalized();
        const V oriented=h.front_face?mapped:mapped*(-1);
        if(t.dot(t)>0 && mapped.dot(n)>0 && oriented.dot(ray.direction)<0) h.normal=oriented;
    }
    return h;
}
__device__ inline BSDFMaterial resolve_material(const SceneView& s,int id,const Surface& h) {
    BSDFMaterial m;
    static_cast<MaterialData&>(m)=s.materials[id];
    m.base=m.texture<0?m.color:sample_map(s,m.texture,h.uv,h.albedo_lod);
    if(m.roughness_map>=0) m.roughness=sample_map(s,m.roughness_map,h.uv).x;
    return m;
}
__device__ inline float area_pdf(const Area& a,V from,V to) {
    const V delta=to-from;
    const float cos=a.normal.dot(delta.normalized()*(-1));
    return cos>0 && a.area>0 ? delta.dot(delta)/(a.area*cos) : 0;
}
struct Light { V direction,radiance; float distance,pdf; bool delta; };
__device__ inline Light sample_light(const SceneView& s,int i,V from,Random& rng) {
    if(i<s.points_count) {
        const auto p=s.points[i]; const V d=p.position-from;
        const float d2=d.dot(d),dist=sqrtf(d2);
        if(dist==0) return {};
        return {d*(1/dist),p.intensity*(1/d2),dist,1,true};
    }
    i-=s.points_count;
    if(i<s.areas_count) {
        const auto& a=s.areas[i];
        const float u=rng.next(),v=rng.next();
        const V target=a.origin+a.u*u+a.v*v,delta=target-from;
        return {delta.normalized(),a.radiance,sqrtf(delta.dot(delta)),area_pdf(a,from,target),false};
    }
    const uint64_t select=rng.next53(); const float azimuth=rng.next(),elevation=rng.next();
    int lo=0,hi=s.env_width*s.env_height;
    while(lo<hi) { const int mid=lo+(hi-lo)/2; if(s.environment[mid].cdf53<=select) lo=mid+1; else hi=mid; }
    i=min(lo,s.env_width*s.env_height-1);
    const int x=i%s.env_width,y=i/s.env_width;
    const float phi=2*pi*(x+azimuth)/s.env_width+s.rotation;
    const float a=cosf(pi*y/s.env_height),b=cosf(pi*(y+1)/s.env_height);
    const float cos=a+(b-a)*elevation,sin=sqrtf(fmaxf(0,(1-cos)*(1+cos)));
    return {{sin*cosf(phi),cos,sin*sinf(phi)},s.environment[i].radiance,CUDART_INF_F,s.environment[i].pdf,false};
}
__device__ inline EnvironmentCell background(const SceneView& s,V d) {
    if(!s.env_width) return {s.background,0,0};
    d=d.normalized();
    const float u=(atan2f(d.z,d.x)-s.rotation)/(2*pi);
    const int x=min(s.env_width-1,int((u-floorf(u))*s.env_width));
    const int y=min(s.env_height-1,int(acosf(clamp(d.y,-1,1))/pi*s.env_height));
    return s.environment[y*s.env_width+x];
}
__device__ inline float mis_weight(float sample,float other) {
    if(sample<=0) return 0;
    if(other<=0) return 1;
    const float ratio=other/sample;
    return 1/(1+ratio*ratio);
}
__device__ inline float emission_weight(SamplingMode mode,bool delta,bool sampled,float previous,float pdf) {
    if(mode==SamplingMode::bsdf || delta || !sampled) return 1;
    return mode==SamplingMode::nee?0:mis_weight(previous,pdf);
}
} // namespace wf
