#include "world.h"

void World::add_area(AreaLight light) {
    const int id = static_cast<int>(lights.areas_.size());
    const int material = static_cast<int>(materials.size());
    // Complete all potentially allocating operations before changing the linked tables.
    materials.reserve(materials.size() + 1);
    scene.objects.reserve(scene.objects.size() + 2);
    lights.areas_.reserve(lights.areas_.size() + 1);

    materials.push_back({Kind::emissive, light.radiance, 1.5});
    Triangle first{light.origin, light.origin + light.u, light.origin + light.u + light.v, material};
    Triangle second{light.origin, light.origin + light.u + light.v, light.origin + light.v, material};
    first.light_id = second.light_id = id;
    scene.objects.push_back(first);
    scene.objects.push_back(second);
    lights.areas_.push_back(light);
}
