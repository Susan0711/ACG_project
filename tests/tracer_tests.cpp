#include "render/path_integrator.h"
#include "scenes/factories.h"
#include <iostream>
#include <stdexcept>
#include <limits>
void check(bool ok,const char* name){if(!ok)throw std::runtime_error(name);}

void check_sah_splits() {
    Scene cluster;
    for (int i = 0; i < 7; ++i) cluster.objects.push_back(Sphere{{0.1*i,0,0}, 0.1});
    cluster.objects.push_back(Sphere{{30,0,0}, 0.1});
    BVH tree;
    tree.build(cluster, 0, 0, BVHSplit::sah);
    const auto& isolated = tree.nodes[tree.nodes.front().right];
    check(isolated.left == -1 && isolated.object == 7, "SAH should isolate the distant sphere");

    Scene tied{{Sphere{{0,0,0},1,3}, Sphere{{0,0,0},1,4}, Sphere{{0,0,0},1,5}}};
    tree.build(tied, 0, 0, BVHSplit::sah);
    HitRecord hit;
    const Ray ray{{0,0,3},{0,0,-1}};
    BVHTraversalStats stats;
    check(tree.intersect(tied,ray,2,2,hit,&stats) && hit.object_index == 0 && hit.material_id == 3,
          "Coincident centers and equal distances use original object order");
    check(stats.box_tests == 5 && stats.primitive_tests == 3, "Count actual box and primitive tests");

    // The smaller original index is in the child visited second.
    Scene symmetric{{Sphere{{0.5,0,0},1,3}, Sphere{{-0.5,0,0},1,4}}};
    tree.build(symmetric, 0, 0, BVHSplit::sah);
    check(tree.intersect(symmetric,ray,0,5,hit) && hit.object_index == 0, "Tie across traversal order");
    Scene empty;
    tree.build(empty, 0, 0, BVHSplit::sah);
    check(tree.nodes.empty() && !tree.intersect(empty,ray,0,5,hit), "Rebuild empty SAH tree");
}

int main(){try {
    check_sah_splits();
    World w=make_world(false);
    BVH bvh; bvh.build(w.scene);
    BVH sah; sah.build(w.scene, 0, 0, BVHSplit::sah);
    RenderScene prepared(w);
    Sampler rng{42};HitRecord a;
    for (int i = 0; i < 10000; ++i) {
        Ray r{{0,0,3.4},{rng.next()*2-1,rng.next()*2-1,-1}};
        const bool found = w.scene.intersect(r,0,100,a);
        for (const BVH* tree : {&bvh, &sah}) {
            HitRecord b;
            check(found == tree->intersect(w.scene,r,0,100,b), "BVH hit mismatch");
            if (found) check(a.t == b.t && a.object_index == b.object_index &&
                             a.material_id == b.material_id && a.light_id == b.light_id, "BVH nearest mismatch");
        }
    }
    AABB box{{0,0,0},{1,1,0}};check(box.intersect({{0.5,0.5,1},{0,0,-1}},1,1),"zero thickness boundary");check(!box.intersect({{2,0.5,1},{0,0,-1}},0,2),"parallel outside slab");
    double nan=std::numeric_limits<double>::quiet_NaN();check(!w.scene.intersect({{nan,0,0},{0,0,-1}},0,10,a),"NaN ray rejected");check(!bvh.intersect(w.scene,{{0,0,0},{0,0,-1}},nan,10,a),"NaN interval rejected");
    Ray r{{0,0,1},{0,0,-1}};a.set(r,1,{0,0,1},0);double cosine=0;
    for(int i=0;i<10000;++i){auto s=sample_bsdf({Kind::diffuse,{0.4,0.5,0.6},1.5},r,a,rng);check(!s.delta&&finite(s.direction)&&s.direction.z>=0&&std::abs(s.direction.dot(s.direction)-1)<1e-12,"diffuse direction");check(std::abs(s.pdf-s.direction.z/pi)<1e-12&&s.weight.x==0.4,"diffuse weight/pdf");cosine+=s.direction.z;}
    check(std::abs(cosine/10000-2.0/3)<0.02,"cosine sampling distribution");check(std::abs(fresnel(1,1,1.5)-0.04)<1e-12,"normal Fresnel");check(fresnel(0.3,1.5,1)==1,"TIR Fresnel");
    Ray oblique{{0,0,1},{0.6,0,-0.8}};a.set(oblique,1,{0,0,1},0);
    bool refracted=false;for(int i=0;i<100;++i){auto s=sample_bsdf({Kind::glass,{1,1,1},1.5},oblique,a,rng);if(s.direction.z<0){check(std::abs(s.direction.x-0.4)<1e-12,"Snell oblique transmission");refracted=true;break;}}
    check(refracted,"oblique transmission observed");a.set(r,1,{0,0,1},0);
    int transmitted=0;for(int i=0;i<1000;++i){auto s=sample_bsdf({Kind::glass,{1,1,1},1.5},r,a,rng);check(s.delta&&finite(s.direction),"glass finite delta");if(s.direction.z<0){++transmitted;check(std::abs(s.weight.x-4.0/9)<1e-12,"radiance eta weight");}}
    check(transmitted>900&&transmitted<990,"Fresnel branching");
    Ray inside{{0,0,0},{std::sqrt(0.91),0,0.3}};a.set(inside,1,{0,0,1},0);auto tir=sample_bsdf({Kind::glass,{1,1,1},1.5},inside,a,rng);check(tir.direction.z<0&&tir.pdf==1,"TIR reflection");check(spawn(a,tir.direction).origin.z<a.position.z,"reflection offset");
    a.set(r,1,{0,0,1},0);auto mirror=sample_bsdf({Kind::mirror,{0.9,0.9,0.9},1.5},r,a,rng);check(mirror.direction.z==1&&mirror.delta,"mirror reflection");check(spawn(a,{0,0,-1}).origin.z<a.position.z,"transmission offset");
    for(int i=0;i<1000;++i)check(finite(integrate(prepared,w.camera.generate(rng.next(),rng.next(),1),rng,16)),"finite integration");
    std::cout<<"PASS: BVH, slabs, NaN, cosine PDF, Fresnel, refraction, TIR, offsets, finite paths\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
