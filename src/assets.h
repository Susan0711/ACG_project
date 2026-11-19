#pragma once
#include "geometry.h"
#include <string>
struct Texture {
    enum class Type { constant, procedural, image };
    Type type=Type::constant;
    Vec3 color{0.5,0.5,0.5};
    int width=0, height=0;
    std::vector<Vec3> pixels;
    void load(const std::string& path,bool decode_srgb=true);
    Vec3 value(UV uv) const;
};
struct Transform {
    Vec3 scale{1,1,1}, translation{};
    double rotation=0;
    Vec3 point(Vec3 p) const;
    Vec3 normal(Vec3 n) const;
};
void load_obj(Scene& scene, const std::string& path, int material, const Transform& transform={});

std::string asset_path(const std::string& root,const std::string& name);
