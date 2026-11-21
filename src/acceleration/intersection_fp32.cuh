#pragma once
#include <cuda_runtime.h>

namespace fp32 {
struct V {
    float x, y, z;
    __host__ __device__ V operator+(V b) const { return {x+b.x,y+b.y,z+b.z}; }
    __host__ __device__ V operator-(V b) const { return {x-b.x,y-b.y,z-b.z}; }
    __host__ __device__ V operator*(float s) const { return {x*s,y*s,z*s}; }
    __device__ float dot(V b) const { return x*b.x+y*b.y+z*b.z; }
    __device__ V cross(V b) const { return {y*b.z-z*b.y,z*b.x-x*b.z,x*b.y-y*b.x}; }
    __device__ V normalized() const {
        const float length = sqrtf(dot(*this));
        return length > 0 ? *this*(1.f/length) : V{};
    }
};
struct Ray { V origin, direction; float time, minimum, maximum; };
struct Hit { float t = 0, u = 0, v = 0; int object = -1; };
struct Triangle { V a, b, c, velocity; };
struct Sphere { V center, velocity; float radius; };
struct Node { float lo[3], hi[3]; int end, object, shape; };

__device__ inline float axis(V p, int k) { return k == 0 ? p.x : k == 1 ? p.y : p.z; }
__device__ inline V sub(V a, V b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
__device__ inline V translate(V p, V v, float t) {
    return {fmaf(v.x,t,p.x), fmaf(v.y,t,p.y), fmaf(v.z,t,p.z)};
}
__device__ inline float dot(V a, V b) { return fmaf(a.x,b.x,fmaf(a.y,b.y,a.z*b.z)); }

struct SlabAxis { float origin, direction, inv_lo, inv_hi; };
struct Projection { int x, y, z; float sx, sy; };
__device__ inline Projection projection(V d) {
    int z = fabsf(d.x) > fabsf(d.y) ? 0 : 1;
    if (fabsf(d.z) > fabsf(axis(d,z))) z = 2;
    const int x = (z+1)%3, y = (x+1)%3;
    return {x,y,z,axis(d,x)/axis(d,z),axis(d,y)/axis(d,z)};
}
__device__ inline bool box_hit(const Node& n, const SlabAxis axes[3], float enter, float exit) {
    #pragma unroll
    for (int i=0; i<3; ++i) {
        const SlabAxis& a = axes[i];
        if (a.direction == 0) {
            if (a.origin < n.lo[i] || a.origin > n.hi[i]) return false;
            continue;
        }
        // Orient the slab along the ray and enclose both distance endpoints.
        const float near_plane = a.direction < 0 ? -n.hi[i] : n.lo[i];
        const float far_plane = a.direction < 0 ? -n.lo[i] : n.hi[i];
        const float origin = a.direction < 0 ? -a.origin : a.origin;
        const float a0 = __fsub_rd(near_plane,origin), a1 = __fsub_ru(far_plane,origin);
        if (isinf(a.inv_hi)) {
            enter = fmaxf(enter,__fdiv_rd(a0,fabsf(a.direction)));
            exit = fminf(exit,__fdiv_ru(a1,fabsf(a.direction)));
        } else {
            enter = fmaxf(enter,__fmul_rd(a0,a0 < 0 ? a.inv_hi : a.inv_lo));
            exit = fminf(exit,__fmul_ru(a1,a1 < 0 ? a.inv_lo : a.inv_hi));
        }
    }
    return enter <= exit;
}

// All triangles apply the same shear to shared vertices. Reversing an edge
// then reverses its rounded edge function, including at a shared boundary.
__device__ inline V shear(V vertex, V origin, const Projection& p) {
    const V v = sub(vertex,origin);
    const float z = axis(v,p.z);
    return {fmaf(-p.sx,z,axis(v,p.x)), fmaf(-p.sy,z,axis(v,p.y)), z};
}
__device__ inline bool triangle_hit(const Triangle& g, const Ray& r, const Projection& p,
                                    float maximum, Hit& h) {
    const V a = shear(translate(g.a,g.velocity,r.time),r.origin,p);
    const V b = shear(translate(g.b,g.velocity,r.time),r.origin,p);
    const V c = shear(translate(g.c,g.velocity,r.time),r.origin,p);
    const float wa = b.x*c.y-b.y*c.x, wb = c.x*a.y-c.y*a.x, wc = a.x*b.y-a.y*b.x;
    if ((wa < 0 || wb < 0 || wc < 0) && (wa > 0 || wb > 0 || wc > 0)) return false;
    const float sum = wa+wb+wc;
    if (sum == 0 || !isfinite(sum)) return false;
    const float u = wb/sum, v = wc/sum;
    const float z = fmaf(u,b.z-a.z,fmaf(v,c.z-a.z,a.z));
    const float t = z/axis(r.direction,p.z);
    if (!isfinite(t) || t < r.minimum || t > maximum) return false;
    h.t=t; h.u=u; h.v=v;
    return true;
}

__device__ inline bool sphere_hit(const Sphere& s, const Ray& r, float maximum, Hit& h) {
    const V oc = sub(r.origin,translate(s.center,s.velocity,r.time));
    const float a = dot(r.direction,r.direction);
    if (!(a > 0) || !(s.radius > 0)) return false;
    const float center_t = -dot(oc,r.direction)/a;
    const V closest = translate(oc,r.direction,center_t);
    const float radial = fmaf(s.radius,s.radius,-dot(closest,closest));
    if (!(radial >= 0)) return false;
    const float half = sqrtf(radial/a);
    float t = center_t-half;
    if (t < r.minimum || t > maximum) t = center_t+half;
    if (!isfinite(t) || t < r.minimum || t > maximum) return false;
    h.t=t; h.u=0; h.v=0;
    return true;
}
}
