#pragma once
#include "lighting/environment.h"
#include "core/sampling.h"
#include <limits>
#include <memory>

struct PointLight { Vec3 position, intensity; };
struct AreaLight { Vec3 origin, u, v, radiance; };

struct LightSample {
    Vec3 direction{}, radiance{};
    double distance = 0, pdf = 0;
    bool delta = false;
};

struct BackgroundLight {
    Vec3 radiance;
    double pdf;
    bool sampled_directly;
};

class Lighting {
public:
    Vec3 background{};
    std::shared_ptr<const Environment> hdr;
    std::vector<PointLight> points;
    const std::vector<AreaLight>& areas() const { return areas_; }

    BackgroundLight escape(Vec3 direction) const;
    double emitter_pdf(int light_id, Vec3 from, Vec3 to) const;

    // Each light is sampled once. A delta light reports probability mass 1.
    template<class Visitor>
    void sample_each(Vec3 from, Sampler& rng, Visitor visit) const {
        for (const auto& point : points) visit(sample(point, from));
        for (const auto& area : areas_) visit(sample(area, from, rng));
        if (hdr) {
            const double select = rng.next(), azimuth = rng.next(), elevation = rng.next();
            const auto s = hdr->sample(select, azimuth, elevation);
            visit(LightSample{s.direction, s.radiance, std::numeric_limits<double>::infinity(), s.pdf, false});
        }
    }

private:
    friend struct World;  // Only World::add_area may register an intersectable emitter.
    std::vector<AreaLight> areas_;
    static LightSample sample(const PointLight&, Vec3 from);
    static LightSample sample(const AreaLight&, Vec3 from, Sampler&);
};
