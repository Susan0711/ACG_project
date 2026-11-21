# ACG Track 1 · CPU 路径追踪器

这是一个用 C++17 编写的离线渲染器。给定相机、三角网格、材质和光源，程序从相机穿过每个像素发出射线，追踪光在场景中的反射与折射，估计到达相机的光，再写成 PPM 图像。

**渲染流程**　读取 JSON、OBJ 和贴图 → 装配 `World` → 建立包围盒层次结构（BVH）→ 逐像素采样并追踪光路 → 对线性 RGB 取平均 → 写出 `path.ppm`。

## 构建与运行

从含有 `CMakeLists.txt`、`scene/` 和 `assets/` 的项目根目录开始。以下两套命令都构建 Release 版本，并用四个 CPU 线程运行同一份 `forest.json`。构建需要 CMake 3.16 或更高版本，以及支持 C++17 的编译器。JSON 解析库已随仓库提供。

首次从源码运行时，`cmake -S . -B build-local` 根据项目配置生成构建文件，`cmake --build` 编译并链接出 `path_tracer`。之后只改 JSON、OBJ 或贴图时，直接用已有的可执行文件重新渲染即可。改了 C++ 源码再执行构建命令。

### Windows · PowerShell

将 CMake 和 MinGW-w64 的 `g++`、`mingw32-make` 加入 PATH 后运行以下命令。

```powershell
cmake -S . -B build-local -G "MinGW Makefiles" -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Release
cmake --build build-local --target path_tracer --parallel
Push-Location build-local
.\path_tracer.exe --scene ..\scene\forest.json --threads 4
Pop-Location
```

### macOS · 终端

首次配置可用 `xcode-select --install` 安装 Apple 命令行工具，安装完成后继续。已使用 Homebrew 的机器可用 `brew install cmake` 安装 CMake。[Apple 命令行工具](https://developer.apple.com/documentation/xcode/installing-the-command-line-tools/) · [Homebrew 的 CMake 配方](https://formulae.brew.sh/formula/cmake)

```sh
cmake -S . -B build-local -G "Unix Makefiles" -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release
cmake --build build-local --target path_tracer --parallel
(cd build-local && ./path_tracer --scene ../scene/forest.json --threads 4)
```

程序把 `path.ppm` 写入**运行时的工作目录**，所以上述命令的输出位置都是 `build-local/path.ppm`。再次运行前，先给该文件改名或换一个空目录。写入器遇到同名文件会报错并保留原文件。

| 命令行参数 | 用法 |
| --- | --- |
| `--scene <文件>` | 必填。从项目根目录运行时，`--scene forest.json` 会查找 `scene/forest.json`。带目录的路径按原样打开，JSON 内的网格和贴图路径始终相对于该 JSON 所在目录。 |
| `--width <像素>`、`--height <像素>` | 可选，每项为 1–16384 的整数。按轴覆盖场景的 `film.width`、`film.height`。若场景和命令行都省略某一轴，该轴取 256。 |
| `--spp <整数>` | 可选，每像素路径样本数，正整数，默认 64。相同尺寸下，提高 spp 会增加计算量，通常能降低采样噪声。 |
| `--threads <整数>` | 可选，范围 1–256，默认 1。线程分担图块，每个像素仍取相同数量的样本。 |

以潮汐观测站为例，在上述运行命令中将文件名换成 `tidewatch.json`。以下两档均使用同一机位、16:9 画幅和默认随机种子。分辨率会参与相机射线计算，固定垂直视场角时，加宽画幅会扩大水平视野。

| 用途 | JSON 中的 film | 运行参数 | 实际采样 |
| --- | --- | --- | --- |
| 预览 | `{"width": 640, "height": 360}` | `--spp 16 --threads 12` | 640 × 360，每像素 16 次 |
| 1080p 成图 | 同上 | `--width 1920 --height 1080 --spp 128 --threads 12` | 1920 × 1080，每像素 128 次 |

日志中的 `render_seconds` 是 `render()` 的实际耗时（秒），从开始分配图块到所有工作线程结束。场景及贴图加载、BVH 构建、画布分配和 PPM 写盘都在这个计时区间之外。

## 场景设置

[forest.json](scene/forest.json) 是最小示例。它在原点放置一块石头，给它指定一份带颜色贴图的漫反射材质，再放置一盏点光源。下面是完整输入。

```json
{
  "camera": { "position": [0, 1, 4], "target": [0, 0.3, 0], "fov_degrees": 40 },
  "materials": { "stone": { "type": "diffuse", "texture": "../assets/rock.ppm" } },
  "objects": [
    { "mesh": "../assets/rock.obj", "position": [0, 0, 0], "material": "stone" }
  ],
  "lights": [
    { "type": "point", "position": [2, 3, 4], "intensity": [40, 40, 40] }
  ]
}
```

### JSON 字段层次

`camera`、`materials`、`objects` 和 `lights` 为必填字段，`film` 可选。`materials` 按名称定义材质，`objects` 和 `lights` 是数组，每项分别放置一个模型或一盏灯。树中按 `type` 展开可选类型。写 JSON 时，所选类型的参数和 `type` 写在同一个对象里。网格和贴图的相对路径都从场景 JSON 所在目录起算。

```text
场景
├── film                            可选，场景默认画幅
│   ├── width                       可选，像素宽度，整数 1–16384，默认 256
│   └── height                      可选，像素高度，整数 1–16384，默认 256
├── camera                          相机，位置与注视点均为世界坐标
│   ├── position                    相机位置 [x, y, z]
│   ├── target                      注视点 [x, y, z]
│   ├── fov_degrees                 垂直视场角，单位为度，0 < 值 < 179
│   ├── lens_radius                 可选，光圈半径 ≥ 0，默认 0（针孔）
│   ├── focus_distance              可选，沿相机前方到焦平面的距离 > 0，默认 4
│   ├── shutter_open                可选，快门开启时刻（秒），与 shutter_close 成对填写
│   └── shutter_close               快门关闭时刻 ≥ 开启时刻，两者省略时固定在 0 秒
├── materials                       材质名称 → 材质定义
│   └── <材质名>                     自定，例如 stone，可被多个物体引用
│       ├── type                    四选一
│       │   ├── "diffuse"           color 或 texture 二选一
│       │   │   ├── color           常量反照率 [r, g, b]，各通道在 [0, 1] 内
│       │   │   └── texture         颜色贴图路径（P3 PPM），按 UV 读取反照率
│       │   ├── "mirror"            color 为镜面反射率 [r, g, b]，各通道在 [0, 1] 内
│       │   ├── "glass"             ior 为介质折射率，必须大于 0
│       │   └── "emissive"          color 为发射辐亮度 [r, g, b]，各通道 ≥ 0，可大于 1
│       ├── opacity                 前三类可选，表面覆盖率在 [0, 1] 内，默认 1
│       └── opacity_map             前三类可选，覆盖率贴图路径（P3 PPM）
├── objects[]                       每项放置一份 OBJ，同一网格可多次摆放
│   ├── mesh                        三角面 OBJ 路径
│   ├── position                    0 秒时相对模型原点施加的平移 [x, y, z]
│   ├── material                    引用 materials 中的名称，例如 "stone"
│   └── velocity                    可选，世界坐标中的平移速度，单位为场景长度/秒，默认 [0, 0, 0]
└── lights[]                        每项定义一盏灯
    └── type                        二选一
        ├── "point"                 点光源
        │   ├── position            光源位置 [x, y, z]
        │   └── intensity           RGB 辐射强度，各通道 ≥ 0
        └── "area"                  单面发光的平行四边形
            ├── origin              发光面的一个角点 [x, y, z]
            ├── u                   从 origin 出发的第一条边向量
            ├── v                   从 origin 出发的第二条边向量，u × v 非零并指向发光侧
            └── radiance            正面发出的 RGB 辐亮度，各通道 ≥ 0
```

### 材质类型与光学行为

1. **漫反射 `diffuse`**。`forest.json` 的石头使用 `{"type": "diffuse", "texture": "../assets/rock.ppm"}`。射线命中时，程序用该点的 UV 查出反照率，再按朗伯模型计算直接光，并在表面上方按余弦加权采样下一跳。纯色物体可以用 `color` 为整个表面给定同一个反照率。
2. **理想镜面 `mirror`**。潮汐观测站的海面使用 `{"type": "mirror", "color": [0.27, 0.61, 0.7]}`。命中点的法线决定唯一的反射方向，`color` 决定红、绿、蓝三个通道分别保留多少反射光。水面的起伏和法线来自 OBJ，镜面材质负责沿该方向继续追踪。
3. **理想玻璃 `glass`**。灯室玻璃使用 `{"type": "glass", "ior": 1.45}`。射线进入或离开玻璃时，程序根据入射角和界面两侧的折射率计算菲涅耳反射概率。每次命中按该概率选择反射或折射，折射方向遵循斯涅尔定律，发生全反射时只走反射方向。
4. **自发光 `emissive`**。灯芯使用 `{"type": "emissive", "color": [9, 3.1, 0.65]}`。这里的 `color` 给出表面自身发出的光。路径击中正面就累加发光贡献，击中背面则贡献为零，两种情况都在这里终止。若希望积分器从其他表面直接向一块发光面采样，可在 `lights[]` 中定义 `area` 灯。

材质名负责把配置和网格连起来。例如，`forest.json` 中的 `objects[0].material` 引用 `"stone"`，加载器便将这份漫反射材质绑定到 `rock.obj` 的每个三角形上。[石头场景](scene/forest.json) · [观测站材质配置](scene/tidewatch.json) · [材质参数解析](src/materials/material_reader.cpp) · [散射实现](src/materials/material.cpp) · [发光命中处理](src/render/path_integrator.cpp)

### 网格和贴图格式

OBJ 使用三角面。`v` 存顶点位置，`vt` 存二维纹理坐标，`vn` 存法线。`f` 的三个面角可用 `v`、`v/vt`、`v//vn` 或 `v/vt/vn` 分别引用这些数据。三个角都有法线时，程序在命中点插值着色法线，否则使用面法线。颜色贴图使用 P3 PPM，加载时从 sRGB 转为线性 RGB。覆盖率贴图同样使用 P3，红通道直接作为线性覆盖率。[岩石网格](assets/rock.obj) · [OBJ 读取](src/io/assets.cpp) · [贴图读取](src/textures/texture.cpp)

## `main()` 的调用流程

执行 `path_tracer` 后，程序从 `src/main.cpp` 的 `main()` 开始。下面按执行顺序列出它直接调用的函数，以及每一步交给下一步的数据。[主入口](src/main.cpp)

```text
main(argc, argv)
├── parse_arguments(...)           → 场景路径和本次运行的覆盖参数
├── load_scene_file(path)          → World，包含相机、图元、材质和光源
├── RenderScene(std::move(world))  → 可供所有线程查询的场景，含 BVH
├── Film(width, height)            → 以最终宽高分配线性 RGB 像素数组
├── render(scene, film, settings)  → 填满 film.pixels，等待所有工作线程结束
└── film.write("path.ppm")         → 在当前工作目录写出 P3 PPM
```

### 1. 命令行参数与场景路径解析

`parse_arguments()` 逐对读取命令行选项。`main()` 将指定的 `spp` 和线程数应用到 `RenderSettings`，给单独的场景文件名补上 `scene/` 前缀。加载 JSON 后再逐轴确定画布尺寸，优先级为命令行、场景 `film`、默认尺寸。最大反弹深度 16、随机种子 1 和默认的直接光采样模式 NEE 均来自 `RenderSettings`。[参数解析](src/main.cpp) · [渲染设置](src/render_scene.h)

### 2. `World` 对象的构建

```text
load_scene_file(path)
├── Json::parse() → json_fields::expect_fields()   解析 JSON，检查顶层字段
├── film_dimension()                               若存在 film，校验并读取场景宽高
├── read_camera() → Camera::validate()             读取并校验视点、朝向、视场角
├── read_material()                                构造 stone 漫反射材质
│   └── read_texture() → Texture::load()           读取 rock.ppm，将颜色解码为线性 RGB
├── load_obj()                                     读取 rock.obj，绑定 stone 的材质编号
│   └── Transform::point()                         将模型中的顶点变换到世界坐标
└── add_light()                                    将这盏点光源登记到 world.lights.points
```

加载器先为 `stone` 分配编号 0，把材质存入 `world.materials[0]`。随后处理 JSON 的 `objects[0]`，按名字查到编号 0，把它连同 OBJ 路径和位置交给 `load_obj()`。所以一条 JSON 摆放记录导入后，对应 `world.scene.objects` 中的八个三角形，每个三角形都带有材质编号和各面角的 UV。点光源只登记位置和强度。如果场景换用面光源，`add_light()` 会调用 `World::add_area()`，再登记发光材质和两个可以被射线击中的灯面三角形。[场景装配](src/scene_init.cpp) · [材质读取](src/materials/material_reader.cpp) · [OBJ 导入](src/io/assets.cpp) · [面光源登记](src/world.cpp)

### 3. BVH 构建与 `Film` 初始化

`RenderScene` 的构造函数接管 `World`，按相机快门区间调用一次 `BVH::build()`。`bounds()` 先计算每个三角形在该区间内的包围盒，局部函数 `build_range` 合并当前组的包围盒，再由 `split_median()` 沿最长轴按盒中心排序、从中间分组，递归到每个叶节点只存一个图元索引。稍后每条射线查询的都是这棵树。[RenderScene 构造](src/render/render.cpp) · [BVH 建树与遍历](src/acceleration/bvh.cpp)

接下来 `main()` 按已解析的宽高创建 `Film`。例如 `forest.json` 省略了 `film`，按默认参数运行时画布为 256 × 256，共 65,536 个像素。工作线程共享只读的 `RenderScene`，将各自负责的像素写入同一个 `Film`。[场景查询接口](src/render/render.h) · [Film 定义](src/render/film.h)

### 4. 工作线程的图块分配与像素采样

顶层的 `render_scene.cpp` 实现整帧入口 `render()`。它分配图块、发起相机采样，并对每条射线调用 `integrate()`。积分器通过 `RenderScene::intersect()` 查找最近命中，直接光计算通过 `RenderScene::visibility()` 查询到光源的透过率。这两个查询都实现在 `render/render.cpp`。[整帧调度](src/render_scene.cpp) · [路径积分](src/render/path_integrator.cpp) · [场景查询](src/render/render.cpp)

```text
render(scene, film, settings)
├── 启动 settings.threads 个线程，执行局部函数 worker
│   ├── next.fetch_add(1) 领取一个 16 × 16 图块，逐行遍历其中的像素
│   └── 对每个像素 (x, y)
│       ├── 循环 settings.spp 次，每次都依次完成下面三步
│       │   ├── Sampler::next() 两次，在像素内随机取样得到 (u, v)
│       │   ├── Camera::sample(u, v, ...) → Camera::generate() → Ray
│       │   └── integrate(scene, ray, ...) → 将这一条路径的线性 RGB 加进 sum
│       └── film.pixels[y * width + x] = sum / settings.spp
└── join() 等待所有工作线程结束，返回 main()
```

`Camera::generate()` 用相机朝向、垂直视场角和画幅宽高比，把画面坐标 `(u, v)` 换成世界空间中的射线方向。`forest.json` 使用针孔和固定快门，每条相机射线从 `[0, 1, 4]` 出发，时刻为 0。其他场景如果设置了光圈或快门区间，`Camera::sample()` 还会采样镜头位置或射线时刻。每份样本的随机序列由种子、像素编号和样本编号决定，因此改变线程数只会改变由哪个线程领取图块。[线程与像素循环](src/render_scene.cpp) · [相机射线](src/render/camera.cpp)

### 5. 单条光路的积分

以下展开一次命中岩石、点光源位于受光侧的反弹。`integrate()` 用循环追踪同一条路径，维护两个 RGB 值。`result` 累加已经收到的光，`throughput` 记录沿途各次散射相乘的权重。[路径积分循环](src/render/path_integrator.cpp)

```text
integrate(scene, ray, ...)
├── scene.intersect() → BVH::intersect()
│   ├── AABB::intersect()                  淘汰射线碰不到的包围盒
│   └── Triangle::intersect()              在候选叶节点求交，最近命中写入 HitRecord
├── scene.material(hit) → material.alpha() 用材质编号找到 stone，检查表面覆盖率
├── direct_lighting()                      估计石头上这个点收到的直接光
│   └── Lighting::sample_each()
│       ├── Lighting::sample(point, ...)    算出朝向灯的方向、距离和按距离平方衰减的光照
│       └── 对这份光源样本执行回调
│           ├── evaluate_bsdf()            计算石头沿这个方向的漫反射
│           │   └── albedo() → Texture::value(hit.uv)
│           └── scene.visibility()         用阴影射线检查命中点到灯之间的遮挡
│               ├── spawn()                从表面稍微偏移的位置发射阴影射线
│               └── shadow_transmittance() → scene.intersect()，逐层累乘透过率
├── sample_bsdf()                          按材质抽取下一跳的方向和权重
└── spawn(hit, sampled.direction)          生成下一跳射线，回到循环顶部求交
```

#### 射线求交与命中记录

`Triangle::intersect()` 用命中点的重心坐标插值 UV。最终留下的 `HitRecord` 包含最近命中的位置、法线、UV 和材质编号。若整条射线都未命中，积分器改调 `Lighting::escape()` 查询背景，计入贡献后结束这条路径。命中自发光面时则检查正反面，计入正面的发光贡献后结束。[图元求交](src/core/geometry.cpp) · [命中记录](src/core/ray.h) · [背景查询](src/lighting/lights.cpp)

#### 直接光照与可见性查询

相机射线确定石头表面的可见点。计算该点接收的直接光时，还需检查它与光源之间的遮挡。`evaluate_bsdf()` 用命中点的贴图反照率求出漫反射系数，`visibility()` 则向点光源发出阴影射线，只检查命中点到灯之间的线段，遇到遮挡层就将透过率乘以 `1 − α`。`direct_lighting()` 根据材质、入射光、入射角和可见性返回直接光贡献，积分器以 `result += throughput × direct` 逐通道累加。[直接光计算](src/render/direct_lighting.cpp) · [逐灯采样](src/lighting/lights.h) · [阴影查询](src/render/render.cpp)

#### 散射采样与路径终止

对石头的漫反射材质，`sample_bsdf()` 按余弦加权分布抽取漫反射方向，返回的权重是该 UV 处的反照率。积分器将权重乘入 `throughput`，用 `spawn()` 生成保持原采样时刻的新射线。从第四次散射起，俄罗斯轮盘赌按路径权重决定是否继续，存活路径除以存活概率作补偿。路径结束时，`integrate()` 把 `result` 返回给第 4 步的样本循环。[材质散射](src/materials/material.cpp) · [路径积分](src/render/path_integrator.cpp)

### 6. 颜色编码与 PPM 输出

所有像素算完后，控制回到 `main()`，它调用 `film.write("path.ppm")`。对每个颜色通道，`Film::write()` 先应用曝光，将非负值 `x` 按 `x / (1 + x)` 压缩到显示范围，再作 sRGB 编码、量化成 0–255 的整数，依次写入 P3 文件。显示变换作用于已经平均好的像素。[写出实现](src/render/film.cpp)

渲染库还提供 HDR 环境光和多重重要性采样（MIS），可从 C++ 测试查看用法。当前 JSON 可配置点光和面光，命令行沿用默认的 NEE 模式。[HDR 与 MIS 测试](tests/environment_tests.cpp)

## 场景示例

沿用上面的命令，将 `forest.json` 换成同目录的文件即可。也可以复制一个示例，修改相机、材质或物体摆放，再交给同一程序读取。

| 场景 | 配置要点 |
| --- | --- |
| [depth_of_field.json](scene/depth_of_field.json) | 同一个岩石网格分别摆在近处和远处。用 `focus_distance` 指定焦平面，用 `lens_radius` 调整光圈半径，设为 0 即恢复针孔相机。 |
| [alpha_shadow.json](scene/alpha_shadow.json) | 一张薄片通过 `opacity_map` 定义叶片覆盖率。阴影射线每穿过一层，就将透过的直接光乘以该处的 `1 − α`。 |
| [motion_blur.json](scene/motion_blur.json) | 相机固定，快门从 0 秒开到 1 秒。石头的 `velocity` 是 `[1.6, 0, 0]`，在射线时刻 `t` 按 `position + velocity × t` 定位。 |
| [tidewatch.json](scene/tidewatch.json) | 潮汐观测站将塔楼、码头、海面和帆船组合在一份 JSON 中。海面用带法线的波浪网格配合镜面材质，船的多个部件共用平移速度。 |

## 项目结构

```text
scene/                   可直接交给 --scene 的 JSON 场景
assets/                  OBJ 网格、颜色与覆盖率贴图、环境贴图
src/
├── main.cpp             path_tracer 命令入口
├── scene_init.cpp       从 JSON 装配 World
├── world.*              相机、几何、材质、光源，以及面光源登记
├── render_scene.*       整帧入口 render() 和 RenderSettings，调度工作线程并写入 Film 像素
├── core/                向量、随机采样、射线、命中记录与图元求交
├── acceleration/        BVH 构建和遍历
├── io/                  OBJ 导入与 JSON 字段校验
├── materials/           材质参数解析、散射求值与方向采样
├── textures/            P3 贴图读取与 UV 查询
├── lighting/            点光源、面光源与环境光采样
├── render/
│   ├── render.*         RenderScene 持有 World 和 BVH，提供求交与阴影透过率查询
│   ├── camera.*         相机射线、镜头与快门采样
│   ├── path_integrator.* 逐次追踪路径并累加辐射亮度
│   ├── direct_lighting.* 光源采样与直接光估计
│   └── film.*           保存线性 RGB 像素并输出图像
└── test/                独立的 normal_preview 法线检查程序
tests/                   单元测试、命令行测试及 C++ 内置回归场景
tools/                   资产生成、对照与历史验证脚本
third_party/nlohmann/    随项目分发的 JSON 解析库
docs/                    设计、素材来源与阶段记录
```
