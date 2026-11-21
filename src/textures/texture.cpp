#include "textures/texture.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
std::string read_token(std::istream& input) {
    std::string token;
    while (input >> token) {
        if (token.front() != '#') return token;
        std::getline(input, token);
    }
    throw std::runtime_error("Unexpected end of PPM");
}

int read_integer(std::istream& input) {
    const std::string token = read_token(input);
    std::size_t used;
    const int value = std::stoi(token, &used);
    if (used != token.size()) throw std::runtime_error("Invalid integer: " + token);
    return value;
}

double srgb_to_linear(double value) {
    return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
}
}  // namespace

Texture::Level Texture::downsample(const std::vector<Vec3>& source, int w, int h) {
    const int nw = std::max(1, w / 2), nh = std::max(1, h / 2);
    Level next{nw, nh, {}};
    next.pixels.reserve(nw * nh);
    // Integer overlap lengths also cover partial source texels for odd dimensions.
    for (int y = 0; y < nh; ++y) {
        for (int x = 0; x < nw; ++x) {
            const int left = x * w, right = left + w, top = y * h, bottom = top + h;
            Vec3 sum{};
            for (int sy = top / nh; sy <= (bottom - 1) / nh; ++sy) {
                const int dy = std::min(bottom, (sy + 1) * nh) - std::max(top, sy * nh);
                for (int sx = left / nw; sx <= (right - 1) / nw; ++sx) {
                    const int dx = std::min(right, (sx + 1) * nw) - std::max(left, sx * nw);
                    sum = sum + source[sy * w + sx] * (dx * dy);
                }
            }
            next.pixels.push_back(sum * (1.0 / (w * h)));
        }
    }
    return next;
}

void Texture::load(const std::filesystem::path& path, bool decode_srgb, Filter filter) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("Cannot open texture: " + path.u8string());
    if (read_token(input) != "P3") throw std::runtime_error("Only ASCII P3 PPM supported");

    const int image_width = read_integer(input);
    const int image_height = read_integer(input);
    const int maximum = read_integer(input);
    if (image_width < 1 || image_width > 8192 || image_height < 1 || image_height > 8192 ||
        maximum < 1 || maximum > 65535)
        throw std::runtime_error("Invalid PPM dimensions/range");

    std::vector<Vec3> image;
    for (int i = 0; i < image_width * image_height; ++i) {
        double channels[3];
        for (double& channel : channels) {
            const int value = read_integer(input);
            if (value < 0 || value > maximum) throw std::runtime_error("PPM sample out of range");
            channel = static_cast<double>(value) / maximum;
            if (decode_srgb) channel = srgb_to_linear(channel);
        }
        image.push_back({channels[0], channels[1], channels[2]});
    }

    std::vector<Level> levels;
    int w = image_width, h = image_height;
    while (filter == Filter::mipmap && (w > 1 || h > 1)) {
        const auto& source = levels.empty() ? image : levels.back().pixels;
        levels.push_back(downsample(source, w, h));
        w = levels.back().width;
        h = levels.back().height;
    }

    lower_levels_ = std::move(levels);
    filter_ = filter;
    pixels = std::move(image);
    width = image_width;
    height = image_height;
    type = Type::image;
}

Vec3 Texture::sample_level(int level, UV uv) const {
    const Level* mip = level ? &lower_levels_[level - 1] : nullptr;
    const int w = mip ? mip->width : width, h = mip ? mip->height : height;
    const auto& texels = mip ? mip->pixels : pixels;
    const double x = (uv.u - std::floor(uv.u)) * w - 0.5;
    const double y = (1 - (uv.v - std::floor(uv.v))) * h - 0.5;
    const int ix = static_cast<int>(std::floor(x)), iy = static_cast<int>(std::floor(y));
    const double fx = x - ix, fy = y - iy;
    const auto row = [&](int j) {
        const int offset = ((j + h) % h) * w;
        return texels[offset + (ix + w) % w] * (1 - fx) + texels[offset + (ix + 1) % w] * fx;
    };
    return row(iy) * (1 - fy) + row(iy + 1) * fy;
}

Vec3 Texture::value(UV uv, double lod) const {
    if(type==Type::constant) return color;
    if(type==Type::procedural) {
        double f=0.7+0.3*std::sin(uv.u*37+std::sin(uv.v*19));
        return color*f;
    }
    if (filter_ != Filter::nearest) {
        lod = std::clamp(lod, 0.0, static_cast<double>(last_level()));
        const int level = static_cast<int>(lod);
        const double fraction = lod - level;
        const Vec3 finer = sample_level(level, uv);
        if (fraction == 0) return finer;
        return finer * (1 - fraction) + sample_level(level + 1, uv) * fraction;
    }
    double u=uv.u-std::floor(uv.u),v=uv.v-std::floor(uv.v);
    int x=std::min(width-1,static_cast<int>(u*width));
    int y=height-1-std::min(height-1,static_cast<int>(v*height));
    return pixels.at(static_cast<std::size_t>(y)*width+x);
}
