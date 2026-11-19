#include "tracer.h"
#include <iostream>
#include <stdexcept>
void require(bool condition,const char* message) {
    if(!condition) throw std::runtime_error(message);
}
int main() {
    try {
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
        World world;
        world.materials={{Kind::diffuse,{1,1,1},1.5},{Kind::emissive,{2,2,2},1.5}};
        world.scene.objects.push_back(Sphere{{0,0,0},1,0});
        // Huge unregistered overhead emitter catches diffuse samples.
        world.scene.objects.push_back(Triangle{{-100,-100,3},{0,100,3},{100,-100,3},1});
        world.bvh.build(world.scene);
        Vec3 on{},off{};
        for(int i=0;i<100;++i) {
            Sampler a{static_cast<std::uint64_t>(i)},b=a;
            Ray ray{{0,0,2},{0,0,-1}};
            on=on+integrate(world,ray,a,3,true,true);
            off=off+integrate(world,ray,b,3,false,true);
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
