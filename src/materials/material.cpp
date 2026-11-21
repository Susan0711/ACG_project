#include "materials/material.h"
#include <algorithm>
#include <stdexcept>

namespace {
double cosine_pdf(Vec3 normal, Vec3 direction) {
    return std::max(0.0, normal.dot(direction)) / pi;
}

Vec3 direction_about(Vec3 normal, double sine, double cosine, double azimuth) {
    const Vec3 axis = std::abs(normal.x) > 0.9 ? Vec3{0, 1, 0} : Vec3{1, 0, 0};
    const Vec3 tangent = axis.cross(normal).normalized();
    const Vec3 bitangent = normal.cross(tangent);
    return (tangent * (sine * std::cos(azimuth)) + bitangent * (sine * std::sin(azimuth))
            + normal * cosine).normalized();
}

Vec3 sample_cosine(Vec3 normal, Sampler& rng) {
    const double u = rng.next();
    return direction_about(normal, std::sqrt(u), std::sqrt(1 - u), 2 * pi * rng.next());
}

Vec3 diffuse_weight(const Material& material, const HitRecord& hit, double cosine) {
    const Vec3 albedo = material.albedo(hit.uv, hit.albedo_lod);
    if (material.toon_bands == 0) return albedo;
    if (cosine <= 0) return {};
    const double intervals = material.toon_bands - 1;
    const double band = std::round(std::min(cosine, 1.0) * intervals) / intervals;
    // Multiplying f by cosine in direct lighting recovers the band. With PDF = cosine/pi,
    // this same albedo-scaled ratio is the weight of a sampled bounce.
    return albedo * (band / cosine);
}

double ggx_alpha(const Material& material, UV uv) {
    const double roughness = material.roughness_at(uv);
    return std::max(0.001, roughness * roughness);
}

double specular_probability(const Material& material) {
    return 0.5 + 0.5 * material.metallic;
}

BSDFEvaluation evaluate_principled(const Material& material, Vec3 view, const HitRecord& hit, Vec3 light) {
    const Vec3 normal = hit.normal;
    const Vec3 geometric = hit.front_face ? hit.outward_normal : hit.outward_normal * -1;
    const double nv = normal.dot(view), nl = normal.dot(light);
    if (nv <= 0 || nl <= 0 || geometric.dot(light) <= 0) return {};

    const Vec3 half = (view + light).normalized();
    const double nh = std::clamp(normal.dot(half), 0.0, 1.0);
    const double vh = std::clamp(view.dot(half), 0.0, 1.0);
    if (vh == 0) return {};
    const double alpha = ggx_alpha(material, hit.uv), a2 = alpha * alpha;
    const double denominator = (1 - nh) * (1 + nh) + a2 * nh * nh;
    const double distribution = a2 / (pi * denominator * denominator);
    // Height-correlated Smith masking/shadowing divided by 4 (n.v) (n.l).
    const double visibility = 0.5 / (nl * std::sqrt(a2 + (1 - a2) * nv * nv)
                                    + nv * std::sqrt(a2 + (1 - a2) * nl * nl));

    const Vec3 base = material.albedo(hit.uv, hit.albedo_lod);
    const double dielectric = 1 - material.metallic;
    const Vec3 f0 = Vec3{0.04, 0.04, 0.04} * dielectric + base * material.metallic;
    const double grazing = std::pow(1 - vh, 5);
    const Vec3 fresnel_rgb = f0 + (Vec3{1, 1, 1} - f0) * grazing;
    const Vec3 diffuse = base * (dielectric * 0.96 * (1 - grazing) / pi);
    const double p_spec = specular_probability(material);
    // Sampling GGX half vectors with density D(h) (n.h) induces this reflection PDF.
    const double specular_pdf = distribution * nh / (4 * vh);
    return {diffuse + fresnel_rgb * (distribution * visibility),
            (1 - p_spec) * cosine_pdf(normal, light) + p_spec * specular_pdf};
}
}

BSDFEvaluation evaluate_bsdf(const Material& material, const Ray& incident, const HitRecord& hit, Vec3 direction) {
    if (material.kind == Kind::principled)
        return evaluate_principled(material, incident.direction.normalized() * -1, hit, direction);
    if (!material.has_continuous_bsdf()) return {};
    const double cosine = std::max(0.0, hit.normal.dot(direction));
    if (cosine == 0) return {};
    return {diffuse_weight(material, hit, cosine) * (1.0 / pi), cosine / pi};
}
double fresnel(double c,double ei,double et) {
    c=std::clamp(c,0.0,1.0);
    double st2=(ei/et)*(ei/et)*(1-c*c);
    if(st2>=1)return 1;
    double ct=std::sqrt(1-st2),rs=(ei*c-et*ct)/(ei*c+et*ct),rp=(et*c-ei*ct)/(et*c+ei*ct);
    return (rs*rs+rp*rp)*0.5;
}
BSDFSample sample_bsdf(const Material& m,const Ray& ray,const HitRecord& h,Sampler& rng) {
    Vec3 d=ray.direction.normalized(),n=h.normal;
    if(m.kind==Kind::diffuse) {
        Vec3 wi = sample_cosine(n, rng);
        const double cosine = std::max(0.0, n.dot(wi));
        return {wi, diffuse_weight(m, h, cosine), cosine / pi, false};
    }
    if (m.kind == Kind::principled) {
        Vec3 wi;
        if (rng.next() < specular_probability(m)) {
            const double u = rng.next();
            const double slope = ggx_alpha(m, h.uv) * std::sqrt(u / (1 - u));
            const double cosine = 1 / std::sqrt(1 + slope * slope);
            const Vec3 half = direction_about(n, slope * cosine, cosine, 2 * pi * rng.next());
            wi = (d - half * (2 * d.dot(half))).normalized();
        } else {
            wi = sample_cosine(n, rng);
        }
        const auto bsdf = evaluate_principled(m, d * -1, h, wi);
        if (bsdf.pdf <= 0) return {wi, {}, 0, false};
        return {wi, bsdf.value * (n.dot(wi) / bsdf.pdf), bsdf.pdf, false};
    }
    Vec3 reflection=(d-n*(2*d.dot(n))).normalized();
    if(m.kind==Kind::mirror)return {reflection,m.color,1,true};
    if(m.kind==Kind::glass) {
        double ei=h.front_face?1:m.ior,et=h.front_face?m.ior:1,eta=ei/et,c=std::clamp(-d.dot(n),0.0,1.0),f=fresnel(c,ei,et);
        if(rng.next()<f)return {reflection,{1,1,1},f,true};
        Vec3 perpendicular=(d+n*c)*eta;
        Vec3 transmission=(perpendicular-n*std::sqrt(std::max(0.0,1-perpendicular.dot(perpendicular)))).normalized();
        return {transmission,Vec3{1,1,1}*(eta*eta),1-f,true};
    }
    return {{0,0,0},{0,0,0},0,false};
}
double Material::alpha(UV uv) const {
    if(kind==Kind::emissive && (opacity!=1 || opacity_map))
        throw std::runtime_error("Emissive alpha is unsupported");
    double value=opacity*(opacity_map?opacity_map->value(uv).x:1);
    if(!std::isfinite(value) || value<0 || value>1)
        throw std::runtime_error("Opacity must be linear coverage in [0,1]");
    return value;
}
