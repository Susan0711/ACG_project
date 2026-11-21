#include "acceleration/bvh.h"
#include "scene_init.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <utility>
#include <vector>

namespace {
constexpr int grid_size = 100;
constexpr int build_trials = 7;

std::vector<Ray> camera_rays(const World& world) {
    Sampler rng{42};
    std::vector<Ray> rays;
    rays.reserve(grid_size * grid_size);
    const double aspect = double(world.film_size.width) / world.film_size.height;
    for (int y = 0; y < grid_size; ++y) {
        for (int x = 0; x < grid_size; ++x) {
            const double u = (x + rng.next()) / grid_size;
            const double v = (y + rng.next()) / grid_size;
            rays.push_back(world.camera.sample(u, v, aspect, rng));
        }
    }
    return rays;
}

struct Measurement {
    BVHSplit split;
    const char* name;
    BVH tree{};
    std::array<double, build_trials> build_ms{};
    BVHTraversalStats stats{};
    std::size_t hits = 0, mismatches = 0;
    double max_t_error = 0;

    void build(const World& world, int trial) {
        BVH fresh;
        const auto start = std::chrono::steady_clock::now();
        fresh.build(world.scene, world.camera.shutter_open, world.camera.shutter_close, split);
        const auto end = std::chrono::steady_clock::now();
        if (trial >= 0) build_ms[trial] = std::chrono::duration<double, std::milli>(end - start).count();
        tree = std::move(fresh);
    }

    void compare(const Scene& scene, const Ray& ray, bool reference_hit, const HitRecord& reference) {
        HitRecord hit;
        const bool found = tree.intersect(scene, ray, 0, std::numeric_limits<double>::infinity(), hit, &stats);
        hits += found;
        if (found && reference_hit) max_t_error = std::max(max_t_error, std::abs(hit.t - reference.t));
        if (found != reference_hit || (found && (hit.object_index != reference.object_index ||
            hit.t != reference.t || hit.material_id != reference.material_id || hit.light_id != reference.light_id))) {
            ++mismatches;
        }
    }

    void print() {
        std::sort(build_ms.begin(), build_ms.end());
        std::cout << name << " hits=" << hits << " nearest_mismatches=" << mismatches
                  << " max_abs_t_error=" << max_t_error << " box_tests=" << stats.box_tests
                  << " primitive_tests=" << stats.primitive_tests
                  << " median_build_ms=" << std::fixed << std::setprecision(3) << build_ms[build_trials / 2]
                  << std::defaultfloat << std::setprecision(17) << '\n';
    }
};
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: bvh_compare <scene.json>\n";
        return 1;
    }
    try {
        const World world = load_scene_file(argv[1]);
        const auto rays = camera_rays(world);
        Measurement median{BVHSplit::median, "median"};
        Measurement sah{BVHSplit::sah, "sah"};
        for (int trial = -1; trial < build_trials; ++trial) {
            // One warm-up per strategy, then alternate the order within each pair.
            const bool median_first = trial % 2 == 0;
            (median_first ? median : sah).build(world, trial);
            (median_first ? sah : median).build(world, trial);
        }
        std::size_t reference_hits = 0;
        for (const Ray& ray : rays) {
            HitRecord reference;
            const bool found = world.scene.intersect(ray, 0, std::numeric_limits<double>::infinity(), reference);
            reference_hits += found;
            median.compare(world.scene, ray, found, reference);
            sah.compare(world.scene, ray, found, reference);
        }
        std::cout << "primitives=" << world.scene.objects.size() << " camera_rays=" << rays.size()
                  << " seed=42 linear_hits=" << reference_hits << " build_trials=" << build_trials << '\n';
        median.print();
        sah.print();
        return median.mismatches == 0 && sah.mismatches == 0 ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
