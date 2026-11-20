#pragma once
#include "world.h"
#include <string>

World make_world(bool point);
World make_forest(bool gray, const std::string& root = "assets");
World make_forest_v03(bool gray, const std::string& root, bool detail = false);
World make_forest_v05(bool gray, const std::string& root, bool detail = false);
World make_effect_scene(const std::string& name, const std::string& root = "assets");
World make_sampling_scene();
World make_texture_scene(const std::string& resource_root);
