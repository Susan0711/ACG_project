#include "materials/material_reader.h"
#include "render/path_integrator.h"
#include "scene_init.h"
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace {
constexpr std::array<Wavelength, 3> bands{Wavelength::red, Wavelength::green, Wavelength::blue};
constexpr std::array<double, 3> indices{1.4824431512543401, 1.52, 1.5852994592388532};
const Ray incident{{0, 0.07, 4}, {0, 0, -1}};

void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

void check_parameters() {
    using Json = nlohmann::json;
    Json entry{{"type", "glass"}, {"ior", 1.52}};
    for (double b : {0.0, 0.04}) {
        if (b > 0) entry["cauchy_b"] = b;
        const Material material = read_material(entry, ".");
        for (std::size_t i = 0; i < bands.size(); ++i) {
            const double expected = b == 0 ? 1.52 : indices[i];
            check(std::abs(material.ior_at(bands[i]) - expected) < 1e-12, "Wrong wavelength-dependent IOR");
        }
    }
    for (const Json& invalid : {Json(-0.1), Json("0.04"), Json(2)}) {
        entry["cauchy_b"] = invalid;
        bool rejected = false;
        try { read_material(entry, "."); } catch (const std::runtime_error&) { rejected = true; }
        check(rejected, "Invalid Cauchy coefficient accepted");
    }
}

void check_prism_angles(const RenderScene& scene) {
    for (std::size_t i = 0; i < bands.size(); ++i) {
        HitRecord entry, exit;
        Sampler rng{0};  // Both Fresnel draws transmit at these angles.
        check(scene.intersect(incident, 0, 10, entry) && entry.front_face, "Enter prism through the front face");
        const auto first = sample_bsdf(scene.material(entry), incident, entry, rng, bands[i]);
        const Ray inside = spawn(entry, first.direction);
        check(first.delta && first.direction.z == -1 && scene.intersect(inside, 0, 10, exit) &&
              !exit.front_face && exit.material_id == entry.material_id, "Reach the inclined glass-air face");
        const auto second = sample_bsdf(scene.material(exit), inside, exit, rng, bands[i]);
        const double wedge = pi / 6;
        const double measured = std::atan2(second.direction.x, -second.direction.z);
        const double expected = std::asin(indices[i] * std::sin(wedge)) - wedge;
        check(std::abs(measured - expected) < 1e-12 && std::abs(first.weight.x * second.weight.x - 1) < 1e-12,
              "Two refractions must obey Snell's law and cancel the radiance eta factors");
        std::cout << static_cast<int>(bands[i]) << " nm: n=" << scene.material(entry).ior_at(bands[i])
                  << " deviation=" << measured * 180 / pi << " deg\n";
    }
}

void check_critical_angle(const Material& glass) {
    // At this internal angle blue undergoes TIR while green transmits.
    const double sine = 0.645;
    const Ray ray{{}, {sine, 0, std::sqrt(1 - sine * sine)}};
    HitRecord hit;
    hit.set(ray, 1, {0, 0, 1}, 0);
    Sampler green_rng{0}, blue_rng{0};
    const auto green = sample_bsdf(glass, ray, hit, green_rng, Wavelength::green);
    const auto blue = sample_bsdf(glass, ray, hit, blue_rng, Wavelength::blue);
    check(green.direction.z > 0 && green.pdf < 1 && blue.direction.z < 0 && blue.pdf == 1,
          "TIR and Fresnel probability must use the current wavelength");
}

void check_oblique_entry(const Material& glass) {
    const Ray ray{{0, 0, 1}, {0.6, 0, -0.8}};
    HitRecord hit;
    hit.set(ray, 1, {0, 0, 1}, 0);
    for (std::size_t i = 0; i < bands.size(); ++i) {
        Sampler rng{0};
        const auto sample = sample_bsdf(glass, ray, hit, rng, bands[i]);
        check(sample.direction.z < 0 && std::abs(sample.direction.x - 0.6 / indices[i]) < 1e-12 &&
              std::abs(sample.pdf - (1 - fresnel(0.8, 1, indices[i]))) < 1e-12,
              "Oblique entry must use the band for both Snell and Fresnel");
    }
}

void check_rgb(const World& world, const RenderScene& dispersive) {
    check(dispersive.uses_dispersion(), "Enable three paths for dispersive glass");
    for (const auto mode : {SamplingMode::bsdf, SamplingMode::nee, SamplingMode::mis}) {
        Sampler rng{0};
        const Vec3 rgb = integrate(dispersive, incident, rng, 8, mode);
        check(rgb.y > 2 && rgb.x < 0.2 && rgb.z < 0.2, "Only green reaches the white slit on the central ray");
        const std::array<double, 3> measured{rgb.x, rgb.y, rgb.z};
        for (std::size_t i = 0; i < bands.size(); ++i) {
            World reference = world;
            reference.materials.front().ior = indices[i];
            reference.materials.front().cauchy_b = 0;
            const RenderScene ordinary(std::move(reference));
            check(!ordinary.uses_dispersion(), "Zero dispersion keeps a single RGB path");
            Sampler reference_rng{0};
            const Vec3 neutral = integrate(ordinary, incident, reference_rng, 8, mode);
            check(neutral.x == neutral.y && neutral.y == neutral.z && std::abs(measured[i] - neutral.x) < 1e-12,
                  "Every band must match its ordinary-glass reference without brightness scaling");
        }
    }
}
}

int main() {
    try {
        check_parameters();
        const World world = load_scene_file("scene/prism_dispersion.json");
        const RenderScene scene(world);
        std::cout << std::setprecision(8);
        check_prism_angles(scene);
        check_oblique_entry(world.materials.front());
        check_critical_angle(world.materials.front());
        check_rgb(world, scene);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
