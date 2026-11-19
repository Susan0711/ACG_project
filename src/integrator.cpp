#include "tracer.h"
#include <limits>
#include <stdexcept>
namespace {
constexpr double infinity=std::numeric_limits<double>::infinity();
double area_pdf(const AreaLight& light,Vec3 from,Vec3 to) {
    Vec3 delta=to-from,cross=light.u.cross(light.v);
    double area=std::sqrt(cross.dot(cross));
    double cosine=cross.normalized().dot(delta.normalized()*(-1));
    return cosine>0 && area>0 ? delta.dot(delta)/(area*cosine):0;
}
double hit_weight(SamplingMode mode,bool delta,double bsdf_pdf,double light_pdf,bool covered) {
    if(mode==SamplingMode::bsdf || delta || !covered) return 1;
    return mode==SamplingMode::nee?0:power_heuristic(bsdf_pdf,light_pdf);
}
Vec3 direct_lighting(const World& world,const HitRecord& h,const Material& material,
                     Sampler& rng,bool accelerated,SamplingMode mode) {
    Vec3 result{};
    // Enumerate all light techniques once: selection probability is 1, not 1/N.
    auto add=[&](Vec3 wi,Vec3 radiance,double pdf,double distance,bool delta) {
        double cosine=std::max(0.0,h.normal.dot(wi));
        if(cosine==0 || pdf<=0) return;
        Ray shadow=spawn(h,wi);
        if(std::isfinite(distance)) {
            Vec3 target=h.position+wi*distance;
            Vec3 offset=target-shadow.origin;
            distance=std::sqrt(offset.dot(offset)); shadow.direction=offset*(1/distance);
            distance-=1e-5;
        }
        double visibility=shadow_transmittance(world,shadow,distance,accelerated);
        double weight=(mode==SamplingMode::mis && !delta)?power_heuristic(pdf,cosine/pi):1;
        result=result+multiply(material.albedo(h.uv),radiance)*(cosine*visibility*weight/(pi*pdf));
    };
    for(const auto& light:world.points) {
        Vec3 delta=light.position-h.position;
        double distance=std::sqrt(delta.dot(delta));
        if(distance>0) add(delta*(1/distance),light.intensity*(1/(distance*distance)),1,distance,true);
    }
    for(const auto& light:world.areas) {
        double u=rng.next(),v=rng.next();
        Vec3 target=light.origin+light.u*u+light.v*v,delta=target-h.position;
        add(delta.normalized(),light.radiance,area_pdf(light,h.position,target),std::sqrt(delta.dot(delta)),false);
    }
    if(world.hdr) {
        double select=rng.next(),azimuth=rng.next(),elevation=rng.next();
        auto sample=world.hdr->sample(select,azimuth,elevation);
        add(sample.direction,sample.radiance,sample.pdf,infinity,false);
    }
    return result;
}
}
double power_heuristic(double a,double b) {
    if(a<=0) return 0;
    if(b<=0) return 1;
    double ratio=b/a;
    return 1/(1+ratio*ratio);
}
Vec3 integrate(const World& world,Ray ray,Sampler& rng,int depth,bool direct,bool accelerated,SamplingMode mode) {
    if(!direct) mode=SamplingMode::bsdf;
    Vec3 result{},throughput{1,1,1},previous_position{};
    bool previous_delta=true;
    double previous_pdf=0;
    int crossings=0;
    for(int bounce=0;bounce<=depth;) {
        HitRecord hit;
        if(!world.intersect(ray,0,infinity,hit,accelerated)) {
            Vec3 radiance=world.hdr?world.hdr->evaluate(ray.direction):world.environment;
            double weight=hit_weight(mode,previous_delta,previous_pdf,world.hdr?world.hdr->pdf(ray.direction):0,bool(world.hdr));
            result=result+multiply(throughput,radiance)*weight;
            break;
        }
        const auto& material=world.materials.at(static_cast<std::size_t>(hit.material_id));
        double alpha=material.alpha(hit.uv);
        if(alpha<1 && (alpha==0 || rng.next()>=alpha)) {
            if(++crossings>4096) throw std::runtime_error("Alpha crossing limit exceeded");
            ray.origin=ray.at(std::nextafter(hit.t,infinity))+ray.direction*1e-7;
            continue;
        }
        if(material.kind==Kind::emissive) {
            if(hit.front_face) {
                bool covered=hit.light_id>=0;
                double pdf=covered?area_pdf(world.areas.at(static_cast<std::size_t>(hit.light_id)),previous_position,hit.position):0;
                double weight=hit_weight(mode,previous_delta,previous_pdf,pdf,covered);
                result=result+multiply(throughput,material.color)*weight;
            }
            break;
        }
        if(bounce==depth) break;
        if(mode!=SamplingMode::bsdf && material.kind==Kind::diffuse)
            result=result+multiply(throughput,direct_lighting(world,hit,material,rng,accelerated,mode));
        auto sampled=sample_bsdf(material,ray,hit,rng);
        throughput=multiply(throughput,sampled.weight);
        previous_delta=sampled.delta;
        previous_pdf=sampled.pdf;
        previous_position=hit.position;
        if(!finite(throughput)||!finite(result)||!finite(sampled.direction)) throw std::runtime_error("Nonfinite path state");
        if(bounce>=3) {
            double survive=std::clamp(std::max({throughput.x,throughput.y,throughput.z}),.05,.95);
            if(rng.next()>=survive) break;
            throughput=throughput*(1/survive);
        }
        ray=spawn(hit,sampled.direction);
        ++bounce;
    }
    return result;
}
World make_sampling_scene() {
    World w;
    w.camera={{0,1.3,5},{0,.6,0},40};
    w.materials={{Kind::diffuse,{.6,.6,.6},1.5},{Kind::mirror,{.94,.94,.94},1.5}};
    w.scene.objects.push_back(Sphere{{-.8,.7,0},.7,0});
    w.scene.objects.push_back(Sphere{{.8,.7,0},.7,1});
    w.scene.objects.push_back(Triangle{{-6,0,4},{6,0,4},{6,0,-6},0});
    w.scene.objects.push_back(Triangle{{-6,0,4},{6,0,-6},{-6,0,-6},0});
    w.add_area({{-.18,3,-.2},{.36,0,0},{0,0,.36},{18,14,9}});
    return w;
}
