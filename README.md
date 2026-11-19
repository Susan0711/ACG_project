# Forest Creek Cabin — CPU Path Tracer

当前 v0.5 主视觉在 v0.4 公共核心上增加独立场景：C++17 CPU 路径追踪、BVH、Lambertian/镜面/玻璃、HDR PFM 环境与 MIS、纹理/OBJ、景深/运动/alpha、确定性多线程均已有实现和 Mac 验证。主图仍明显带有低多边形风格，尚非最终画质。Windows 公共基线已交队友验证；本仓库没有 Windows 实跑结果。

## 快速构建与运行

Mac（Apple Silicon 本次验证）：
```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-release
ctest --test-dir build-release --output-on-failure
./build-release/path_tracer --scene forest-v05 --resource-root assets --environment studio-hdr.pfm --env-rotation 115 --env-intensity 1.1 --mode mis --width 640 --height 360 --spp 64 --depth 12 --seed 19 --threads 4 --output build-release/forest-v05-new.ppm
```
Intel Mac 可省略 arm64 参数并使用独立构建目录。输出已存在会报错，请选新文件名。资源根目录指向 assets；从 build 目录运行时传 `--resource-root ../assets`。

Windows / MSVC（待实际验证）：
```powershell
cmake -S . -B build-msvc -G "Visual Studio 17 2022" -A x64
cmake --build build-msvc --config Release
ctest --test-dir build-msvc -C Release --output-on-failure
.\build-msvc\Release\path_tracer.exe --scene forest-v05 --resource-root assets --environment studio-hdr.pfm --env-rotation 115 --env-intensity 1.1 --mode mis --width 160 --height 90 --spp 8 --depth 12 --seed 19 --threads 4 --output build-msvc/forest-v05-new.ppm
```

`forest-v05-gray` 使用主镜头灰模，`forest-v05-detail` 检查小屋/水岸；原 forest、forest-v03 及 normal_preview 保留。v0.5 仅改场景与注册，核心接口不变。无新第三方库/资产。

## 验证与交接入口

- [当前实际架构与添加场景步骤](docs/ARCHITECTURE.md)、[本轮重构验证](docs/SCENE_DECOUPLING_VALIDATION.md)

- [基础功能展示包 quick/full 与现场顺序](docs/DEMO_GUIDE.md)：`tools/demo_pack.py`，标准库、独立输出目录及 manifest
- [Midterm 素材提纲](docs/MIDTERM_OUTLINE.md)：两页 SIGGRAPH 写作准备，尚非正式报告

- [本轮视觉、命令与耗时](docs/V05_VALIDATION.md)
- [HDR/MIS 数值验证](docs/V04_VALIDATION.md)
- [基础项核对表](docs/REQUIREMENTS_STATUS.md)、[规划](docs/PROJECT_PLAN.md)、[素材来源](docs/ASSET_SOURCES.md)
- [Windows 验证步骤](docs/WINDOWS_VALIDATION.md)、`tools/validate_windows.ps1`
- 历次证据：[v0](docs/V0_VALIDATION.md)、[v0.1](docs/V01_VALIDATION.md)、[v0.2](docs/V02_VALIDATION.md)、[v0.3](docs/V03_VALIDATION.md)

## 历史版本说明与接口参考

以下按实现时间保留；“未实现”等描述仅反映当时阶段，不代表当前能力。当前状态以上文和最新验证记录为准。

### 最初的 normal_preview 阶段

最小 C++17/CMake 命令行示例，无第三方依赖。生成针孔相机射线，与球体及双面三角形求交，将世界空间几何外法线按 `(n + 1) / 2` 映射为 RGB，输出 ASCII PPM（P3）。这不是路径追踪器；未实现材质、BVH、外部模型或正式场景。

`--scene single`（默认）保留原单球；`--scene multi` 包含前后遮挡的两个球和两个三角形，其中前方三角形背面朝向相机，显示黄色。

```sh
ctest --test-dir build --output-on-failure
./build/normal_preview --scene multi --width 320 --height 180 --output build/multi.ppm
```

Windows 测试命令：`ctest --test-dir build-msvc -C Release --output-on-failure`。

共享接口在 `src/geometry.h`，实现位于 `src/geometry.cpp`：`Sphere`、`Triangle`、`Scene` 均提供 `intersect(ray, t_min, t_max, hit)`，使用闭区间，失败不改变 hit。Scene 线性返回最近命中，等距时保留首个物体。Ray 为 `origin + t * direction`，方向无需单位化，只有单位方向时 t 才是距离；输入应为有限数值，零方向不命中。球半径须为正。三角形外法线由顶点绕序 `(b-a) × (c-a)` 决定；退化及近平行射线不命中。零向量归一化返回零。

HitRecord 保存 `t`、`position`、`outward_normal`（单位几何外法线）、`normal`（朝入射侧）、`front_face`（方向与外法线点积小于零）及仅作标识的 `material_id`。预览只使用 outward_normal，不隐藏正反面。相机和图像缓冲暂留 main.cpp。

## macOS

在仓库根目录运行（需要 CMake 和 Apple Clang）：

```sh
cmake -S . -B build
cmake --build build
./build/normal_preview --width 320 --height 180 --output build/normal.ppm
```

## Windows / MSVC

在安装了 CMake、Visual Studio 2022 C++ 工具的终端中，从仓库根目录运行：

```powershell
cmake -S . -B build-msvc -G "Visual Studio 17 2022" -A x64
cmake --build build-msvc --config Release
.\build-msvc\Release\normal_preview.exe --width 320 --height 180 --output build-msvc/normal.ppm
```

Windows 命令尚未实际验证。

参数默认值：`--width 640 --height 360 --output normal.ppm`；支持 `--help`。宽高为 1–8192 的整数。输出目录必须存在；为保护已有文件，输出文件存在时会报错，请选择新文件名。相对路径以运行时工作目录为基准。

`Ray` 保存原点和方向；`Camera` 按像素中心生成射线（垂直视场角 90°）；`Image` 保存 RGB 缓冲并写文件。右手坐标系，Y 向上，相机朝 -Z，图像从上到下排列。球心 `(0,0,-2)`，半径 `0.75`；背景为深灰。法线颜色直接量化，不作 gamma 变换。每像素一条射线，尚无抗锯齿。


## CPU Path Tracer v0

单线程 C++17，无运行时第三方依赖。原 `normal_preview` 命令及颜色规则保留；新增 `path_tracer`：

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
ctest --test-dir build-release --output-on-failure
./build-release/path_tracer --scene cornell --width 256 --height 256 --spp 128 --seed 1 --depth 16 --accel bvh --direct on --output build-release/cornell-new.ppm
./build-release/path_tracer --scene point --width 256 --height 256 --spp 64 --output build-release/point-new.ppm
```

Windows（未实际验证）：
```powershell
cmake -S . -B build-msvc -G "Visual Studio 17 2022" -A x64
cmake --build build-msvc --config Release
ctest --test-dir build-msvc -C Release --output-on-failure
.\build-msvc\Release\path_tracer.exe --scene cornell --width 256 --height 256 --spp 128 --seed 1 --depth 16 --accel bvh --direct on --output build-msvc/cornell-new.ppm
```

默认参数为 cornell、256×256、64 spp、seed 1、depth 16、bvh、direct on、path.ppm。支持 `--accel linear` 和 `--direct off`；点光是 delta 光源，纯 BSDF 模式无法击中它，因此点光场景关闭直接采样会变黑。尺寸限制 1–8192，spp 1–10000000，depth 1–1024，seed 为 uint64（可为零）。现有输出文件拒绝覆盖。程序输出实际耗时与显示转换前全图 RGB 均值。

核心实现：`geometry.*` 保存几何接口；`tracer.h` 声明 Camera/Film/Sampler、Material/BSDFSample、Lights、BVH/World 和积分入口；`tracer.cpp` 实现采样与积分；`bvh.cpp` 实现最长轴质心中分二叉 BVH；`scenes.cpp` 创建内置场景；`path_main.cpp` 负责 CLI。修改 World.scene 后须重新 build BVH。

- BSDFSample.direction 为单位出射方向；weight 为 `f * abs(cos) / pdf`；漫反射 PDF 为立体角密度 `cos/pi`，weight=albedo。delta 标记镜面/玻璃；其 pdf 是离散分支概率，不能当作普通立体角密度。理想玻璃使用精确非偏振 Fresnel，空气/玻璃界面，透射 radiance 权重含 `(eta_i/eta_t)^2`，全反射必选反射。不支持嵌套介质栈。
- 反射/透射按出射方向与几何外法线点积选择偏移侧，固定偏移 1e-5 适用于当前单位尺度。相机支持 origin/target/fov，Y 向上；当前内置配置避免视线与 up 平行。
- 点光 intensity 按距离平方衰减；矩形均匀面积采样的立体角 PDF 为 `distance²/(area*cos_light)`，单面发光。每个灯各采一次，不需要光源选择概率。阴影射线把包括玻璃在内的几何视为遮挡，折射连接由路径采样完成，不做透明直线阴影近似。
- 直接采样开启时，只有相机首段和上一事件为 delta 时累计击中灯面的发光；漫反射后的直接发光由灯光采样估计。所有内置发光面均登记为 AreaLight；扩展场景时必须保持此约定。关闭时累计全部灯面命中。没有 MIS。
- 像素随机抖动、余弦半球采样、SplitMix64 固定种子；第 4 次散射起 Russian Roulette，存活概率 clamp(max throughput, .05, .95)，存活后除以该概率。最大深度会截断路径，仍有有限深度偏差。
- Film 保存线性平均辐亮度，不逐样本裁剪；最终 exposure（接口默认 1）→Reinhard→sRGB→8 位 PPM。normal_preview 继续直接映射法线颜色。相同构建/参数可复现，不承诺跨编译器逐字节相同。
- AABB slab 对零方向单独处理，闭区间允许接触；三角形包围盒扩张 1e-9。BVH 与线性等距命中都优先原始物体顺序。不做 SAH。

测试涵盖原几何用例、10000 条 BVH/线性射线、零厚度/平行 slab、NaN 输入、余弦采样统计及 PDF、Fresnel、Snell、全反射、偏移和有限路径。v0 不含纹理、模型导入、森林场景、景深、运动模糊、alpha shadow、GUI；不能视为全部课程核心项完成。

## v0.1 texture / OBJ / forest

Run from the repository root so `assets/` resolves relative to the working directory. Added `--scene forest`, `forest-gray`, and `texture`. Existing commands still work. Example:

```sh
./build-release/path_tracer --scene forest --width 480 --height 270 --spp 64 --depth 10 --seed 19 --output build-release/forest-new.ppm
```

`assets.h/.cpp` provides Texture and a transactional triangular OBJ loader. HitRecord now includes UV and light_id; triangle UVs interpolate barycentrically, sphere u uses atan2(z,x), v uses asin(y), +Y at v=1. Image textures load once: ASCII P3 only, max value 1–65535, comments accepted, sRGB decoded to linear. UV origin is bottom-left; nearest-neighbor repeat addressing (including negative UVs), integer UVs wrap. Procedural textures output linear color. Both diffuse NEE evaluation and BSDF sampling call Material::albedo with the same hit UV. `texture` renders the original wood on a UV-mapped square.

OBJ accepts v, two-component vt, vn, triangular f in v, v/vt, v//vn, v/vt/vn forms, positive and relative negative indices; o/g/s metadata are ignored. Other directives (including MTL/usemtl) and polygons error with file/line. Caller chooses material. Transform supports nonzero nonuniform scale, Y rotation in radians and translation. Normals use inverse transpose; v0.1 deliberately uses flat geometric shading, validating vn without interpolation. Geometry controls front_face and ray offsets. Imports append only after complete validation.

World::add_area is the creation API for linked rectangular light geometry/material/sampling data: do not mutate its generated geometry or light independently. Hit light_id=-1 denotes emission not covered by NEE; such emission remains visible after diffuse events. This supersedes v0's rule requiring every emitter to be registered. Environment contributes only on escape multiplied by throughput. Forest uses constant cool sky plus a finite outdoor point light, two warm window area lights and a mirror creek. No HDRI, transparency foliage or fluid effects.

Assets are original assistant-generated examples, documented in ASSET_SOURCES. Forest is a first composition preview, not the final polished course scene. Windows remains unverified. See V01_VALIDATION for actual commands and limitations.

## v0.2: thin lens, shutter time, thin-surface coverage

New validation scenes: `dof`, `motion`, `alpha-zero`, `alpha-half`, `alpha-one`, `alpha-double`, `alpha-pattern`. Existing scenes default to zero lens radius and instantaneous shutter at time 0. Example (run from repository root):

```sh
./build-release/path_tracer --scene dof --width 320 --height 200 --spp 128 --depth 8 --seed 27 --lens-radius .22 --focus-distance 6 --output build-release/dof-new.ppm
./build-release/path_tracer --scene motion --width 320 --height 200 --spp 128 --depth 8 --seed 27 --shutter-open 0 --shutter-close 1 --output build-release/motion-new.ppm
./build-release/path_tracer --scene alpha-pattern --width 320 --height 200 --spp 128 --depth 8 --seed 27 --output build-release/alpha-new.ppm
```

- `Camera::sample(u,v,aspect,rng)` adds effects to the retained pinhole `generate` interface. `--lens-radius` is aperture **radius**, in world units, not diameter/f-number. `--focus-distance` is positive axial distance to the plane perpendicular to camera forward, in world units (not optical focal length). Lens samples use `r=R*sqrt(U), theta=2*pi*V`, uniform in disk area. Zero radius uses the original pinhole ray exactly and consumes no lens random numbers. FOV (degrees), aspect, focus, finite shutter bounds, zero/parallel-up direction are validated. Camera up remains +Y.
- `Ray::time` and `HitRecord::time` carry scene time. `--shutter-open/--shutter-close` are seconds in these examples; equal bounds give an instantaneous ray without random sampling. Camera samples time once; all subsequent spawn calls, straight transparent crossings and shadow queries retain it. Sphere center is `center + velocity*time`, with velocity in world units per second and center specified at time 0. BVH::build accepts shutter endpoints and unions both endpoint boxes; rebuild after changing shutter or motion. Query times must lie inside the build interval. Only translating spheres move; lights/camera/triangles are static.
- Material `opacity` defaults to 1; optional `opacity_map` is multiplied with it. Load opacity P3 via `Texture::load(path, false)` to preserve **linear data**; the red channel is coverage. Color textures still decode sRGB by default. Original leaf silhouette generation is in tools/generate_opacity.py. Repeat/nearest UV addressing is the same as image color textures.
- This is thin-surface coverage, not glass or participating media. Camera rays stochastically accept each surface with probability alpha, otherwise continue in the same direction/time without throughput scaling, bounce increment, RR, or changing previous_delta. Alpha 0/1 consumes no acceptance RNG. Shadow transmission deterministically multiplies `(1-alpha)` over intersections until escape or an opaque blocker. Two half-covered layers give 0.25. Each geometric crossing counts as a layer (closed meshes have entry and exit). Distinct layers should not be exactly coplanar.
- Independent 4096-crossing limits throw diagnostic errors instead of silently biasing pathological transparent stacks. Camera crossings step forward 1e-7 world units (unit path directions); shadow queries advance their t lower bound with nextafter. Extremely thin/coincident geometry below numerical offset scales remains a limitation. Emitters must be fully opaque: emissive alpha throws an error. Glass still refracts and blocks straight NEE shadow connections, unchanged.

Implementation and test scenes live in `src/effects.cpp`, tested by `tests/effects_tests.cpp`. `tools/render_v02.py` runs the recorded comparison suite and refuses existing output paths through the CLI. Windows commands use the existing Release executable layout; Windows remains unverified. See docs/V02_VALIDATION.md for actual results and all commands.

## v0.3 scene and deterministic threads

Original `forest`/`forest-gray` are retained. Added `forest-v03`, `forest-v03-gray` (same main camera), `forest-v03-detail` (closer inspection camera). Default effects remain off. Example:

```sh
./build-release/path_tracer --scene forest-v03 --resource-root assets --width 640 --height 360 --spp 64 --depth 10 --seed 19 --threads 4 --output build-release/forest-v03-new.ppm
```

`--resource-root DIR` is the **assets directory**, relative to the process working directory or an explicit user-supplied path; default `assets` preserves old commands. No machine-specific path is compiled in. A missing resource reports the exact attempted path and suggests the flag. From the build directory use `--resource-root ../assets`. Output paths remain relative to the working directory. Windows handoff: [docs/WINDOWS_VALIDATION.md](docs/WINDOWS_VALIDATION.md), or tools/validate_windows.ps1 (pending Windows execution).

`render.h` exposes `render(const World&, Film&, const RenderSettings&)`. `--threads` accepts 1–256, defaults to 1. Standard C++17 threads claim 16×16 tiles atomically; each pixel has exactly one writer and accumulates samples in increasing sample index. Independent SplitMix-style integer hashes derive the RNG state from seed, linear pixel index and sample index. Thread number/order is never part of the seed. World, BVH and loaded textures are read-only throughout rendering. Exceptions cancel future tile work, all launched threads join, and the main thread rethrows; no partial image is written. Thread-launch failures also join existing workers.

This replaces the v0–v0.2 single global RNG stream, so old stochastic images are no longer byte-identical even with one thread. Their assets/scenes/outputs and algorithm tests remain intact; normal_preview is unchanged. Within one build/machine the new scheme is exactly invariant to thread count. No cross-platform bit-identity claim. Reporting separates scene loading, BVH build, render and write time; statistics are reduced in pixel order after workers finish. Scene factories support optional deferred BVH construction for accurate CLI timing.

The new scene uses continuous curved channel cross sections, sloping banks, irregular ring-mesh boulders, geometric window openings with inset lights, frames/door/foundation/eaves/plank joints, varied tree groups, branches, a fallen log and sparse opaque ground plants. Ground color varies slowly per patch rather than repeating UV stripes; wood UV scale follows board geometry. It remains a stylized first readable natural scene, not final quality. No MIS, SAH, HDRI, new BSDF, volumes or GUI.

Local Mac validation encountered a Rosetta/native mixed-object rebuild when CMake was launched from a differently architected Python. Recovery on the Apple M1 used `cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64` followed by build/CTest. This flag belongs to that Mac invocation only; it is not in project CMake and must not be added to the Windows commands. Prefer one architecture per build directory.

## v0.4 floating HDR and MIS

```sh
./build-release/path_tracer --scene sampling --environment studio-hdr.pfm --mode mis --width 320 --height 200 --spp 128 --depth 12 --seed 19 --threads 4 --output build-release/hdr-new.ppm --linear-output build-release/hdr-new.pfm
./build-release/path_tracer --scene forest-v03 --environment studio-hdr.pfm --env-rotation 30 --env-intensity .7 --mode mis --width 640 --height 360 --spp 64 --depth 12 --seed 19 --threads 4 --output build-release/forest-hdr-new.ppm
```

`--environment` resolves through `--resource-root` (default assets). It replaces the scene's constant escape environment but retains its existing geometry and point/window lights. `--env-rotation` is azimuth degrees, positive moving +X toward +Z; `--env-intensity` is a nonnegative linear multiplier. `--exposure` is a positive display multiplier, applied only on PPM writing. `--linear-output` optionally writes untonemapped RGB float PFM for numerical comparisons; existing files are rejected.

HDR input is true linear **RGB PFM**, not JPEG/PNG, Radiance RGBE or EXR. Reader supports PF, width/height, signed scale (endianness and magnitude), bottom-up float32 RGB; malformed/truncated/nonfinite/negative radiance is rejected. No image library dependency was added. In-memory rows run north (+Y) to south; azimuth 0 is +X and pi/2 is +Z. Nearest texel evaluation intentionally matches the piecewise-constant importance distribution. No bilinear filtering or HDR photographic asset is claimed.

Each cell receives probability proportional to luminance `(0.2126 R + 0.7152 G + 0.0722 B)` times its exact solid angle `Δphi*(cos(theta0)-cos(theta1))`. The latter integrates sin(theta), instead of approximating at the pixel center. A selected cell samples uniform phi and uniform cos(theta), giving solid-angle PDF `cell_probability/cell_solid_angle`; this avoids division by sin(theta) near poles. Azimuth wraps at the seam; polar rows have finite PDFs; all-black maps fall back to uniform solid-angle sampling with zero radiance. CDF and pixels load once and remain read-only across threads. Endpoint lens-independent environment samples avoid exact poles by an insignificant open-interval clamp.

Modes: `--mode bsdf|nee|mis` (default nee preserves the old interface). `--direct off` forces bsdf regardless of mode; `--direct on` permits the selected mode. `bsdf` hits emissive surfaces/escapes naturally; delta point lights cannot be randomly hit, so scenes containing point lights are not equivalent BSDF comparisons. `nee` samples lights and suppresses only covered diffuse-event light hits. `mis` uses power heuristic β=2 instead of suppression.

The implementation enumerates **every point light, every registered area light, and the HDR environment once** per diffuse vertex. Thus selection probability is 1 per technique; there is no hidden 1/N light lottery. Area PDF is distance²/(area*cos_light), environment PDF is per steradian, and Lambertian BSDF PDF is cos/pi. Light-side MIS uses p_light²/(p_light²+p_bsdf²); BSDF-side covered-emission/escape uses its complement. Emitters not registered for NEE retain full hit contribution. Main rays and previous mirror/glass delta events get weight 1; point lights have no competing continuous BSDF technique and weight 1. Depth now counts scattering events: after the last permitted scatter the next emission/escape endpoint is still evaluated, without allowing another scatter. This gives all modes the same truncated path family (including depth=1), and retains complementary MIS weights at that final scatter. The finite depth cap still biases transport relative to infinite paths.

Previous BSDF position/PDF/delta state survives alpha crossings; shadow visibility still multiplies thin coverage, and time is propagated unchanged. RR compensation remains unchanged; weights on both sides use the same directional PDFs (without folding RR/coverage into one side only). Constant legacy environments without an HDR map remain BSDF-only and retain their full escape contribution. `src/environment.*` handles HDR, `src/integrator.cpp` holds lighting/integration, and `tests/environment_tests.cpp` checks normalization, mapping, energy, delta and thread behavior.

Reproducible validation: `python3 tools/validate_v04.py` writes new files and records exact commands, float outputs, multi-seed means/errors/times. It does not overwrite previous results. See V04_VALIDATION for reference noise and the limited observed benefit of MIS over NEE. Windows remains pending; the existing Windows entry point is retained. No SAH, Principled, normal maps, volume or GUI added.
