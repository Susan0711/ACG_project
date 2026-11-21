#include "materials/material_reader.h"
#include "render/render.h"
#include "scene_init.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool close(double a, double b) {
    return std::isfinite(a) && std::isfinite(b)
           && std::abs(a - b) <= 1e-10 * std::max({1.0, std::abs(a), std::abs(b)});
}

bool close(Vec3 a, Vec3 b) {
    return close(a.x, b.x) && close(a.y, b.y) && close(a.z, b.z);
}

void check_json() {
    using Json = nlohmann::json;
    const Json base = {{"type", "principled"}, {"color", {0.8, 0.32, 0.12}}};
    const Material defaults = read_material(base, ".");
    check(defaults.kind == Kind::principled && defaults.has_continuous_bsdf()
          && defaults.metallic == 0 && defaults.roughness == 0.5, "JSON defaults or direct-light eligibility");
    const Json textured = {{"type", "principled"}, {"texture", "test_colors.ppm"},
                           {"metallic", 0.7}, {"roughness", 0.35}};
    const Material loaded = read_material(textured, "assets");
    check(loaded.texture && loaded.metallic == 0.7 && loaded.roughness == 0.35
          && close(loaded.albedo({0.25, 0.25}).x, 0.21586050011389926), "Textured principled JSON");
    for (const auto key : {"metallic", "roughness"}) {
        for (const Json& invalid : {Json(-0.1), Json(1.1), Json("0.5")}) {
            Json entry = base;
            entry[key] = invalid;
            bool rejected = false;
            try { read_material(entry, "."); } catch (const std::runtime_error&) { rejected = true; }
            check(rejected, "Invalid metallic or roughness accepted");
        }
    }
}

void check_material_maps() {
    const std::filesystem::path path = "scene/principled_maps.json";
    World world = load_scene_file(path);
    std::ifstream input(path);
    const auto panel = nlohmann::json::parse(input).at("materials").at("panel");
    auto defaults = panel;
    defaults.erase("normal_map_enabled");
    defaults.erase("roughness_map_enabled");
    const auto mapped = read_material(defaults, path.parent_path());
    check(mapped.normal_map && mapped.roughness_map, "Supplying map paths enables both maps by default");
    for (bool normals : {false, true}) {
        for (bool roughness : {false, true}) {
            auto entry = panel;
            entry["normal_map_enabled"] = normals;
            entry["roughness_map_enabled"] = roughness;
            const auto material = read_material(entry, path.parent_path());
            world.materials.at(0) = material;
            const RenderScene scene(world);
            for (double v : {0.25, 0.75}) {
                // Texel 2 of the ridge map encodes (184,127,240) with max value 254.
                const double u = 2.5 / 64;
                const Ray incident{{2*u - 1, 1, 2*v - 1}, {0,-1,0}};
                HitRecord hit;
                check(scene.intersect(incident, 0, 2, hit), "Hit the mapped quad");
                const Vec3 expected_n = normals ? Vec3{57,113,0}.normalized() : Vec3{0,1,0};
                const double expected_r = roughness ? (v < 0.5 ? 0.2 : 0.75) : 0.35;
                check(close(hit.uv.u, u) && close(hit.uv.v, v) && close(hit.normal, expected_n)
                      && close(hit.outward_normal, {0,1,0}) && close(material.roughness_at(hit.uv), expected_r),
                      "Independent switches, linear texels and UV orientation");

                Material reference = material;
                reference.normal_map.reset();
                reference.roughness_map.reset();
                reference.roughness = expected_r;
                HitRecord expected_hit = hit;
                expected_hit.normal = expected_n;
                Sampler rng{42}, reference_rng{42};
                for (int i = 0; i < 256; ++i) {
                    const auto s = sample_bsdf(material, incident, hit, rng);
                    const auto ref = sample_bsdf(reference, incident, expected_hit, reference_rng);
                    const auto e = evaluate_bsdf(material, incident, hit, s.direction);
                    const auto ref_eval = evaluate_bsdf(reference, incident, expected_hit, s.direction);
                    check(close(s.direction, ref.direction) && close(s.weight, ref.weight)
                          && close(s.pdf, e.pdf) && close(e.pdf, ref_eval.pdf) && close(e.value, ref_eval.value),
                          "Mapped BSDF evaluation and sampling use the same normal and local GGX roughness");
                }
            }
        }
    }
}

void check_uv_frames() {
    Triangle tri{{0,0,0},{0,0,1},{1,0,0},0,{0,0},{0,1},{1,0}};
    const Ray ray{{0.25,1,0.25},{0,-1,0}};
    HitRecord hit;
    check(tri.intersect(ray, 0, 2, hit), "Hit UV triangle");
    const Vec3 local = Vec3{1,2,3}.normalized();
    check(close(tri.mapped_normal(hit, local), {local.x,local.z,local.y}), "Positive U and V orientation");
    tri.tc.u = -1;
    check(close(tri.mapped_normal(hit, local), {-local.x,local.z,local.y}), "Mirrored U preserves V");
    HitRecord back;
    check(tri.intersect({{0.25,-1,0.25},{0,1,0}}, 0, 2, back)
          && close(tri.mapped_normal(back, local), {local.x,-local.z,-local.y}), "Orient the back face once");
    tri.has_complete_uvs = false;
    check(close(tri.mapped_normal(hit, local), hit.normal), "Incomplete OBJ UVs preserve the shading normal");
    tri.has_complete_uvs = true;
    tri.tc.u = 0;
    check(close(tri.mapped_normal(hit, local), hit.normal), "Degenerate UV frame falls back");

    const Sphere sphere{{0,0,0}, 1};
    check(sphere.intersect({{2,0,0},{-1,0,0}}, 0, 2, hit)
          && close(sphere.mapped_normal(hit, local), {local.z,local.y,local.x}), "Longitude and latitude frame");
    check(sphere.intersect({{0,2,0},{0,-1,0}}, 0, 2, hit)
          && close(sphere.mapped_normal(hit, local), hit.normal), "Sphere pole keeps a finite normal");
}

void check_endpoints(Material material, const Ray& ray, const HitRecord& hit) {
    // At normal incidence and roughness 1, D=1/pi, G=1 and Fresnel equals F0.
    material.roughness = 1;
    const Vec3 dielectric = (material.color * 0.96 + Vec3{0.01, 0.01, 0.01}) * (1 / pi);
    const Vec3 conductor = material.color * (1 / (4 * pi));
    for (double metallic : {0.0, 0.5, 1.0}) {
        material.metallic = metallic;
        const auto bsdf = evaluate_bsdf(material, ray, hit, hit.normal);
        check(close(bsdf.value, dielectric * (1 - metallic) + conductor * metallic),
              "Normal-incidence dielectric/metal blend");
    }
    material.roughness = 0;
    const auto sharp = evaluate_bsdf(material, ray, hit, hit.normal);
    check(close(sharp.pdf, 1 / (4 * pi * 1e-6)) && close(sharp.value, material.color * sharp.pdf),
          "Sharp GGX peak stays finite at the roughness floor");
    std::cout << "Roughness 0, normal-incidence metal: PDF=" << sharp.pdf << " weight_r="
              << sharp.value.x / sharp.pdf << '\n';
}

void check_samples(Material material, const HitRecord& hit) {
    Sampler rng{42};
    double max_pdf_error = 0;
    for (double metallic : {0.0, 0.5, 1.0}) {
        for (double roughness : {0.0, 1e-6, 0.35, 1.0}) {
            material.metallic = metallic;
            material.roughness = roughness;
            for (double view_cosine : {1.0, 0.6, 1e-8}) {
                const Ray ray{{}, {std::sqrt(1 - view_cosine * view_cosine), 0, -view_cosine}};
                int valid = 0;
                for (int i = 0; i < 512; ++i) {
                    const auto s = sample_bsdf(material, ray, hit, rng);
                    const auto e = evaluate_bsdf(material, ray, hit, s.direction);
                    check(!s.delta && close(s.direction.dot(s.direction), 1) && finite(s.weight)
                          && finite(e.value) && std::isfinite(s.pdf) && s.pdf >= 0
                          && close(s.pdf, e.pdf), "Invalid sample or inconsistent mixture PDF");
                    max_pdf_error = std::max(max_pdf_error, std::abs(s.pdf - e.pdf));
                    const double cosine = hit.normal.dot(s.direction);
                    if (cosine <= 0) {
                        check(s.pdf == 0 && close(s.weight, {}) && close(e.value, {}), "Below-surface sample");
                    } else {
                        ++valid;
                        check(s.pdf > 0 && close(s.weight, e.value * (cosine / e.pdf)),
                              "Sample weight must use the full BSDF and mixture PDF");
                    }
                }
                check(valid > 0, "No valid samples for this material/view");
            }
        }
    }
    std::cout << "Maximum sample/evaluation PDF difference=" << max_pdf_error << '\n';
}

void check_sampling_distribution(Material material, const Ray& ray, const HitRecord& hit) {
    // Count all draws, including rejected ones. For a normal view the reflected
    // direction is inside the 60-degree cone when the GGX half-angle is below 30 degrees.
    material.metallic = 0.5;
    material.roughness = 0.75;
    const double alpha = material.roughness * material.roughness;
    const double ggx_cone = 1 / (1 + 3 * alpha * alpha);
    // For a normal view, p_GGX(wi) = D(h)/4. At 60 degrees, (n.h)^2 = 3/4.
    const auto boundary = evaluate_bsdf(material, ray, hit, {std::sqrt(0.75), 0, 0.5});
    const double boundary_ggx_pdf = alpha * alpha / (4 * pi * std::pow(0.25 + 0.75 * alpha * alpha, 2));
    check(close(boundary.pdf, 0.25 * (0.5 / pi) + 0.75 * boundary_ggx_pdf), "Mixed PDF at 60 degrees");
    const double expected = 0.25 * 0.75 + 0.75 * ggx_cone;
    Sampler rng{17};
    int inside = 0;
    constexpr int count = 32768;
    for (int i = 0; i < count; ++i) {
        const auto sample = sample_bsdf(material, ray, hit, rng);
        if (sample.direction.z >= 0.5) ++inside;
    }
    const double observed = double(inside) / count;
    check(std::abs(observed - expected) < 0.012, "Direction frequencies disagree with mixture PDF");
    std::cout << "Cone probability: observed=" << observed << " expected=" << expected << '\n';
}
}

int main() {
    try {
        check_json();
        check_material_maps();
        check_uv_frames();
        Material material{Kind::principled, {0.8, 0.32, 0.12}};
        const Ray ray{{0, 0, 1}, {0, 0, -1}};
        HitRecord hit;
        hit.set(ray, 1, {0, 0, 1}, 0);
        check_endpoints(material, ray, hit);
        check_samples(material, hit);
        check_sampling_distribution(material, ray, hit);
        std::cout << "PASS JSON, metallic endpoints, roughness floor, sampling weights and PDFs\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
