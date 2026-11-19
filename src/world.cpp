#include "tracer.h"
void World::add_area(AreaLight light) {
    const int id = static_cast<int>(areas.size());
    const int material = static_cast<int>(materials.size());
    materials.push_back({Kind::emissive, light.radiance, 1.5});
    Triangle first{light.origin, light.origin+light.u, light.origin+light.u+light.v, material};
    Triangle second{light.origin, light.origin+light.u+light.v, light.origin+light.v, material};
    first.light_id = second.light_id = id;
    scene.objects.push_back(first);
    scene.objects.push_back(second);
    areas.push_back(light);
}
