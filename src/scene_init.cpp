#include "scene_init.h"
#include "io/assets.h"
#include "io/json_fields.h"
#include "materials/material_reader.h"
#include <cmath>
#include <fstream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
using Json = nlohmann::json;

int film_dimension(const Json& entry, const char* key, int fallback) {
    const auto it = entry.find(key);
    if (it == entry.end()) return fallback;
    if (!it->is_number_integer() || *it < 1 || *it > FilmSize::max_dimension) {
        throw std::runtime_error(std::string("film.") + key + " must be an integer in [1, " +
                                 std::to_string(FilmSize::max_dimension) + "]");
    }
    return it->get<int>();
}

Camera read_camera(const Json& entry) {
    json_fields::expect_fields(entry, {"position", "target", "fov_degrees", "lens_radius",
                                      "focus_distance", "shutter_open", "shutter_close"});
    Camera camera;
    camera.origin = json_fields::vector3(entry, "position");
    camera.target = json_fields::vector3(entry, "target");
    camera.fov = json_fields::number(entry.at("fov_degrees"), "fov_degrees");
    camera.lens_radius = json_fields::optional_number(entry, "lens_radius", camera.lens_radius);
    camera.focus_distance = json_fields::optional_number(entry, "focus_distance", camera.focus_distance);
    if (entry.contains("shutter_open") != entry.contains("shutter_close"))
        throw std::runtime_error("Specify shutter_open and shutter_close together");
    if (entry.contains("shutter_open")) {
        camera.shutter_open = json_fields::number(entry.at("shutter_open"), "shutter_open");
        camera.shutter_close = json_fields::number(entry.at("shutter_close"), "shutter_close");
    }
    camera.validate();
    return camera;
}

HomogeneousMedium read_medium(const Json& entry) {
    json_fields::expect_fields(entry, {"min", "max", "density", "albedo", "emission"});
    const Vec3 lo = json_fields::vector3(entry, "min"), hi = json_fields::vector3(entry, "max");
    if (!finite(lo) || !finite(hi) || lo.x >= hi.x || lo.y >= hi.y || lo.z >= hi.z)
        throw std::runtime_error("medium.min must be below medium.max on each axis, with finite bounds");
    const double density = json_fields::number(entry.at("density"), "medium.density");
    const double albedo = json_fields::number(entry.at("albedo"), "medium.albedo");
    if (!std::isfinite(density) || density < 0)
        throw std::runtime_error("medium.density must be finite and nonnegative");
    if (!std::isfinite(albedo) || albedo < 0 || albedo > 1)
        throw std::runtime_error("medium.albedo must be in [0, 1]");
    const Vec3 emission = entry.contains("emission") ? json_fields::rgb(entry, "emission") : Vec3{};
    if (!finite(emission)) throw std::runtime_error("medium.emission must have finite RGB channels");
    return {{lo, hi}, density, albedo, emission};
}

void add_light(World& world, const Json& entry, const std::filesystem::path& directory) {
    const auto type = entry.at("type").get<std::string>();
    if (type == "point") {
        json_fields::expect_fields(entry, {"type", "position", "intensity"});
        world.lights.points.push_back({json_fields::vector3(entry, "position"), json_fields::rgb(entry, "intensity")});
    } else if (type == "area") {
        json_fields::expect_fields(entry, {"type", "origin", "u", "v", "radiance"});
        AreaLight light{json_fields::vector3(entry, "origin"), json_fields::vector3(entry, "u"),
                        json_fields::vector3(entry, "v"), json_fields::rgb(entry, "radiance")};
        const auto normal = light.u.cross(light.v);
        const double area_squared = normal.dot(normal);
        if (!std::isfinite(area_squared) || area_squared <= 0)
            throw std::runtime_error("Area light needs two edges spanning a finite, nonzero area");
        world.add_area(light);
    } else if (type == "environment") {
        json_fields::expect_fields(entry, {"type", "map", "rotation_degrees", "intensity"});
        if (world.lights.hdr) throw std::runtime_error("Only one environment light is supported per scene");
        const auto map = std::filesystem::u8path(entry.at("map").get<std::string>());
        if (map.empty()) throw std::runtime_error("environment.map must name an RGB PFM file");
        const double degrees = json_fields::optional_number(entry, "rotation_degrees", 0);
        const double intensity = json_fields::optional_number(entry, "intensity", 1);
        if (!std::isfinite(degrees)) throw std::runtime_error("environment.rotation_degrees must be finite");
        if (!std::isfinite(intensity) || intensity < 0)
            throw std::runtime_error("environment.intensity must be finite and nonnegative");
        auto env = std::make_shared<Environment>();
        env->rotation = std::remainder(degrees, 360.0) * (pi / 180.0);
        env->intensity = intensity;
        env->load(directory / map);
        world.lights.hdr = env;
    } else {
        throw std::runtime_error("Unknown light type: " + type);
    }
}
}

World load_scene_file(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("Cannot open scene file: " + path.u8string());
    const Json scene = Json::parse(input);
    json_fields::expect_fields(scene, {"camera", "materials", "objects", "lights", "film", "medium"});

    World world;
    if (scene.contains("film")) {
        const Json& film = scene.at("film");
        json_fields::expect_fields(film, {"width", "height"});
        world.film_size.width = film_dimension(film, "width", world.film_size.width);
        world.film_size.height = film_dimension(film, "height", world.film_size.height);
    }
    world.camera = read_camera(scene.at("camera"));
    if (scene.contains("medium")) world.medium = read_medium(scene.at("medium"));

    std::map<std::string, int> material_ids;
    for (const auto& [name, entry] : scene.at("materials").get_ref<const Json::object_t&>()) {
        material_ids.emplace(name, static_cast<int>(world.materials.size()));
        world.materials.push_back(read_material(entry, path.parent_path()));
    }

    for (const Json& object : scene.at("objects").get_ref<const Json::array_t&>()) {
        json_fields::expect_fields(object, {"mesh", "position", "material", "velocity"});
        const auto name = object.at("material").get<std::string>();
        const auto material = material_ids.find(name);
        if (material == material_ids.end()) throw std::runtime_error("Unknown material: " + name);
        Transform transform;
        transform.translation = json_fields::vector3(object, "position");
        const auto mesh = std::filesystem::u8path(object.at("mesh").get<std::string>());
        const Vec3 velocity = object.contains("velocity") ? json_fields::vector3(object, "velocity") : Vec3{};
        load_obj(world.scene, path.parent_path() / mesh, material->second, transform, velocity);
    }

    for (const Json& light : scene.at("lights").get_ref<const Json::array_t&>())
        add_light(world, light, path.parent_path());
    return world;
}
