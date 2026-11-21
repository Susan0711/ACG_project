#include "render/wavefront.h"
#include "render/direct_lighting.h"
#include <algorithm>
#include <exception>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace {
constexpr double infinity = std::numeric_limits<double>::infinity();

template<class F> void parallel_for(int count, int threads, F body) {
    std::exception_ptr error;
    #pragma omp parallel for schedule(static) num_threads(threads) if(count >= 512 && threads > 1)
    for (int i = 0; i < count; ++i) {
        try { body(i); }
        catch (...) {
            #pragma omp critical(acg_wavefront_error)
            { if (!error) error = std::current_exception(); }
        }
    }
    if (error) std::rethrow_exception(error);
}

struct CameraSample { int x, y, sample; Vec3 color{}; };
class CameraQueue {
    const int width, height, spp;
    int x0 = 0, y0 = 0, pixel = 0, sample = 0;
public:
    CameraQueue(int w, int h, int s) : width(w), height(h), spp(s) {}
    bool next(CameraSample& out) {
        if (y0 >= height) return false;
        const int w = std::min(16, width-x0), h = std::min(16, height-y0);
        out = {x0 + pixel%w, y0 + pixel/w, sample};
        if (++pixel == w*h) {
            pixel = 0;
            if (++sample == spp) {
                sample = 0;
                x0 += 16;
                if (x0 >= width) { x0 = 0; y0 += 16; }
            }
        }
        return true;
    }
};
struct Lane {
    PathState path{};
    Sampler initial_rng{}, rng{};
    Ray camera{};
    int sample_slot = 0, channel = 0;
    bool continuing = true;
    Vec3 direct_weight{};
    std::vector<DirectLightSample> lights;
    std::size_t light_index = 0;
    Vec3 direct{};
    // Keep a single FP32 query ray through successive transparent surfaces.
    // The CPU integrator still measures free flight from its advanced origin.
    Vec3 alpha_origin{};
    double alpha_distance = 0, alpha_minimum = 0;
    bool has_shadow() const { return light_index < lights.size(); }
};
RayQuery query(const Ray& r, double lo, double hi) { return {r.origin, r.direction, r.time, lo, hi}; }
HitRecord finish(const RenderScene& scene, const Ray& ray, GeometryHit h) {
    HitRecord hit;
    const auto& object = scene.geometry().objects[h.object];
    std::visit([&](const auto& shape) { shape.finish_hit(ray, h.parameters, hit); },object);
    hit.object_index = static_cast<std::size_t>(h.object);
    return hit;
}
void restore_surface_position(const RenderScene& scene, const Ray& ray, const HitParameters& params, HitRecord& hit) {
    const auto& object = scene.geometry().objects[hit.object_index];
    // Place secondary rays on the selected surface using its local coordinates.
    // A rounded FP32 t on a long ray can otherwise exceed the spawn offset.
    if (const auto* tri = std::get_if<Triangle>(&object)) {
        hit.position = tri->a + (tri->b-tri->a)*params.u + (tri->c-tri->a)*params.v
                     + tri->velocity*ray.time;
    } else {
        const auto& sphere = std::get<Sphere>(object);
        hit.position = sphere.center_at(ray.time) + hit.outward_normal*sphere.radius;
    }
}
void finish_shadow(const RenderScene& scene, Lane& lane, float surface_transmittance) {
    const auto& light = lane.lights[lane.light_index];
    const double visibility = scene.medium().transmittance(light.segment.ray, light.segment.distance) * surface_transmittance;
    lane.direct = lane.direct + light.contribution(visibility);
    if (++lane.light_index == lane.lights.size()) {
        lane.path.result = lane.path.result + multiply(lane.direct_weight, lane.direct);
        lane.direct = {};
    }
}
}

QueueStats render_wavefront(const RenderScene& scene, Film& film, const RenderSettings& settings,
                            CudaBVH& gpu, int threshold) {
    constexpr int capacity = CudaBVH::capacity;
    if (threshold < 0 || threshold > capacity || settings.spp < 1 || settings.depth < 1 ||
        settings.threads < 1 || settings.threads > RenderSettings::max_threads)
        throw std::invalid_argument("Invalid wavefront render settings");
    const double spread = scene.uses_mipmaps() ? 2*std::tan(scene.camera().fov*pi/360)/film.height : 0;
    const bool spectral = scene.uses_dispersion();
    const Wavelength bands[] = {Wavelength::red, Wavelength::green, Wavelength::blue};
    CameraQueue source(film.width, film.height, settings.spp);
    std::fill(film.pixels.begin(), film.pixels.end(), Vec3{});
    std::vector<Lane> lanes(capacity);
    std::vector<RayQuery> queries(capacity);
    std::vector<GeometryHit> hits(capacity);
    std::vector<float> shadow_transmittance(capacity);
    std::vector<int> active, ordered(capacity), free_slots(capacity);
    std::iota(free_slots.begin(), free_slots.end(), 0);
    std::vector<CameraSample> samples;
    // Bound pending film results even if one early sample follows a very long path.
    constexpr int window = 8 * capacity;
    samples.reserve(window);
    active.reserve(capacity);
    QueueStats stats;
    for (;;) {
        CameraSample sample;
        samples.clear();
        while (samples.size() < window && source.next(sample)) samples.push_back(sample);
        if (samples.empty()) break;
        std::size_t next = 0;
        while (next < samples.size() || !active.empty()) {
            if ((active.empty() || int(active.size()) < threshold) && next < samples.size()) {
                ++stats.refills;
                while (!free_slots.empty() && next < samples.size()) {
                    const auto& s = samples[next];
                    const int slot = free_slots.back();
                    free_slots.pop_back();
                    Lane& lane = lanes[slot];
                    lane.sample_slot = int(next++);
                    lane.initial_rng = pixel_sampler(settings.seed, std::size_t(s.y)*film.width+s.x, s.sample);
                    lane.camera = camera_sample(scene, film, s.x, s.y, spread, lane.initial_rng);
                    lane.rng = lane.initial_rng;
                    lane.path = PathState{lane.camera};
                    lane.channel = 0;
                    lane.lights.clear();
                    lane.light_index = 0;
                    lane.continuing = true;
                    lane.alpha_minimum = lane.alpha_distance = 0;
                    active.push_back(slot);
                }
                stats.peak_paths = std::max(stats.peak_paths, int(active.size()));
            }
            const int count = int(active.size());
            // Submit both contiguous queues together. A shadow returns one scalar
            // regardless of the number of transparent surfaces before its light.
            int main_count = 0;
            for (int slot : active) if (!lanes[slot].has_shadow()) ordered[main_count++] = slot;
            int tail = main_count;
            for (int slot : active) if (lanes[slot].has_shadow()) ordered[tail++] = slot;
            for (int i=0; i<count; ++i) {
                const Lane& lane = lanes[ordered[i]];
                if (i < main_count) {
                    queries[i] = query(lane.path.ray, lane.alpha_minimum, infinity);
                    if (lane.alpha_minimum > 0) queries[i].origin = lane.alpha_origin;
                }
                else {
                    const auto& segment = lane.lights[lane.light_index].segment;
                    queries[i] = query(segment.ray, 0, segment.distance - 1e-5);
                }
            }
            gpu.intersect_batch(queries.data(), hits.data(), main_count, shadow_transmittance.data(), count-main_count);
            parallel_for(count, settings.threads, [&](int i) {
                Lane& lane = lanes[ordered[i]];
                if (i >= main_count) {
                    finish_shadow(scene, lane, shadow_transmittance[i-main_count]);
                    return;
                }
                HitRecord hit;
                const bool found = hits[i].object >= 0;
                if (found) {
                    GeometryHit local = hits[i];
                    local.parameters.t -= lane.alpha_distance;
                    hit = finish(scene, lane.path.ray, local);
                    restore_surface_position(scene,lane.path.ray,local.parameters,hit);
                    scene.apply_surface_maps(lane.path.ray, hit);
                }
                lane.lights.clear();
                lane.light_index = 0;
                const int crossings = lane.path.alpha_crossings;
                lane.continuing = advance_path(scene, lane.path, lane.rng, settings.depth, settings.mode,
                    spectral ? bands[lane.channel] : Wavelength::green, found, hit, &lane.lights, lane.direct_weight);
                if (lane.continuing && lane.path.alpha_crossings > crossings) {
                    lane.alpha_origin = queries[i].origin;
                    const Ray& next = lane.path.ray;
                    lane.alpha_distance = (next.origin-lane.alpha_origin).dot(next.direction)/next.direction.dot(next.direction);
                    lane.alpha_minimum = std::max(lane.alpha_distance, std::nextafter(hits[i].parameters.t,infinity));
                } else {
                    lane.alpha_minimum = lane.alpha_distance = 0;
                }
            });
            auto keep = active.begin();
            for (int slot : active) {
                Lane& lane = lanes[slot];
                if (!lane.continuing && !lane.has_shadow()) {
                    auto& color = samples[lane.sample_slot].color;
                    if (!spectral) color = lane.path.result;
                    else {
                        if (lane.channel == 0) color.x = lane.path.result.x;
                        if (lane.channel == 1) color.y = lane.path.result.y;
                        if (lane.channel == 2) color.z = lane.path.result.z;
                        if (++lane.channel < 3) {
                            lane.path = PathState{lane.camera};
                            lane.rng = lane.initial_rng;
                            lane.continuing = true;
                        }
                    }
                }
                if (lane.continuing || lane.has_shadow()) *keep++ = slot;
                else free_slots.push_back(slot);
            }
            active.erase(keep, active.end());
        }
        for (const auto& s : samples) {
            auto& pixel = film.pixels[std::size_t(s.y)*film.width+s.x];
            pixel = pixel + s.color;
        }
    }
    for (auto& pixel : film.pixels) pixel = pixel * (1.0/settings.spp);
    return stats;
}
