# Implementation handoff checklist (not a grading claim)

Authority remains the course announcement identified in PROJECT_PLAN. This table links repository evidence, not awarded points. Every row has macOS Release checks; Windows/MSVC remains pending via WINDOWS_VALIDATION.md.

| Core item | Implementation / evidence | Limitations / next verification |
|---|---|---|
| Path tracing, diffuse/specular | src/tracer.cpp; tracer_tests; V0_VALIDATION Cornell | Finite depth bias; MIS now has v0.4 evidence |
| Custom scene / geometry credits | forest_v03.cpp; ASSET_SOURCES; V03_VALIDATION | First readable scene, still stylized; no final quality claim |
| CPU BVH | bvh.cpp; tracer_tests, effects_tests; V01 benchmark | Median split, no SAH |
| Transmission | ideal Fresnel dielectric; tracer_tests; Cornell | Ideal glass, no nested media; final nontrivial scene demonstration pending |
| Original color texture / mapping | assets/wood.ppm, generate_assets.py; pipeline_tests; V01 texture image | Nearest filtering, procedural wood remains stylized |
| Importance sampling / RR | cosine BSDF and compensated RR; tracer_tests; V0 comparison | V0/V04 statistical checks, not a proof of unbiasedness |
| Motion blur | effects.cpp; effects_tests; V02 motion pair | Translating spheres/global shutter only |
| Depth of field | Camera::sample; effects_tests; V02 dof pair | Thin lens only |
| Alpha shadow | linear coverage + layered transmission; effects_tests; V02 leaf image | Thin surfaces, opaque emitters, not volumes |
| Point / area lights | linked area geometry + NEE; pipeline_tests; V0/V03 images | No moving lights; straight shadow paths block glass |
| Anti-aliasing | random pixel samples; render.cpp | Finite sample noise |

Ordinary BVH and importance sampling are core, not optional. SAH, Principled, volume rendering and GUI are absent. HDR environment lighting (original linear PFM, not a downloaded photographic HDRI) and MIS are now implemented; see the technical evidence below. Optional-point eligibility for presentation or texture categories is not asserted. Multithreading is infrastructure, not a claimed course optional score.


## v0.4 optional-feature evidence (not scoring)

| Feature | Code / validation | Scope / ownership |
|---|---|---|
| Multiple importance sampling | src/integrator.cpp; environment_tests; V04_VALIDATION multi-seed means/RMSE/time | Power heuristic, all-lights enumeration, diffuse versus area/HDR techniques; modest scene-dependent gain; authored by Codex |
| HDR environment lighting | src/environment.*; assets/studio-hdr.pfm; V04 rotation/intensity/forest images | Real linear floats >1, importance sampling, PFM only; analytic original rather than photographic HDRI; authored by Codex |

Both remain pending Windows verification and course review. Optional-point attribution to either teammate is not established here. Existing Windows entry point still runs the expanded CTest suite.
