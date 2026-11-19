#include "geometry.h"
#include <iostream>
#include <stdexcept>
void check_at(bool ok, int line) { if (!ok) throw std::runtime_error("geometry_tests.cpp line " + std::to_string(line)); }
#define check(...) check_at((__VA_ARGS__), __LINE__)
int main() {
    HitRecord h;
    Sphere s{{0,0,0},2,7};
    check(s.intersect({{0,0,0},{0,0,2}},0,10,h));
    check(h.t==1 && !h.front_face && h.outward_normal.z==1 && h.normal.z==-1 && h.material_id==7);
    check(h.position.z==2);
    check(!s.intersect({{0,0,0},{0,0,0}},0,10,h));
    check(Vec3{0,0,0}.normalized().dot(Vec3{1,1,1})==0);
    Triangle tri{{-1,-1,-2},{1,-1,-2},{0,1,-2},9};
    check(tri.intersect({{0,0,0},{0,0,-1}},2,2,h) && h.front_face && h.normal.z==1);
    check(tri.intersect({{0,0,-3},{0,0,1}},0,2,h) && !h.front_face && h.normal.z==-1 && h.outward_normal.z==1);
    check(!tri.intersect({{0,0,0},{1,0,0}},0,10,h));
    check(!Triangle{{0,0,0},{1,0,0},{2,0,0},0}.intersect({{0,0,1},{0,0,-1}},0,10,h));
    Scene scene{{Sphere{{0,0,-5},1,1},tri,Sphere{{0,0,-2},0.5,2}}};
    const Ray ray{{0,0,0},{0,0,-1}};
    check(scene.intersect(ray,0,10,h) && h.t==1.5 && h.material_id==2);
    check(scene.intersect(ray,1.6,2.1,h) && h.t==2 && h.material_id==9);
    check(!scene.intersect(ray,2.6,3.9,h));
    check(!scene.intersect(ray,5,1,h));
    const Scene multi=preview_scene(true);
    check(multi.intersect({{0,0,0},{0.3,0,-1}},0,10,h) && h.material_id==4 && !h.front_face && h.outward_normal.z==-1);
    check(multi.intersect({{0,0,0},{-0.14,0,-1}},0,10,h) && h.material_id==1);
    std::cout << "Passed interior, interval, nearest, two-sided, parallel, degenerate and preview occlusion checks\n";
}
