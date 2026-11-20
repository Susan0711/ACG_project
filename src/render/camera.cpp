#include "render/camera.h"
#include <stdexcept>

Ray Camera::generate(double u,double v,double aspect) const {
    validate();
    if(!std::isfinite(aspect) || aspect<=0) throw std::runtime_error("Camera aspect must be positive");
    Vec3 forward=(target-origin).normalized(),right=forward.cross({0,1,0}).normalized(),up=right.cross(forward);
    double scale=std::tan(fov*pi/360);
    return {origin,(forward+right*((2*u-1)*aspect*scale)+up*((1-2*v)*scale)).normalized()};
}
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
