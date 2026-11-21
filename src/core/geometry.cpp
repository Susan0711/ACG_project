#include "core/geometry.h"
namespace { bool valid(const Ray& r,double lo,double hi) { auto f=[](Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);};return std::isfinite(r.time)&&f(r.origin)&&f(r.direction)&&!std::isnan(lo)&&!std::isnan(hi)&&lo<=hi;} }

namespace {
Vec3 from_tangent_space(const HitRecord& hit, Vec3 tangent, double handedness, Vec3 local) {
    // Build the UV frame on the outward-facing shading normal, then orient the result to the hit side.
    const Vec3 n = hit.front_face ? hit.normal : hit.normal * -1;
    tangent = (tangent - n * n.dot(tangent)).normalized();
    if (tangent.dot(tangent) == 0) return hit.normal;
    const Vec3 bitangent = n.cross(tangent) * handedness;
    const Vec3 mapped = (tangent * local.x + bitangent * local.y + n * local.z).normalized();
    if (mapped.dot(hit.outward_normal) <= 0) return hit.normal;
    return hit.front_face ? mapped : mapped * -1;
}
}

Vec3 Triangle::mapped_normal(const HitRecord& hit, Vec3 local) const {
    if (!has_complete_uvs) return hit.normal;
    const double du1 = tb.u - ta.u, dv1 = tb.v - ta.v;
    const double du2 = tc.u - ta.u, dv2 = tc.v - ta.v;
    const double det = du1 * dv2 - du2 * dv1;
    if (std::abs(det) <= 1e-12 * (std::abs(du1 * dv2) + std::abs(du2 * dv1))) return hit.normal;
    const double handedness = det < 0 ? -1 : 1;
    const Vec3 tangent = ((b - a) * dv2 - (c - a) * dv1) * handedness;
    return from_tangent_space(hit, tangent, handedness, local);
}

Vec3 Sphere::mapped_normal(const HitRecord& hit, Vec3 local) const {
    // U increases with longitude; V increases toward +Y in Sphere::intersect.
    const Vec3 tangent{-hit.outward_normal.z, 0, hit.outward_normal.x};
    return from_tangent_space(hit, tangent, -1, local);
}

std::optional<UVGradients> Triangle::uv_gradients(const HitRecord& hit) const {
    if (!has_complete_uvs) return UVGradients{};
    const Vec3 e1 = b - a, e2 = c - a, n = hit.outward_normal;
    const double area = e1.dot(e2.cross(n));
    const Vec3 grad_b = e2.cross(n) * (1 / area), grad_c = n.cross(e1) * (1 / area);
    return UVGradients{grad_b * (tb.u - ta.u) + grad_c * (tc.u - ta.u),
                       grad_b * (tb.v - ta.v) + grad_c * (tc.v - ta.v)};
}

std::optional<UVGradients> Sphere::uv_gradients(const HitRecord& hit) const {
    const Vec3 n = hit.outward_normal;
    const double horizontal2 = n.x * n.x + n.z * n.z;
    if (horizontal2 < 1e-20) return std::nullopt;  // Longitude is undefined at the poles.
    return UVGradients{Vec3{-n.z, 0, n.x} * (1 / (2 * pi * radius * horizontal2)),
                       (Vec3{0, 1, 0} - n * n.y) * (1 / (pi * radius * std::sqrt(horizontal2)))};
}

bool Sphere::intersect(const Ray& ray, double lo, double hi, HitRecord& hit) const {
    if (!valid(ray,lo,hi)) return false;
    HitParameters params;
    if (!closest_sphere({center, velocity, radius}, ray.origin, ray.direction, ray.time, lo, hi, params)) return false;
    finish_hit(ray, params, hit);
    return true;
}
void Sphere::finish_hit(const Ray& ray, const HitParameters& params, HitRecord& hit) const {
    const double t = params.t;
    hit.set(ray,t,(ray.at(t)-center_at(ray.time)).normalized(),material_id);
    hit.uv = {0.5 + std::atan2(hit.outward_normal.z, hit.outward_normal.x)/(2*3.141592653589793), 0.5 + std::asin(hit.outward_normal.y)/3.141592653589793};
    hit.light_id = -1;
}
bool Triangle::intersect(const Ray& ray, double lo, double hi, HitRecord& hit) const {
    if (!valid(ray,lo,hi)) return false;
    HitParameters params;
    if (!closest_triangle({a, b-a, c-a, velocity}, ray.origin, ray.direction, ray.time, lo, hi, params)) return false;
    finish_hit(ray, params, hit);
    return true;
}
void Triangle::finish_hit(const Ray& ray, const HitParameters& params, HitRecord& hit) const {
    const double t = params.t, u = params.u, v = params.v;
    const Vec3 n = (b-a).cross(c-a);
    hit.set(ray,t,n.normalized(),material_id);
    if (corner_normals) {
        const auto& normals = *corner_normals;
        Vec3 shading = (normals[0]*(1-u-v) + normals[1]*u + normals[2]*v).normalized();
        if (shading.dot(hit.normal) < 0) shading = shading * -1;
        if (shading.dot(ray.direction) < 0) hit.normal = shading;
    }
    hit.uv = {ta.u*(1-u-v)+tb.u*u+tc.u*v, ta.v*(1-u-v)+tb.v*u+tc.v*v};
    hit.light_id = light_id;
}
bool Scene::intersect(const Ray& ray, double lo, double hi, HitRecord& hit) const {
    bool found=false;
    for (std::size_t index = 0; index < objects.size(); ++index) {
        HitRecord candidate;
        if (std::visit([&](const auto& shape) { return shape.intersect(ray,lo,hi,candidate); },objects[index])) {
            if (!found || candidate.t < hi) {
                hit = candidate;
                hit.object_index = index;
                hi = candidate.t;
                found = true;
            }
        }
    }
    return found;
}
Ray spawn(const HitRecord& h,Vec3 direction) {return {h.position+h.outward_normal*(direction.dot(h.outward_normal)>=0?1e-5:-1e-5),direction,h.time};
}
