#include "scene_init.h"
#include "render/render.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace {
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool close(double a, double b) { return std::abs(a - b) < 1e-10; }

void check_pyramid() {
    Texture tex;
    const UV red{0.25, 0.75};
    tex.load("assets/test_colors.ppm", true, Texture::Filter::mipmap);
    const double linear_gray = 0.21586050011389926;
    const double average = (1 + linear_gray) / 4;
    check(tex.last_level() == 1 && close(tex.value(red, -1).x, 1), "Magnification uses level zero");
    check(close(tex.value(red, 10).x, average), "Minification averages linear colors");
    check(close(tex.value(red, 0.5).x, (1 + average) / 2), "Interpolate adjacent mip levels");
    check(close(tex.value({0, 0.75}).x, 0.5) && close(tex.value({-1, 0.75}).x, 0.5),
          "Base filter wraps across the U seam");
    check(close(tex.value({0.25, 1}).x, (1 + linear_gray) / 2), "Base filter wraps across the V seam");

    tex.load("tests/scenes/mipmap_odd.ppm", true, Texture::Filter::mipmap);
    check(tex.last_level() == 2 && close(tex.value({0.75, 0.5}, 1).x, 2.0 / 15) &&
          close(tex.value({0.25, 0.5}, 1).x, 0) && close(tex.value({}, 2).x, 1.0 / 15),
          "Odd rectangular levels retain the last row and column and preserve mean energy");
    tex.load("assets/test_colors.ppm", true, Texture::Filter::bilinear);
    check(tex.last_level() == 0 && close(tex.value(red, 10).x, 1) && close(tex.value({0, 0.75}).x, 0.5),
          "Single-level comparison uses the same bilinear base filter");
    tex.load("assets/test_colors.ppm");
    check(close(tex.value({0, 0.75}).x, 1), "Default preserves the point-sampled baseline");
}

void check_pier(Acceleration backend, BVHSplit split) {
    World world = load_scene_file("scene/mipmap_pier.json");
    check(world.materials.front().texture->last_level() == 8, "Pier wood keeps levels 0 through 8");
    const Camera camera = world.camera;
    const double spread = 2 * std::tan(camera.fov * pi / 360) / world.film_size.height;
    const RenderScene scene(std::move(world), backend, split);
    const auto wood_hit = [&](const Ray& ray) {
        HitRecord hit;
        check(scene.intersect(ray, 0, 100, hit) && hit.outward_normal.y > 0.999, "Hit top of pier board");
        return hit;
    };
    const Vec3 target{0, 0.095, 0};
    // The 0.19-unit board spans V=0.5..0.766 on its top; V is the denser axis.
    const double v_texels_per_unit = 256 * (0.766 - 0.5) / 0.19;
    const Ray vertical{target + Vec3{0, 1, 0}, {0, -1, 0}, 0, {0, 2 / v_texels_per_unit}};
    const HitRecord normal_hit = wood_hit(vertical);
    check(close(normal_hit.albedo_lod, 1), "A two-texel footprint selects level one");
    const Vec3 oblique{0, -0.25, std::sqrt(15.0) / 4};
    check(close(wood_hit({target - oblique, oblique, 0, vertical.cone}).albedo_lod, 3),
          "Grazing projection expands the footprint along V by four");
    check(close(wood_hit({vertical.origin, vertical.direction * 2, 0, vertical.cone}).albedo_lod, 1),
          "Cone growth uses traveled world distance");
    const Material& boards = scene.material(normal_hit);
    Sampler rng{1};
    const auto bsdf = sample_bsdf(boards, vertical, normal_hit, rng);
    const auto direct = evaluate_bsdf(boards, vertical, normal_hit, bsdf.direction);
    const Vec3 expected = boards.texture->value(normal_hit.uv, 1);
    check(close(bsdf.weight.x, expected.x) && close(direct.value.x * pi, expected.x),
          "Direct lighting and BSDF sampling use the selected level");

    const auto camera_lod = [&](double z, double pixel_spread) {
        const Vec3 target{0, 0.095, z};
        const Vec3 direction = (target - camera.origin).normalized();
        const HitRecord hit = wood_hit({camera.origin, direction, 0, {0, pixel_spread}});
        const Vec3 error = hit.position - target;
        check(error.dot(error) < 1e-20, "Camera probe reaches the intended board top without occlusion");
        return hit.albedo_lod;
    };
    // Board centers lie on the shared diagonal of the two top triangles. At a
    // grazing angle, rounding can reject both triangles there and hit a side
    // of the next board instead. Probe visible interiors, away from this seam.
    constexpr double near_z = 0.22, far_z = -5.18;
    const double near_lod = camera_lod(near_z, spread), far_lod = camera_lod(far_z, spread);
    check(far_lod > near_lod + 2 && close(camera_lod(far_z, spread / 2), far_lod - 1),
          "Distant boards use coarser levels; doubling resolution restores one detail level");
    std::cout << "pier LOD at 480x320: near=" << near_lod << " far=" << far_lod << '\n';
}
}

int main() {
    try {
        check_pyramid();
        check_pier(Acceleration::linear, BVHSplit::median);
        check_pier(Acceleration::bvh, BVHSplit::median);
        check_pier(Acceleration::bvh, BVHSplit::sah);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
