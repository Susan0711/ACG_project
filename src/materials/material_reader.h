#pragma once
#include "materials/material.h"
#include <nlohmann/json.hpp>
#include <filesystem>

Material read_material(const nlohmann::json& entry, const std::filesystem::path& scene_directory);
