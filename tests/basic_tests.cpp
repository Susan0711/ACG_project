#include "render_scene.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void check_subpixel_coverage() {
    // A vertical triangle edge bisects a single square pixel. Only x >= 0 emits.
    World world;
    world.camera = {{0,0,1}, {0,0,0}, 45};
    world.materials = {{Kind::emissive, {1,1,1}}};
    world.scene.objects.push_back(Triangle{{0,-10,0}, {10,-10,0}, {0,10,0}, 0});
    const RenderScene scene(world);
    Film one(1,1), many(1,1);
    render(scene, one, {1,1,1,19});
    render(scene, many, {8192,1,1,19});
    const double single = one.pixels.front().x, mean = many.pixels.front().x;
    require(single == 0 || single == 1, "Single ray must hit one side of the edge");
    require(std::abs(mean - 0.5) < 0.02, "Expected 50% subpixel coverage");
    std::cout << "Subpixel coverage: " << mean << " (expected 0.5)\n";
}

void check_roulette() {
    // Four parallel mirror patches reflect the ray in a zigzag, then it escapes.
    // The exact throughput is 0.8^4. RR first acts after the fourth reflection.
    World world;
    world.materials = {{Kind::mirror, {0.8,0.8,0.8}}};
    world.lights.background = {1,1,1};
    for (int bounce = 0; bounce < 4; ++bounce) {
        const double x = 2 * bounce + 1, z = bounce % 2 == 0 ? 1 : -1;
        world.scene.objects.push_back(Triangle{{x-0.5,-1,z}, {x+0.5,-1,z}, {x,1,z}, 0});
    }
    const RenderScene scene(world);
    const Ray ray{{0,0,0}, Vec3{1,0,1}.normalized()};
    Sampler rng{493};
    constexpr int paths = 20000;
    int survivors = 0;
    double sum = 0;
    for (int i = 0; i < paths; ++i) {
        const double value = integrate(scene, ray, rng, 4).x;
        if (value > 0) {
            ++survivors;
            require(std::abs(value - 1) < 1e-12, "Surviving RR path must be compensated");
        }
        sum += value;
    }
    require(survivors > 0 && survivors < paths, "RR must both terminate and retain paths");
    require(std::abs(sum / paths - std::pow(0.8,4)) < 0.012, "Four-bounce mean radiance");
    std::cout << "RR: " << survivors << "/" << paths << " survived, mean " << sum / paths
              << " (expected 0.4096)\n";
}

void check_scene_preparation() {
    World world;
    world.camera.shutter_open = -1;
    world.camera.shutter_close = 2;
    world.scene.objects.push_back(Sphere{{0,0,0}, 1, 0, {2,0,0}});
    RenderScene indexed(world, Acceleration::bvh), linear(world, Acceleration::linear);
    world.scene.objects.clear();
    for (double time : {-1.0, 0.5, 2.0, 10.0}) {
        const Ray ray{{2*time,0,3}, {0,0,-1}, time};
        HitRecord a, b;
        require(indexed.intersect(ray, 0, 5, a) && linear.intersect(ray, 0, 5, b),
                "Prepared geometry must cover the final shutter interval and own its data");
        require(a.t == 2 && a.t == b.t && a.time == time && b.time == time, "Backend equivalence");
    }
}
}

int main() {
    try {
        check_subpixel_coverage();
        check_roulette();
        check_scene_preparation();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
