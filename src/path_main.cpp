#include "render.h"
#include <charconv>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv) {try {
        int width=256,height=256,spp=64,depth=16,threads=1;
        double lens_radius=0, focus_distance=0, shutter_open=0, shutter_close=0;
        bool focus_set=false;
        SamplingMode mode=SamplingMode::nee;
        std::string environment_file,linear_output;
        double rotation=0,intensity=1,exposure=1;
        std::uint64_t seed=1;
        bool accelerated=true,direct=true;
        std::string scene="cornell",output="path.ppm",resource_root="assets";
        auto number=[](const std::string& s,std::uint64_t maximum){std::uint64_t n=0;
            auto r=std::from_chars(s.data(),s.data()+s.size(),n);
            if(r.ec!=std::errc{}||r.ptr!=s.data()+s.size()||n>maximum)throw std::runtime_error("Invalid integer: "+s);
            return n;
        };
        auto real=[](const std::string& s) {
            std::size_t used=0;
            double value=std::stod(s,&used);
            if(used!=s.size() || !std::isfinite(value)) throw std::runtime_error("Invalid real: "+s);
            return value;
        };
        for(int i=1;i<argc;++i){std::string key=argv[i];
            if(key=="--help"){std::cout<<"path_tracer --scene cornell|point|forest|forest-gray|forest-v03|forest-v03-gray|forest-v03-detail|texture|dof|motion|alpha-zero|alpha-half|alpha-one|alpha-double|alpha-pattern|sampling --width N --height N --spp N --seed N --depth N --accel bvh|linear --direct on|off --lens-radius R --focus-distance D --shutter-open T --shutter-close T --mode bsdf|nee|mis --environment FILE --env-rotation DEGREES --env-intensity S --exposure S --linear-output FILE --threads N --resource-root DIR --output PATH\n";
                return 0;
            }if(++i==argc)throw std::runtime_error("Missing value: "+key);
            std::string value=argv[i];
            if(key=="--width")width=static_cast<int>(number(value,8192));
            else if(key=="--height")height=static_cast<int>(number(value,8192));
            else if(key=="--spp")spp=static_cast<int>(number(value,10000000));
            else if(key=="--depth")depth=static_cast<int>(number(value,1024));
            else if(key=="--environment") environment_file=value;
            else if(key=="--env-rotation") rotation=std::fmod(real(value),360.0)*pi/180;
            else if(key=="--env-intensity") intensity=real(value);
            else if(key=="--exposure") exposure=real(value);
            else if(key=="--linear-output") linear_output=value;
            else if(key=="--mode") {
                if(value=="bsdf") mode=SamplingMode::bsdf;
                else if(value=="nee") mode=SamplingMode::nee;
                else if(value=="mis") mode=SamplingMode::mis;
                else throw std::runtime_error("Invalid sampling mode");
            }
            else if(key=="--threads") threads=static_cast<int>(number(value,256));
            else if(key=="--resource-root") resource_root=value;
            else if(key=="--seed")seed=number(value,UINT64_MAX);
            else if(key=="--lens-radius") lens_radius=real(value);
            else if(key=="--focus-distance") { focus_distance=real(value); focus_set=true; }
            else if(key=="--shutter-open") shutter_open=real(value);
            else if(key=="--shutter-close") shutter_close=real(value);
            else if(key=="--scene")scene=value;
            else if(key=="--output")output=value;
            else if(key=="--accel"){if(value!="bvh"&&value!="linear")throw std::runtime_error("Invalid accel");
                accelerated=value=="bvh";
            }else if(key=="--direct"){if(value!="on"&&value!="off")throw std::runtime_error("Invalid direct mode");
                direct=value=="on";
            }else throw std::runtime_error("Unknown option: "+key);
        }
        if(width<1||height<1||spp<1||depth<1||threads<1)throw std::runtime_error("Dimensions, spp, depth and threads must be positive");
        if(scene!="cornell"&&scene!="point"&&scene!="forest"&&scene!="forest-gray"&&scene!="texture"&&scene!="dof"&&scene!="motion"&&scene!="alpha-zero"&&scene!="alpha-half"&&scene!="alpha-one"&&scene!="alpha-double"&&scene!="alpha-pattern"&&scene!="forest-v03"&&scene!="forest-v03-gray"&&scene!="forest-v03-detail"&&scene!="sampling")throw std::runtime_error("Unknown scene");
        if(std::filesystem::exists(output))throw std::runtime_error("Output already exists: "+output);
        if(intensity<0 || exposure<=0) throw std::runtime_error("Environment intensity must be nonnegative, exposure positive");
        if(!linear_output.empty() && (linear_output==output || std::filesystem::exists(linear_output))) throw std::runtime_error("Linear output exists or equals display output");
        auto start=std::chrono::steady_clock::now();
        World world;
        if(scene=="forest" || scene=="forest-gray") world=make_forest(scene=="forest-gray",resource_root,false);
        if(scene=="cornell" || scene=="point") world=make_world(scene=="point",false);
        if(scene=="texture") {
        world=World{};
        auto texture=std::make_shared<Texture>();
        texture->load(asset_path(resource_root,"wood.ppm"));
        world.materials.push_back({Kind::diffuse,{1,1,1},1.5,texture});
        world.scene.objects.push_back(Triangle{{-1,-1,0},{1,-1,0},{1,1,0},0,{0,0},{1,0},{1,1}});
        world.scene.objects.push_back(Triangle{{-1,-1,0},{1,1,0},{-1,1,0},0,{0,0},{1,1},{0,1}});
        world.environment={1,1,1};
        world.camera={{0,0,3},{0,0,0},40};
    }
    if(scene=="dof" || scene=="motion" || scene.rfind("alpha-",0)==0) world=make_effect_scene(scene,resource_root);
    if(scene.rfind("forest-v03",0)==0) world=make_forest_v03(scene=="forest-v03-gray",resource_root,scene=="forest-v03-detail");
    if(scene=="sampling") world=make_sampling_scene();
    if(!environment_file.empty()) {
        world.hdr=std::make_shared<Environment>();
        world.hdr->load(asset_path(resource_root,environment_file));
        world.hdr->rotation=rotation;
        world.hdr->intensity=intensity;
    }
    double load_seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    world.camera.lens_radius=lens_radius;
    if(focus_set) world.camera.focus_distance=focus_distance;
    world.camera.shutter_open=shutter_open;
    world.camera.shutter_close=shutter_close;
    world.camera.validate();
    auto build_start=std::chrono::steady_clock::now();
        world.bvh.build(world.scene,shutter_open,shutter_close);
        double build_seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-build_start).count();
        std::size_t triangles=0;
        for(const auto& object:world.scene.objects) if(std::holds_alternative<Triangle>(object)) ++triangles;
        start=std::chrono::steady_clock::now();
        Film film(width,height);
        render(world,film,{spp,depth,threads,seed,direct,accelerated,mode});
        double render_seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        Vec3 mean{};
        for(Vec3 pixel:film.pixels) mean=mean+pixel;
        start=std::chrono::steady_clock::now();
        film.write(output,exposure);
        if(!linear_output.empty()) film.write_linear(linear_output);
        double write_seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        std::cout<<"triangles="<<triangles<<" bvh_seconds="<<build_seconds<<"\n";
        std::cout<<"threads="<<threads<<" load_seconds="<<load_seconds<<" render_seconds="<<render_seconds<<" write_seconds="<<write_seconds<<"\n";
        mean=mean*(1.0/(double(width)*height));
        std::cout<<width<<'x'<<height<<" spp="<<spp<<" depth="<<depth<<" seed="<<seed<<" mean_linear="<<mean.x<<','<<mean.y<<','<<mean.z<<" output="<<output<<'\n';
    }catch(const std::exception& e){std::cerr<<"Error: "<<e.what()<<'\n';
        return 1;
    }}
