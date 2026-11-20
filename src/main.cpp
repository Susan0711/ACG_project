#include "render/render.h"
#include "scene_init.h"
#include <charconv>
#include <exception>
#include <iostream>
#include <string_view>
#include <utility>

namespace {
bool parse_arguments(int argc, char** argv, std::filesystem::path& scene_path, RenderSettings& settings) {
    bool have_threads = false;
    for (int i = 1; i < argc; i += 2) {
        if (i + 1 == argc) return false;
        const std::string_view option = argv[i], value = argv[i + 1];
        if (value.empty()) return false;
        if (option == "--scene" && scene_path.empty()) {
            scene_path = argv[i + 1];
        } else if (option == "--threads" && !have_threads) {
            const char* end = value.data() + value.size();
            const auto parsed = std::from_chars(value.data(), end, settings.threads);
            if (parsed.ec != std::errc{} || parsed.ptr != end ||
                settings.threads < 1 || settings.threads > RenderSettings::max_threads) return false;
            have_threads = true;
        } else {
            return false;
        }
    }
    return !scene_path.empty();
}
}

int main(int argc, char** argv) {
    std::filesystem::path path;
    try {
        RenderSettings settings;
        if (!parse_arguments(argc, argv, path, settings)) {
            std::cerr << "Usage: path_tracer --scene <file.json> [--threads <1-"
                      << RenderSettings::max_threads << ">]\n";
            return 1;
        }
        if (!path.has_parent_path()) path = std::filesystem::path("scene") / path;

        World world = load_scene_file(path);
        std::cout << "Loaded " << path.generic_u8string()
                  << " (primitives=" << world.scene.objects.size()
                  << ", materials=" << world.materials.size() << ")\n";

        RenderScene scene(std::move(world));
        Film film(256, 256);
        std::cout << "Prepared RenderScene and Film (" << film.width << 'x' << film.height << ")\n";

        render(scene, film, settings);
        std::cout << "Rendered " << film.pixels.size() << " pixels ("
                  << settings.spp << " samples/pixel, threads=" << settings.threads << ")\n";

        const char* output_path = "path.ppm";
        film.write(output_path);
        std::cout << "Wrote " << output_path << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error processing " << path.generic_u8string() << ": " << error.what() << '\n';
        return 1;
    }
}
