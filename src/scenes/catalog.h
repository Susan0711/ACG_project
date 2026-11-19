#pragma once
#include "tracer.h"
#include <iosfwd>
#include <string_view>
#include <vector>
struct SceneEntry {
    std::string_view name;
    std::string_view description;
    World (*factory)(const std::string& resource_root);
};
const std::vector<SceneEntry>& scene_catalog();
const SceneEntry& find_scene(std::string_view name);
World create_scene(std::string_view name,const std::string& resource_root);
void write_scene_names(std::ostream& output);
World make_texture_scene(const std::string& resource_root);
