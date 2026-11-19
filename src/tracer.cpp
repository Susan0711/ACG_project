#include "tracer.h"
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <limits>
#include <cstring>
Ray Camera::generate(double u,double v,double aspect) const {
    validate();
    if(!std::isfinite(aspect) || aspect<=0) throw std::runtime_error("Camera aspect must be positive");
    Vec3 forward=(target-origin).normalized(),right=forward.cross({0,1,0}).normalized(),up=right.cross(forward);
    double scale=std::tan(fov*pi/360);
    return {origin,(forward+right*((2*u-1)*aspect*scale)+up*((1-2*v)*scale)).normalized()};
}
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
double fresnel(double c,double ei,double et) {
    c=std::clamp(c,0.0,1.0);
    double st2=(ei/et)*(ei/et)*(1-c*c);
    if(st2>=1)return 1;
    double ct=std::sqrt(1-st2),rs=(ei*c-et*ct)/(ei*c+et*ct),rp=(et*c-ei*ct)/(et*c+ei*ct);
    return (rs*rs+rp*rp)*0.5;
}
BSDFSample sample_bsdf(const Material& m,const Ray& ray,const HitRecord& h,Sampler& rng) {
    Vec3 d=ray.direction.normalized(),n=h.normal;
    if(m.kind==Kind::diffuse) {
        double u=rng.next(),phi=2*pi*rng.next();
        Vec3 tangent=(std::abs(n.x)>0.9?Vec3{0,1,0}:Vec3{1,0,0}).cross(n).normalized(),bitangent=n.cross(tangent);
        Vec3 wi=(tangent*(std::sqrt(u)*std::cos(phi))+bitangent*(std::sqrt(u)*std::sin(phi))+n*std::sqrt(1-u)).normalized();
        return {wi,m.albedo(h.uv),std::max(0.0,n.dot(wi))/pi,false};
    }
    Vec3 reflection=(d-n*(2*d.dot(n))).normalized();
    if(m.kind==Kind::mirror)return {reflection,m.color,1,true};
    if(m.kind==Kind::glass) {
        double ei=h.front_face?1:m.ior,et=h.front_face?m.ior:1,eta=ei/et,c=std::clamp(-d.dot(n),0.0,1.0),f=fresnel(c,ei,et);
        if(rng.next()<f)return {reflection,{1,1,1},f,true};
        Vec3 perpendicular=(d+n*c)*eta;
        Vec3 transmission=(perpendicular-n*std::sqrt(std::max(0.0,1-perpendicular.dot(perpendicular)))).normalized();
        return {transmission,Vec3{1,1,1}*(eta*eta),1-f,true};
    }
    return {{0,0,0},{0,0,0},0,false};
}
Ray spawn(const HitRecord& h,Vec3 direction) {return {h.position+h.outward_normal*(direction.dot(h.outward_normal)>=0?1e-5:-1e-5),direction,h.time};
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
