#pragma once
#include <cstdint>

struct Sampler {
    std::uint64_t state;
    double next() { state+=0x9e3779b97f4a7c15ULL;
        auto z=state;
        z=(z^(z>>30))*0xbf58476d1ce4e5b9ULL;
        z=(z^(z>>27))*0x94d049bb133111ebULL;
        return double((z^(z>>31))>>11)*0x1.0p-53;
    }
};
