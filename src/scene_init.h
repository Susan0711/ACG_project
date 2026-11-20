#pragma once
#include "world.h"
#include <filesystem>

World load_scene_file(const std::filesystem::path& path);
