# Forest Creek Cabin — project plan

## Goal and authority

Two-person Advanced Computer Graphics 2026 Image Rendering project. Build a C++17/CMake **CPU path tracer** that produces a polished static image of a forest creek and cabin: cool woodland light, warm window light, reflective creek, wood, rock, and restrained vegetation. The local `../ACG_Project_Announcement.pdf` is the sole authority for course requirements and grading; this file is a working plan.

## Requirements checked against the announcement

**Core (red):** path tracing with diffuse and specular materials; an attractive custom scene with credit for external geometry; CPU acceleration structure (BVH); nontrivial transmissive material; self-created nontrivial color texture with correct mapping; importance sampling with Russian Roulette; motion blur, depth of field, and alpha shadow; point and area lights; anti-aliasing. Demonstrate each core feature in the main image or a small controlled scene. The announcement explicitly requires a path tracer, not a rasterizer.

**Optional (black):** scene tidiness/attractiveness 1 pt; SAH or another advanced acceleration algorithm 2 pts; Principled BSDF 2 pts; functional texture maps 1 pt each (up to 2); multiple importance sampling 2 pts; HDR environment lighting 2 pts. Other optional material, volume, special-effect, mipmap, and simulation items remain outside the initial scope. Ordinary BVH and importance sampling with Russian Roulette are core work, not optional points.

For two people, completing the red items gives each 15 oral-report points; each black item can be credited to only one person, with up to 10 more points per person. The team midterm report is due **2026-12-02** (two SIGGRAPH-template pages, 5 pts). Each person gives a roughly 5-minute oral presentation plus 3-minute Q&A in Week 15. Individual final reports (at least three SIGGRAPH-template pages) and team source code are due **2027-01-13**. Inaccurate implementation or contribution claims can reduce the code-verification coefficient.

## Minimal scene and technical route

One camera-facing cabin across a shallow creek; low-poly terrain, 4–8 reusable rocks, a few simple trees and shrubs, and a static water mesh. Model the cabin and terrain in-house. Use third-party assets sparingly, after license and polygon-count checks. Start with constant environment light, one point light, and one window area light; consider HDR after the core works. Avoid fluid simulation, dense foliage, and complex volume effects in the first two weeks.

Implementation order: `Ray/Hit/Film/Camera` → primitive intersection → simple BVH → path integrator and BSDF → importance sampling/Russian Roulette → point and area lights → image texture/UV → camera effects and alpha shadow → scene integration. Use deterministic seeds for comparison images. A CLI selects scene, size, samples, seed, and output path; no GUI or real-time interaction is required.

## Ownership and evidence

| Person | Primary work | Independent evidence | Optional target after core |
| --- | --- | --- | --- |
| A | Integrator, diffuse/specular sampling, BVH, random sampling/RR, point and area lighting, film output | Cornell Box image, spp/noise and BVH timing comparisons | MIS (2 pts) or SAH (2 pts), according to the measured bottleneck |
| B | Transmissive material, self-created texture/UV, OBJ/scene loading, cabin/creek scene, depth of field, motion blur, alpha shadow | Material/texture and camera-effect comparisons; dated scene renders | Normal map (1 pt), then HDR (2 pts) if time permits |

Both review the shared interfaces, build on their own operating systems, check core requirements, and document their own contributions. Optional points are targets, not assumed awards.

## Milestones through December 2

| Dates | Deliverable |
| --- | --- |
| Nov 18–20 | Shared interfaces and CMake CLI; Cornell Box or equivalent first image on Mac and Windows. |
| Nov 21–23 | Diffuse/specular paths, basic BVH, point/area-light test; B supplies transmissive test and forest greybox. |
| Nov 24–26 | Importance sampling and RR; self-created mapped color texture; first colored cabin/creek preview. |
| Nov 27–29 | Core camera effects and alpha-shadow test images; scene assets and composition; BVH and sampling comparisons. |
| Nov 30–Dec 1 | Stabilize one presentable image and evidence; write the two-page midterm with honest completed/remaining status. |
| Dec 2 | Submit midterm; preserve time afterward for final image polish and selected optional features. |

## First-week tasks and stop rules

1. Agree on world units, right-handed coordinates, linear RGB, UV orientation, material IDs, scene paths, and `Ray`/`HitRecord`/`Material` interfaces before parallel implementation.
2. A: compile a CLI that renders spheres/triangles, then Cornell Box with diffuse/specular paths and basic BVH.
3. B: build the Cornell material scene, simple dielectric sample, self-created texture prototype, and a low-poly cabin/creek greybox. Record every external asset in `ASSET_SOURCES.md` before use.
4. Both: run the same small scene on Mac and Windows by Nov 23; save build instructions and a reproducible low-resolution render command.
5. If integration or render speed slips, reduce foliage and image resolution first. Defer all optional features until the red core is demonstrated. If any core item remains incomplete on Dec 2, state it plainly in the midterm and schedule its completion next.


## Implemented checkpoint — 2026-11-19: CPU Path Tracer v0

Implemented and tested on macOS/Apple Clang 17 in Release: reusable sphere/triangle intersection, linear Scene and binary BVH, configurable pinhole camera, floating-point film, deterministic sampling, multi-bounce Lambertian/mirror/ideal dielectric transport (Fresnel/refraction/TIR), emissive rectangles, point/area direct sampling, cosine importance sampling, compensated Russian Roulette, pixel jitter, Cornell and point-light test scenes. Normal previews remain available. No MIS; direct-light versus emitted-hit accounting uses the previous-event delta convention documented in README.

Validation: original geometry tests plus tracer tests; 256×256 Cornell 128 spp / depth 16 and point scene 64 spp / depth 16 rendered. Repeat-seed and BVH/linear small renders matched byte-for-byte; original single sphere matched the prior output. Windows/MSVC remains unverified. Render evidence and direct-sampling statistics are recorded in docs/V0_VALIDATION.md.

Still incomplete: final attractive forest/cabin scene, external-asset workflow in practice, self-created mapped texture, motion blur, depth of field, alpha shadow, and full course-feature demonstrations/reporting. Ideal glass is a v0 material test, not completion of the final nontrivial transmissive-scene requirement. No optional-point completion is claimed. The finite depth cap biases transport; glass caustics remain noisy without specialized sampling. Earlier milestones above are plans, not completion claims.

## v0.1 checkpoint — 2026-11-19

Implemented original PPM wood texture with UV mapping, linear procedural/constant textures, sRGB image decoding, triangular OBJ with relative indices and scale/rotation/translation, linked rectangular emitters and unregistered-emission handling, escape-only environment, and first forest cabin scene. Cabin, banks, creek, rocks and trees are original generated geometry; rocks actually use the OBJ pipeline. This is assistant-produced implementation, not a claim about either teammate's contribution.

Pending: final composition/material refinement, broader asset/transform robustness, Windows verification, filtered textures, smooth shading. Still absent: motion blur, depth of field, alpha shadow, formal course demonstrations and reports. No MIS, SAH, Principled, GUI or multithreading added. First forest preview does not establish completion of all core requirements.

## v0.2 checkpoint — 2026-11-19

Implemented and verified on macOS Release: thin-lens depth of field with uniform disk sampling, single shutter-time sampling and translating sphere motion with swept BVH bounds, and thin-surface alpha coverage for camera paths and multi-layer shadows. Dedicated dof/motion/alpha validation scenes and original leaf opacity asset accompany comparisons and tests. Existing forest remains unchanged with effects disabled; forest/Cornell/normal byte regressions passed. See V02_VALIDATION.md.

These establish implementation of the three effects in controlled scenes, not a claim that the final forest composition demonstrates every course requirement. Pending: Windows verification, final scene/material/lighting quality, final demonstration integration, reports and broader robustness. No MIS, SAH, volumes, complex water, upgraded vegetation or multithreading added. Motion is translating spheres only; alpha is coverage rather than dielectric/volume transmission; opaque emitters only.

## v0.3 checkpoint — 2026-11-19

Original forest baseline retained; new forest-v03 supplies curved creek, continuous banks, more varied original tree/rock geometry, actual cabin window openings with inset lights, frames/door/eaves/foundation and foreground details. Main/gray/detail cameras and evidence are in V03_VALIDATION. This is a readable first natural composition, not final realism or final image quality.

Implemented C++17 deterministic tile rendering, independent per-pixel/sample RNG, joined worker exception propagation, phase timing and explicit resource-root CLI. New thread-count-invariance tests complement previous algorithm tests. The sampling allocation change intentionally replaces old stochastic byte baselines; old images and normal regression remain preserved.

Windows entry point and checklist prepared, **not executed**. Team member may independently verify Windows/MSVC and later take a scoped technical module such as texture filtering/mesh robustness or an agreed optional feature. These are open tasks, not completed contributions. Remaining final-scene polish, report/demonstration integration and all unimplemented optional items remain pending; no course score is claimed.

## v0.4 checkpoint — 2026-11-19

Implemented true float PFM latlong environment evaluation/rotation/intensity and luminance×solid-angle importance sampling, plus BSDF/NEE/MIS modes with power heuristic and explicit per-light enumeration probabilities. Self-authored analytic HDR environment, not a photographic HDRI or tone-mapped color image. Integrator now queries the final emission/escape endpoint after the last allowed scattering event so all modes share the same depth truncation. Prior geometry/effects remain intact; forest-v03 HDR version changes only lighting.

Evidence: environment_tests, multi-seed linear PFM comparisons, independent high-spp reference pair, near-equal-time comparison and actual HDR/rotation/forest images in V04_VALIDATION. Implementation and analytic assets were authored by Codex at the user's request; no teammate contribution is implied. Windows remains unverified. MIS and HDR are implemented features with evidence, **not awarded optional points**. Limitations include PFM-only input, nearest texel lookup, finite-depth/reference noise, no nested media and no specialized caustic sampling. SAH, Principled, normal maps, volumes and GUI remain absent.

## v0.5 main visual checkpoint — 2026-11-19

Added independent forest-v05/gray/detail scenes while leaving the shared v0.4 algorithms, public interfaces and original forest/v03 source unchanged for teammate Windows validation. Scene iteration reduces overstrong wood grain, maps the gable per board, uses asymmetric foliage fans and grouped trees, irregular boulder clusters/open bank stretches, and zoned vegetation. Two low-spp HDR lighting candidates compared; selected 115° / intensity 1.1 for subject readability and softer warm windows. Actual evidence is in V05_VALIDATION.

Still low-poly/stylized, not final semi-realistic quality. Windows remains pending; public baseline handoff is not claimed as completed teammate contribution. Next scene work can improve sky/background integration, foliage mass and ground/shore material transitions after current scope review. No new algorithms or optional-point claims.
