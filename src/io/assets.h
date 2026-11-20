#pragma once
#include "core/geometry.h"
#include <filesystem>
#include <string>
struct Transform {
    Vec3 scale{1,1,1}, translation{};
    double rotation=0;
    Vec3 point(Vec3 p) const;
    Vec3 normal(Vec3 n) const;
};
void load_obj(Scene& scene, const std::filesystem::path& path, int material,
              const Transform& transform={}, Vec3 velocity={});

std::string asset_path(const std::string& root,const std::string& name);
