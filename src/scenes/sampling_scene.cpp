#include "tracer.h"
World make_sampling_scene() {
    World w;
    w.camera={{0,1.3,5},{0,.6,0},40};
    w.materials={{Kind::diffuse,{.6,.6,.6},1.5},{Kind::mirror,{.94,.94,.94},1.5}};
    w.scene.objects.push_back(Sphere{{-.8,.7,0},.7,0});
    w.scene.objects.push_back(Sphere{{.8,.7,0},.7,1});
    w.scene.objects.push_back(Triangle{{-6,0,4},{6,0,4},{6,0,-6},0});
    w.scene.objects.push_back(Triangle{{-6,0,4},{6,0,-6},{-6,0,-6},0});
    w.add_area({{-.18,3,-.2},{.36,0,0},{0,0,.36},{18,14,9}});
    return w;
}
