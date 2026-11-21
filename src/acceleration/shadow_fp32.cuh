#pragma once
#include "acceleration/intersection_fp32.cuh"

namespace fp32 {
enum class AlphaFilter { constant, procedural, nearest, bilinear };
struct AlphaTexture {
    AlphaFilter filter;
    float red;
    int width, height;
    size_t offset;
};
struct AlphaMaterial { float opacity; int texture; };
struct ShadowSurface { int material, uv_index; }; // Negative UV index encodes a sphere.
struct TriangleUV { float2 a, b, c; };
// Texel-space affine coefficients, split on upload so nearest filtering can
// resolve which side of an integer edge a barycentric hit lies on using FP32.
struct TexelAxis { float origin, du, dv, origin_tail, du_tail, dv_tail; };
struct NearestUV { TexelAxis x,y; };
struct ShadowData {
    const ShadowSurface* surfaces;
    const TriangleUV* uvs;
    const NearestUV* nearest_uvs;
    const AlphaMaterial* materials;
    const AlphaTexture* textures;
    const float* texels;
};

__device__ inline int nearest_texel(const TexelAxis& a, float u, float v, int period) {
    const float p = u*a.du, q = v*a.dv;
    const float x = fmaf(v,a.dv,fmaf(u,a.du,a.origin));
    const float edge = nearbyintf(x);
    const float scale = fabsf(a.origin)+fabsf(p)+fabsf(q)+1;
    float cell = floorf(x);
    if (fabsf(x-edge) <= 4*1.192092896e-7f*scale) {
        // Cancel the large integer first, then restore product/sum roundoff.
        const float d = a.origin-edge;
        const float s = d+p, b = s-d;
        const float sum_error = (d-(s-b))+(p-b);
        const float tail = fmaf(v,a.dv_tail,fmaf(u,a.du_tail,a.origin_tail))
                         + fmaf(u,a.du,-p) + fmaf(v,a.dv,-q) + sum_error;
        const bool below = (s+q)+tail < 0;
        int i = int(fmodf(edge,float(period)));
        if (below) --i;
        return (i+period)%period;
    }
    if (cell >= 0 && cell < period) return int(cell);
    const int i = int(fmodf(cell,float(period)));
    return (i+period)%period;
}

__device__ inline float2 shadow_uv(const ShadowData& data, int index, const Sphere* spheres,
                                   const Ray& ray, const Hit& hit) {
    if (index >= 0) {
        const auto& uv = data.uvs[index];
        // Interpolate from one corner. Equal corner coordinates then contribute
        // an exact zero, even when the UV lies one ULP from a texel boundary.
        return {fmaf(hit.v,uv.c.x-uv.a.x,fmaf(hit.u,uv.b.x-uv.a.x,uv.a.x)),
                fmaf(hit.v,uv.c.y-uv.a.y,fmaf(hit.u,uv.b.y-uv.a.y,uv.a.y))};
    }
    const auto& s = spheres[~index];
    const V p = sub(translate(ray.origin,ray.direction,hit.t), translate(s.center,s.velocity,ray.time));
    const float length = norm3df(p.x,p.y,p.z);
    const float ny = length > 0 ? p.y/length : 0;
    constexpr float pi = 3.14159265358979323846f;
    return {0.5f + atan2f(p.z,p.x)/(2*pi), 0.5f + asinf(fminf(1,fmaxf(-1,ny)))/pi};
}

// Match Texture::value at LOD zero, including repeating UVs and flipped image rows.
__device__ inline float sample_alpha(const AlphaTexture& t, const float* texels, float2 uv) {
    if (t.filter == AlphaFilter::procedural)
        return t.red*(0.7f + 0.3f*sinf(uv.x*37 + sinf(uv.y*19)));
    const float u = uv.x-floorf(uv.x), v = uv.y-floorf(uv.y);
    const int w = t.width, h = t.height;
    const float* pixels = texels+t.offset;
    if (t.filter == AlphaFilter::nearest) {
        const int x = min(w-1,int(u*w)), y = h-1-min(h-1,int(v*h));
        return pixels[y*w+x];
    }
    const float x = u*w-0.5f, y = (1-v)*h-0.5f;
    const int ix = int(floorf(x)), iy = int(floorf(y));
    const float fx = x-ix, fy = y-iy;
    const int x0 = (ix+w)%w, x1 = (ix+1)%w;
    const int row0 = ((iy+h)%h)*w, row1 = ((iy+1)%h)*w;
    return (pixels[row0+x0]*(1-fx) + pixels[row0+x1]*fx)*(1-fy)
         + (pixels[row1+x0]*(1-fx) + pixels[row1+x1]*fx)*fy;
}

__device__ inline float surface_alpha(const ShadowData& data, const Sphere* spheres,
                                      const Ray& ray, const Hit& hit) {
    const auto surface = data.surfaces[hit.object];
    if (surface.material < 0) return 1;
    const auto m = data.materials[surface.material];
    if (m.texture < 0) return m.opacity;
    const auto& t = data.textures[m.texture];
    if (t.filter == AlphaFilter::constant) return m.opacity*t.red;
    if (t.filter == AlphaFilter::nearest && surface.uv_index >= 0) {
        const auto& uv = data.nearest_uvs[surface.uv_index];
        const int x = nearest_texel(uv.x,hit.u,hit.v,t.width);
        const int y = t.height-1-nearest_texel(uv.y,hit.u,hit.v,t.height);
        return m.opacity*data.texels[t.offset+size_t(y)*t.width+x];
    }
    return m.opacity*sample_alpha(t, data.texels, shadow_uv(data,surface.uv_index,spheres,ray,hit));
}
}
