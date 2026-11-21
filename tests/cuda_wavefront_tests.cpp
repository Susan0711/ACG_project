#include "render/wavefront.h"
#include "scene_init.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void compare_frame(const char* file, int spp) {
    RenderScene scene(load_scene_file(file), Acceleration::bvh, BVHSplit::sah);
    CudaBVH gpu(scene.bvh(), scene.geometry());
    RenderSettings settings;
    settings.threads = 4;
    settings.spp = spp;
    settings.mode = SamplingMode::mis;
    Film cpu(67,37);
    render(scene, cpu, settings);
    Film first(cpu.width,cpu.height);
    for (int threshold : {0, CudaBVH::capacity/4}) {
        Film device(cpu.width, cpu.height);
        const auto queue = render_wavefront(scene, device, settings, gpu, threshold);
        if (spp == 16 && queue.refills < 2) throw std::runtime_error("Expected multiple camera refills");
        double error = 0, energy = 0;
        int large_differences = 0;
        for (std::size_t i=0; i<cpu.pixels.size(); ++i) {
            const Vec3 a = cpu.pixels[i], b = device.pixels[i], d = a-b;
            const double scale = std::max({1.0,std::abs(a.x),std::abs(a.y),std::abs(a.z)});
            if (!finite(b)) throw std::runtime_error("Nonfinite GPU image");
            if (threshold == 0) first.pixels[i] = b;
            else if (first.pixels[i].x != b.x || first.pixels[i].y != b.y || first.pixels[i].z != b.z)
                throw std::runtime_error("Refill threshold changed the FP32 image");
            energy += std::abs(a.x)+std::abs(a.y)+std::abs(a.z);
            error += std::abs(d.x)+std::abs(d.y)+std::abs(d.z);
            if (std::max({std::abs(d.x),std::abs(d.y),std::abs(d.z)}) > 0.01*scale) ++large_differences;
        }
        // A rounded hit can change an individual Monte Carlo path. Bound
        // both the linear-image error and the number of conspicuous outliers.
        if (error > 0.01*energy || large_differences > 0.005*cpu.pixels.size())
            throw std::runtime_error(std::string(file)+" FP32 image error="+std::to_string(error/energy)+
                                     " outlier pixels="+std::to_string(large_differences));
    }
}
}
int main() {
    try {
        compare_frame("scene/volume_alpha_shadow.json", 16);
        compare_frame("scene/prism_dispersion.json", 2);
        compare_frame("scene/motion_blur.json", 2);
        std::cout << "CPU/GPU linear frames agree at both refill thresholds\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
