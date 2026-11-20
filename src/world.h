#pragma once
#include "render/camera.h"
#include "core/geometry.h"
#include "lighting/lights.h"
#include "materials/material.h"

// Mutable description populated by a scene factory, before building an index.
struct World {
    Scene scene;
    std::vector<Material> materials;
    Lighting lights;
    Camera camera;
    void add_area(AreaLight light);
};
