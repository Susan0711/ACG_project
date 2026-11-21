#pragma once
#include "render/camera.h"
#include "render/medium.h"
#include "core/geometry.h"
#include "lighting/lights.h"
#include "materials/material.h"

struct FilmSize {
    static constexpr int max_dimension = 16384;
    int width = 256, height = 256;
};

// Mutable description populated by a scene factory, before building an index.
struct World {
    Scene scene;
    std::vector<Material> materials;
    Lighting lights;
    Camera camera;
    FilmSize film_size;
    HomogeneousMedium medium;
    void add_area(AreaLight light);
};
