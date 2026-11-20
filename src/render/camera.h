#pragma once
#include "core/ray.h"
#include "core/sampling.h"

struct Camera {
    Vec3 origin{0,0,3.4}, target{0,0,-1};
    double fov=40;
    double lens_radius=0, focus_distance=4;
    double shutter_open=0, shutter_close=0;
    void validate() const;
    Ray sample(double u,double v,double aspect,Sampler&) const;
    Ray generate(double u,double v,double aspect) const;
};
