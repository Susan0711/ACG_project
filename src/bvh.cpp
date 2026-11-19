#include "tracer.h"
#include <functional>
#include <numeric>
#include <limits>
namespace {
    double axis(Vec3 v,int a) { return a==0?v.x:a==1?v.y:v.z;
    }
    AABB merge(AABB a,AABB b) { return {{std::min(a.lo.x,b.lo.x),std::min(a.lo.y,b.lo.y),std::min(a.lo.z,b.lo.z)},{std::max(a.hi.x,b.hi.x),std::max(a.hi.y,b.hi.y),std::max(a.hi.z,b.hi.z)}};
    }
    AABB bounds(const Sphere& s,double open,double close) { Vec3 r{s.radius,s.radius,s.radius};
        return merge({s.center_at(open)-r,s.center_at(open)+r},{s.center_at(close)-r,s.center_at(close)+r});
    }
    AABB bounds(const Triangle& t,double,double) { AABB b=merge({t.a,t.a},merge({t.b,t.b},{t.c,t.c}));
        Vec3 e{1e-9,1e-9,1e-9};
        return {b.lo-e,b.hi+e};
    }
}
bool AABB::intersect(const Ray& r,double lo,double hi) const {
    if (!finite(r.origin)||!finite(r.direction)||std::isnan(lo)||std::isnan(hi)||lo>hi) return false;
    for(int a=0;a<3;++a) { double o=axis(r.origin,a),d=axis(r.direction,a),mn=axis(this->lo,a),mx=axis(this->hi,a);
        if(d==0) { if(o<mn||o>mx)return false;
            continue;
        }
        double t0=(mn-o)/d,t1=(mx-o)/d;
        if(t0>t1)std::swap(t0,t1);
        lo=std::max(lo,t0);
        hi=std::min(hi,t1);
        if(lo>hi)return false;
    } return true;
}
void BVH::build(const Scene& scene,double time_open,double time_close) {
    nodes.clear();
    std::vector<std::size_t> ids(scene.objects.size());
    std::iota(ids.begin(),ids.end(),0);
    std::function<int(std::size_t,std::size_t)> recurse=[&](std::size_t begin,std::size_t end) {
        int index=static_cast<int>(nodes.size());
        nodes.push_back({});
        AABB box=std::visit([&](const auto& s){return bounds(s,time_open,time_close);},scene.objects[ids[begin]]);
        for(auto i=begin+1;i<end;++i)box=merge(box,std::visit([&](const auto& s){return bounds(s,time_open,time_close);},scene.objects[ids[i]]));
        nodes[index].box=box;
        if(end-begin==1)nodes[index].object=ids[begin];
        else { Vec3 extent=box.hi-box.lo;
            int a=extent.x>extent.y?0:1;
            if(extent.z>axis(extent,a))a=2;
            std::stable_sort(ids.begin()+static_cast<std::ptrdiff_t>(begin),ids.begin()+static_cast<std::ptrdiff_t>(end),[&](auto x,auto y){auto bx=std::visit([&](const auto& s){return bounds(s,time_open,time_close);},scene.objects[x]);auto by=std::visit([&](const auto& s){return bounds(s,time_open,time_close);},scene.objects[y]);return axis(bx.lo+bx.hi,a)<axis(by.lo+by.hi,a);});
            auto mid=begin+(end-begin)/2;
            int l=recurse(begin,mid),r=recurse(mid,end);
            nodes[index].left=l;
            nodes[index].right=r;
        }return index;
    };
    if(!ids.empty())recurse(0,ids.size());
}
bool BVH::intersect(const Scene& scene,const Ray& ray,double lo,double hi,HitRecord& hit) const {
    bool found=false;
    std::size_t best=std::numeric_limits<std::size_t>::max();
    auto visit=[&](auto&& self,int index)->void { const auto& n=nodes[index];
        if(!n.box.intersect(ray,lo,hi))return;
        if(n.left<0) { HitRecord h;
            if(std::visit([&](const auto& s){return s.intersect(ray,lo,hi,h);},scene.objects[n.object])&&(!found||h.t<hi||(h.t==hi&&n.object<best))) {found=true;
                hi=h.t;
                hit=h;
                best=n.object;
            } }
        else {self(self,n.left);
            self(self,n.right);
        }
    };
    if(!nodes.empty())visit(visit,0);
    return found;
}
