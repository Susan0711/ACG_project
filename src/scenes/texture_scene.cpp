#include "scenes/catalog.h"
World make_texture_scene(const std::string& resource_root) {
    World world;

    auto texture=std::make_shared<Texture>();
    texture->load(asset_path(resource_root,"wood.ppm"));
    world.materials.push_back({Kind::diffuse,{1,1,1},1.5,texture});
    world.scene.objects.push_back(Triangle{{-1,-1,0},{1,-1,0},{1,1,0},0,{0,0},{1,0},{1,1}});
    world.scene.objects.push_back(Triangle{{-1,-1,0},{1,1,0},{-1,1,0},0,{0,0},{1,1},{0,1}});
    world.environment={1,1,1};
    world.camera={{0,0,3},{0,0,0},40};
    return world;
}
