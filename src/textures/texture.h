#pragma once
#include "core/vec3.h"
#include <vector>
#include <filesystem>

struct Texture {
    enum class Type { constant, procedural, image };
    Type type=Type::constant;
    Vec3 color{0.5,0.5,0.5};
    int width=0, height=0;
    std::vector<Vec3> pixels;
    void load(const std::filesystem::path& path,bool decode_srgb=true);
    Vec3 value(UV uv) const;
};
