#include "assets.h"
#include <fstream>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <algorithm>
namespace {
std::string token(std::istream& in) {
    std::string s;
    while(in>>s) {
        if(s[0]=='#') { std::getline(in,s); continue; }
        return s;
    }
    throw std::runtime_error("Unexpected end of PPM");
}
int integer(const std::string& s) {
    std::size_t used=0;
    int value=std::stoi(s,&used);
    if(used!=s.size()) throw std::runtime_error("Invalid integer: "+s);
    return value;
}
double linear(double c) {
    return c<=0.04045 ? c/12.92 : std::pow((c+0.055)/1.055,2.4);
}
int index(const std::string& s,std::size_t count) {
    int i=integer(s);
    int result=i>0?i-1:static_cast<int>(count)+i;
    if(i==0 || result<0 || result>=static_cast<int>(count)) throw std::runtime_error("OBJ index out of range");
    return result;
}
}
void Texture::load(const std::string& path,bool decode_srgb) {
    std::ifstream in(path);
    if(!in) throw std::runtime_error("Cannot open texture: "+path);
    if(token(in)!="P3") throw std::runtime_error("Only ASCII P3 PPM supported");
    width=integer(token(in)); height=integer(token(in));
    int maximum=integer(token(in));
    if(width<1 || height<1 || width>8192 || height>8192 || maximum<1 || maximum>65535)
        throw std::runtime_error("Invalid PPM dimensions/range");
    pixels.clear();
    for(int i=0;i<width*height;++i) {
        double channels[3];
        for(double& c:channels) {
            int value=integer(token(in));
            if(value<0 || value>maximum) throw std::runtime_error("PPM sample out of range");
            c=decode_srgb?linear(double(value)/maximum):double(value)/maximum;
        }
        pixels.push_back({channels[0],channels[1],channels[2]});
    }
    type=Type::image;
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
Vec3 Transform::point(Vec3 p) const {
    p={p.x*scale.x,p.y*scale.y,p.z*scale.z};
    return Vec3{std::cos(rotation)*p.x+std::sin(rotation)*p.z,p.y,-std::sin(rotation)*p.x+std::cos(rotation)*p.z}+translation;
}
Vec3 Transform::normal(Vec3 n) const {
    n={n.x/scale.x,n.y/scale.y,n.z/scale.z};
    return Vec3{std::cos(rotation)*n.x+std::sin(rotation)*n.z,n.y,-std::sin(rotation)*n.x+std::cos(rotation)*n.z}.normalized();
}
void load_obj(Scene& scene,const std::string& path,int material,const Transform& transform) {
    if(transform.scale.x==0 || transform.scale.y==0 || transform.scale.z==0) throw std::runtime_error("Singular OBJ transform");
    std::ifstream in(path);
    if(!in) throw std::runtime_error("Cannot open OBJ: "+path);
    std::vector<Vec3> positions,normals;
    std::vector<UV> texcoords;
    std::vector<Triangle> triangles;
    std::string line;
    int line_number=0;
    try {
        while(std::getline(in,line)) {
            ++line_number;
            line=line.substr(0,line.find('#'));
            std::istringstream row(line);
            std::string key;
            if(!(row>>key)) continue;
            if(key=="v" || key=="vn") {
                Vec3 p; std::string extra;
                if(!(row>>p.x>>p.y>>p.z) || row>>extra || !std::isfinite(p.x+p.y+p.z)) throw std::runtime_error("Invalid vector");
                if(key=="v") positions.push_back(transform.point(p));
                else {
                    if(p.dot(p)==0) throw std::runtime_error("Zero normal");
                    normals.push_back(transform.normal(p));
                }
            } else if(key=="vt") {
                UV uv; std::string extra;
                if(!(row>>uv.u>>uv.v) || row>>extra || !std::isfinite(uv.u+uv.v)) throw std::runtime_error("Expected two-component vt");
                texcoords.push_back(uv);
            } else if(key=="f") {
                std::string vertices[3],extra;
                if(!(row>>vertices[0]>>vertices[1]>>vertices[2]) || row>>extra) throw std::runtime_error("Only triangular faces supported");
                Vec3 p[3]; UV uv[3];
                for(int i=0;i<3;++i) {
                    std::string s=vertices[i]; auto slash=s.find('/');
                    p[i]=positions.at(index(s.substr(0,slash),positions.size()));
                    if(slash!=std::string::npos) {
                        auto second=s.find('/',slash+1);
                        auto t=s.substr(slash+1,second==std::string::npos?second:second-slash-1);
                        if(!t.empty()) uv[i]=texcoords.at(index(t,texcoords.size()));
                        if(second!=std::string::npos) (void)normals.at(index(s.substr(second+1),normals.size()));
                        else if(t.empty()) throw std::runtime_error("Empty texture index");
                    }
                }
                // Flat shading intentionally uses transformed geometric normals; vn is validated, not interpolated.
                Triangle t{p[0],p[1],p[2],material,uv[0],uv[1],uv[2]};
                if((t.b-t.a).cross(t.c-t.a).dot((t.b-t.a).cross(t.c-t.a))==0) throw std::runtime_error("Degenerate face");
                triangles.push_back(t);
            } else if(key!="o" && key!="g" && key!="s") {
                throw std::runtime_error("Unsupported OBJ directive: "+key);
            }
        }
    } catch(const std::exception& e) {
        throw std::runtime_error(path+":"+std::to_string(line_number)+": "+e.what());
    }
    for(const auto& t:triangles) scene.objects.push_back(t);
}

std::string asset_path(const std::string& root,const std::string& name) {
    auto path=std::filesystem::path(root)/name;
    if(!std::filesystem::is_regular_file(path))
        throw std::runtime_error("Asset not found: "+path.string()+"; set --resource-root to the assets directory");
    return path.string();
}
