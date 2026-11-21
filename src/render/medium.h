#pragma once
#include "acceleration/bvh.h"
#include "core/sampling.h"
#include <optional>

struct HomogeneousMedium {
    AABB bounds{};
    double density = 0;
    double albedo = 0;

    double transmittance(const Ray&, double t_max) const;
    std::optional<Vec3> sample_collision(const Ray&, double t_max, Sampler&) const;
};

inline constexpr double isotropic_phase_pdf = 1 / (4 * pi);
Vec3 sample_isotropic_direction(Sampler&);
