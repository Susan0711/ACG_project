#pragma once
#include "acceleration/bvh.h"
#include "core/sampling.h"
#include <optional>

struct MediumSegmentSample {
    Vec3 radiance{};  // Integrated emission up to the segment endpoint.
    std::optional<Vec3> collision;
};

struct HomogeneousMedium {
    AABB bounds{};
    double density = 0;
    double albedo = 0;
    Vec3 emission{};  // Emitted radiance per unit optical depth.

    double transmittance(const Ray&, double t_max) const;
    MediumSegmentSample sample_segment(const Ray&, double t_max, Sampler&) const;
};

inline constexpr double isotropic_phase_pdf = 1 / (4 * pi);
Vec3 sample_isotropic_direction(Sampler&);
