#include "io/assets.h"
#include <cmath>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <stdexcept>

namespace {
int index(const std::string& s,std::size_t count) {
    std::size_t used=0;
    int i=std::stoi(s,&used);
    if(used!=s.size()) throw std::runtime_error("Invalid integer: "+s);
    int result=i>0?i-1:static_cast<int>(count)+i;
    if(i==0 || result<0 || result>=static_cast<int>(count)) throw std::runtime_error("OBJ index out of range");
    return result;
}
}
Vec3 Transform::point(Vec3 p) const {
    p={p.x*scale.x,p.y*scale.y,p.z*scale.z};
    return Vec3{std::cos(rotation)*p.x+std::sin(rotation)*p.z,p.y,-std::sin(rotation)*p.x+std::cos(rotation)*p.z}+translation;
}
Vec3 Transform::normal(Vec3 n) const {
    n={n.x/scale.x,n.y/scale.y,n.z/scale.z};
    return Vec3{std::cos(rotation)*n.x+std::sin(rotation)*n.z,n.y,-std::sin(rotation)*n.x+std::cos(rotation)*n.z}.normalized();
}
void load_obj(Scene& scene, const std::filesystem::path& path, int material,
              const Transform& transform, Vec3 velocity) {
    if(transform.scale.x==0 || transform.scale.y==0 || transform.scale.z==0) throw std::runtime_error("Singular OBJ transform");
    std::ifstream in(path);
    if(!in) throw std::runtime_error("Cannot open OBJ: "+path.u8string());
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
                std::array<Vec3, 3> face_normals{};
                int normal_count = 0, uv_count = 0;
                for(int i=0;i<3;++i) {
                    std::string s=vertices[i]; auto slash=s.find('/');
                    p[i]=positions.at(index(s.substr(0,slash),positions.size()));
                    if(slash!=std::string::npos) {
                        auto second=s.find('/',slash+1);
                        auto t=s.substr(slash+1,second==std::string::npos?second:second-slash-1);
                        if(!t.empty()) {
                            uv[i]=texcoords.at(index(t,texcoords.size()));
                            ++uv_count;
                        }
                        if(second!=std::string::npos) {
                            face_normals[i] = normals.at(index(s.substr(second+1), normals.size()));
                            ++normal_count;
                        }
                        else if(t.empty()) throw std::runtime_error("Empty texture index");
                    }
                }
                Triangle t{p[0],p[1],p[2],material,uv[0],uv[1],uv[2]};
                t.has_complete_uvs = uv_count == 3;
                if (normal_count == 3) t.corner_normals = face_normals;
                t.velocity = velocity;
                if((t.b-t.a).cross(t.c-t.a).dot((t.b-t.a).cross(t.c-t.a))==0) throw std::runtime_error("Degenerate face");
                triangles.push_back(t);
            } else if(key!="o" && key!="g" && key!="s") {
                throw std::runtime_error("Unsupported OBJ directive: "+key);
            }
        }
    } catch(const std::exception& e) {
        throw std::runtime_error(path.u8string()+":"+std::to_string(line_number)+": "+e.what());
    }
    for(const auto& t:triangles) scene.objects.push_back(t);
}

std::string asset_path(const std::string& root,const std::string& name) {
    auto path=std::filesystem::path(root)/name;
    if(!std::filesystem::is_regular_file(path))
        throw std::runtime_error("Asset not found: "+path.string()+"; set --resource-root to the assets directory");
    return path.string();
}
