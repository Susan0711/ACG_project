#pragma once
#include "core/vec3.h"
#include <filesystem>
#include <vector>
struct EnvironmentSample { Vec3 direction,radiance; double pdf=0; };
struct Environment {
    int width=0,height=0;
    double rotation=0,intensity=1;
    std::vector<Vec3> pixels;
    std::vector<double> probabilities,cdf;
    void load(const std::filesystem::path& path);
    void prepare();
    Vec3 evaluate(Vec3 direction) const;
    double pdf(Vec3 direction) const;
    EnvironmentSample sample(double select,double azimuth,double elevation) const;
private:
    std::size_t cell(Vec3 direction) const;
    double solid_angle(int row) const;
};
