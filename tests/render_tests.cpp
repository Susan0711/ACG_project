#include "render.h"
#include <iostream>
#include <stdexcept>
int main() {
    try {
        for(const std::string name:{"cornell","motion","alpha-half"}) {
            World world=name=="cornell"?make_world(false):make_effect_scene(name);
            world.camera.shutter_close=name=="motion"?1:0;
            world.bvh.build(world.scene,0,world.camera.shutter_close);
            Film a(24,17),b(24,17);
            render(world,a,{8,8,1,17,true,true});
            render(world,b,{8,8,4,17,true,true});
            for(std::size_t i=0;i<a.pixels.size();++i)
                if(a.pixels[i].x!=b.pixels[i].x || a.pixels[i].y!=b.pixels[i].y || a.pixels[i].z!=b.pixels[i].z)
                    throw std::runtime_error(name+": thread count changed pixel");
        }
        World cabin=make_forest_v03(false,"assets");
        cabin.bvh.build(cabin.scene);
        HitRecord window;
        if(!cabin.intersect({{1.55,1.5,0},{0,0,-1}},0,10,window,true) ||
           cabin.materials[window.material_id].kind!=Kind::emissive || window.position.z>=-2.12)
            throw std::runtime_error("Cabin window is not an opening to an inset emitter");
        bool missing=false;
        try {asset_path("missing-resource-test","wood.ppm");}
        catch(const std::runtime_error& e) {missing=std::string(e.what()).find("--resource-root")!=std::string::npos;}
        if(!missing) throw std::runtime_error("Resource error lacks actionable path hint");
        World bad=make_world(false);
        bad.materials.clear();
        Film film(24,17);
        bool caught=false;
        try {render(bad,film,{2,4,4,1,true,true});}
        catch(const std::out_of_range&) {caught=true;}
        if(!caught) throw std::runtime_error("Worker exception was not propagated");
        std::cout<<"PASS deterministic tiles for Cornell/motion/alpha and worker exception propagation\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
