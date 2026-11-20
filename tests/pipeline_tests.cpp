#include "render/path_integrator.h"
#include "io/assets.h"
#include "scene_init.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
void require(bool condition,const char* message) {
    if(!condition) throw std::runtime_error(message);
}

void check_obj_corner_normals() {
    Scene mesh;
    load_obj(mesh, "tests/scenes/corner_normals.obj", 0);
    const Ray ray{{0.5,0,1}, {0,0,-1}};
    HitRecord hits[4];
    require(mesh.objects.size() == 4, "Load all normal assignments");
    for (int i = 0; i < 4; ++i) {
        const auto& face = std::get<Triangle>(mesh.objects[i]);
        require(face.intersect(ray, 0, 2, hits[i]), "Hit the shared edge");
    }
    const Vec3 midpoint = Vec3{0,0.6,1.8}.normalized();
    require(hits[0].normal.dot(midpoint) > 1-1e-12 && hits[1].normal.dot(midpoint) > 1-1e-12,
            "Shared corner normals interpolate continuously across a fold");
    require(hits[0].outward_normal.z == 1 && hits[1].outward_normal.y > 0.7,
            "Both faces keep their own geometric normal");
    require(hits[2].normal.dot(hits[2].outward_normal) > 1-1e-12 && hits[2].normal.dot(midpoint) < 0.95,
            "Distinct corner indices preserve a hard edge at the same positions");
    require(hits[3].normal.dot(hits[3].outward_normal) > 1-1e-12,
            "A face with incomplete normal indices uses its geometric normal");
    HitRecord back;
    require(std::get<Triangle>(mesh.objects[0]).intersect({{0.5,0,-1},{0,0,1}}, 0, 2, back)
            && !back.front_face && back.normal.dot(midpoint) < -1+1e-12 && back.outward_normal.z == 1,
            "Back-face shading keeps geometric sidedness");
}

void check_rock_texture() {
    const World world = load_scene_file("scene/forest.json");
    require(world.scene.objects.size() == 8, "Rock mesh must have eight faces");
    const Material& stone = world.materials.at(0);
    require(stone.texture && stone.texture->type == Texture::Type::image, "Bind rock image to its material");
    Vec3 first_color{};
    bool have_variation = false;
    for (std::size_t i = 0; i < world.scene.objects.size(); ++i) {
        const auto& face = std::get<Triangle>(world.scene.objects[i]);
        const double uv_area = (face.tb.u - face.ta.u) * (face.tc.v - face.ta.v)
                             - (face.tc.u - face.ta.u) * (face.tb.v - face.ta.v);
        require(std::abs(uv_area) > 0, "Each rock face needs a nondegenerate UV triangle");
        const Vec3 center = (face.a + face.b + face.c) * (1.0 / 3);
        const Vec3 normal = (face.b - face.a).cross(face.c - face.a).normalized();
        HitRecord hit;
        require(face.intersect({center + normal, normal * -1}, 0, 2, hit), "Hit face center");
        require(hit.material_id == 0, "Rock face refers to stone");
        const Vec3 color = world.materials.at(hit.material_id).albedo(hit.uv);
        if (i == 0) first_color = color;
        if (color.x != first_color.x || color.y != first_color.y || color.z != first_color.z)
            have_variation = true;
    }
    require(have_variation, "Different face centers must sample different colors on the rock");
}

void check_motion_scene() {
    const World world = load_scene_file("scene/motion_blur.json");
    require(world.camera.shutter_open == 0 && world.camera.shutter_close == 1, "Load shutter interval");
    require(world.scene.objects.size() == 8, "Load all faces of the moving OBJ");
    for (const auto& object : world.scene.objects) {
        const Vec3 velocity = std::get<Triangle>(object).velocity;
        require(velocity.x == 1.6 && velocity.y == 0 && velocity.z == 0, "Every face inherits world velocity");
    }
    const Ray start{{-0.65,0.3,3}, {0,0,-1}, 0};
    HitRecord reference;
    require(world.scene.intersect(start, 0, 10, reference), "Hit initial rock");
    for (auto backend : {Acceleration::linear, Acceleration::bvh}) {
        const RenderScene scene(world, backend);
        for (double time : {0.0, 0.5, 1.0}) {
            Ray ray = start;
            ray.time = time;
            ray.origin.x += 1.6 * time;
            HitRecord hit;
            require(scene.intersect(ray, 0, 10, hit), "Hit translated OBJ throughout the shutter");
            require(std::abs(hit.t - reference.t) < 1e-12 && hit.time == time &&
                    std::abs(hit.uv.u - reference.uv.u) < 1e-12 && std::abs(hit.uv.v - reference.uv.v) < 1e-12,
                    "Translation preserves depth and UV at the corresponding surface point");
            require(scene.shadow_transmittance(ray, 10) == 0, "Moving OBJ casts a shadow at ray.time");
        }
        Ray vacated = start;
        vacated.time = 1;
        require(scene.shadow_transmittance(vacated, 10) == 1, "Moving OBJ vacates its initial position");
    }
}

void check_json_lens_and_alpha() {
    const Camera camera = load_scene_file("scene/depth_of_field.json").camera;
    require(camera.lens_radius == 0.35 && camera.focus_distance == 3.6, "Load lens and focus distance");
    const World world = load_scene_file("scene/alpha_shadow.json");
    const auto leaf_id = std::get<Triangle>(world.scene.objects.front()).material_id;
    const auto& leaf = world.materials.at(leaf_id);
    require(leaf.opacity_map && leaf.opacity == 1, "Load leaf coverage relative to the scene file");
    // The quad maps U to +X and V to +Z. Start just below the leaf and above the receiver.
    const RenderScene scene(world);
    double lowest = 1, highest = 0;
    for (int i = 0; i < leaf.opacity_map->width; ++i) {
        const UV uv{(i + 0.5) / leaf.opacity_map->width, 0.51};
        const Ray ray{{-1.5 + 2 * uv.u, 0.1, -1 + 2 * uv.v}, {0,1,0}};
        const double transmission = scene.shadow_transmittance(ray, 1);
        require(std::abs(transmission - (1 - leaf.alpha(uv))) < 1e-12, "Shadow follows loaded mask UVs");
        lowest = std::min(lowest, transmission);
        highest = std::max(highest, transmission);
    }
    require(lowest < 0.1 && highest > 0.9, "Mask has both leaf and uncovered parts");
}

int main() {
    try {
        check_obj_corner_normals();
        check_rock_texture();
        check_motion_scene();
        check_json_lens_and_alpha();
        HitRecord hit;
        Triangle triangle{{0,0,0},{1,0,0},{0,1,0},0};
        require(triangle.intersect({{0.25,0.5,1},{0,0,-1}},0,2,hit),"UV triangle hit");
        require(hit.uv.u==0.25 && hit.uv.v==0.5,"Barycentric UV");
        Sphere sphere{{0,0,0},1,0};
        require(sphere.intersect({{2,0,0},{-1,0,0}},0,3,hit) && hit.uv.u==0.5 && hit.uv.v==0.5,"Sphere UV");
        Texture texture;
        texture.load("assets/test_colors.ppm");
        require(std::abs(texture.value({0.25,0.25}).x-0.2158605)<1e-6,"sRGB decode and bottom origin");
        require(texture.value({1.25,-0.75}).x==texture.value({0.25,0.25}).x,"Repeat addressing");
        require(texture.value({0.25,0.75}).x==1,"Top row orientation");
        Scene mesh;
        Transform transform;
        transform.scale={2,3,4}; transform.translation={1,2,3};
        load_obj(mesh,"assets/rock.obj",4,transform);
        require(mesh.objects.size()==8,"OBJ face count and negative indices");
        auto first=std::get<Triangle>(mesh.objects[0]);
        require(first.a.x==-1 && first.a.y==2 && first.a.z==3,"OBJ transformed vertex");
        Vec3 n=transform.normal({1,1,0});
        require(std::abs(n.x/n.y-1.5)<1e-12,"Inverse transpose normal");
        Scene indices;
        load_obj(indices,"assets/index_cases.obj",0);
        require(indices.objects.size()==2,"OBJ slash forms and negative vt/vn");
        bool rejected=false;
        try { load_obj(indices,"assets/invalid_quad.obj",0); }
        catch(const std::exception&) { rejected=true; }
        require(rejected && indices.objects.size()==2,"Reject polygon transactionally");
        Material textured{Kind::diffuse,{1,1,1},1.5,std::make_shared<Texture>(texture)};
        hit.uv={0.25,0.25}; hit.normal={0,0,1};
        Sampler sampler{1};
        auto sampled=sample_bsdf(textured,{{0,0,1},{0,0,-1}},hit,sampler);
        require(sampled.weight.x==textured.albedo(hit.uv).x,"BSDF shares textured albedo");
        const auto evaluation = evaluate_bsdf(textured, {{0,0,1},{0,0,-1}}, hit, sampled.direction);
        require(evaluation.pdf == sampled.pdf, "continuous eval and sample PDF agree");
        require(std::abs(evaluation.value.x * pi - sampled.weight.x) < 1e-12, "textured Lambertian eval");
        require(evaluate_bsdf(textured, {}, hit, {0,0,-1}).pdf == 0, "back hemisphere has no density");
        require(evaluate_bsdf({Kind::mirror}, {}, hit, {0,0,1}).pdf == 0, "mirror has no continuous density");
        World world;
        world.materials={{Kind::diffuse,{1,1,1},1.5},{Kind::emissive,{2,2,2},1.5}};
        world.scene.objects.push_back(Sphere{{0,0,0},1,0});
        // Huge unregistered overhead emitter catches diffuse samples.
        world.scene.objects.push_back(Triangle{{-100,-100,3},{0,100,3},{100,-100,3},1});
        RenderScene prepared(world);
        Vec3 on{},off{};
        for(int i=0;i<100;++i) {
            Sampler a{static_cast<std::uint64_t>(i)},b=a;
            Ray ray{{0,0,2},{0,0,-1}};
            on=on+integrate(prepared,ray,a,3,SamplingMode::nee);
            off=off+integrate(prepared,ray,b,3,SamplingMode::bsdf);
        }
        require(on.x>100 && on.x==off.x,"Unregistered emission survives non-delta hit");
        world.add_area({{0,4,0},{1,0,0},{0,0,1},{5,4,3}});
        const auto& linked=std::get<Triangle>(world.scene.objects.back());
        require(linked.light_id==0 && world.materials[linked.material_id].color.x==5,"Area binding");
        std::cout<<"PASS UV, color space, addressing, OBJ, transform, unregistered emission\n";
    } catch(const std::exception& e) {
        std::cerr<<e.what()<<'\n'; return 1;
    }
}
