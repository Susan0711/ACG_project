#include "materials/material_reader.h"
#include "io/json_fields.h"
#include <algorithm>
#include <cmath>
#include <limits>
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
                                           const std::filesystem::path& directory, bool decode_srgb,
                                           Texture::Filter filter=Texture::Filter::nearest) {
    const auto file = std::filesystem::u8path(entry.at(field).get<std::string>());
    auto texture = std::make_shared<Texture>();
    texture->load(directory / file, decode_srgb, filter);
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
        json_fields::expect_fields(entry, {"type", "ior", "cauchy_b", "opacity", "opacity_map"});
        material.kind = Kind::glass;
        material.ior = json_fields::number(entry.at("ior"), "ior");
        if (!std::isfinite(material.ior) || material.ior <= 0) throw std::runtime_error("ior must be finite and positive");
        material.cauchy_b = json_fields::optional_number(entry, "cauchy_b", 0);
        if (!std::isfinite(material.cauchy_b) || material.cauchy_b < 0 ||
            material.ior_at(Wavelength::red) <= 0 || !std::isfinite(material.ior_at(Wavelength::blue)))
            throw std::runtime_error("cauchy_b must be nonnegative and give finite positive RGB refractive indices");
    } else if (type == "emissive") {
        json_fields::expect_fields(entry, {"type", "color"});
        material.kind = Kind::emissive;
        material.color = json_fields::rgb(entry, "color");
        return material;
    } else if (type == "diffuse" || type == "mirror" || type == "principled" || type == "coated") {
        const bool textured = type != "mirror" && entry.contains("texture");
        const char* base_field = textured ? "texture" : "color";
        if (type == "principled" || type == "coated") {
            // Validate the coat fields separately so Principled keeps its existing schema.
            auto base_entry = entry;
            if (type == "coated") {
                for (const auto* key : {"coat_weight", "coat_ior", "coat_roughness", "coat_thickness", "coat_absorption"})
                    base_entry.erase(key);
                material.coat_weight = unit_parameter(entry, "coat_weight", material.coat_weight);
                material.coat_roughness = unit_parameter(entry, "coat_roughness", material.coat_roughness);
                material.coat_ior = json_fields::optional_number(entry, "coat_ior", material.coat_ior);
                material.coat_thickness = json_fields::optional_number(entry, "coat_thickness", 0);
                if (entry.contains("coat_absorption")) material.coat_absorption = json_fields::rgb(entry, "coat_absorption");
                if (!std::isfinite(material.coat_ior) || material.coat_ior < 1 ||
                    !std::isfinite(material.coat_thickness) || material.coat_thickness < 0 ||
                    !finite(material.coat_absorption))
                    throw std::runtime_error("Coat needs finite ior >= 1, thickness >= 0 and nonnegative absorption");
            }
            json_fields::expect_fields(base_entry, {"type", base_field, "metallic", "roughness", "opacity", "opacity_map",
                                              "normal_map", "normal_map_enabled", "roughness_map", "roughness_map_enabled",
                                              "texture_filter"});
            material.kind = type == "coated" ? Kind::coated : Kind::principled;
            material.metallic = unit_parameter(entry, "metallic", material.metallic);
            material.roughness = unit_parameter(entry, "roughness", material.roughness);
            material.normal_map = read_data_map(entry, "normal_map", scene_directory);
            material.roughness_map = read_data_map(entry, "roughness_map", scene_directory);
        } else if (type == "diffuse") {
            json_fields::expect_fields(entry, {"type", base_field, "opacity", "opacity_map", "texture_filter", "toon_bands"});
            const auto bands = entry.find("toon_bands");
            if (bands != entry.end()) {
                if (!bands->is_number_integer() || *bands < 0 || *bands == 1 || *bands > std::numeric_limits<int>::max())
                    throw std::runtime_error("toon_bands must be 0 or an integer from 2 to INT_MAX");
                material.toon_bands = bands->get<int>();
            }
        } else {
            json_fields::expect_fields(entry, {"type", base_field, "opacity", "opacity_map", "texture_filter"});
            material.kind = Kind::mirror;
        }
        if (entry.contains("texture_filter") && !textured)
            throw std::runtime_error("texture_filter requires a base color texture");
        if (textured) {
            const auto name = entry.value("texture_filter", std::string{"nearest"});
            auto filter = Texture::Filter::nearest;
            if (name == "bilinear") filter = Texture::Filter::bilinear;
            else if (name == "mipmap") filter = Texture::Filter::mipmap;
            else if (name != "nearest") throw std::runtime_error("Unknown texture_filter: " + name);
            material.texture = read_texture(entry, "texture", scene_directory, true, filter);
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
