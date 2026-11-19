# v0.2 validation — 2026-11-19

macOS Apple M1 / Apple Clang 17, CMake Release, single thread. Windows not executed. No third-party rendering dependencies.

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
ctest --test-dir build-release --output-on-failure
```

Final result: 4/4 passed, 2.21 s; no compiler warnings observed. Old geometry/tracer/pipeline tests retained. New tests cover exact zero-aperture/no-RNG degeneration, 10000 lens samples converging on a common focal-plane point and radial second moment within .002 of R²/2, invalid cameras, 101 motion times over [-1,2] with BVH/linear agreement, spawn and transparent/shadow time inheritance, emission eligibility after alpha=0, linear opacity decoding, emitter-alpha rejection, and shadow transmission exactly 1/.5/0/.25 plus terminal opaque blockage. A 20000-path alpha camera test estimates two-layer transmission within .015 of .25 at depth=1 (crossings consume no bounce).

## Recorded renders

All comparisons: 320×200, 128 spp, depth 8, seed 27. Lens default 0, shutter default [0,0]. Commands below were executed; render seconds include writing and exclude scene/BVH setup. Early regression runs overlapped the first comparisons, so timings are observations rather than controlled benchmarks.

```sh
./build-release/path_tracer --scene dof --width 320 --height 200 --spp 128 --depth 8 --seed 27 --output build-release/v02-dof-off.ppm
```

triangles=2 bvh_seconds=2.125e-06
320x200 spp=128 depth=8 seed=27 mean_linear=0.145744,0.128995,0.12234 seconds=4.66066 output=build-release/v02-dof-off.ppm

```sh
./build-release/path_tracer --scene dof --width 320 --height 200 --spp 128 --depth 8 --seed 27 --lens-radius .22 --focus-distance 6 --output build-release/v02-dof-on.ppm
```

triangles=2 bvh_seconds=1.875e-06
320x200 spp=128 depth=8 seed=27 mean_linear=0.145773,0.129022,0.122385 seconds=4.06705 output=build-release/v02-dof-on.ppm

```sh
./build-release/path_tracer --scene motion --width 320 --height 200 --spp 128 --depth 8 --seed 27 --shutter-open .5 --shutter-close .5 --output build-release/v02-motion-off.ppm
```

triangles=2 bvh_seconds=1.208e-06
320x200 spp=128 depth=8 seed=27 mean_linear=0.156201,0.142852,0.141336 seconds=2.45408 output=build-release/v02-motion-off.ppm

```sh
./build-release/path_tracer --scene motion --width 320 --height 200 --spp 128 --depth 8 --seed 27 --shutter-open 0 --shutter-close 1 --output build-release/v02-motion-on.ppm
```

triangles=2 bvh_seconds=1.083e-06
320x200 spp=128 depth=8 seed=27 mean_linear=0.156236,0.14286,0.141341 seconds=2.70501 output=build-release/v02-motion-on.ppm

```sh
./build-release/path_tracer --scene alpha-zero --width 320 --height 200 --spp 128 --depth 8 --seed 27 --output build-release/v02-alpha-zero.ppm
```

triangles=4 bvh_seconds=1.375e-06
320x200 spp=128 depth=8 seed=27 mean_linear=0.252419,0.252419,0.252419 seconds=2.91961 output=build-release/v02-alpha-zero.ppm

```sh
./build-release/path_tracer --scene alpha-half --width 320 --height 200 --spp 128 --depth 8 --seed 27 --output build-release/v02-alpha-half.ppm
```

triangles=4 bvh_seconds=1.209e-06
320x200 spp=128 depth=8 seed=27 mean_linear=0.231157,0.238259,0.231409 seconds=3.00588 output=build-release/v02-alpha-half.ppm

```sh
./build-release/path_tracer --scene alpha-one --width 320 --height 200 --spp 128 --depth 8 --seed 27 --output build-release/v02-alpha-one.ppm
```

triangles=4 bvh_seconds=1.167e-06
320x200 spp=128 depth=8 seed=27 mean_linear=0.21203,0.224923,0.212486 seconds=3.02691 output=build-release/v02-alpha-one.ppm

```sh
./build-release/path_tracer --scene alpha-double --width 320 --height 200 --spp 128 --depth 8 --seed 27 --output build-release/v02-alpha-double.ppm
```

triangles=6 bvh_seconds=1.792e-06
320x200 spp=128 depth=8 seed=27 mean_linear=0.218985,0.228577,0.219315 seconds=4.77722 output=build-release/v02-alpha-double.ppm

```sh
./build-release/path_tracer --scene alpha-pattern --width 320 --height 200 --spp 128 --depth 8 --seed 27 --output build-release/v02-alpha-pattern.ppm
```

triangles=4 bvh_seconds=2.083e-06
320x200 spp=128 depth=8 seed=27 mean_linear=0.241776,0.245462,0.241907 seconds=3.12269 output=build-release/v02-alpha-pattern.ppm

PPM files retain originals; PNG inspection copies use identical basenames. Inspection showed: middle green sphere substantially sharper than near red/far blue with lens radius .22, all sharp with zero radius; moving sphere and its shadow spread along horizontal velocity during [0,1] exposure, versus a centered instantaneous sphere at t=.5; alpha=0 removes both sheet and shadow, alpha=1 blocks, half layers cast progressively darker overlapping shadows, and leaf silhouette/holes appear in both camera view and ground projection. Display-mapped image intensities are not used as numerical transmittance proof; the linear shadow tests establish .25 exactly. Sphere surfaces are curved so only the focal-plane slice is exactly focused, not every point on the middle sphere.

## Regression and repeatability

```sh
./build-release/path_tracer --scene forest --width 120 --height 68 --spp 8 --depth 10 --seed 19 --output build-release/forest-v02-regression.ppm
./build-release/path_tracer --width 48 --height 48 --spp 32 --seed 93 --output build-release/cornell-v02-regression.ppm
./build-release/normal_preview --width 320 --height 180 --output build-release/normal-v02-regression.ppm
```

Forest 0.297961 s, Cornell 0.820368 s. All three byte-identical to corresponding v0.1/v0 files. No expected-value changes used to excuse regressions. Forest still has no lens blur, motion or alpha enabled.

Motion repeated twice with BVH and once linear (80×50, 64 spp, depth 8, seed 31, shutter [0,1]); all outputs byte-identical, times .0849579/.0873675/.0691196 s. Exact commands and logs: build-release/v02-repeat.json. Invalid negative lens radius, zero focus distance, reversed shutter and NaN lens rejected with exit 1. No promise of cross-platform byte identity.

## Scope and limitations

Only sphere translation, static camera during exposure, global shutter and uniform disk thin lens. Shutter endpoints must match BVH build interval. Alpha is independent thin-surface coverage per crossing, not refraction/volume; no alpha emitters, no exact coplanar layer ordering, and extremely small gaps may interact with offsets. Safety crossing cap throws at 4096 rather than counting crossings as BSDF depth. No texture filtering or optical diffraction. Motion/alpha remain Monte Carlo noisy at finite spp. Final forest aesthetic integration and Windows verification remain pending; no extra optional features implemented.
