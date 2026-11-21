#pragma once
#include "render/device_scene.cuh"

namespace wf {
__device__ inline float fresnel(float c,float ei,float et);
__device__ inline float cosine_pdf(V normal, V direction) {
    return fmaxf(0.0f, normal.dot(direction)) / pi;
}

__device__ inline V direction_about(V normal, float sine, float cosine, float azimuth) {
    const V axis = fabsf(normal.x) > 0.9f ? V{0, 1, 0} : V{1, 0, 0};
    const V tangent = axis.cross(normal).normalized();
    const V bitangent = normal.cross(tangent);
    return (tangent * (sine * cosf(azimuth)) + bitangent * (sine * sinf(azimuth))
            + normal * cosine).normalized();
}

__device__ inline V sample_cosine(V normal, Random& rng) {
    const float u = rng.next();
    return direction_about(normal, sqrtf(u), sqrtf(1 - u), 2 * pi * rng.next());
}

__device__ inline V diffuse_weight(const BSDFMaterial& material, const Surface& hit, float cosine) {
    const V albedo = material.albedo(hit.uv, hit.albedo_lod);
    if (material.toon_bands == 0) return albedo;
    if (cosine <= 0) return {};
    const float intervals = material.toon_bands - 1;
    const float band = roundf(fminf(cosine, 1.0f) * intervals) / intervals;
    // Multiplying f by cosine in direct lighting recovers the band. With PDF = cosine/pi,
    // this same albedo-scaled ratio is the weight of a sampled bounce.
    return albedo * (band / cosine);
}

__device__ inline float ggx_alpha(const BSDFMaterial& material, float2 uv) {
    const float roughness = material.roughness_at(uv);
    return fmaxf(0.001f, roughness * roughness);
}

__device__ inline float specular_probability(const BSDFMaterial& material) {
    return 0.5f + 0.5f * material.metallic;
}

__device__ inline V sample_ggx(V incident, V normal, float alpha, Random& rng) {
    const float u = rng.next();
    const float slope = alpha * sqrtf(u / (1 - u));
    const float cosine = 1 / sqrtf(1 + slope * slope);
    const V half = direction_about(normal, slope * cosine, cosine, 2 * pi * rng.next());
    return (incident - half * (2 * incident.dot(half))).normalized();
}

__device__ inline Evaluation evaluate_principled(const BSDFMaterial& material, V view, const Surface& hit, V light) {
    const V normal = hit.normal;
    const V geometric = hit.front_face ? hit.outward_normal : hit.outward_normal * -1;
    const float nv = normal.dot(view), nl = normal.dot(light);
    if (nv <= 0 || nl <= 0 || geometric.dot(light) <= 0) return {};

    const V half = (view + light).normalized();
    const float nh = clamp(normal.dot(half), 0.0f, 1.0f);
    const float vh = clamp(view.dot(half), 0.0f, 1.0f);
    if (vh == 0) return {};
    const float alpha = ggx_alpha(material, hit.uv), a2 = alpha * alpha;
    const float denominator = (1 - nh) * (1 + nh) + a2 * nh * nh;
    const float distribution = a2 / (pi * denominator * denominator);
    // Height-correlated Smith masking/shadowing divided by 4 (n.v) (n.l).
    const float visibility = 0.5f / (nl * sqrtf(a2 + (1 - a2) * nv * nv)
                                    + nv * sqrtf(a2 + (1 - a2) * nl * nl));

    const V base = material.albedo(hit.uv, hit.albedo_lod);
    const float dielectric = 1 - material.metallic;
    const V f0 = V{0.04f, 0.04f, 0.04f} * dielectric + base * material.metallic;
    const float grazing = powf(1 - vh, 5);
    const V fresnel_rgb = f0 + (V{1, 1, 1} - f0) * grazing;
    const V diffuse = base * (dielectric * 0.96f * (1 - grazing) / pi);
    const float p_spec = specular_probability(material);
    // Sampling GGX half vectors with density D(h) (n.h) induces this reflection PDF.
    const float specular_pdf = distribution * nh / (4 * vh);
    return {diffuse + fresnel_rgb * (distribution * visibility),
            (1 - p_spec) * cosine_pdf(normal, light) + p_spec * specular_pdf};
}

__device__ inline float coat_probability(const BSDFMaterial& m) { return 0.5f * m.coat_weight; }

__device__ inline Evaluation evaluate_coated(const BSDFMaterial& m, V view, const Surface& hit, V light) {
    const auto base = evaluate_principled(m, view, hit, light);
    if (m.coat_weight == 0) return base;
    const V n = hit.normal;
    const float nv = n.dot(view), nl = n.dot(light);
    const V geometric = hit.front_face ? hit.outward_normal : hit.outward_normal * -1;
    if (nv <= 0 || nl <= 0 || geometric.dot(light) <= 0) return {};

    const V half = (view + light).normalized();
    const float nh = clamp(n.dot(half), 0.0f, 1.0f);
    const float vh = clamp(view.dot(half), 0.0f, 1.0f);
    if (vh == 0) return {};
    const float alpha = fmaxf(0.001f, m.coat_roughness * m.coat_roughness), a2 = alpha * alpha;
    const float denom = (1 - nh) * (1 + nh) + a2 * nh * nh;
    const float distribution = a2 / (pi * denom * denom);
    const float visibility = 0.5f / (nl * sqrtf(a2 + (1 - a2) * nv * nv)
                                    + nv * sqrtf(a2 + (1 - a2) * nl * nl));
    const float coat = fresnel(vh, 1, m.coat_ior) * distribution * visibility;
    const float transmission = (1 - fresnel(nv, 1, m.coat_ior)) * (1 - fresnel(nl, 1, m.coat_ior));
    // Approximate a flat absorbing film. Refracted cosines set the path length;
    // the substrate BRDF is evaluated in external directions (no refraction Jacobian
    // or internal multiple reflections). Coat and base share the shading normal.
    const auto internal_cosine = [&](float cosine) {
        return sqrtf(fmaxf(1e-12f, 1 - (1 - cosine * cosine) / (m.coat_ior * m.coat_ior)));
    };
    const float distance = m.coat_thickness * (1 / internal_cosine(nv) + 1 / internal_cosine(nl));
    const auto attenuation = [&](float sigma) {
        return sigma == 0 ? 1.0f : expf(-sigma * distance);
    };
    const V absorption{attenuation(m.coat_absorption.x), attenuation(m.coat_absorption.y),
                          attenuation(m.coat_absorption.z)};
    const V base_scale = V{1, 1, 1} * (1 - m.coat_weight)
                            + absorption * (m.coat_weight * transmission);
    const float q = coat_probability(m);
    return {V{coat, coat, coat} * m.coat_weight + multiply(base.value, base_scale),
            q * distribution * nh / (4 * vh) + (1 - q) * base.pdf};
}

__device__ inline Evaluation evaluate_bsdf(const BSDFMaterial& material, const fp32::Ray& incident, const Surface& hit, V direction) {
    if (material.kind == Kind::coated)
        return evaluate_coated(material, incident.direction.normalized() * -1, hit, direction);
    if (material.kind == Kind::principled)
        return evaluate_principled(material, incident.direction.normalized() * -1, hit, direction);
    if (!material.has_continuous_bsdf()) return {};
    const float cosine = fmaxf(0.0f, hit.normal.dot(direction));
    if (cosine == 0) return {};
    return {diffuse_weight(material, hit, cosine) * (1.0f / pi), cosine / pi};
}
__device__ inline float fresnel(float c,float ei,float et) {
    c=clamp(c,0.0f,1.0f);
    float st2=(ei/et)*(ei/et)*(1-c*c);
    if(st2>=1)return 1;
    float ct=sqrtf(1-st2),rs=(ei*c-et*ct)/(ei*c+et*ct),rp=(et*c-ei*ct)/(et*c+ei*ct);
    return (rs*rs+rp*rp)*0.5f;
}
__device__ inline float BSDFMaterial::ior_at(Wavelength band) const {
    if (cauchy_b == 0 || band == Wavelength::green) return ior;
    const float lambda = static_cast<int>(band) * 0.001f;
    constexpr float reference = 0.55f;
    return ior + cauchy_b * (1 / (lambda * lambda) - 1 / (reference * reference));
}

__device__ inline Scatter sample_principled(const BSDFMaterial& m,V d,const Surface& h,Random& rng) {
    const V n=h.normal;
    V wi;
    if (rng.next() < specular_probability(m)) {
        wi = sample_ggx(d, n, ggx_alpha(m, h.uv), rng);
    } else {
        wi = sample_cosine(n, rng);
    }
    const auto bsdf = evaluate_principled(m, d * -1, h, wi);
    if (bsdf.pdf <= 0) return {wi, {}, 0, false};
    return {wi, bsdf.value * (n.dot(wi) / bsdf.pdf), bsdf.pdf, false};
}

__device__ inline Scatter sample_bsdf(const BSDFMaterial& m,const fp32::Ray& ray,const Surface& h,Random& rng, Wavelength band) {
    V d=ray.direction.normalized(),n=h.normal;
    if (m.kind == Kind::coated) {
        BSDFMaterial base = m;
        base.kind = Kind::principled;
        if (m.coat_weight == 0) return sample_principled(base, d, h, rng);
        const V wi = rng.next() < coat_probability(m)
            ? sample_ggx(d, n, fmaxf(0.001f, m.coat_roughness * m.coat_roughness), rng)
            : sample_principled(base, d, h, rng).direction;
        const auto bsdf = evaluate_coated(m, d * -1, h, wi);
        if (bsdf.pdf <= 0) return {wi, {}, 0, false};
        return {wi, bsdf.value * (n.dot(wi) / bsdf.pdf), bsdf.pdf, false};
    }
    if(m.kind==Kind::diffuse) {
        V wi = sample_cosine(n, rng);
        const float cosine = fmaxf(0.0f, n.dot(wi));
        return {wi, diffuse_weight(m, h, cosine), cosine / pi, false};
    }
    if (m.kind == Kind::principled) return sample_principled(m,d,h,rng);
    V reflection=(d-n*(2*d.dot(n))).normalized();
    if(m.kind==Kind::mirror)return {reflection,m.color,1,true};
    if(m.kind==Kind::glass) {
        const float index = m.ior_at(band);
        const float ei = h.front_face ? 1 : index, et = h.front_face ? index : 1;
        const float eta = ei / et, c = clamp(-d.dot(n), 0.0f, 1.0f), f = fresnel(c, ei, et);
        if(rng.next()<f)return {reflection,{1,1,1},f,true};
        V perpendicular=(d+n*c)*eta;
        V transmission=(perpendicular-n*sqrtf(fmaxf(0.0f,1-perpendicular.dot(perpendicular)))).normalized();
        return {transmission,V{1,1,1}*(eta*eta),1-f,true};
    }
    return {{0,0,0},{0,0,0},0,false};
}

} // namespace wf
