#include "tracer.h"
#include <limits>
#include <stdexcept>

void Camera::validate() const {
    Vec3 forward=target-origin;
    if(!finite(origin) || !finite(target) || forward.dot(forward)==0 ||
       forward.normalized().cross({0,1,0}).dot(forward.normalized().cross({0,1,0}))<1e-12)
        throw std::runtime_error("Camera direction must be nonzero and not parallel to up");
    if(!std::isfinite(fov) || fov<=0 || fov>=179 || !std::isfinite(focus_distance) || focus_distance<=0 ||
       !std::isfinite(lens_radius) || lens_radius<0 || !std::isfinite(shutter_open) ||
       !std::isfinite(shutter_close) || shutter_close<shutter_open)
        throw std::runtime_error("Invalid camera FOV, focus, lens radius or shutter interval");
}
Ray Camera::sample(double u,double v,double aspect,Sampler& rng) const {
    Ray ray=generate(u,v,aspect);
    if(lens_radius>0) {
        Vec3 forward=(target-origin).normalized();
        Vec3 right=forward.cross({0,1,0}).normalized(),up=right.cross(forward);
        Vec3 focal=origin+ray.direction*(focus_distance/ray.direction.dot(forward));
        double radius=lens_radius*std::sqrt(rng.next());
        double angle=2*pi*rng.next();
        ray.origin=origin+right*(radius*std::cos(angle))+up*(radius*std::sin(angle));
        ray.direction=(focal-ray.origin).normalized();
    }
    ray.time=shutter_open;
    if(shutter_close>shutter_open) ray.time+=(shutter_close-shutter_open)*rng.next();
    return ray;
}
double Material::alpha(UV uv) const {
    if(kind==Kind::emissive && (opacity!=1 || opacity_map))
        throw std::runtime_error("Emissive alpha is unsupported");
    double value=opacity*(opacity_map?opacity_map->value(uv).x:1);
    if(!std::isfinite(value) || value<0 || value>1)
        throw std::runtime_error("Opacity must be linear coverage in [0,1]");
    return value;
}
double shadow_transmittance(const World& world,Ray ray,double distance,bool accelerated) {
    double transmission=1,minimum=0;
    for(int crossing=0;crossing<4096;++crossing) {
        HitRecord hit;
        if(!world.intersect(ray,minimum,distance,hit,accelerated)) return transmission;
        transmission*=1-world.materials.at(static_cast<std::size_t>(hit.material_id)).alpha(hit.uv);
        if(transmission==0) return 0;
        minimum=std::nextafter(hit.t,std::numeric_limits<double>::infinity());
    }
    throw std::runtime_error("Shadow crossing limit exceeded");
}
World make_effect_scene(const std::string& name,const std::string& root) {
    World w;
    w.environment={0.08,0.08,0.08};
    w.camera={{0,1,6},{0,1,0},38};
    w.camera.focus_distance=6;
    w.materials={{Kind::diffuse,{0.75,0.15,0.08},1.5},
                 {Kind::diffuse,{0.12,0.65,0.18},1.5},
                 {Kind::diffuse,{0.12,0.25,0.8},1.5},
                 {Kind::diffuse,{0.7,0.7,0.7},1.5}};
    auto quad=[&](Vec3 a,Vec3 u,Vec3 v,int material) {
        w.scene.objects.push_back(Triangle{a,a+u,a+u+v,material,{0,0},{1,0},{1,1}});
        w.scene.objects.push_back(Triangle{a,a+u+v,a+v,material,{0,0},{1,1},{0,1}});
    };
    quad({-6,0,4},{12,0,0},{0,0,-12},3);
    w.points.push_back({{-3,6,4},{65,65,65}});
    if(name=="dof") {
        w.scene.objects.push_back(Sphere{{-1,0.65,2},0.65,0});
        w.scene.objects.push_back(Sphere{{0,0.65,0},0.65,1});
        w.scene.objects.push_back(Sphere{{1.5,0.65,-2},0.65,2});
    } else if(name=="motion") {
        w.scene.objects.push_back(Sphere{{-1,0.7,0},0.7,0,{2,0,0}});
    } else {
        w.camera={{4,4,6},{0,0.3,0},42};
        w.points={{{-2,5,1},{60,60,60}}};
        Material sheet{Kind::diffuse,{0.12,0.4,0.13},1.5};
        if(name=="alpha-pattern") {
            sheet.opacity_map=std::make_shared<Texture>();
            sheet.opacity_map->load(asset_path(root,"leaf-opacity.ppm"),false);
        } else if(name=="alpha-zero") sheet.opacity=0;
        else if(name=="alpha-half" || name=="alpha-double") sheet.opacity=0.5;
        w.materials.push_back(sheet);
        quad({-1.3,1,-1.3},{2.6,0,0},{0,0,2.6},4);
        if(name=="alpha-double") quad({-1.3,1.2,-1.3},{2.6,0,0},{0,0,2.6},4);
    }
    return w;
}
