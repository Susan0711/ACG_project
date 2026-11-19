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
