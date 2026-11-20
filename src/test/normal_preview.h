#pragma once
#include "core/geometry.h"

inline Scene preview_scene(bool multi) {
    if (!multi) return {{Sphere{{0,0,-2},0.75,0}}};
    return {{Sphere{{-0.35,0,-2.5},0.8,1}, Sphere{{0.55,-0.15,-3.3},0.7,2},
             Triangle{{-1.8,-1,-4},{1.8,-1,-4},{0,1.5,-4},3},
             Triangle{{0.15,-0.7,-1.7},{0.25,0.7,-1.7},{0.95,-0.7,-1.7},4}}};
}
