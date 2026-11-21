#include "render/device_storage.cuh"
#include <unordered_map>

namespace {
wf::V pack(Vec3 v) { return {float(v.x),float(v.y),float(v.z)}; }
float2 pack(UV uv) { return {float(uv.u),float(uv.v)}; }
}
CudaWavefront::CudaWavefront(const RenderScene& scene,const CudaBVH& bvh)
    : storage_(std::make_unique<Storage>(bvh)) {
    auto& storage=*storage_;
    auto& s=storage.view;
    const Vec3 origin=bvh.coordinate_origin();
    s.geo=bvh.device_geometry();
    if (!scene.geometry().objects.empty() && !s.geo.shadow.surfaces)
        throw std::invalid_argument("CUDA wavefront rendering requires BVH material data");
    s.mipmaps=scene.uses_mipmaps();
    s.spectral=scene.uses_dispersion();
    const auto& c=scene.camera();
    const Vec3 forward=(c.target-c.origin).normalized(), right=forward.cross({0,1,0}).normalized();
    s.camera={pack(c.origin-origin),pack(forward),pack(right),pack(right.cross(forward)),
              float(std::tan(c.fov*pi/360)),float(c.lens_radius),float(c.focus_distance),
              float(c.shutter_open),float(c.shutter_close)};
    const auto& fog=scene.medium();
    s.fog={pack(fog.bounds.lo-origin),pack(fog.bounds.hi-origin),pack(fog.emission),float(fog.density),
           float(fog.profile==DensityProfile::homogeneous ? fog.density : fog.density_top),float(fog.albedo)};
    std::vector<wf::Object> objects;
    std::vector<wf::V> normals,pixels;
    objects.reserve(scene.geometry().objects.size());
    for (const auto& shape : scene.geometry().objects) {
        wf::Object o{};
        o.normals=o.light=-1;
        if (const auto* t=std::get_if<Triangle>(&shape)) {
            o.material=t->material_id;
            o.light=t->light_id;
            o.has_uv=t->has_complete_uvs;
            o.uv={pack(t->ta),pack(t->tb),pack(t->tc)};
            if (t->corner_normals) {
                o.normals=int(normals.size());
                for (const auto n:*t->corner_normals) normals.push_back(pack(n));
            }
        } else o.material=std::get<Sphere>(shape).material_id;
        objects.push_back(o);
    }
    storage.objects.upload(objects); s.objects=storage.objects.ptr;
    storage.normals.upload(normals); s.normals=storage.normals.ptr;
    std::vector<wf::MaterialData> materials;
    std::vector<wf::Level> levels;
    std::vector<wf::Map> maps;
    std::unordered_map<const Texture*,int> ids;
    auto texture_id=[&](const std::shared_ptr<const Texture>& ptr) {
        if (!ptr) return -1;
        const auto entry=ids.emplace(ptr.get(),int(maps.size()));
        if (entry.second) {
            maps.push_back({ptr->type,ptr->filter(),pack(ptr->color),int(levels.size()),ptr->last_level()});
            int w=ptr->width,h=ptr->height;
            if (ptr->type==Texture::Type::image) for (int l=0;l<=ptr->last_level();++l) {
                levels.push_back({pixels.size(),w,h});
                for (Vec3 v:ptr->level_pixels(l)) pixels.push_back(pack(v));
                w=std::max(1,w/2); h=std::max(1,h/2);
            }
        }
        return entry.first->second;
    };
    for (const auto& m:scene.materials())
        materials.push_back({m.kind,pack(m.color),pack(m.coat_absorption),float(m.ior),float(m.cauchy_b),
            float(m.metallic),float(m.roughness),float(m.coat_weight),float(m.coat_roughness),float(m.coat_ior),
            float(m.coat_thickness),m.toon_bands,texture_id(m.texture),texture_id(m.normal_map),texture_id(m.roughness_map)});
    storage.materials.upload(materials); s.materials=storage.materials.ptr;
    storage.maps.upload(maps); s.maps=storage.maps.ptr;
    storage.levels.upload(levels); s.levels=storage.levels.ptr;
    storage.pixels.upload(pixels); s.pixels=storage.pixels.ptr;
    const auto& lights=scene.lights();
    s.background=pack(lights.background);
    std::vector<wf::Point> points;
    for (const auto& p:lights.points) points.push_back({pack(p.position-origin),pack(p.intensity)});
    storage.points.upload(points); s.points=storage.points.ptr; s.points_count=int(points.size());
    std::vector<wf::Area> areas;
    for (const auto& a:lights.areas()) {
        const auto n=a.u.cross(a.v);
        areas.push_back({pack(a.origin-origin),pack(a.u),pack(a.v),pack(a.radiance),pack(n.normalized()),float(std::sqrt(n.dot(n)))});
    }
    storage.areas.upload(areas); s.areas=storage.areas.ptr; s.areas_count=int(areas.size());
    if (lights.hdr) {
        const auto& e=*lights.hdr;
        s.env_width=e.width; s.env_height=e.height; s.rotation=float(e.rotation);
        std::vector<wf::EnvironmentCell> cells;
        cells.reserve(e.pixels.size());
        for(int y=0;y<e.height;++y) {
            const double omega=(2*pi/e.width)*(std::cos(pi*y/e.height)-std::cos(pi*(y+1)/e.height));
            for(int x=0;x<e.width;++x) {
                const size_t i=size_t(y)*e.width+x;
                // Exact upper_bound for the sampler's 53 random bits, including tiny HDR cells.
                cells.push_back({pack(e.pixels[i]*e.intensity),float(e.probabilities[i]/omega),
                                 uint64_t(std::ceil(e.cdf[i]*0x1p53))});
            }
        }
        storage.environment.upload(cells); s.environment=storage.environment.ptr;
    }
}
CudaWavefront::~CudaWavefront()=default;
wf::SceneView CudaWavefront::device_scene() const { return storage_->view; }
