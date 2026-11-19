#pragma once
#include <cmath>
#include <variant>
#include <vector>
struct Vec3 {
    double x, y, z;
    Vec3 operator+(Vec3 b) const { return {x+b.x, y+b.y, z+b.z}; }
    Vec3 operator-(Vec3 b) const { return {x-b.x, y-b.y, z-b.z}; }
    Vec3 operator*(double s) const { return {x*s, y*s, z*s}; }
    Vec3 cross(Vec3 b) const { return {y*b.z-z*b.y, z*b.x-x*b.z, x*b.y-y*b.x}; }
    double dot(Vec3 b) const { return x*b.x + y*b.y + z*b.z; }
    Vec3 normalized() const { const double length = std::sqrt(dot(*this)); return length > 0 ? *this * (1.0 / length) : Vec3{0,0,0}; }
};
struct UV { double u=0, v=0; };
struct Ray {
    Vec3 origin, direction;
    double time=0;
    Vec3 at(double t) const { return origin + direction*t; }
};

struct HitRecord {
    UV uv;
    int light_id=-1;
    double t{};
    double time=0;
    Vec3 position{}, outward_normal{}, normal{};
    bool front_face{};
    int material_id{};
    void set(const Ray& ray, double parameter, Vec3 outward, int material) {
        time = ray.time;
        t = parameter; position = ray.at(t); outward_normal = outward;
        front_face = ray.direction.dot(outward) < 0;
        normal = front_face ? outward : outward*(-1);
        material_id = material;
    }
};
struct Sphere {
    Vec3 center;
    double radius;
    int material_id = 0;
    Vec3 velocity{};
    Vec3 center_at(double time) const { return center + velocity*time; }
    bool intersect(const Ray&, double t_min, double t_max, HitRecord&) const;
};
struct Triangle {
    Vec3 a, b, c;
    int material_id = 0;
    UV ta{0,0}, tb{1,0}, tc{0,1};
    int light_id=-1;
    bool intersect(const Ray&, double t_min, double t_max, HitRecord&) const;
};
struct Scene {
    std::vector<std::variant<Sphere, Triangle>> objects;
    bool intersect(const Ray&, double t_min, double t_max, HitRecord&) const;
};
Scene preview_scene(bool multi);
