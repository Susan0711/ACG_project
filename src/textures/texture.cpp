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

void Texture::load(const std::filesystem::path& path, bool decode_srgb) {
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

    pixels = std::move(image);
    width = image_width;
    height = image_height;
    type = Type::image;
}

Vec3 Texture::value(UV uv) const {
    if(type==Type::constant) return color;
    if(type==Type::procedural) {
        double f=0.7+0.3*std::sin(uv.u*37+std::sin(uv.v*19));
        return color*f;
    }
    double u=uv.u-std::floor(uv.u),v=uv.v-std::floor(uv.v);
    int x=std::min(width-1,static_cast<int>(u*width));
    int y=height-1-std::min(height-1,static_cast<int>(v*height));
    return pixels.at(static_cast<std::size_t>(y)*width+x);
}
