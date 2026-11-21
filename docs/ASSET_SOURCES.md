# Forest Creek Cabin — references and asset sources

The forest reference list dates from 2026-11-19. Reference images are for visual study only. The project keeps its asset-generation scripts locally and ships the generated meshes, textures, and scene files. The Tidewatch scene uses the following external assets.

## Tidewatch · Kenney Castle Kit 2.0

Kenney created and distributed Castle Kit 2.0 under CC0 1.0 Universal, which allows adapting and redistributing the models and their color atlas. The archive used here is the release posted by Kenney on OpenGameArt. [Kenney asset page](https://kenney.nl/assets/castle-kit) · [Author's release and ZIP](https://opengameart.org/content/castle-kit) · [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/)

Archive SHA-256: `921f3f73927bb23106cae34bc21d5ab4b033a9fc120475e96f714a406e3169df`. The generator reads selected files from `Models/OBJ format/`, bakes each module's scale and yaw, keeps its UVs, drops repeated triangles and zero-area faces, and relies on geometric normals for flat shading. It converts Kenney's sRGB color atlas from a 512×512 PNG to a 128×128 P3 PPM. The following counts are per converted OBJ before placement. [Tidewatch scene](../scene/tidewatch.json)

| Kenney OBJ | Bundled OBJ in `assets/tidewatch/` | Triangles |
| --- | --- | ---: |
| tower-hexagon-base / tower-hexagon-mid / tower-hexagon-top / tower-hexagon-roof | beacon-base / beacon-gallery / beacon-eaves / beacon-roof | 68 / 404 / 168 / 68 |
| tower-square-base / tower-square-mid-windows / tower-square-top-roof-high-windows | keep-base / keep-windows / keep-roof | 28 / 356 / 254 |
| wall-doorway / wall-narrow | gate / wall-front / wall-side | 498 / 126 / 126 |
| flag-pennant / tree-large / tree-small / rocks-large | pennant / cypress / sapling / reefs | 118 / 229 / 164 / 150 |

The generator builds the island cliffs and turf, sea surface, offshore ridges, sky and sun geometry, lantern glazing and lamp, foliage cards, jetty, sailboat and wake specifically for this scene. The jetty and ivy also reuse the project's wood and leaf textures. The assembled scene and preconverted OBJ files render offline.

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
- The rock now has a UV layout on all eight faces. Its JSON material uses `assets/rock.ppm`, a 256 × 256 sRGB P3 color map produced with the built-in ImageGen tool and converted to the renderer's input format. Generation prompt: “灰绿色溪石的可平铺正方形漫反射贴图。正交俯视，岩石表面铺满画面，哑光细颗粒、几条细且不规则的浅灰石英脉，均匀无阴影、无高光，无物体轮廓、文字和边框。”
- `assets/test_colors.ppm`: original 2×2 hand-authored sRGB test pattern for orientation/transfer-function checks.
- Cabin, terrain, creek and trees: original procedural geometry in `tests/scenes/forest.cpp`; no external model/texture rights required.
- `assets/index_cases.obj` and `assets/invalid_quad.obj`: original hand-authored parser fixtures by Codex; the latter intentionally contains an unsupported quad. No external source/license dependency.

## v0.2 original opacity asset

The JSON alpha-shadow example reuses [quad.obj](../assets/quad.obj) for the leaf and the ground tiles. This square was authored in the project with four vertices, two triangles and one full UV tile.

`assets/leaf-opacity.ppm` — created by Codex assistant in this repository at the user's request; no external URL/source or third-party license restriction. 128×128 ASCII P3, linear binary opacity (not sRGB). `tools/generate_opacity.py` constructs four rotated elliptical leaf outlines with narrow vein holes and a diagonal stem. No source photograph and no teammate authorship claimed. Project owner controls distribution; no new project-wide license asserted. Used solely in the controlled alpha-pattern scene; no forest vegetation upgrade.

## v0.3 original scene geometry

`tests/scenes/forest_v03.cpp` is original procedural geometry authored by Codex in this repository under the user's direction; no external asset URL or third-party content. No teammate participation is asserted. Existing original wood.ppm is reused without editing. Project owner controls distribution; no additional license granted here.

Methods: shared sinusoidal channel cross sections and connected bank strips; slab/box cabin construction with omitted front wall regions for actual window holes, inset rectangular lamps, framing and board seams; tapered seven-sided branch cylinders; three irregular layered crown profiles with deterministic position/scale/phase variation; flattened three-ring nine-sided boulders with irregular heights; sparse opaque triangular grass clusters. Seed 7003 fixes scene generation. 11,162 triangles plus one spherical door handle in the current main/detail/gray geometry. Trees/rocks are generated directly, while original forest retains the OBJ import demonstration. No downloaded resource added.

## v0.4 original floating-point environment

The [HDR rotation scene](../scene/hdr_environment.json) uses an original unit sphere with radial vertex normals and a two-triangle ground plane. Both spheres in the scene use the same [mesh](../assets/studio-sphere.obj).

`assets/studio-hdr.pfm`: original analytic environment authored by Codex under the user's request, generated by `tools/generate_environment.py`; no photograph, external download, source URL or third-party library. RGB PFM, 128×64, IEEE float32 linear radiance. Cool hemispheric sky gradient plus a finite Gaussian sun lobe (amplitude 70, angular scale .075 radians) at theta=.65, phi=2.3. Values above 1 are retained, not derived from a tone-mapped image. Generator refuses overwrites. Project owner controls distribution; no teammate authorship or additional license grant asserted. No new CC0 asset was needed/downloaded, and no third-party license file is required for the self-written PFM reader.

## v0.5 original scene and quieter timber

`tests/scenes/forest_v05.cpp`: scene-only adaptation of original forest_v03 authored by Codex under user direction; no teammate contribution inferred. Uses asymmetric branch/foliage fans, clustered trees and shore boulders, patch-selected grass, per-board gable mapping. No external assets or licenses introduced.

`assets/wood-soft-v05.ppm`: 256×256 ASCII P3 sRGB, derived from retained original wood.ppm by tools/generate_wood_v05.py. Channel contrast compressed to 25% around weathered cedar base (108,83,59); original file unchanged. UV repeats increase on scene geometry and gable is mapped per narrow board, reducing oversized grain. No photo source or URL; owner controls redistribution, no new license grant asserted. Existing studio-hdr.pfm reused unchanged.

## B6 coated-material comparison

`scene/coated_material.json`: comparison layout and material parameters authored by Codex at the user's request. Reuses the existing project-authored `studio-sphere.obj`, `studio-floor.obj` and `studio-hdr.pfm`; no new external mesh, texture or reference image. Left to right: bare Principled, clear coat, absorbing coat. Layered scattering background: https://pbr-book.org/4ed/Light_Transport_II_Volume_Rendering/Scattering_from_Layered_Materials ; the implemented model is an explicit single-scattering approximation, not copied PBRT source or its full layered solver.

