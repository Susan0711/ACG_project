#include "render_scene.h"
#include "scene_init.h"
#include "render/wavefront.h"
#include <charconv>
#include <chrono>
#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>

namespace {
struct Options {
    std::filesystem::path scene_path;
    std::optional<int> width, height, spp, threads;
    std::optional<bool> gpu;
    std::optional<int> refill_threshold;
    std::optional<BVHSplit> bvh;
    std::optional<SamplingMode> mode;
};

bool read_option(std::string_view text, int maximum, std::optional<int>& option, int minimum = 1) {
    if (option) return false;
    int value = 0;
    const char* end = text.data() + text.size();
    const auto parsed = std::from_chars(text.data(), end, value);
    if (parsed.ec != std::errc{} || parsed.ptr != end || value < minimum || value > maximum) return false;
    option = value;
    return true;
}

bool parse_arguments(int argc, char** argv, Options& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        const auto equals = arg.find('=');
        const auto option = arg.substr(0, equals);
        if (equals == std::string_view::npos && i + 1 == argc) return false;
        const auto value = equals == std::string_view::npos ? std::string_view(argv[++i]) : arg.substr(equals+1);
        if (value.empty()) return false;
        if (option == "--scene" && options.scene_path.empty()) {
            options.scene_path = std::filesystem::u8path(value);
        } else if (option == "--width") {
            if (!read_option(value, FilmSize::max_dimension, options.width)) return false;
        } else if (option == "--height") {
            if (!read_option(value, FilmSize::max_dimension, options.height)) return false;
        } else if (option == "--spp") {
            if (!read_option(value, std::numeric_limits<int>::max(), options.spp)) return false;
        } else if (option == "--threads") {
            if (!read_option(value, RenderSettings::max_threads, options.threads)) return false;
        } else if (option == "--backend" && !options.gpu.has_value()) {
            if (value == "gpu") options.gpu = true;
            else if (value == "cpu") options.gpu = false;
            else return false;
        } else if (option == "--gpu-refill-threshold") {
            if (!read_option(value, CudaBVH::capacity, options.refill_threshold, 0)) return false;
        } else if (option == "--bvh" && !options.bvh) {
            if (value == "median") options.bvh = BVHSplit::median;
            else if (value == "sah") options.bvh = BVHSplit::sah;
            else return false;
        } else if (option == "--mode" && !options.mode) {
            if (value == "bsdf") options.mode = SamplingMode::bsdf;
            else if (value == "nee") options.mode = SamplingMode::nee;
            else if (value == "mis") options.mode = SamplingMode::mis;
            else return false;
        } else {
            return false;
        }
    }
    return !options.scene_path.empty() && (!options.refill_threshold || options.gpu.value_or(false));
}
}

int main(int argc, char** argv) {
    Options options;
    auto& path = options.scene_path;
    try {
        RenderSettings settings;
        if (!parse_arguments(argc, argv, options)) {
            std::cerr << "Usage: path_tracer --scene <file.json> [--width <1-" << FilmSize::max_dimension
                      << ">] [--height <1-" << FilmSize::max_dimension << ">] [--spp <positive integer>] [--threads <1-"
                      << RenderSettings::max_threads << ">] [--backend cpu|gpu] [--gpu-refill-threshold 0-32768] [--bvh sah|median] [--mode bsdf|nee|mis]\n";
            return 1;
        }
        if (!path.has_parent_path()) path = std::filesystem::path("scene") / path;
        settings.spp = options.spp.value_or(settings.spp);
        settings.threads = options.threads.value_or(settings.threads);
        settings.mode = options.mode.value_or(settings.mode);

#ifndef ACG_CUDA
        if (options.gpu.value_or(false)) throw std::runtime_error("GPU backend requires a build with -DACG_CUDA=ON");
#endif
        World world = load_scene_file(path);
        std::cout << "Loaded " << path.generic_u8string()
                  << " (primitives=" << world.scene.objects.size()
                  << ", materials=" << world.materials.size() << ")\n";

        const int width = options.width.value_or(world.film_size.width);
        const int height = options.height.value_or(world.film_size.height);
        RenderScene scene(std::move(world), Acceleration::bvh, options.bvh.value_or(BVHSplit::sah));
        Film film(width, height);
        std::cout << "Prepared RenderScene and Film (" << film.width << 'x' << film.height << ")\n";

#ifdef ACG_CUDA
        std::unique_ptr<CudaBVH> gpu;
        if (options.gpu.value_or(false)) {
            const auto upload = std::chrono::steady_clock::now();
            gpu = std::make_unique<CudaBVH>(scene.bvh(), scene.geometry(), scene.materials());
            std::cout << "gpu_prepare_seconds=" << std::chrono::duration<double>(std::chrono::steady_clock::now()-upload).count() << '\n';
        }
        QueueStats queue;
        const int threshold = options.refill_threshold.value_or(CudaBVH::capacity/4);
#endif
        const auto start = std::chrono::steady_clock::now();
#ifdef ACG_CUDA
        if (gpu) queue = render_wavefront(scene, film, settings, *gpu, threshold);
        else
#endif
            render(scene, film, settings);
        const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
        std::cout << "Rendered " << film.pixels.size() << " pixels ("
                  << settings.spp << " samples/pixel, threads=" << settings.threads << ")\n";
        std::cout << "render_seconds=" << std::fixed << std::setprecision(6) << elapsed.count() << '\n';

#ifdef ACG_CUDA
        if (gpu) {
            const auto timing = gpu->stats();
            std::cout << "gpu_refill_threshold=" << threshold << " capacity=" << CudaBVH::capacity
                      << " peak_active=" << queue.peak_paths << " refills=" << queue.refills
                      << " queries=" << timing.queries << " batches=" << timing.batches
                      << " kernel_ms=" << timing.kernel_ms << " batch_ms=" << timing.batch_ms << '\n';
        }
#endif
        const char* output_path = "path.ppm";
        film.write(output_path);
        std::cout << "Wrote " << output_path << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error processing " << path.generic_u8string() << ": " << error.what() << '\n';
        return 1;
    }
}
