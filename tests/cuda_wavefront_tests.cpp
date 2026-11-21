#include "render/wavefront_cuda.h"
#include "scene_init.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void check_shadow_texel_boundary() {
    auto texture = std::make_shared<Texture>();
    texture->type = Texture::Type::image;
    texture->width = 448;
    texture->height = 1;
    texture->pixels.resize(448, Vec3{1,1,1});
    texture->pixels[391] = {0.5,0.5,0.5};
    std::vector<Material> materials(1);
    materials[0].opacity_map = texture;
    Scene scene{{Triangle{{0,0,1},{1,0,1},{0,1,1},0,{0.99625,0},{0.99625,0},{0.75375,0}}}};
    BVH tree;
    tree.build(scene,0,1,BVHSplit::sah);
    CudaBVH gpu(tree,scene,materials);
    // The two points straddle texels 391 and 392 in a partial final block.
    std::vector<RayQuery> rays(1025,{{0.036403682f,0.500000238f,0},{0,0,1},0,0,2});
    for (size_t i=1; i<rays.size(); i+=2) rays[i].origin.y = 0.499999762f;
    std::vector<float> transmission(rays.size());
    gpu.intersect_batch(rays.data(),nullptr,0,transmission.data(),int(rays.size()));
    for (size_t i=0; i<rays.size(); ++i)
        if (transmission[i] != (i%2 ? 0 : 0.5f)) throw std::runtime_error("Wrong nearest opacity texel");
}
void check_ordered_shadow_layers() {
    std::vector<Material> materials(2);
    materials[0].opacity = 0.125;
    Scene scene;
    for (int z=20; z>0; --z)
        scene.objects.push_back(Triangle{{0,0,double(z)},{1,0,double(z)},{0,1,double(z)},0});
    // Coincident opaque triangles lose the tie to their transparent counterparts.
    for (int z : {2,9,18})
        scene.objects.push_back(Triangle{{0,0,double(z)},{1,0,double(z)},{0,1,double(z)},1});
    BVH tree;
    tree.build(scene,0,1,BVHSplit::sah);
    CudaBVH gpu(tree,scene,materials);
    const RayQuery q{{0.25,0.25,0},{0,0,1},0,0,20};
    float result, reference=1;
    gpu.intersect_batch(&q,nullptr,0,&result,1);
    for (int i=0; i<20; ++i) reference *= 0.875f;
    if (result != reference) throw std::runtime_error("Shadow layer order or same-distance tie changed");
}
void check_shadow_segments(Texture::Filter filter) {
    auto map = std::make_shared<Texture>();
    map->load("assets/test_colors.ppm", false, filter);
    auto procedural = std::make_shared<Texture>();
    procedural->type = Texture::Type::procedural;
    std::vector<Material> materials(4);
    materials[0].opacity = 0.5;
    materials[0].opacity_map = map;
    materials[1].opacity = 0.25;
    materials[2].opacity_map = procedural;
    Triangle leaf{{0,0,4},{1,0,4},{0,1,4},0,{-2,2},{-1,2},{-2,3}};
    const float second = std::nextafter(4.f,5.f);
    Scene scene{{leaf, Triangle{{0,0,second},{1,0,second},{0,1,second},1},
                 Sphere{{1.25,0.3,7},1,2,{-2,0,0}},
                 Triangle{{0,0,10},{1,0,10},{0,1,10},3}}};
    BVH tree;
    tree.build(scene,0,1,BVHSplit::sah);
    CudaBVH gpu(tree,scene,materials);
    std::vector<RayQuery> rays;
    for (double maximum : {3.,4.,double(second),7.,9.,10.})
        rays.push_back({{0.25,0.3,0},{0,0,1},0.5,0,maximum});
    rays.push_back({{9,9,0},{0,0,1},0.5,0,10});
    rays.insert(rays.begin(),rays.back()); // A closest-hit miss followed by the shadow queue.
    GeometryHit main_hit;
    std::vector<float> results(rays.size()-1);
    gpu.intersect_batch(rays.data(),&main_hit,1,results.data(),int(results.size()));
    if (main_hit.object >= 0) throw std::runtime_error("Mixed batch closest hit");
    for (size_t i=0; i<results.size(); ++i) {
        auto q = rays[i+1];
        double reference = 1;
        for (int layer=0; reference != 0; ++layer) {
            if (layer > 6) throw std::runtime_error("C1 shadow did not advance");
            GeometryHit h;
            gpu.intersect(&q,&h,1);
            if (h.object < 0) break;
            HitRecord surface;
            std::visit([&](const auto& shape){ shape.finish_hit({q.origin,q.direction,q.time},h.parameters,surface); },
                       scene.objects.at(h.object));
            reference *= 1-materials.at(surface.material_id).alpha(surface.uv);
            q.minimum = std::nextafter(surface.t,std::numeric_limits<double>::infinity());
        }
        if (!std::isfinite(results[i]) || std::abs(results[i]-reference)>2e-6 ||
            (results[i]==0) != (reference==0))
            throw std::runtime_error("GPU shadow segment differs at index "+std::to_string(i));
    }
}
void compare_frame(const char* file, int spp, SamplingMode mode=SamplingMode::mis, bool extra_lights=false) {
    auto world=load_scene_file(file);
    if (extra_lights) {
        for (int i=0;i<6;++i) world.lights.points.push_back({{double(i)-3,4,2},{1,2,1}});
    }
    RenderScene scene(std::move(world), Acceleration::bvh, BVHSplit::sah);
    CudaBVH gpu(scene.bvh(), scene.geometry(), scene.materials());
    RenderSettings settings;
    settings.threads = 4;
    settings.spp = spp;
    settings.mode = mode;
    Film cpu(67,37);
    render(scene, cpu, settings);
    CudaWavefront wavefront(scene,gpu);
    Film first(cpu.width,cpu.height);
    for (int threshold : {0, CudaBVH::capacity/4}) {
        Film device(cpu.width, cpu.height);
        const auto queue = wavefront.render(device,settings,threshold);
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
        check_shadow_texel_boundary();
        check_ordered_shadow_layers();
        for (auto filter : {Texture::Filter::nearest, Texture::Filter::bilinear, Texture::Filter::mipmap})
            check_shadow_segments(filter);
        compare_frame("scene/volume_alpha_shadow.json", 16);
        compare_frame("scene/prism_dispersion.json", 2);
        compare_frame("scene/motion_blur.json", 2);
        compare_frame("scene/volume_alpha_shadow.json", 3, SamplingMode::nee, true);
        compare_frame("scene/mis_sampling.json", 4, SamplingMode::bsdf);
        compare_frame("scene/coated_material.json", 4);
        compare_frame("scene/height_fog.json", 4);
        compare_frame("scene/principled_maps.json", 4);
        compare_frame("scene/hdr_environment.json", 4);
        compare_frame("scene/mipmap_pier.json", 4);
        compare_frame("scene/toon_shading.json", 4);
        compare_frame("scene/depth_of_field.json", 4);
        std::cout << "CPU/GPU linear frames agree at both refill thresholds\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
