#include "tracer.h"
World make_world(bool point,bool build) {
    World w;
    w.materials={{Kind::diffuse,{0.73,0.73,0.73},1.5},{Kind::diffuse,{0.65,0.06,0.04},1.5},{Kind::diffuse,{0.05,0.55,0.08},1.5},{Kind::mirror,{0.95,0.95,0.95},1.5},{Kind::glass,{1,1,1},1.5},{Kind::emissive,{12,12,12},1.5}};
    auto quad=[&](Vec3 a,Vec3 u,Vec3 v,int m){w.scene.objects.push_back(Triangle{a,a+u,a+u+v,m});
        w.scene.objects.push_back(Triangle{a,a+u+v,a+v,m});
    };
    quad({-1,-1,1},{2,0,0},{0,0,-3},0);
    quad({-1,1,-2},{2,0,0},{0,0,3},0);
    quad({-1,-1,-2},{2,0,0},{0,2,0},0);
    quad({-1,-1,1},{0,0,-3},{0,2,0},1);
    quad({1,-1,-2},{0,0,3},{0,2,0},2);
    w.scene.objects.push_back(Sphere{{-0.48,-0.57,-0.85},0.43,3});
    w.scene.objects.push_back(Sphere{{0.43,-0.55,-0.35},0.45,4});
    w.scene.objects.push_back(Sphere{{0,-0.78,-1.55},0.22,0});
    if(point)w.points.push_back({{0,0.65,0},{8,8,8}});
    else {AreaLight light{{-0.35,0.99,-1.15},{0.7,0,0},{0,0,0.7},{12,12,12}};
        w.add_area(light);
    }
    if(build) w.bvh.build(w.scene);
    return w;
}
void World::add_area(AreaLight light) {
    const int id = static_cast<int>(areas.size());
    const int material = static_cast<int>(materials.size());
    materials.push_back({Kind::emissive, light.radiance, 1.5});
    Triangle first{light.origin, light.origin+light.u, light.origin+light.u+light.v, material};
    Triangle second{light.origin, light.origin+light.u+light.v, light.origin+light.v, material};
    first.light_id = second.light_id = id;
    scene.objects.push_back(first);
    scene.objects.push_back(second);
    areas.push_back(light);
}
