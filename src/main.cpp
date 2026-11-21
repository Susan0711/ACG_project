#include "render_scene.h"
#include "scene_init.h"
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
};

bool read_option(std::string_view text, int maximum, std::optional<int>& option) {
    if (option) return false;
    int value = 0;
    const char* end = text.data() + text.size();
    const auto parsed = std::from_chars(text.data(), end, value);
    if (parsed.ec != std::errc{} || parsed.ptr != end || value < 1 || value > maximum) return false;
    option = value;
    return true;
}

bool parse_arguments(int argc, char** argv, Options& options) {
    for (int i = 1; i < argc; i += 2) {
        if (i + 1 == argc) return false;
        const std::string_view option = argv[i], value = argv[i + 1];
        if (value.empty()) return false;
        if (option == "--scene" && options.scene_path.empty()) {
            options.scene_path = argv[i + 1];
        } else if (option == "--width") {
            if (!read_option(value, FilmSize::max_dimension, options.width)) return false;
        } else if (option == "--height") {
            if (!read_option(value, FilmSize::max_dimension, options.height)) return false;
        } else if (option == "--spp") {
            if (!read_option(value, std::numeric_limits<int>::max(), options.spp)) return false;
        } else if (option == "--threads") {
            if (!read_option(value, RenderSettings::max_threads, options.threads)) return false;
        } else {
            return false;
        }
    }
    return !options.scene_path.empty();
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
                      << RenderSettings::max_threads << ">]\n";
            return 1;
        }
        if (!path.has_parent_path()) path = std::filesystem::path("scene") / path;
        settings.spp = options.spp.value_or(settings.spp);
        settings.threads = options.threads.value_or(settings.threads);

        World world = load_scene_file(path);
        std::cout << "Loaded " << path.generic_u8string()
                  << " (primitives=" << world.scene.objects.size()
                  << ", materials=" << world.materials.size() << ")\n";

        const int width = options.width.value_or(world.film_size.width);
        const int height = options.height.value_or(world.film_size.height);
        RenderScene scene(std::move(world));
        Film film(width, height);
        std::cout << "Prepared RenderScene and Film (" << film.width << 'x' << film.height << ")\n";

        const auto start = std::chrono::steady_clock::now();
        render(scene, film, settings);
        const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
        std::cout << "Rendered " << film.pixels.size() << " pixels ("
                  << settings.spp << " samples/pixel, threads=" << settings.threads << ")\n";
        std::cout << "render_seconds=" << std::fixed << std::setprecision(6) << elapsed.count() << '\n';

        const char* output_path = "path.ppm";
        film.write(output_path);
        std::cout << "Wrote " << output_path << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error processing " << path.generic_u8string() << ": " << error.what() << '\n';
        return 1;
    }
}
