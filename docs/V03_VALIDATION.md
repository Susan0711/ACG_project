# v0.3 validation — 2026-11-19

Platform: macOS, Apple M1, Apple Clang 17, CMake Release. Windows/MSVC and the PowerShell handoff script have **not** been executed. All rendering is CPU-only with C++17 standard threads.

## Build and algorithm checks

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
ctest --test-dir build-release --output-on-failure
```

Five suites retain original geometry, material/BVH, texture/OBJ/emission and DOF/motion/alpha tests, plus render_tests for exact 1/4-thread linear pixel equality in Cornell/motion/alpha and worker exception rethrow after joining. No expected values in prior suites were changed to hide the new sampling allocation.

The v0.3 RNG is a hash of user seed, pixel index and sample index. A pixel is completed by one worker in ascending sample order. Therefore stochastic v0–v0.2 PPMs are not new exact reference values. Old files and original forest scene are retained. This intentionally changes noise, not the transport equations. normal_preview retains its exact regression.

## Image-guided changes

1. `v03-draft.ppm/png` (320×180, 8 spp, depth 10, seed 19, 4 threads) showed a too-small cabin and dim exterior. Camera moved from (8,4.5,11) to (7,3.8,8), aim to (1.6,1.1,-2), FOV 44; cool exterior illumination increased. Water gained 0.006-unit continuous axial undulation.
2. `v03-adjusted.ppm/png` confirmed larger cabin and a readable foreground-to-background creek. The first 640×360 output `v03-color.ppm/png` revealed exposed brown trunk tips; trunks were shortened to 86% of crown height. Intermediate images remain preserved.
3. Final images use the same fixed main camera for color/gray, and a separate closer camera for details. No depth of field, fog or motion hides geometry.

Geometry: 11,162 triangles + one small spherical door handle. Continuous shared shoreline sections prevent terrain from crossing the water; banks rise from below the waterline to terrain, rocks are partly embedded. Cabin is supported by a foundation; walls omit actual window regions, with lamps behind the openings and no window glass. New terrain extends outside the camera frame. Old forest remains at 1,523 triangles + no added v0.3 objects.

Visual improvement over v0.1: diagonal curved creek replaces straight cross-frame water; cabin is larger, framed by irregular tree groups; frame/mullion/recessed opening/door/eave/foundation details are legible; rocks have flattened irregular ring silhouettes; slow ground color variations replace UV stripes. Remaining issues: deliberately low-poly trees and grass, relatively uniform sky, simple mirror water, visibly procedural wood/grain, and finite-spp noise around interior light paths. This is not final realism or a final-quality claim.

## Timing protocol

`tools/render_v03.py` sequentially renders identical 320×180 / 32 spp / depth 10 / seed 19 scenes at 1 then 4 threads, then color, gray, detail and original baseline. No other render runs concurrently with the benchmark pair. Timings are wall-clock observations on this machine, not a guarantee based on core count. Scene load, BVH build, render and PPM write are separate. Render time includes thread startup/join but not writing. Output pixel reduction is serial and ordered after rendering.

Final exact commands and observed values follow below. Raw machine-readable logs are retained under ignored build-release/v03-final-renders.json. Preview PNG copies were generated with Pillow only for local inspection; the C++ executable has no Pillow dependency.

```sh
./build-release/path_tracer --scene forest-v03 --width 320 --height 180 --spp 32 --depth 10 --seed 19 --threads 1 --resource-root assets --output build-release/v03-final-bench-one.ppm
```

```text
triangles=11162 bvh_seconds=0.0348234
threads=1 load_seconds=0.0310985 render_seconds=24.3648 write_seconds=0.0219463
320x180 spp=32 depth=10 seed=19 mean_linear=0.0898114,0.0993607,0.0989885 output=build-release/v03-final-bench-one.ppm
```

```sh
./build-release/path_tracer --scene forest-v03 --width 320 --height 180 --spp 32 --depth 10 --seed 19 --threads 4 --resource-root assets --output build-release/v03-final-bench-four.ppm
```

```text
triangles=11162 bvh_seconds=0.0335112
threads=4 load_seconds=0.0211481 render_seconds=7.75435 write_seconds=0.0231536
320x180 spp=32 depth=10 seed=19 mean_linear=0.0898114,0.0993607,0.0989885 output=build-release/v03-final-bench-four.ppm
```

```sh
./build-release/path_tracer --scene forest-v03 --width 640 --height 360 --spp 64 --depth 10 --seed 19 --threads 4 --resource-root assets --output build-release/v03-final-color.ppm
```

```text
triangles=11162 bvh_seconds=0.0388895
threads=4 load_seconds=0.0237642 render_seconds=66.1416 write_seconds=0.0895563
640x360 spp=64 depth=10 seed=19 mean_linear=0.0899646,0.0994357,0.0990249 output=build-release/v03-final-color.ppm
```

```sh
./build-release/path_tracer --scene forest-v03-gray --width 640 --height 360 --spp 32 --depth 10 --seed 19 --threads 4 --resource-root assets --output build-release/v03-final-gray.ppm
```

```text
triangles=11162 bvh_seconds=0.0340124
threads=4 load_seconds=0.0209674 render_seconds=31.9564 write_seconds=0.0955037
640x360 spp=32 depth=10 seed=19 mean_linear=0.154292,0.17368,0.214712 output=build-release/v03-final-gray.ppm
```

```sh
./build-release/path_tracer --scene forest-v03-detail --width 480 --height 320 --spp 64 --depth 10 --seed 19 --threads 4 --resource-root assets --output build-release/v03-final-detail.ppm
```

```text
triangles=11162 bvh_seconds=0.0333779
threads=4 load_seconds=0.0212741 render_seconds=67.3336 write_seconds=0.0604187
480x320 spp=64 depth=10 seed=19 mean_linear=0.134404,0.0782502,0.0396563 output=build-release/v03-final-detail.ppm
```

```sh
./build-release/path_tracer --scene forest --width 480 --height 270 --spp 32 --depth 10 --seed 19 --threads 4 --resource-root assets --output build-release/v03-final-baseline.ppm
```

```text
triangles=1523 bvh_seconds=0.00222992
threads=4 load_seconds=0.02097 render_seconds=6.12977 write_seconds=0.0541775
480x270 spp=32 depth=10 seed=19 mean_linear=0.128879,0.140923,0.16485 output=build-release/v03-final-baseline.ppm
```

Final benchmark: 3.142× render-only speedup for 4 threads; PPMs byte-identical. Exact linear pixel equality is separately tested before display conversion in CTest. This does not predict other CPU/scenes.


## Final checks and handoff

Release tests passed 5/5 before the final diagnostic-only change. A subsequent Python-subprocess rebuild selected x86_64 against cached arm64 libraries and failed to link; no source-interface failure was involved. Recovery explicitly configures `-DCMAKE_OSX_ARCHITECTURES=arm64` on this Apple Silicon machine and rebuilds all targets. Successful build/test results are recorded below. render_tests also verifies that a ray through the cabin window reaches an emitter behind the front wall and resource errors suggest --resource-root. The final incremental build only changes diagnostic timing output placement, not rendering math.

Resource-root smoke was executed from build-release:
```sh
./path_tracer --scene forest-v03 --resource-root ../assets --width 64 --height 36 --spp 2 --threads 4 --output v03-resource-smoke.ppm
./path_tracer --scene forest-v03 --resource-root does-not-exist --output v03-should-not-exist.ppm
```
First succeeds, second exits 1 naming does-not-exist/wood.ppm and the --resource-root remedy; no output file created for the error. Original normal regression:
```sh
./build-release/normal_preview --width 320 --height 180 --output build-release/v03-normal-regression.ppm
```
Byte-identical to original build/normal.ppm. Cornell, motion and alpha-pattern also rendered at 96×60, 16 spp, seed 27, 4 threads into v03-smoke-SCENE.ppm; motion uses --shutter-close 1. CTest checks exact 1/4-thread equality for these algorithm categories. Old stochastic images remain in place; new RNG allocation is documented rather than forcing old expectations to change.

Final inspected images: v03-final-color.png, v03-final-gray.png, v03-final-detail.png, v03-final-baseline.png (same-basename PPM originals). No visible water-bottom exposure or floating cabin in the main/detail views. Close geometric intersections beyond these views and perfect terrain/rock blending still warrant future polish. Some faceting, repetitive crown structure and procedural wood scale remain evident. Thin model details/interior illumination still have finite-spp noise.

WINDOWS_VALIDATION.md and tools/validate_windows.ps1 provide Release configuration, all CTests, non-root working-directory resource checks and 1/4-thread image hashes. **Neither Windows nor that script has actually been executed here.** Do not attribute these future verification tasks or technical modules to a teammate before they perform them.


Mac build environment recovery command (platform-specific invocation, not hardcoded in project CMake):
```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-release
ctest --test-dir build-release --output-on-failure
```
The system Python used for the attempted final build ran through a different architecture context; mixing object architectures caused linker warnings and failure. Render binaries used for the recorded measurements were arm64. On Intel Macs use the corresponding native build configuration; do not copy arm64 into Windows commands.

Recovery result: explicit arm64 full recompilation succeeded; final CTest **5/5 passed in 2.80 s** with no warnings in the successful build. The earlier mixed-architecture failure is retained above rather than omitted.
