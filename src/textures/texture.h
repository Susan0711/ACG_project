#pragma once
#include "core/vec3.h"
#include <vector>
#include <filesystem>

struct Texture {
    enum class Type { constant, procedural, image };
    enum class Filter { nearest, bilinear, mipmap };
    Type type=Type::constant;
    Vec3 color{0.5,0.5,0.5};
    int width=0, height=0;
    std::vector<Vec3> pixels;
    void load(const std::filesystem::path& path, bool decode_srgb=true, Filter filter=Filter::nearest);
    Vec3 value(UV uv, double lod=0) const;
    int last_level() const { return static_cast<int>(lower_levels_.size()); }

private:
    struct Level { int width, height; std::vector<Vec3> pixels; };
    std::vector<Level> lower_levels_;
    Filter filter_ = Filter::nearest;
    static Level downsample(const std::vector<Vec3>& source, int w, int h);
    Vec3 sample_level(int level, UV uv) const;
};
