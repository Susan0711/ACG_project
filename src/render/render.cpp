#include "render/render.h"
#include <algorithm>
#include <atomic>
#include <exception>
#include <mutex>
#include <thread>
#include <stdexcept>
namespace {
std::uint64_t mix(std::uint64_t x) {
    x=(x^(x>>30))*0xbf58476d1ce4e5b9ULL;
    x=(x^(x>>27))*0x94d049bb133111ebULL;
    return x^(x>>31);
}
}
void render(const RenderScene& scene, Film& film, const RenderSettings& settings) {
    if(settings.threads<1 || settings.threads>RenderSettings::max_threads || settings.spp<1 || settings.depth<1)
        throw std::runtime_error("Invalid render settings");
    const int tiles_x=(film.width+15)/16, tiles_y=(film.height+15)/16;
    std::atomic<int> next{0};
    std::atomic<bool> stop{false};
    std::mutex error_mutex;
    std::exception_ptr error;
    auto worker=[&] {
        try {
            while(!stop.load()) {
                int tile=next.fetch_add(1);
                if(tile>=tiles_x*tiles_y) break;
                int x0=(tile%tiles_x)*16, y0=(tile/tiles_x)*16;
                for(int y=y0;y<std::min(y0+16,film.height);++y) {
                    for(int x=x0;x<std::min(x0+16,film.width);++x) {
                        std::size_t pixel=static_cast<std::size_t>(y)*film.width+x;
                        Vec3 sum{};
                        for(int s=0;s<settings.spp;++s) {
                            Sampler rng{mix(settings.seed ^ mix(pixel+0x9e3779b97f4a7c15ULL) ^ mix(static_cast<std::uint64_t>(s)+0xd1b54a32d192ed03ULL))};
                            double u=(x+rng.next())/film.width;
                            double v=(y+rng.next())/film.height;
                            Ray ray=scene.camera().sample(u,v,double(film.width)/film.height,rng);
                            sum=sum+integrate(scene,ray,rng,settings.depth,settings.mode);
                        }
                        film.pixels[pixel]=sum*(1.0/settings.spp);
                    }
                }
            }
        } catch(...) {
            std::lock_guard<std::mutex> lock(error_mutex);
            if(!error) error=std::current_exception();
            stop.store(true);
        }
    };
    std::vector<std::thread> workers;
    try {
        for(int i=0;i<settings.threads;++i) workers.emplace_back(worker);
    } catch(...) {
        stop.store(true);
        for(auto& thread:workers) thread.join();
        throw;
    }
    for(auto& thread:workers) thread.join();
    if(error) std::rethrow_exception(error);
}
