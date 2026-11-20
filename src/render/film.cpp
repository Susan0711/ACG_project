#include "render/film.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>

void Film::write(const std::string& path,double exposure) const {
    if(std::filesystem::exists(path))throw std::runtime_error("Output already exists: "+path);
    std::ofstream out(path);
    if(!out)throw std::runtime_error("Cannot open output: "+path);
    out<<"P3\n"<<width<<' '<<height<<"\n255\n";
    auto convert=[&](double x){x=std::max(0.0,x*exposure);
        x=x/(1+x);
        x=x<=0.0031308?12.92*x:1.055*std::pow(x,1/2.4)-0.055;
        return static_cast<int>(std::lround(255*x));
    };
    for(auto c:pixels) {if(!finite(c))throw std::runtime_error("Nonfinite film pixel");
        out<<convert(c.x)<<' '<<convert(c.y)<<' '<<convert(c.z)<<'\n';
    }
    out.close();
    if(!out)throw std::runtime_error("Write failed: "+path);
}
void Film::write_linear(const std::string& path) const {
    if(std::filesystem::exists(path)) throw std::runtime_error("Output already exists: "+path);
    std::ofstream out(path,std::ios::binary);
    if(!out) throw std::runtime_error("Cannot open linear output: "+path);
    out<<"PF\n"<<width<<' '<<height<<"\n-1.0\n";
    for(int y=height-1;y>=0;--y) for(int x=0;x<width;++x) {
        Vec3 pixel=pixels[static_cast<std::size_t>(y)*width+x];
        for(double value:{pixel.x,pixel.y,pixel.z}) {
            float f=static_cast<float>(value);
            if(!std::isfinite(f)) throw std::runtime_error("PFM pixel overflow");
            std::uint32_t bits; std::memcpy(&bits,&f,4);
            unsigned char bytes[4];
            for(int i=0;i<4;++i) bytes[i]=static_cast<unsigned char>((bits>>(8*i))&255);
            out.write(reinterpret_cast<const char*>(bytes),4);
        }
    }
    out.close();
    if(!out) throw std::runtime_error("Linear output write failed");
}
