#include "render_scene.h"
#include "scene_init.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
constexpr int comparison_spp = 32;
constexpr int reference_half_spp = 2048;
constexpr std::array<std::uint64_t, 2> reference_seeds{901, 902};

struct Estimate {
    SamplingMode mode;
    std::string name;
    Film image;
    std::array<double, 3> seconds{};
};

double render_seconds(const RenderScene& scene, Film& image, const RenderSettings& settings) {
    const auto start = std::chrono::steady_clock::now();
    render(scene, image, settings);
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

double rgb_rmse(const Film& a, const Film& b) {
    double sum = 0;
    for (std::size_t i = 0; i < a.pixels.size(); ++i) {
        const Vec3 error = a.pixels[i] - b.pixels[i];
        sum += error.dot(error);
    }
    return std::sqrt(sum / (3.0 * a.pixels.size()));
}

void save_image(const Film& image, const std::string& name) {
    image.write(name + ".ppm");
    image.write_linear(name + ".pfm");
}
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: mis_compare <scene.json> <new-output-directory>\n";
        return 1;
    }
    try {
        World world = load_scene_file(argv[1]);
        const int w = world.film_size.width, h = world.film_size.height;
        const RenderScene scene(std::move(world), Acceleration::bvh, BVHSplit::sah);
        const std::filesystem::path output(argv[2]);
        if (!std::filesystem::create_directories(output))
            throw std::runtime_error("Choose a new output directory: " + output.u8string());
        std::filesystem::current_path(output);

        RenderSettings settings;
        settings.spp = comparison_spp;
        settings.seed = 1;
        settings.threads = 12;
        std::array<Estimate, 3> estimates{{
            {SamplingMode::bsdf, "bsdf", Film(w,h)},
            {SamplingMode::nee,  "nee",  Film(w,h)},
            {SamplingMode::mis,  "mis",  Film(w,h)}
        }};
        std::cout << std::setprecision(12) << "width=" << w << " height=" << h
                  << " spp=" << settings.spp << " seed=" << settings.seed << " depth=" << settings.depth
                  << " threads=" << settings.threads << " bvh=sah" << std::endl;

        // Each mode runs first, second, and third once. Repetitions time the same samples.
        for (std::size_t trial = 0; trial < estimates.size(); ++trial) {
            for (std::size_t offset = 0; offset < estimates.size(); ++offset) {
                auto& estimate = estimates[(trial + offset) % estimates.size()];
                settings.mode = estimate.mode;
                estimate.seconds[trial] = render_seconds(scene, estimate.image, settings);
                std::cout << "trial=" << trial + 1 << " mode=" << estimate.name
                          << " render_seconds=" << estimate.seconds[trial] << std::endl;
            }
        }
        for (const auto& estimate : estimates) save_image(estimate.image, estimate.name);

        settings.mode = SamplingMode::mis;
        settings.spp = reference_half_spp;
        std::array<Film, 2> halves{Film(w,h), Film(w,h)};
        for (std::size_t i = 0; i < halves.size(); ++i) {
            settings.seed = reference_seeds[i];
            const double seconds = render_seconds(scene, halves[i], settings);
            std::cout << "reference_seed=" << settings.seed << " spp=" << settings.spp
                      << " render_seconds=" << seconds << std::endl;
        }
        const double reference_noise = rgb_rmse(halves[0], halves[1]) * 0.5;
        Film& reference = halves[0];
        for (std::size_t i = 0; i < reference.pixels.size(); ++i)
            reference.pixels[i] = (reference.pixels[i] + halves[1].pixels[i]) * 0.5;
        save_image(reference, "reference");
        std::cout << "reference_spp=" << 2 * reference_half_spp
                  << " estimated_reference_rmse=" << reference_noise << '\n';
        for (auto& estimate : estimates) {
            std::sort(estimate.seconds.begin(), estimate.seconds.end());
            std::cout << "mode=" << estimate.name << " rgb_rmse=" << rgb_rmse(estimate.image, reference)
                      << " median_render_seconds=" << estimate.seconds[1] << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
