#include "io/assets.h"
#include "factories.h"
World make_forest(bool gray,const std::string& root) {
    World w;
    w.lights.background={0.15,0.22,0.34};
    w.camera={{6,4.2,8},{0,1,-1},44};
    auto wood=std::make_shared<Texture>();
    wood->load(asset_path(root,"wood.ppm"));
    auto ground=std::make_shared<Texture>();
    ground->type=Texture::Type::procedural;
    ground->color={0.13,0.19,0.075};
    w.materials={{Kind::diffuse,{0.3,0.2,0.1},1.5,wood},
                 {Kind::diffuse,{0.12,0.18,0.08},1.5,ground},
                 {Kind::diffuse,{0.065,0.11,0.14},1.5},
                 {Kind::mirror,{0.65,0.77,0.85},1.5},
                 {Kind::diffuse,{0.28,0.31,0.3},1.5},
                 {Kind::diffuse,{0.04,0.15,0.065},1.5}};
    auto quad=[&](Vec3 a,Vec3 u,Vec3 v,int m) {
        w.scene.objects.push_back(Triangle{a,a+u,a+u+v,m,{0,0},{1,0},{1,1}});
        w.scene.objects.push_back(Triangle{a,a+u+v,a+v,m,{0,0},{1,1},{0,1}});
    };
    // Two banks leave an actual opening for the lower mirror creek.
    for(int side=0;side<2;++side) {
        double z0=side==0?0:5.5;
        double extent=side==0?7:4;
        for(int x=0;x<24;++x) for(int z=0;z<12;++z) {
            double xx=-8+x*(16.0/24),zz=z0-z*extent/12;
            quad({xx,0,zz},{16.0/24,0,0},{0,0,-extent/12},1);
        }
    }
    quad({-8,-0.08,1.5},{16,0,0},{0,0,-1.5},3);
    // Cabin front, sides, back and pitched roof.
    quad({-1.4,0,-1},{2.8,0,0},{0,2,0},0);
    quad({-1.4,0,-3.3},{0,0,2.3},{0,2,0},0);
    quad({1.4,0,-1},{0,0,-2.3},{0,2,0},0);
    quad({1.4,0,-3.3},{-2.8,0,0},{0,2,0},0);
    quad({-1.65,1.95,-0.8},{1.65,1.05,0},{0,0,-2.7},2);
    quad({0,3,-0.8},{1.65,-1.05,0},{0,0,-2.7},2);
    w.scene.objects.push_back(Triangle{{-1.4,2,-1},{1.4,2,-1},{0,3,-1},0});
    w.add_area({{-0.95,0.85,-0.985},{0.65,0,0},{0,0.8,0},{7,2.5,0.55}});
    w.add_area({{0.35,0.85,-0.985},{0.65,0,0},{0,0.8,0},{7,2.5,0.55}});
    // Cool outdoor fill is a finite point light, plus escape-only sky radiance.
    w.lights.points.push_back({{-3,7,4},{65,85,115}});
    for(int i=0;i<9;++i) {
        double x=(i%2==0?-1:1)*(2.3+(i%4)*0.8),z=-2-(i/2)*1.2;
        quad({x-0.12,0,z},{0.24,0,0},{0,2.7,0},0);
        for(int layer=0;layer<3;++layer) {
            double base=0.8+layer*0.7,radius=1-layer*0.2;
            for(int k=0;k<10;++k) {
                double a=2*pi*k/10,b=2*pi*(k+1)/10;
                w.scene.objects.push_back(Triangle{{x+radius*std::cos(a),base,z+radius*std::sin(a)},
                    {x,base+1.8,z},{x+radius*std::cos(b),base,z+radius*std::sin(b)},5});
            }
        }
    }
    for(int i=0;i<8;++i) {
        Transform t;
        t.scale={0.35+0.07*(i%3),0.4,0.3};
        t.translation={-3.5+i,0.1,i%2==0?0.05:1.5};
        t.rotation=i*0.7;
        load_obj(w.scene,asset_path(root,"rock.obj"),4,t);
    }
    if(gray) for(auto& m:w.materials) if(m.kind==Kind::diffuse) {
        m.color={0.45,0.45,0.45}; m.texture.reset();
    }
    return w;
}
