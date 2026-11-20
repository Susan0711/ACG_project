#include "lighting/lights.h"

namespace {
double area_pdf(const AreaLight& light, Vec3 from, Vec3 to) {
    const Vec3 offset = to - from, cross = light.u.cross(light.v);
    const double area = std::sqrt(cross.dot(cross));
    const double cosine = cross.normalized().dot(offset.normalized() * -1);
    return cosine > 0 && area > 0 ? offset.dot(offset) / (area * cosine) : 0;
}
}

LightSample Lighting::sample(const PointLight& light, Vec3 from) {
    const Vec3 offset = light.position - from;
    const double distance = std::sqrt(offset.dot(offset));
    if (distance == 0) return {};
    return {offset * (1 / distance), light.intensity * (1 / (distance * distance)), distance, 1, true};
}

LightSample Lighting::sample(const AreaLight& light, Vec3 from, Sampler& rng) {
    const double u = rng.next(), v = rng.next();
    const Vec3 target = light.origin + light.u * u + light.v * v, offset = target - from;
    return {offset.normalized(), light.radiance, std::sqrt(offset.dot(offset)), area_pdf(light, from, target), false};
}

double Lighting::emitter_pdf(int id, Vec3 from, Vec3 to) const {
    return id < 0 ? 0 : area_pdf(areas_.at(static_cast<std::size_t>(id)), from, to);
}

BackgroundLight Lighting::escape(Vec3 direction) const {
    if (hdr) return {hdr->evaluate(direction), hdr->pdf(direction), true};
    return {background, 0, false};
}
