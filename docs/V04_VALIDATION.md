# v0.4 validation — 2026-11-19

macOS Apple M1, Apple Clang 17, CMake Release arm64, 4 threads unless stated. Windows/MSVC remains unverified. No external HDRI download or image library: the original 128×64 RGB float PFM is generated analytically and has radiance above display range. Geometry of forest-v03 is unchanged (11,162 triangles + one handle sphere).

## Build and test commands

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-release
ctest --test-dir build-release --output-on-failure
```

Six suites retain all previous tests and add environment mapping, seam/rotation/intensity, exact constant-environment PDF, black-map fallback/poles, CDF normalization and 50000 nonuniform samples with frequency tolerance .006, sample/evaluation consistency, preserved HDR values >10, MIS complementary/zero PDFs, primary/delta-mirror/glass full weights, unregistered emitters, two enumerated delta point lights, and 1/4-thread HDR MIS pixel equality. A constant HDR diffuse test checks all three modes against analytic radiance 1.2 over 30000 paths at depth=1 (tolerance .025). No old expected values were changed.

During validation the depth convention was corrected: the last scattering event now still queries its emission/escape endpoint. Previously NEE could estimate that segment while BSDF stopped before querying it. Final comparisons were rerun under the same corrected cap; preliminary v04-* files remain preserved but are not the final evidence. All modes now share the same truncated path family; finite maximum depth still biases the infinite-path solution. CLI help/rotation wrap and a redundant internal flag cleanup do not change the recorded ordinary-angle results.

## PDF and energy contract

Every registered area light, point light and HDR environment gets one direct sample per diffuse vertex; each technique's inclusion probability is 1. There is no random selection among N lights. Rectangle solid-angle PDF is distance²/(area*cos_light); environment texel mass is luminance times exact solid angle and samples uniform phi/cos(theta) within that cell; Lambertian PDF is cos/pi. Power heuristic β=2 uses the matching directional PDFs on both techniques. Primary/delta events and unregistered emission retain weight 1. Point light samples have no continuous BSDF competitor. Alpha crossings retain previous true BSDF state, position/PDF and time; RR and visibility compensation remain consistent. Constant legacy environments without HDR maps remain escape-only. No extra ambient surface term.

## Multi-seed linear comparison

The same `sampling` scene contains a diffuse sphere, ideal mirror, ground, a small .36×.36 area source, and the high-luminance HDR sun lobe; **no point lights**. All modes use identical geometry/radiance. Measurements use untonemapped float PFM RGB, not display PPM.

48×30, depth 12, 256 spp, seeds 1–8 per mode. Reference is the average of two independent MIS renders at 16384 spp (seeds 900/901), equivalent to 32768 samples in expectation. Error is RGB RMSE averaged over image/channel and seeds. Mean uncertainty is sample standard error of image luminance across eight seeds, not a confidence guarantee for every pixel.

| Mode | Mean linear luminance | SE across seeds | RGB RMSE | Mean render seconds |
|---|---:|---:|---:|---:|
| bsdf | 0.23145545 | 0.00089788 | 0.07508465 | 0.068721 |
| nee | 0.23177927 | 0.00018678 | 0.02817660 | 0.136013 |
| mis | 0.23174333 | 0.00023333 | 0.02757087 | 0.126456 |

Reference mean 0.23173648; estimated reference noise RMSE from half the independent-reference difference 0.00312070. Means agree within finite-sample uncertainty; this does not prove unbiasedness.

MIS improves RMSE over NEE by 2.15% here. Paired NEE−MIS MSE difference 3.3767952e-05, paired SE 3.4200435e-06. This is a modest scene-specific improvement, not a universal advantage. Ideal specular paths and reference noise still contribute residual error. Small-image wall times fluctuate with scheduling/startup; do not interpret MIS's slightly lower observed time than NEE as an algorithmic speed guarantee.

Approximate equal-budget bracket (same 8 seeds): BSDF 432 spp takes 0.108690 s, RMSE 0.05767809; BSDF 512 spp takes 0.151214 s, RMSE 0.05267680. MIS 256 spp is between those observed budgets and has lower error. These are near-time brackets, not perfectly time-locked runs. Runs were sequential with no simultaneous rendering.

Reproduction scripts:
```sh
python3 tools/validate_v04.py
python3 tools/compare_v04_time.py
```
They refuse existing image paths through the CLI; use a fresh output namespace on reruns. Current equal-budget script runs 512 spp; the earlier 432-spp commands remain in v04-final-equal-time.json. Full logs: build-release/v04-final-renders.json, v04-final-statistics.json, v04-final-equal-time.json and v04-final-equal-time-512.json. These are local ignored artifacts; numerical summaries are preserved here.

Exact common small-scene command (MODE=bsdf/nee/mis; SEED=1..8; unique TAG):
```sh
./build-release/path_tracer --scene sampling --environment studio-hdr.pfm --mode MODE --width 48 --height 30 --spp 256 --depth 12 --seed SEED --threads 4 --output build-release/v04-final-TAG.ppm --linear-output build-release/v04-final-TAG.pfm
```
Reference uses --mode mis --spp 16384 --seed 900/901 and reference/reference-check tags. No exposure multiplier enters the error statistics.

## Actual display renders

```sh
./build-release/path_tracer --scene sampling --environment studio-hdr.pfm --mode mis --width 320 --height 200 --spp 128 --depth 12 --seed 19 --threads 4 --output build-release/v04-final-rotation-0.ppm --linear-output build-release/v04-final-rotation-0.pfm --env-rotation 0
```

```text
triangles=4 bvh_seconds=6.042e-06
threads=4 load_seconds=0.00118871 render_seconds=2.27738 write_seconds=0.0311572
320x200 spp=128 depth=12 seed=19 mean_linear=0.215417,0.233099,0.266521 output=build-release/v04-final-rotation-0.ppm
```

```sh
./build-release/path_tracer --scene sampling --environment studio-hdr.pfm --mode mis --width 320 --height 200 --spp 128 --depth 12 --seed 19 --threads 4 --output build-release/v04-final-rotation-90.ppm --linear-output build-release/v04-final-rotation-90.pfm --env-rotation 90
```

```text
triangles=4 bvh_seconds=4.834e-06
threads=4 load_seconds=0.00182558 render_seconds=2.20179 write_seconds=0.0319885
320x200 spp=128 depth=12 seed=19 mean_linear=0.197539,0.218794,0.257575 output=build-release/v04-final-rotation-90.ppm
```

```sh
./build-release/path_tracer --scene sampling --environment studio-hdr.pfm --mode mis --width 320 --height 200 --spp 128 --depth 12 --seed 19 --threads 4 --output build-release/v04-final-intensity-2.ppm --linear-output build-release/v04-final-intensity-2.pfm --env-rotation 0 --env-intensity 2
```

```text
triangles=4 bvh_seconds=3.875e-06
threads=4 load_seconds=0.000737042 render_seconds=2.05791 write_seconds=0.0316634
320x200 spp=128 depth=12 seed=19 mean_linear=0.418339,0.45648,0.526795 output=build-release/v04-final-intensity-2.ppm
```

```sh
./build-release/path_tracer --scene forest-v03 --environment studio-hdr.pfm --mode mis --width 640 --height 360 --spp 64 --depth 12 --seed 19 --threads 4 --output build-release/v04-final-forest-hdr.ppm --linear-output build-release/v04-final-forest-hdr.pfm --env-rotation 30 --env-intensity .7
```

```text
triangles=11162 bvh_seconds=0.0344451
threads=4 load_seconds=0.025291 render_seconds=62.1664 write_seconds=0.104969
640x360 spp=64 depth=12 seed=19 mean_linear=0.0744741,0.0759109,0.0617567 output=build-release/v04-final-forest-hdr.ppm
```


Inspected PNG copies have the same basename as each PPM. Rotation moves the HDR highlight in the mirror and changes ground illumination/shadows; intensity 2 brightens only the environment, while the fixed area light is unchanged, so total image luminance is not expected to double. The forest HDR version retains all v0.3 geometry, camera, window lamps and outdoor point light; only environment lighting differs. No DOF/fog is used. Grain/fireflies remain around specular/bright light paths and cabin eaves at 64 spp; nearest low-resolution HDR produces visible sky bands. It is an analytic illumination test, not a photographic sky or final-quality forest.

## Regression / threading

HDR MIS sampling scene: 80×50, 64 spp, depth 12, seed 31 at 1 and 4 threads. Both PFM and PPM byte-identical (0.205170 / 0.0610154 s). CTest also checks linear doubles directly. Exact commands/logs: build-release/v04-regressions.json. Original normal_preview at 320×180 is byte-identical to build/normal.ppm.

Cornell, motion (shutter [0,1]) and alpha-pattern were rerendered with MIS at 96×60, 32 spp, depth 16, seed 27, 4 threads; motion/alpha additionally use the HDR map. Outputs: v04-check-cornell.ppm/png, v04-check-motion.ppm/png, v04-check-alpha-pattern.ppm/png. Inspected images retain mirror/glass behavior, motion smearing and patterned shadows. Their samples differ from older images because NEE/MIS changes random consumption and the terminal segment convention was corrected; adding HDR deliberately changes lighting. Old images are retained, not substituted with changed expectations.

## Scope / ownership

PFM only (RGB float32, standard simple header, either byte order); not an EXR/RGBE decoder. Piecewise-constant/nearest environment, no HDR filtering. Light enumeration cost grows with light count. No alpha emitters, nested media, caustic-specific sampling, SAH, Principled, normal maps, volumes or GUI. HDR/MIS implementation and analytic asset authored by Codex at the user's request; no teammate contribution or course score claimed. Windows remains pending and its existing entry point is retained.

Final successful Release CTest: **6/6 passed in 6.28 s**, no compiler warnings in this build. After final CLI/help and internal-flag cleanup, a repeat of the 80×50 / 64 spp / depth 12 / seed 31 / 4-thread HDR MIS render (`v04-final-build-check.pfm`) compared byte-identical to `v04-thread-4.pfm`, confirming the recorded results still match the final executable.
