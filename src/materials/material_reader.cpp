#include "materials/material_reader.h"
#include "io/json_fields.h"
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
std::shared_ptr<const Texture> read_texture(const nlohmann::json& entry, const char* field,
                                           const std::filesystem::path& directory, bool decode_srgb) {
    const auto file = std::filesystem::u8path(entry.at(field).get<std::string>());
    auto texture = std::make_shared<Texture>();
    texture->load(directory / file, decode_srgb);
    return texture;
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
    } else if (type == "diffuse" || type == "mirror") {
        material.kind = type == "diffuse" ? Kind::diffuse : Kind::mirror;
        if (material.kind == Kind::diffuse && entry.contains("texture")) {
            json_fields::expect_fields(entry, {"type", "texture", "opacity", "opacity_map"});
            material.texture = read_texture(entry, "texture", scene_directory, true);
        } else {
            json_fields::expect_fields(entry, {"type", "color", "opacity", "opacity_map"});
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
