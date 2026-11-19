# v0.1 validation — 2026-11-19

Hardware: Apple M1, macOS, Apple Clang 17, CMake Release, single thread. Windows/MSVC not executed. Build and final tests:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
ctest --test-dir build-release --output-on-failure
```

3/3 tests passed (0.88 seconds); no compiler warnings observed. Final no-change incremental build: 1.9692 seconds; clean full build duration was not timed. Original geometry and tracer tests retained. Pipeline tests exercise barycentric/spherical UV, repeat and bottom-origin lookup, sRGB decoding, shared textured albedo, OBJ forms including negative vt/vn and v//vn, nonuniform transforms/inverse transpose, transactional rejection of quads, light binding, and a diffuse-to-unregistered-emitter scene (direct on/off contributions identical and nonzero).

## Actual render commands

```sh
./build-release/path_tracer --scene forest --width 240 --height 135 --spp 4 --depth 10 --output build-release/forest-low.ppm
./build-release/path_tracer --scene forest --width 480 --height 270 --spp 64 --depth 10 --seed 19 --output build-release/forest-color-v01-final.ppm
./build-release/path_tracer --scene forest-gray --width 480 --height 270 --spp 16 --depth 10 --seed 19 --output build-release/forest-gray-v01-final.ppm
./build-release/path_tracer --scene texture --width 128 --height 128 --spp 16 --output build-release/texture-v01.ppm
```

Low preview: 0.593091 seconds; it exposed a bank placement bug. Subsequent visual inspection exposed the row progression sign too; both fixed. Intermediate files preserved, not final evidence. Final color: 35.6444 seconds, BVH construction 0.00208904 seconds; final gray: 9.07926 seconds, BVH 0.00204483 seconds. Render timer includes PPM writing but excludes scene load/build. Texture example 0.0617066 seconds. Model: 1523 triangles, including eight imported 8-triangle rocks (64 imported faces). PNG inspection copies have the same basename; Pillow is only a local inspection utility, not a renderer dependency.

Final visual inspection: upright wood growth detail and knot/seams in mapping example; cabin wall grain, warm windows, cool exterior, mirrored window/tree silhouettes in the creek, rock occlusion and ground contact. Flat low-poly normals show distinct roof/rock/tree facets. Gray scene preserves lights and mirror water while removing diffuse colors/textures. This is a simple first composition: trees/trunks are stylized, ground repetition is conspicuous, creek is a perfect flat mirror, no water roughness or sky variation. Not a polished final scene.

## Final-scene BVH comparison

Sequential runs, no other render job active:

```sh
./build-release/path_tracer --scene forest --width 120 --height 68 --spp 8 --depth 10 --seed 19 --accel bvh --output build-release/forest-final-bench-bvh.ppm
./build-release/path_tracer --scene forest --width 120 --height 68 --spp 8 --depth 10 --seed 19 --accel linear --output build-release/forest-final-bench-linear.ppm
```

1523 triangles in both. BVH render 0.289189 s, linear 6.14566 s; BVH builds 0.00204646 / 0.00203379 s. Output PPM byte-identical, linear RGB mean 0.128666, 0.140643, 0.164338. Approx. 21.3× for this one configuration, not a general performance guarantee. Raw results: ignored build-release/forest-final-benchmark.json.

## Regressions

```sh
./build-release/path_tracer --width 48 --height 48 --spp 32 --seed 93 --accel bvh --output build-release/cornell-regression-v01.ppm
./build-release/normal_preview --width 320 --height 180 --output build-release/normal-v01.ppm
./build-release/path_tracer --scene cornell --width 128 --height 128 --spp 16 --output build-release/cornell-v01.ppm
./build-release/path_tracer --scene point --width 128 --height 128 --spp 16 --output build-release/point-v01.ppm
```

Cornell regression byte-identical to v0 build-release/bvh-a.ppm (0.234461 s). Normal regression byte-identical to original build/normal.ppm. Cornell and point smoke renders succeed (0.845927 / 0.768855 s). Linked lights retain Cornell's sampling order and image behavior. Only previously unregistered emission gains its missing diffuse-path contribution; test expectations were not changed to conceal a regression.

Limitations: P3 only, nearest/repeat filtering, no smooth normal interpolation (vn validated/transformed but flat shading selected), no MTL or polygons, scale/Y-rotation/translation rather than arbitrary affine matrices, assets resolved from repository working directory. Linked lights must be created through add_area and treated as immutable afterward. No depth of field, motion blur, alpha shadow, MIS, SAH, Principled, GUI or multithreading. Windows unverified. Original assets and authorship are recorded in ASSET_SOURCES; no teammate contribution inferred.
