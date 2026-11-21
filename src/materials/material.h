#pragma once
#include "core/ray.h"
#include "textures/texture.h"
#include "core/sampling.h"
#include <memory>

enum class Kind { diffuse, mirror, glass, emissive, principled };
struct Material {
    Kind kind = Kind::diffuse;
    Vec3 color{0.75,0.75,0.75};
    double ior=1.5;
    std::shared_ptr<const Texture> texture{};
    double opacity=1;
    std::shared_ptr<const Texture> opacity_map{};
    double metallic = 0;
    double roughness = 0.5;
    std::shared_ptr<const Texture> normal_map{};
    std::shared_ptr<const Texture> roughness_map{};
    double roughness_at(UV uv) const { return roughness_map ? roughness_map->value(uv).x : roughness; }
    bool has_continuous_bsdf() const { return kind == Kind::diffuse || kind == Kind::principled; }
    bool is_emitter() const { return kind == Kind::emissive; }
    double alpha(UV uv) const;
    Vec3 albedo(UV uv) const { return texture ? texture->value(uv) : color; }
};
struct BSDFSample {
    Vec3 direction, weight;
    double pdf;
    bool delta;
};
double fresnel(double cosine,double eta_i,double eta_t);
BSDFSample sample_bsdf(const Material&,const Ray&,const HitRecord&,Sampler&);

// For continuous scattering, value is f and pdf is a density per steradian.
// Ideal mirror and dielectric events have discrete probabilities in BSDFSample.
struct BSDFEvaluation {
    Vec3 value{};
    double pdf = 0;
};
BSDFEvaluation evaluate_bsdf(const Material&, const Ray& incident, const HitRecord&, Vec3 direction);
