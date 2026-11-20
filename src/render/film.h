#pragma once
#include "core/vec3.h"
#include <vector>
#include <string>

struct Film {
    int width,height;
    std::vector<Vec3> pixels;
    Film(int w,int h):width(w),height(h),pixels(static_cast<std::size_t>(w)*h){}
    void write(const std::string&,double exposure=1) const;
    void write_linear(const std::string&) const;
};
