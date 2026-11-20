#include "materials/material.h"
#include <algorithm>
#include <stdexcept>

namespace {
double cosine_pdf(Vec3 normal, Vec3 direction) {
    return std::max(0.0, normal.dot(direction)) / pi;
}
}

BSDFEvaluation evaluate_bsdf(const Material& material, const Ray&, const HitRecord& hit, Vec3 direction) {
    if (!material.has_continuous_bsdf()) return {};
    double density = cosine_pdf(hit.normal, direction);
    if (density == 0) return {};
    return {material.albedo(hit.uv) * (1.0 / pi), density};
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
        double u=rng.next(),phi=2*pi*rng.next();
        Vec3 tangent=(std::abs(n.x)>0.9?Vec3{0,1,0}:Vec3{1,0,0}).cross(n).normalized(),bitangent=n.cross(tangent);
        Vec3 wi=(tangent*(std::sqrt(u)*std::cos(phi))+bitangent*(std::sqrt(u)*std::sin(phi))+n*std::sqrt(1-u)).normalized();
        return {wi,m.albedo(h.uv),cosine_pdf(n,wi),false};
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
