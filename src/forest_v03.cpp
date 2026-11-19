#include "tracer.h"
namespace {
struct Builder {
    World w;
    void face(Vec3 a,Vec3 b,Vec3 c,Vec3 d,int m,double scale=1) {
        w.scene.objects.push_back(Triangle{a,b,c,m,{0,0},{scale,0},{scale,1}});
        w.scene.objects.push_back(Triangle{a,c,d,m,{0,0},{scale,1},{0,1}});
    }
    void box(Vec3 p,Vec3 size,int m) {
        Vec3 x{size.x,0,0},y{0,size.y,0},z{0,0,size.z};
        face(p,p+x,p+x+y,p+y,m,size.x);
        face(p+z,p+y+z,p+x+y+z,p+x+z,m,size.x);
        face(p,p+y,p+y+z,p+z,m,size.z);
        face(p+x,p+x+z,p+x+y+z,p+x+y,m,size.z);
        face(p+y,p+x+y,p+x+y+z,p+y+z,m,size.x);
        face(p,p+z,p+x+z,p+x,m,size.x);
    }
    void branch(Vec3 a,Vec3 b,double radius,int m) {
        Vec3 axis=(b-a).normalized();
        Vec3 u=axis.cross(std::abs(axis.y)<.9?Vec3{0,1,0}:Vec3{1,0,0}).normalized();
        Vec3 v=axis.cross(u);
        for(int i=0;i<7;++i) {
            double t=2*pi*i/7, next=2*pi*(i+1)/7;
            Vec3 r=(u*std::cos(t)+v*std::sin(t))*radius;
            Vec3 q=(u*std::cos(next)+v*std::sin(next))*radius;
            face(a+r,a+q,b+q*.65,b+r*.65,m);
        }
    }
    void rock(Vec3 center,Vec3 size,double phase,int m) {
        // Three irregular rings form a weathered, flattened boulder, not a pointed octahedron.
        Vec3 rings[3][9];
        for(int j=0;j<3;++j) for(int i=0;i<9;++i) {
            double angle=2*pi*i/9+phase;
            double r=(j==1?1:.68)*(1+.12*std::sin(i*4.7+phase));
            rings[j][i]=center+Vec3{size.x*r*std::cos(angle),size.y*(j*.48-.25+.06*std::sin(i*3.1)),size.z*r*std::sin(angle)};
        }
        for(int j=0;j<2;++j) for(int i=0;i<9;++i)
            face(rings[j][i],rings[j][(i+1)%9],rings[j+1][(i+1)%9],rings[j+1][i],m);
        for(int i=1;i<8;++i) w.scene.objects.push_back(Triangle{rings[2][0],rings[2][i],rings[2][i+1],m});
    }
};
double channel(double z) { return -.8+1.4*std::sin(z*.24); }
double width(double z) { return .85+.22*std::sin(z*.41+1); }
double terrain(double x,double z) { return .18+.06*std::sin(x*.5+z*.31)+.035*std::sin(z*.9-x*.6); }
}
World make_forest_v03(bool gray,const std::string& root,bool detail) {
    Builder b;
    auto wood=std::make_shared<Texture>(); wood->load(asset_path(root,"wood.ppm"));
    b.w.materials={{Kind::diffuse,{.3,.17,.07},1.5,wood},
                   {Kind::diffuse,{.15,.19,.1},1.5},
                   {Kind::diffuse,{.065,.09,.11},1.5},
                   {Kind::mirror,{.65,.76,.81},1.5},
                   {Kind::diffuse,{.24,.27,.25},1.5},
                   {Kind::diffuse,{.045,.12,.075},1.5},
                   {Kind::diffuse,{.19,.1,.045},1.5},
                   {Kind::diffuse,{.55,.43,.28},1.5}};
    b.w.environment={.18,.24,.32};
    b.w.camera={{7,3.8,8},{1.6,1.1,-2},44};
    if(detail) b.w.camera={{6,2.8,4},{2.7,1.35,-2.4},36};
    b.w.points.push_back({{-5,11,6},{135,165,205}});
    Sampler random{7003};
    // Continuous cross sections share exact shoreline vertices. Banks extend beyond the frame.
    for(int row=0;row<72;++row) {
        double z=14-row*.5,zz=z-.5;
        double c=channel(z),cc=channel(zz),r=width(z),rr=width(zz);
        double water=-.025+.006*std::sin(z*.8), next_water=-.025+.006*std::sin(zz*.8);
        b.face({c-r,water,z},{c+r,water,z},{cc+rr,next_water,zz},{cc-rr,next_water,zz},3);
        for(int side:{-1,1}) {
            double edge=c+side*r,other=cc+side*rr;
            b.face({edge,-.08,z},{edge+side*.4,terrain(edge,z),z},
                   {other+side*.4,terrain(other,zz),zz},{other,-.08,zz},4);
            for(int strip=0;strip<10;++strip) {
                double x=edge+side*(.4+strip*2),xx=edge+side*(.4+(strip+1)*2);
                double y=other+side*(.4+strip*2),yy=other+side*(.4+(strip+1)*2);
                double variation=.85+.12*std::sin(x*.32+z*.19)+.08*std::sin(x*.83-z*.55);
                int material=static_cast<int>(b.w.materials.size());
                b.w.materials.push_back({Kind::diffuse,Vec3{.13,.17,.08}*variation,1.5});
                b.face({x,terrain(x,z),z},{xx,terrain(xx,z),z},{yy,terrain(yy,zz),zz},{y,terrain(y,zz),zz},material);
            }
        }
    }
    // Cabin on a low stone foundation, front at z=-2, interior extends backward.
    b.box({1,.12,-5},{3.4,.35,3},4);
    b.box({1,.47,-5},{3.4,.08,3},7);
    b.box({1,.55,-5},{.12,2.25,3},0);
    b.box({4.28,.55,-5},{.12,2.25,3},0);
    b.box({1,.55,-5},{3.4,2.25,.12},0);
    // Front wall strips leave actual window holes, rather than luminous wall stickers.
    b.box({1,.55,-2.12},{3.4,.55,.12},0);
    b.box({1,2.1,-2.12},{3.4,.7,.12},0);
    for(auto span:std::vector<std::pair<double,double>>{{1,.3},{2.1,1.1},{4,.4}})
        b.box({span.first,1.1,-2.12},{span.second,1,.12},0);
    b.box({2.27,.55,-1.98},{.72,1.58,.06},6);
    b.w.scene.objects.push_back(Sphere{{2.86,1.25,-1.90},.04,7});
    for(double x:{1.3,3.2}) {
        b.box({x-.05,1.05,-1.98},{.9,.07,.16},7);
        b.box({x-.05,2.08,-1.98},{.9,.07,.16},7);
        b.box({x-.05,1.1,-1.98},{.06,1,.16},7);
        b.box({x+.79,1.1,-1.98},{.06,1,.16},7);
        b.box({x+.38,1.1,-1.96},{.04,1,.12},7);
        // Lamp inside, facing toward the open window. Interior walls catch warm bounce light.
        b.w.add_area({{x+.08,1.2,-2.55},{.64,0,0},{0,.75,0},{9,3.7,1.1}});
    }
    for(int i=0;i<18;++i) {
        double x=1+i*.19;
        b.box({x,.56,-1.99},{.015,.52,.025},6);
        b.box({x,2.15,-1.99},{.015,.63,.025},6);
    }
    b.w.scene.objects.push_back(Triangle{{1,2.8,-2},{4.4,2.8,-2},{2.7,3.85,-2},0});
    b.face({.7,2.74,-1.65},{2.7,3.95,-1.65},{2.7,3.95,-5.35},{.7,2.74,-5.35},2);
    b.face({2.7,3.95,-1.65},{4.7,2.74,-1.65},{4.7,2.74,-5.35},{2.7,3.95,-5.35},2);
    b.branch({.7,2.72,-1.64},{2.7,3.93,-1.64},.055,6);
    b.branch({2.7,3.93,-1.64},{4.7,2.72,-1.64},.055,6);
    b.box({2.15,.2,-1.7},{1,.18,.5},4);
    // Varied irregular tree groups; three crown profiles, branches and actual round trunks.
    for(int i=0;i<32;++i) {
        double x=-10+20*random.next(),z=-5-14*random.next();
        if(x>0 && x<5.5 && z>-6) continue;
        double height=3.8+3.5*random.next(),radius=.7+.65*random.next(),phase=6*random.next();
        double base=terrain(x,z);
        b.branch({x,base,z},{x+.08,base+height*.86,z},.13+.05*random.next(),0);
        for(int layer=0;layer<5;++layer) {
            double y=base+height*(.25+layer*.13),r=radius*(1-layer*.15);
            for(int k=0;k<9;++k) {
                double a=2*pi*k/9+phase,n=2*pi*(k+1)/9+phase;
                Vec3 p{x+r*std::cos(a),y+.12*std::sin(k*3+layer),z+r*std::sin(a)};
                Vec3 q{x+r*std::cos(n),y+.12*std::sin((k+1)*3+layer),z+r*std::sin(n)};
                Vec3 tip{x+.1*std::sin(layer+phase),y+height*(i%3==0?.35:.24),z};
                b.w.scene.objects.push_back(Triangle{p,tip,q,5});
                if(layer<2 && k%3==0) b.branch({x,y+.2,z},p,.035,6);
            }
        }
    }
    for(int i=0;i<55;++i) {
        double z=9-23*random.next(),side=i%2?1:-1;
        double x=channel(z)+side*(width(z)+.2+.35*random.next());
        b.rock({x,.08,z},{.22+.35*random.next(),.22+.28*random.next(),.25+.45*random.next()},i*.7,4);
    }
    b.branch({-3,.4,1},{-4,.5,-2.5},.24,0);
    b.branch({-3.5,.5,-.5},{-2.9,1,-1.1},.08,6);
    for(int i=0;i<140;++i) {
        double z=8-16*random.next(),x=-7+14*random.next();
        if(std::abs(x-channel(z))<width(z)+.6 || (x>1&&x<4.5&&z>-5&&z<-1.5)) continue;
        double y=terrain(x,z);
        for(int k=0;k<3;++k) {
            double a=k*2.1+i;
            b.w.scene.objects.push_back(Triangle{{x-.06*std::cos(a),y,z-.06*std::sin(a)},
                {x+.15*std::cos(a),y+.18+.12*random.next(),z+.15*std::sin(a)},
                {x+.06*std::cos(a),y,z+.06*std::sin(a)},5});
        }
    }
    if(gray) for(auto& material:b.w.materials) if(material.kind==Kind::diffuse) {
        material.color={.45,.45,.45}; material.texture.reset();
    }
    return b.w;
}
