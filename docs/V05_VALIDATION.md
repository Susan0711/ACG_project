# Forest Creek Cabin main visual v0.5

Scene-only iteration, 2026-11-19. macOS Apple M1, Apple Clang 17, Release arm64. Public v0.4 baseline is with teammate for Windows validation; no Windows run or teammate contribution is claimed here.

## Scope and build

Added src/forest_v05.cpp/.h, original wood-soft-v05.ppm and generator, candidate/final rendering scripts. Minimal edits to CMake and CLI scene registration. README homepage is reorganized, with older descriptions clearly marked historical. PROJECT_PLAN/ASSET_SOURCES updated. Public headers and BVH, integrator, BSDF, environment sampling, geometry, RNG and threading implementations were not modified; SHA256 comparison against hashes captured before this task passed. Original forest/forest_v03 sources and all older images preserved.

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-release
ctest --test-dir build-release --output-on-failure
```

Release configuration/build succeeded without warnings; all 6 CTests passed (final run 3.65 s). No-change incremental build measured 4.398 s; initial changed-source build was not wall-clock timed. No new algorithm tests/expectations were changed. New scene thread equality is checked with CLI renders below.

## Scene / camera / light choices

Main camera: origin (7,3.8,8), target (1.6,1.1,-2), vertical FOV 44 degrees. Gray uses exactly the same camera. Detail: origin (6,2.8,4), target (2.7,1.35,-2.4), FOV 36. Lens radius=0, instantaneous shutter at 0; no blur to hide geometry.

Original HDR studio-hdr.pfm reused. Candidate A: rotation 30 degrees/intensity .7. Candidate B: 115 degrees/intensity 1.1. Both used 320×180, 8 spp, depth 12, seed 19, 4 threads, MIS. Exterior point: (-5,11,6), intensity (85,110,145); window emitters keep inset geometry and use radiance (2.3,1.05,.38), reduced from (9,3.7,1.1). Exposure stays 1.

A was darker and less readable; B selected for clearer cabin/shore and cool/warm separation. First images showed too-thin foliage fans and visible branch skeletons. A targeted second pass widened fans from .25r to .43r and raised their crest, removing excess lower internal cylinders. Candidate 18,113 triangles became final 15,593 triangles plus one handle sphere, below 50k. Adjusted low-spp image v05-adjusted.ppm/png was inspected before final renders.

Scene improvements over v0.4 forest-v03 HDR: lower wood contrast and desaturation, doubled texture repeats, individually mapped narrow gable boards; asymmetric open branch fans replace complete stacked cones; grouped trees vary height/phase; rock clusters leave open shoreline and vary size/embed depth; vegetation follows sparse patches rather than uniform random grass. Retained channel/terrain share boundary vertices; water remains perfect mirror with existing small axial geometric undulation. No rough-water/fluid claim. Final main/gray/detail views checked for floating foundation, shoreline gaps and exposed water bottom; no obvious gaps in those views, while some rock/earth intersections remain stylized.

Still clearly low polygon: triangular foliage, simple cylinders and rock facets; not final semi-realism. Large sky is flat/banded from the retained analytic low-resolution HDR, ground remains simple and interior light paths show finite-spp noise. No new shader, environment filter or third-party asset was introduced to address these limitations.

## Actual renders

All files are new; existing output paths are rejected. Exact commands/timing follow. PPM originals have PNG copies for visual inspection (Pillow used only as a local inspection tool).

```sh
./build-release/path_tracer --scene forest-v05 --resource-root assets --environment studio-hdr.pfm --env-rotation 115 --env-intensity 1.1 --mode mis --width 640 --height 360 --spp 64 --depth 12 --seed 19 --threads 4 --output build-release/v05-color.ppm
```
```text
triangles=15593 bvh_seconds=0.0534764
threads=4 load_seconds=0.0227895 render_seconds=76.892 write_seconds=0.0820963
640x360 spp=64 depth=12 seed=19 mean_linear=0.066724,0.0882175,0.0853216 output=build-release/v05-color.ppm
```

```sh
./build-release/path_tracer --scene forest-v05-gray --resource-root assets --environment studio-hdr.pfm --env-rotation 115 --env-intensity 1.1 --mode mis --width 640 --height 360 --spp 32 --depth 12 --seed 19 --threads 4 --output build-release/v05-gray.ppm
```
```text
triangles=15593 bvh_seconds=0.0536004
threads=4 load_seconds=0.0229248 render_seconds=31.7944 write_seconds=0.1044
640x360 spp=32 depth=12 seed=19 mean_linear=0.166223,0.188963,0.230843 output=build-release/v05-gray.ppm
```

```sh
./build-release/path_tracer --scene forest-v05-detail --resource-root assets --environment studio-hdr.pfm --env-rotation 115 --env-intensity 1.1 --mode mis --width 480 --height 320 --spp 64 --depth 12 --seed 19 --threads 4 --output build-release/v05-detail.ppm
```
```text
triangles=15593 bvh_seconds=0.0625706
threads=4 load_seconds=0.0265353 render_seconds=63.1209 write_seconds=0.0531011
480x320 spp=64 depth=12 seed=19 mean_linear=0.0593941,0.0566202,0.0437799 output=build-release/v05-detail.ppm
```

```sh
./build-release/path_tracer --scene forest-v03 --resource-root assets --environment studio-hdr.pfm --env-rotation 30 --env-intensity 0.7 --mode mis --width 320 --height 180 --spp 16 --depth 12 --seed 19 --threads 4 --output build-release/v05-baseline-v04-light.ppm
```
```text
triangles=11162 bvh_seconds=0.0330794
threads=4 load_seconds=0.0225195 render_seconds=3.63495 write_seconds=0.0225042
320x180 spp=16 depth=12 seed=19 mean_linear=0.0744703,0.0759279,0.0617698 output=build-release/v05-baseline-v04-light.ppm
```

```sh
./build-release/path_tracer --scene forest-v03 --resource-root assets --environment studio-hdr.pfm --env-rotation 115 --env-intensity 1.1 --mode mis --width 160 --height 90 --spp 16 --depth 12 --seed 19 --threads 4 --output build-release/v05-benchmark-v03.ppm
```
```text
triangles=11162 bvh_seconds=0.0375461
threads=4 load_seconds=0.024148 render_seconds=0.958424 write_seconds=0.00683479
160x90 spp=16 depth=12 seed=19 mean_linear=0.0907835,0.0997128,0.0877209 output=build-release/v05-benchmark-v03.ppm
```

```sh
./build-release/path_tracer --scene forest-v05 --resource-root assets --environment studio-hdr.pfm --env-rotation 115 --env-intensity 1.1 --mode mis --width 160 --height 90 --spp 16 --depth 12 --seed 19 --threads 4 --output build-release/v05-benchmark-v05.ppm
```
```text
triangles=15593 bvh_seconds=0.0587999
threads=4 load_seconds=0.0250357 render_seconds=1.02937 write_seconds=0.00717767
160x90 spp=16 depth=12 seed=19 mean_linear=0.0666181,0.0881731,0.0853727 output=build-release/v05-benchmark-v05.ppm
```

```sh
./build-release/path_tracer --scene forest-v05 --resource-root assets --environment studio-hdr.pfm --env-rotation 115 --env-intensity 1.1 --mode mis --width 80 --height 45 --spp 16 --depth 12 --seed 19 --threads 1 --output build-release/v05-threads-one.ppm
```
```text
triangles=15593 bvh_seconds=0.0581808
threads=1 load_seconds=0.0245104 render_seconds=1.05238 write_seconds=0.00205429
80x45 spp=16 depth=12 seed=19 mean_linear=0.0676632,0.0887781,0.0856594 output=build-release/v05-threads-one.ppm
```

```sh
./build-release/path_tracer --scene forest-v05 --resource-root assets --environment studio-hdr.pfm --env-rotation 115 --env-intensity 1.1 --mode mis --width 80 --height 45 --spp 16 --depth 12 --seed 19 --threads 4 --output build-release/v05-threads-four.ppm
```
```text
triangles=15593 bvh_seconds=0.054588
threads=4 load_seconds=0.0214493 render_seconds=0.259876 write_seconds=0.00182742
80x45 spp=16 depth=12 seed=19 mean_linear=0.0676632,0.0887781,0.0856594 output=build-release/v05-threads-four.ppm
```

Final main render: 76.892 s, gray 31.7944 s, detail 63.1209 s. Same-parameter sequential 160×90 / 16 spp scene comparison: v03 .958424 s versus v05 1.02937 s (~7.4% higher), triangles +39.7%. This is changed scene geometry/material/lighting visibility, not an algorithm performance conclusion; small-scene timings also fluctuate. More detailed foliage increases intersections/shadow work, partly offset by fewer branch cylinders and boulder gaps. No higher spp was used to conceal modeling problems.

New scene 1/4-thread 80×45 / 16 spp outputs byte-identical (1.05238 / .259876 s). Existing suites verify double-pixel invariance and worker exceptions. Baseline v05-baseline-v04-light uses forest-v03 with v0.4 HDR 30°/.7; new selected image changes geometry, timber texture, point/window intensities and HDR direction/scale. Visual brightness differences are therefore not evidence of algorithmic improvement. Original v04-final-forest-hdr remains preserved too.

Reproduce full final suite with `python3 tools/render_v05_final.py` only with fresh output names; it rejects existing images. Windows needs the newly compiled scene registration but no shared interface changes. Windows build/test remains pending. The generated tools/__pycache__ is an untracked local cache and must not be staged; helper now disables future bytecode writes. No Git staging, branch switching, commit or remote operation performed.

Candidate commands (before fan-width adjustment):
```sh
./build-release/path_tracer --scene forest-v05 --resource-root assets --environment studio-hdr.pfm --env-rotation 30 --env-intensity .7 --mode mis --width 320 --height 180 --spp 8 --depth 12 --seed 19 --threads 4 --output build-release/v05-candidate-a.ppm
./build-release/path_tracer --scene forest-v05 --resource-root assets --environment studio-hdr.pfm --env-rotation 115 --env-intensity 1.1 --mode mis --width 320 --height 180 --spp 8 --depth 12 --seed 19 --threads 4 --output build-release/v05-candidate-b.ppm
```
A/B render times 2.31912/2.42688 s, BVH .0556277/.0634508 s, 18,113 triangles. The adjusted same-B-light low render v05-adjusted.ppm used identical common parameters, 15,593 triangles, 1.91955 s render, .0545514 s BVH. No lighting was changed after selecting B.
