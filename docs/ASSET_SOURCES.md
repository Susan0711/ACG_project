# Forest Creek Cabin — references and asset sources

Links below were checked on 2026-11-19. **Reference images are for visual study only.** No external model, image, HDRI, or texture has been downloaded or approved for inclusion in this repository. For each asset actually used, add the exact downloaded variant, author, license, format, edits, polygon count, and whether redistribution is permitted.

## Rendering references (view only; rights remain with creators)

| Work / creator | Link | Visual cue |
| --- | --- | --- |
| Realistic Forest Cabin / Saad Razzaq | https://blenderartists.org/t/realistic-forest-cabin-blender-cycles/1602780 | Cabin, water reflection, wood and rocks; Blender Cycles. |
| Forest Cabins at Dusk / MR Rendering | https://blenderartists.org/t/archviz-forest-cabins-at-dusk-storytelling-lighting-blender-cycles-mr-rendering/1615643 | Warm windows against cool woods; Blender Cycles. |
| Cabin in the Forest / Paulina Kochanowicz | https://blenderartists.org/t/cabin-in-the-forest/1525632 | Layered forest composition; Blender Cycles. |
| A Cabin in the Woods / Nick Mez | https://blenderartists.org/t/a-cabin-in-the-woods/1338628 | Warm cabin in misty woods; Blender Cycles. |
| Jungle Retreat / Soheil Mahmoudi | https://blenderartists.org/t/jungle-retreat-a-secluded-cabin-by-the-river/1458633 | Creek, cabin and tree framing; Blender Cycles. |
| Morning Creek / 起凡 pan | https://zs66384.artstation.com/projects/zzJrVZ | Creek, mossy rocks and soft morning light; UE5 3D environment. |

## Candidate assets (selection and import pending)

Poly Haven confirms its models, textures and HDRIs are [CC0](https://polyhaven.com/license); [ambientCG confirms CC0](https://docs.ambientcg.com/license/). Credit even where CC0 does not require it, as the course announcement asks for geometry sources. The cabin, terrain, creek surface, and the required original color texture should be created by the team.

| Candidate / author | Link | License; offered formats | Import note |
| --- | --- | --- | --- |
| Rock Moss Set 01 / Kless Gyzen | https://polyhaven.com/a/rock_moss_set_01 | CC0; model with image maps, check selected download format | ~63K triangles for six rocks; use a subset or simplify. Low to medium effort for OBJ/glTF importer. |
| Pine Tree 01 / Rico Cilliers, Rob Tuytel | https://polyhaven.com/a/pine_tree_01 | CC0; model and PNG/JPG/EXR maps | Full model is ~17M triangles with alpha foliage: **reference only for now**. Prefer self-made low-poly trees. |
| Pine Bark / Dimitrios Savva | https://polyhaven.com/a/pine_bark | CC0; PNG/JPG/EXR maps, material package | Download a small diffuse map if needed; UV mapping and image read required. |
| Xanderklinge forest-stream HDRI / Andreas Mischok | https://polyhaven.com/a/xanderklinge | CC0; HDR/EXR | Use a 1K–2K variant only after HDR environment sampling exists; high-resolution variant is unnecessary. |
| Misty Pines HDRI / Greg Zaal | https://polyhaven.com/a/misty_pines | CC0; HDR/EXR | Softer fallback lighting; same HDR importer dependency. |
| ambientCG materials / site contributors | https://ambientcg.com/ | CC0; individual assets commonly offer image-map packages | Choose exact wood/ground/rock pages later and record their IDs before use; this is a catalog, not a selected asset. |

## Asset intake record template

`Name | creator | original asset URL | license URL/version | exact file/format/resolution | triangle count | edits | local path | redistribution allowed?`

Do not add a third-party asset to Git until its exact license and redistribution status are recorded. Keep course PDFs and reference artwork out of the repository.

## v0.1 original assets (2026-11-19)

Created in this repository by the Codex assistant at the user's request; no teammate authorship claimed, no external downloads or source URL. No third-party license restrictions introduced; project owner controls distribution (no new project-wide license granted here).

- `assets/wood.ppm`: ASCII P3, 256×256 sRGB. Original deterministic Python generator `tools/generate_assets.py`: warped longitudinal growth lines, elliptical knot distortion, plank seams, fine pores/noise (seed 19). No source image. Used on cabin/trunks; no edits after generation.
- `assets/rock.obj`: original 8-triangle octahedral rock, authored by the same generator. v/vt/vn and positive/negative index examples. Instances use nonuniform scaling and Y rotation; flat shading.
- `assets/test_colors.ppm`: original 2×2 hand-authored sRGB test pattern for orientation/transfer-function checks.
- Cabin, terrain, creek and trees: original procedural geometry in `src/forest.cpp`; no external model/texture rights required.
- `assets/index_cases.obj` and `assets/invalid_quad.obj`: original hand-authored parser fixtures by Codex; the latter intentionally contains an unsupported quad. No external source/license dependency.

## v0.2 original opacity asset

`assets/leaf-opacity.ppm` — created by Codex assistant in this repository at the user's request; no external URL/source or third-party license restriction. 128×128 ASCII P3, linear binary opacity (not sRGB). `tools/generate_opacity.py` constructs four rotated elliptical leaf outlines with narrow vein holes and a diagonal stem. No source photograph and no teammate authorship claimed. Project owner controls distribution; no new project-wide license asserted. Used solely in the controlled alpha-pattern scene; no forest vegetation upgrade.

## v0.3 original scene geometry

`src/forest_v03.cpp` is original procedural geometry authored by Codex in this repository under the user's direction; no external asset URL or third-party content. No teammate participation is asserted. Existing original wood.ppm is reused without editing. Project owner controls distribution; no additional license granted here.

Methods: shared sinusoidal channel cross sections and connected bank strips; slab/box cabin construction with omitted front wall regions for actual window holes, inset rectangular lamps, framing and board seams; tapered seven-sided branch cylinders; three irregular layered crown profiles with deterministic position/scale/phase variation; flattened three-ring nine-sided boulders with irregular heights; sparse opaque triangular grass clusters. Seed 7003 fixes scene generation. 11,162 triangles plus one spherical door handle in the current main/detail/gray geometry. Trees/rocks are generated directly, while original forest retains the OBJ import demonstration. No downloaded resource added.

## v0.4 original floating-point environment

`assets/studio-hdr.pfm`: original analytic environment authored by Codex under the user's request, generated by `tools/generate_environment.py`; no photograph, external download, source URL or third-party library. RGB PFM, 128×64, IEEE float32 linear radiance. Cool hemispheric sky gradient plus a finite Gaussian sun lobe (amplitude 70, angular scale .075 radians) at theta=.65, phi=2.3. Values above 1 are retained, not derived from a tone-mapped image. Generator refuses overwrites. Project owner controls distribution; no teammate authorship or additional license grant asserted. No new CC0 asset was needed/downloaded, and no third-party license file is required for the self-written PFM reader.

## v0.5 original scene and quieter timber

`src/forest_v05.cpp/.h`: scene-only adaptation of original forest_v03 authored by Codex under user direction; no teammate contribution inferred. Uses asymmetric branch/foliage fans, clustered trees and shore boulders, patch-selected grass, per-board gable mapping. No external assets or licenses introduced.

`assets/wood-soft-v05.ppm`: 256×256 ASCII P3 sRGB, derived from retained original wood.ppm by tools/generate_wood_v05.py. Channel contrast compressed to 25% around weathered cedar base (108,83,59); original file unchanged. UV repeats increase on scene geometry and gable is mapped per narrow board, reducing oversized grain. No photo source or URL; owner controls redistribution, no new license grant asserted. Existing studio-hdr.pfm reused unchanged.
