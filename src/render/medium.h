#pragma once
#include "acceleration/bvh.h"
#include "core/sampling.h"
#include <optional>

struct MediumSegmentSample {
    Vec3 radiance{};  // Integrated emission up to the segment endpoint.
    std::optional<Vec3> collision;
};

enum class DensityProfile { homogeneous, height_linear };

struct Medium {
    AABB bounds{};
    double density = 0;
    double albedo = 0;
    Vec3 emission{};  // Emitted radiance per unit optical depth.
    DensityProfile profile = DensityProfile::homogeneous;
    double density_top = 0;  // Used only by height_linear; density is the bottom value.

    double density_at(Vec3 position) const;
    double optical_depth(const Ray&, double t_max) const;
    double transmittance(const Ray&, double t_max) const;
    MediumSegmentSample sample_segment(const Ray&, double t_max, Sampler&) const;
};

inline constexpr double isotropic_phase_pdf = 1 / (4 * pi);
Vec3 sample_isotropic_direction(Sampler&);
