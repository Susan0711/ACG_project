#include "materials/material_reader.h"
#include "render/direct_lighting.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

bool close(Vec3 a, Vec3 b) {
    const Vec3 error = a - b;
    return error.dot(error) < 1e-22;
}

void check_parameters() {
    using Json = nlohmann::json;
    Json entry{{"type", "diffuse"}, {"color", {0.8, 0.4, 0.2}}};
    check(read_material(entry, ".").toon_bands == 0, "Omitted field selects Lambertian shading");
    for (int n : {0, 2, 4, 12}) {
        entry["toon_bands"] = n;
        check(read_material(entry, ".").toon_bands == n, "Read diffuse toon_bands");
    }
    for (const Json& invalid : {Json(1), Json(-1), Json(4.5), Json(true), Json("4"), Json(4294967298ULL)}) {
        entry["toon_bands"] = invalid;
        bool rejected = false;
        try { read_material(entry, "."); } catch (const std::runtime_error&) { rejected = true; }
        check(rejected, "Invalid band count accepted");
    }
}

void check_response(const HitRecord& hit, const Ray& ray, const Material& toon) {
    Material smooth = toon;
    smooth.toon_bands = 0;
    const auto check_cosine = [&](double c, double expected) {
        const Vec3 direction{std::sqrt(1 - c * c), c, 0};
        const auto eval = evaluate_bsdf(toon, ray, hit, direction);
        const auto baseline = evaluate_bsdf(smooth, ray, hit, direction);
        check(close(eval.value * (pi * c), toon.color * expected), "Unexpected quantized cosine");
        check(eval.pdf == baseline.pdf && close(baseline.value * (pi * c), smooth.color * std::max(0.0, c)),
              "Continuous response and sampling density");
    };
    for (double c : {-1.0, 0.0, 1e-15, 1.0 / 6 - 1e-8}) check_cosine(c, 0);
    check_cosine(1.0 / 6, 1.0 / 3);
    check_cosine(0.5 - 1e-8, 1.0 / 3);
    check_cosine(0.5, 2.0 / 3);
    check_cosine(5.0 / 6 - 1e-8, 2.0 / 3);
    check_cosine(5.0 / 6, 1);
    check_cosine(1, 1);

    Sampler rng{9};
    for (int i = 0; i < 512; ++i) {
        const auto s = sample_bsdf(toon, ray, hit, rng);
        const auto e = evaluate_bsdf(toon, ray, hit, s.direction);
        const double c = hit.normal.dot(s.direction);
        check(s.pdf > 0 && s.pdf == e.pdf && close(s.weight, e.value * (c / s.pdf)),
              "Sample weight and PDF agree with the evaluated BSDF");
    }
}

void check_direct(const HitRecord& hit, const Ray& ray, const Material& toon) {
    World world;
    world.lights.points.push_back({{1.6, 1.2, 0}, {4, 4, 4}});  // Distance 2, cosine 0.6.
    const Vec3 unoccluded = toon.color * ((2.0 / 3) / pi);
    Sampler rng{1};
    const auto direct = [&](SamplingMode mode) {
        return direct_lighting(RenderScene(world), ray, hit, toon, rng, mode);
    };
    check(close(direct(SamplingMode::nee), unoccluded), "Point lighting uses the band once");
    world.lights.points.front().position = world.lights.points.front().position * 2;
    check(close(direct(SamplingMode::mis), unoccluded * 0.25), "Doubling distance quarters the direct light");
    world.scene.objects.push_back(Triangle{{-5, 0.6, -5}, {0, 0.6, 5}, {5, 0.6, -5}, 0});
    world.materials.emplace_back().opacity = 0.5;
    check(close(direct(SamplingMode::mis), unoccluded * 0.125), "Visibility attenuates the selected band");
    world.materials.front().opacity = 1;
    check(close(direct(SamplingMode::nee), {}), "Opaque blocker removes direct light");
}
}

int main() {
    try {
        check_parameters();
        const Ray ray{{0, 1, 0}, {0, -1, 0}};
        HitRecord hit;
        hit.set(ray, 1, {0, 1, 0}, 0);
        Material toon;
        toon.toon_bands = 4;
        toon.color = {0.8, 0.4, 0.2};
        check_response(hit, ray, toon);
        check_direct(hit, ray, toon);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
