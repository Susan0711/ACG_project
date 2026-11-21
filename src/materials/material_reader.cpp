#include "materials/material_reader.h"
#include "io/json_fields.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
double unit_parameter(const nlohmann::json& entry, const char* field, double fallback) {
    const double value = json_fields::optional_number(entry, field, fallback);
    if (!std::isfinite(value) || value < 0 || value > 1)
        throw std::runtime_error(std::string(field) + " must be in [0, 1]");
    return value;
}

std::shared_ptr<const Texture> read_texture(const nlohmann::json& entry, const char* field,
                                           const std::filesystem::path& directory, bool decode_srgb) {
    const auto file = std::filesystem::u8path(entry.at(field).get<std::string>());
    auto texture = std::make_shared<Texture>();
    texture->load(directory / file, decode_srgb);
    return texture;
}

std::shared_ptr<const Texture> read_data_map(const nlohmann::json& entry, const char* field,
                                             const std::filesystem::path& directory) {
    const std::string toggle = std::string(field) + "_enabled";
    const bool enabled = entry.value(toggle, true);
    if (!entry.contains(field)) return {};
    if (!entry.at(field).is_string()) throw std::runtime_error(std::string(field) + " must be a texture path");
    return enabled ? read_texture(entry, field, directory, false) : nullptr;
}
}

Material read_material(const nlohmann::json& entry, const std::filesystem::path& scene_directory) {
    const auto type = entry.at("type").get<std::string>();
    Material material;
    if (type == "glass") {
        json_fields::expect_fields(entry, {"type", "ior", "opacity", "opacity_map"});
        material.kind = Kind::glass;
        material.ior = json_fields::number(entry.at("ior"), "ior");
        if (material.ior <= 0) throw std::runtime_error("ior must be positive");
    } else if (type == "emissive") {
        json_fields::expect_fields(entry, {"type", "color"});
        material.kind = Kind::emissive;
        material.color = json_fields::rgb(entry, "color");
        return material;
    } else if (type == "diffuse" || type == "mirror" || type == "principled") {
        const bool textured = type != "mirror" && entry.contains("texture");
        const char* base_field = textured ? "texture" : "color";
        if (type == "principled") {
            json_fields::expect_fields(entry, {"type", base_field, "metallic", "roughness", "opacity", "opacity_map",
                                              "normal_map", "normal_map_enabled", "roughness_map", "roughness_map_enabled"});
            material.kind = Kind::principled;
            material.metallic = unit_parameter(entry, "metallic", material.metallic);
            material.roughness = unit_parameter(entry, "roughness", material.roughness);
            material.normal_map = read_data_map(entry, "normal_map", scene_directory);
            material.roughness_map = read_data_map(entry, "roughness_map", scene_directory);
        } else {
            json_fields::expect_fields(entry, {"type", base_field, "opacity", "opacity_map"});
            material.kind = type == "diffuse" ? Kind::diffuse : Kind::mirror;
        }
        if (textured) {
            material.texture = read_texture(entry, "texture", scene_directory, true);
        } else {
            material.color = json_fields::rgb(entry, "color");
            const Vec3 c = material.color;
            if (std::max({c.x, c.y, c.z}) > 1) throw std::runtime_error("Reflectance color must be in [0, 1]");
        }
    } else {
        throw std::runtime_error("Unknown material type: " + type);
    }
    material.opacity = json_fields::optional_number(entry, "opacity", material.opacity);
    if (material.opacity < 0 || material.opacity > 1) throw std::runtime_error("opacity must be in [0, 1]");
    if (entry.contains("opacity_map"))
        material.opacity_map = read_texture(entry, "opacity_map", scene_directory, false);
    return material;
}
