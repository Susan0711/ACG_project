#include "core/geometry.h"
namespace { bool valid(const Ray& r,double lo,double hi) { auto f=[](Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);};return std::isfinite(r.time)&&f(r.origin)&&f(r.direction)&&!std::isnan(lo)&&!std::isnan(hi)&&lo<=hi;} }

bool Sphere::intersect(const Ray& ray, double lo, double hi, HitRecord& hit) const {
    if (!valid(ray,lo,hi)) return false;
    const Vec3 oc = ray.origin-center_at(ray.time);
    const double a = ray.direction.dot(ray.direction), b = oc.dot(ray.direction);
    if (!(a > 0) || !(radius > 0) || lo > hi) return false;
    const double d = b*b-a*(oc.dot(oc)-radius*radius);
    if (d < 0) return false;
    double t = (-b-std::sqrt(d))/a;
    if (t < lo || t > hi) t = (-b+std::sqrt(d))/a;
    if (t < lo || t > hi) return false;
    hit.set(ray,t,(ray.at(t)-center_at(ray.time)).normalized(),material_id);
    hit.uv = {0.5 + std::atan2(hit.outward_normal.z, hit.outward_normal.x)/(2*3.141592653589793), 0.5 + std::asin(hit.outward_normal.y)/3.141592653589793};
    hit.light_id = -1;
    return true;
}
bool Triangle::intersect(const Ray& ray, double lo, double hi, HitRecord& hit) const {
    if (!valid(ray,lo,hi)) return false;
    const Vec3 e1=b-a, e2=c-a, n=e1.cross(e2);
    const double scale = std::sqrt(n.dot(n)*ray.direction.dot(ray.direction));
    const Vec3 p=ray.direction.cross(e2);
    const double det=e1.dot(p);
    if (!(scale > 0) || std::abs(det) <= 1e-12*scale || lo > hi) return false;
    const Vec3 offset = ray.origin - (a + velocity * ray.time);
    const double u=offset.dot(p)/det;
    const Vec3 q=offset.cross(e1);
    const double v=ray.direction.dot(q)/det;
    if (u < 0 || v < 0 || u+v > 1) return false;
    const double t=e2.dot(q)/det;
    if (t < lo || t > hi) return false;
    hit.set(ray,t,n.normalized(),material_id);
    if (corner_normals) {
        const auto& normals = *corner_normals;
        Vec3 shading = (normals[0]*(1-u-v) + normals[1]*u + normals[2]*v).normalized();
        if (shading.dot(hit.normal) < 0) shading = shading * -1;
        if (shading.dot(ray.direction) < 0) hit.normal = shading;
    }
    hit.uv = {ta.u*(1-u-v)+tb.u*u+tc.u*v, ta.v*(1-u-v)+tb.v*u+tc.v*v};
    hit.light_id = light_id;
    return true;
}
bool Scene::intersect(const Ray& ray, double lo, double hi, HitRecord& hit) const {
    bool found=false;
    for (const auto& object : objects) {
        HitRecord candidate;
        if (std::visit([&](const auto& shape) { return shape.intersect(ray,lo,hi,candidate); },object)) {
            if (!found || candidate.t < hi) { hit=candidate; hi=candidate.t; found=true; }
        }
    }
    return found;
}
Ray spawn(const HitRecord& h,Vec3 direction) {return {h.position+h.outward_normal*(direction.dot(h.outward_normal)>=0?1e-5:-1e-5),direction,h.time};
}
