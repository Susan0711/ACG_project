# Scene layer decoupling — stage 1 validation

2026-11-19, macOS Apple M1, Apple Clang 17, Release arm64. Uses the actual dirty working tree including v0.5 and demo_pack, not a checkout of v0.4. No branch switch, staging, commit, remote operation, external course/cache reads, or deletion of user files. Old renders retained.

## Pre-refactor evidence

Before editing C++/CMake:
```sh
cmake -S . -B build-scene-decouple -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-scene-decouple -j4
ctest --test-dir build-scene-decouple --output-on-failure
python3 build-scene-evidence/check.py before
```
New Release build succeeded; existing 6/6 tests passed in 3.64 s. Baselines under ignored build-scene-evidence/before were freshly rendered by this build; no older v04/v05 picture substituted. Local check.py and per-phase commands.json retain executable paths, every argument, and renderer timing output. Common scene parameters: 80×50, 16 spp, depth 12, seed 71, 4 threads, MIS; both PPM and linear PFM saved. DOF lens .22/focus 6, motion shutter [0,1] explicitly enabled.

Exact pre-refactor commands (relative output files, resources/executable resolved from this repository):

```sh
/Users/susan/Desktop/大二上/ACG/ACG_project/build-scene-decouple/path_tracer --scene cornell --resource-root /Users/susan/Desktop/大二上/ACG/ACG_project/assets --width 80 --height 50 --spp 16 --depth 12 --seed 71 --threads 4 --mode mis --output build-scene-evidence/before/cornell.ppm --linear-output build-scene-evidence/before/cornell.pfm
```

```sh
/Users/susan/Desktop/大二上/ACG/ACG_project/build-scene-decouple/path_tracer --scene texture --resource-root /Users/susan/Desktop/大二上/ACG/ACG_project/assets --width 80 --height 50 --spp 16 --depth 12 --seed 71 --threads 4 --mode mis --output build-scene-evidence/before/texture.ppm --linear-output build-scene-evidence/before/texture.pfm
```

```sh
/Users/susan/Desktop/大二上/ACG/ACG_project/build-scene-decouple/path_tracer --scene sampling --resource-root /Users/susan/Desktop/大二上/ACG/ACG_project/assets --width 80 --height 50 --spp 16 --depth 12 --seed 71 --threads 4 --mode mis --environment studio-hdr.pfm --output build-scene-evidence/before/sampling.ppm --linear-output build-scene-evidence/before/sampling.pfm
```

```sh
/Users/susan/Desktop/大二上/ACG/ACG_project/build-scene-decouple/path_tracer --scene dof --resource-root /Users/susan/Desktop/大二上/ACG/ACG_project/assets --width 80 --height 50 --spp 16 --depth 12 --seed 71 --threads 4 --mode mis --lens-radius .22 --focus-distance 6 --output build-scene-evidence/before/dof.ppm --linear-output build-scene-evidence/before/dof.pfm
```

```sh
/Users/susan/Desktop/大二上/ACG/ACG_project/build-scene-decouple/path_tracer --scene motion --resource-root /Users/susan/Desktop/大二上/ACG/ACG_project/assets --width 80 --height 50 --spp 16 --depth 12 --seed 71 --threads 4 --mode mis --shutter-open 0 --shutter-close 1 --output build-scene-evidence/before/motion.ppm --linear-output build-scene-evidence/before/motion.pfm
```

```sh
/Users/susan/Desktop/大二上/ACG/ACG_project/build-scene-decouple/path_tracer --scene alpha-pattern --resource-root /Users/susan/Desktop/大二上/ACG/ACG_project/assets --width 80 --height 50 --spp 16 --depth 12 --seed 71 --threads 4 --mode mis --output build-scene-evidence/before/alpha-pattern.ppm --linear-output build-scene-evidence/before/alpha-pattern.pfm
```

```sh
/Users/susan/Desktop/大二上/ACG/ACG_project/build-scene-decouple/path_tracer --scene forest-v03 --resource-root /Users/susan/Desktop/大二上/ACG/ACG_project/assets --width 80 --height 50 --spp 16 --depth 12 --seed 71 --threads 4 --mode mis --environment studio-hdr.pfm --env-rotation 30 --env-intensity .7 --output build-scene-evidence/before/forest-v03.ppm --linear-output build-scene-evidence/before/forest-v03.pfm
```

```sh
/Users/susan/Desktop/大二上/ACG/ACG_project/build-scene-decouple/path_tracer --scene forest-v05 --resource-root /Users/susan/Desktop/大二上/ACG/ACG_project/assets --width 80 --height 50 --spp 16 --depth 12 --seed 71 --threads 4 --mode mis --environment studio-hdr.pfm --env-rotation 115 --env-intensity 1.1 --output build-scene-evidence/before/forest-v05.ppm --linear-output build-scene-evidence/before/forest-v05.pfm
```

```sh
./build-scene-decouple/normal_preview --width 160 --height 90 --output build-scene-evidence/before/normal.ppm
```


## Post-refactor results

Same configure/build/CTest commands after the extraction: all original suites plus catalog **7/7 passed**, final run 2.30 s, no compiler warnings observed. Catalog tests cover 19 unique old names, descriptions/factories, successful construction of every entry, identity of lookup entries, ordered help names, and informative unknown-name rejection. Actual executable --help was separately parsed (19 unique names), and --scene missing-test returned exit 1 with its name.

`python3 build-scene-evidence/check.py after`, followed by a final-source run `... check.py final`: Cornell, texture, sampling+HDR/MIS, dof, motion, alpha-pattern, forest-v03, forest-v05 all match the before PPM **and** linear PFM byte-for-byte. normal_preview 160×90 PPM also byte-identical. 17/17 exact comparisons; no mean-only tolerance and no old test expectation edits.

Forest-v05 supplemental 1/4-thread check: 80×50, 16 spp, depth 12, seed 71, MIS, studio-hdr.pfm 115°/1.1. Both PPM/PFM byte-identical. Exact argv and timings retained in ignored build-scene-evidence/thread-commands.json. Existing render_tests additionally compare double pixel buffers for Cornell/motion/alpha.

Quick demo:
```sh
python3 tools/demo_pack.py --executable build-scene-decouple/path_tracer --resources assets --output build-scene-evidence/demo --profile quick
```
13/13 successful, 0 failures, total wall 2.338 s. Directory: build-scene-evidence/demo/demo-quick-20261119T152659Z-28478d03; manifest.json includes exact argv, timing, dirty Git provenance and PPM checksums/completeness. No old output overwritten. Forest-v05/DOF/alpha images inspected from this new run. No high-cost MIS statistics rerun was necessary.

## Mechanical changes / known coupling

World::add_area moved unchanged to world.cpp, in tracer. Demonstration factories moved without body changes from integrator/effects/CLI; all forest geometry stays in place. scene_library depends on tracer; tracer depends on geometry and Threads; no reverse link to demos. Compatibility factory declarations remain in tracer.h. The geometry-only normal preview retains its small historical preview_scene helper; this stage does not migrate it. Legacy factories may build BVH themselves while CLI rebuilds afterward; behavior retained, not optimized. Existing factory declarations do not imply that linking tracer alone resolves demos; call sites now explicitly link scene_library where needed.

Windows/MinGW verification on older commits does not validate this layout. This stage has **no Windows/MSVC verification**; teammate must reconfigure/rebuild/retest because CMake target dependencies changed. No teammate's unsubmitted files were used. See ARCHITECTURE for scene addition steps and later interface questions.
