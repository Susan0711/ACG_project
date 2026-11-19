#include "geometry.h"
#include <limits>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// Right-handed world: +Y up, camera looks along -Z. Pixel rows start at top.
struct Camera {
    Vec3 origin{0,0,0};
    Ray generate(double u, double v, double aspect) const {
        return {origin, Vec3{(2*u-1)*aspect, 1-2*v, -1}.normalized()};
    }
};
struct Image {
    int width, height;
    std::vector<Vec3> pixels;
    Image(int w, int h) : width(w), height(h), pixels(static_cast<std::size_t>(w)*h) {}
    Vec3& at(int x, int y) { return pixels[static_cast<std::size_t>(y)*width+x]; }
    void write(const std::string& path) const {
        if (std::filesystem::exists(path)) throw std::runtime_error("Output already exists: " + path);
        std::ofstream out(path);
        if (!out) throw std::runtime_error("Cannot open output: " + path);
        out << "P3\n" << width << ' ' << height << "\n255\n";
        const auto channel = [](double c) { return static_cast<int>(std::lround(255*std::clamp(c,0.0,1.0))); };
        for (Vec3 c : pixels) out << channel(c.x) << ' ' << channel(c.y) << ' ' << channel(c.z) << '\n';
        out.close();
        if (!out) throw std::runtime_error("Failed writing output: " + path);
    }
};
int dimension(const std::string& s) {
    int n = 0;
    const auto result = std::from_chars(s.data(), s.data()+s.size(), n);
    if (result.ec != std::errc{} || result.ptr != s.data()+s.size() || n < 1 || n > 8192)
        throw std::runtime_error("Dimensions must be integers in [1, 8192]");
    return n;
}
int main(int argc, char** argv) {
    try {
        int width = 640, height = 360;
        std::string scene_name = "single";
        std::string output = "normal.ppm";
        for (int i=1; i<argc; ++i) {
            const std::string option = argv[i];
            if (option == "--help") {
                std::cout << "normal_preview [--scene single|multi] [--width 640] [--height 360] [--output normal.ppm]\n";
                return 0;
            }
            if (option != "--width" && option != "--height" && option != "--output" && option != "--scene")
                throw std::runtime_error("Unknown option: " + option);
            if (++i == argc) throw std::runtime_error("Missing value: " + option);
            if (option == "--width") width = dimension(argv[i]);
            else if (option == "--height") height = dimension(argv[i]);
            else if (option == "--scene") scene_name = argv[i];
            else output = argv[i];
        }
        if (scene_name != "single" && scene_name != "multi") throw std::runtime_error("Unknown scene: " + scene_name);
        const Scene scene = preview_scene(scene_name == "multi");
        Image image(width,height);
        const Camera camera;
        for (int y=0; y<height; ++y) for (int x=0; x<width; ++x) {
            const Ray ray = camera.generate((x+0.5)/width,(y+0.5)/height,static_cast<double>(width)/height);
            HitRecord hit;
            Vec3 color{0.05,0.05,0.05};
            if (scene.intersect(ray, 0.000001, std::numeric_limits<double>::infinity(), hit)) {
                const Vec3 normal = hit.outward_normal;
                color = (normal+Vec3{1,1,1})*0.5;
            }
            image.at(x,y) = color;
        }
        image.write(output);
        std::cout << "Wrote " << width << 'x' << height << " PPM to " << output << '\n';
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}
