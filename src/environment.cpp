#include "environment.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <cstdint>
namespace {
constexpr double pi=3.14159265358979323846;
double unit(double x) { return std::clamp(x,0.0,std::nextafter(1.0,0.0)); }
}
void Environment::load(const std::string& path) {
    std::ifstream in(path,std::ios::binary);
    if(!in) throw std::runtime_error("Cannot open floating HDR environment: "+path);
    std::string magic,line;
    std::getline(in,magic);
    if(!magic.empty() && magic.back()=='\r') magic.pop_back();
    if(magic!="PF") throw std::runtime_error("Environment requires RGB float PFM (PF)");
    std::getline(in,line); std::istringstream dimensions(line);
    if(!(dimensions>>width>>height) || width<1 || height<1 || width>16384 || height>8192)
        throw std::runtime_error("Invalid PFM dimensions");
    std::getline(in,line); double scale=std::stod(line);
    if(!std::isfinite(scale) || scale==0) throw std::runtime_error("Invalid PFM scale");
    std::uint16_t endian=1;
    bool host_little=*reinterpret_cast<unsigned char*>(&endian)==1;
    pixels.resize(static_cast<std::size_t>(width)*height);
    for(int y=height-1;y>=0;--y) for(int x=0;x<width;++x) {
        double values[3];
        for(double& value:values) {
            unsigned char bytes[4];
            if(!in.read(reinterpret_cast<char*>(bytes),4)) throw std::runtime_error("Truncated PFM");
            if(host_little!=(scale<0)) std::reverse(bytes,bytes+4);
            float f; std::memcpy(&f,bytes,4); value=double(f)*std::abs(scale);
            if(!std::isfinite(value) || value<0) throw std::runtime_error("HDR radiance must be finite and nonnegative");
        }
        pixels[static_cast<std::size_t>(y)*width+x]={values[0],values[1],values[2]};
    }
    prepare();
}
double Environment::solid_angle(int row) const {
    return (2*pi/width)*(std::cos(pi*row/height)-std::cos(pi*(row+1)/height));
}
void Environment::prepare() {
    if(width<1 || height<1 || pixels.size()!=static_cast<std::size_t>(width)*height)
        throw std::runtime_error("Invalid environment image");
    probabilities.resize(pixels.size()); cdf.resize(pixels.size());
    double total=0;
    for(std::size_t i=0;i<pixels.size();++i) {
        Vec3 p=pixels[i];
        if(!std::isfinite(p.x+p.y+p.z) || std::min({p.x,p.y,p.z})<0) throw std::runtime_error("Invalid environment radiance");
        probabilities[i]=(.2126*p.x+.7152*p.y+.0722*p.z)*solid_angle(static_cast<int>(i)/width);
        total+=probabilities[i];
    }
    if(!std::isfinite(total)) throw std::runtime_error("Environment distribution overflow");
    double sum=0;
    for(std::size_t i=0;i<pixels.size();++i) {
        probabilities[i]=total>0?probabilities[i]/total:solid_angle(static_cast<int>(i)/width)/(4*pi);
        sum+=probabilities[i]; cdf[i]=sum;
    }
    cdf.back()=1;
}
std::size_t Environment::cell(Vec3 direction) const {
    direction=direction.normalized();
    double phi=std::atan2(direction.z,direction.x)-rotation;
    double u=phi/(2*pi); u-=std::floor(u);
    double theta=std::acos(std::clamp(direction.y,-1.0,1.0));
    int x=std::min(width-1,static_cast<int>(u*width));
    int y=std::min(height-1,static_cast<int>(theta/pi*height));
    return static_cast<std::size_t>(y)*width+x;
}
Vec3 Environment::evaluate(Vec3 direction) const { return pixels.at(cell(direction))*intensity; }
double Environment::pdf(Vec3 direction) const {
    auto i=cell(direction);
    return probabilities.at(i)/solid_angle(static_cast<int>(i)/width);
}
EnvironmentSample Environment::sample(double select,double azimuth,double elevation) const {
    auto it=std::upper_bound(cdf.begin(),cdf.end(),unit(select));
    std::size_t i=std::min(static_cast<std::size_t>(it-cdf.begin()),cdf.size()-1);
    int x=static_cast<int>(i)%width,y=static_cast<int>(i)/width;
    double phi=2*pi*(x+unit(azimuth))/width+rotation;
    double c0=std::cos(pi*y/height),c1=std::cos(pi*(y+1)/height);
    double cosine=c0+(c1-c0)*std::clamp(elevation,1e-12,1-1e-12);
    double sine=std::sqrt(std::max(0.0,1-cosine*cosine));
    Vec3 direction{sine*std::cos(phi),cosine,sine*std::sin(phi)};
    return {direction,pixels[i]*intensity,probabilities[i]/solid_angle(y)};
}
