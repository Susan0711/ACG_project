#include "io/assets.h"
#include "factories.h"
World make_effect_scene(const std::string& name,const std::string& root) {
    World w;
    w.lights.background={0.08,0.08,0.08};
    w.camera={{0,1,6},{0,1,0},38};
    w.camera.focus_distance=6;
    w.materials={{Kind::diffuse,{0.75,0.15,0.08},1.5},
                 {Kind::diffuse,{0.12,0.65,0.18},1.5},
                 {Kind::diffuse,{0.12,0.25,0.8},1.5},
                 {Kind::diffuse,{0.7,0.7,0.7},1.5}};
    auto quad=[&](Vec3 a,Vec3 u,Vec3 v,int material) {
        w.scene.objects.push_back(Triangle{a,a+u,a+u+v,material,{0,0},{1,0},{1,1}});
        w.scene.objects.push_back(Triangle{a,a+u+v,a+v,material,{0,0},{1,1},{0,1}});
    };
    quad({-6,0,4},{12,0,0},{0,0,-12},3);
    w.lights.points.push_back({{-3,6,4},{65,65,65}});
    if(name=="dof") {
        w.scene.objects.push_back(Sphere{{-1,0.65,2},0.65,0});
        w.scene.objects.push_back(Sphere{{0,0.65,0},0.65,1});
        w.scene.objects.push_back(Sphere{{1.5,0.65,-2},0.65,2});
    } else if(name=="motion") {
        w.scene.objects.push_back(Sphere{{-1,0.7,0},0.7,0,{2,0,0}});
    } else {
        w.camera={{4,4,6},{0,0.3,0},42};
        w.lights.points={{{-2,5,1},{60,60,60}}};
        Material sheet{Kind::diffuse,{0.12,0.4,0.13},1.5};
        if(name=="alpha-pattern") {
            auto mask=std::make_shared<Texture>();
            mask->load(asset_path(root,"leaf-opacity.ppm"),false);
            sheet.opacity_map=mask;
        } else if(name=="alpha-zero") sheet.opacity=0;
        else if(name=="alpha-half" || name=="alpha-double") sheet.opacity=0.5;
        w.materials.push_back(sheet);
        quad({-1.3,1,-1.3},{2.6,0,0},{0,0,2.6},4);
        if(name=="alpha-double") quad({-1.3,1.2,-1.3},{2.6,0,0},{0,0,2.6},4);
    }
    return w;
}
