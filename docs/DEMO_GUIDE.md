# 基础功能展示包

脚本仅使用 Python 3 标准库，调用已构建的可执行文件；不重新构建，不自动安装依赖。每次创建 UTC 时间戳加随机后缀的新目录，保留旧图。manifest.json 保存精确 argv 列表、可执行文件哈希、实际进程耗时、退出码、标准输出/错误和 PPM 完整性检查。Git HEAD 仅作历史标识；dirty/untracked 工作树不能声称完全对应该提交，也不能证明二进制来自该提交。

## 运行

Mac（已实际验证 quick；full 是参数入口，未在本轮完整执行）：
```sh
python3 tools/demo_pack.py --executable build-release/path_tracer --resources assets --output build-release/demo-packs --profile quick
python3 tools/demo_pack.py --executable build-release/path_tracer --resources assets --output build-release/demo-packs --profile full
```

Windows（待队友验证，Python 需另外可用）：
```powershell
py -3 tools/demo_pack.py --executable "build-msvc/Release/path_tracer.exe" --resources "assets" --output "build-msvc/demo packs" --profile quick
```

三个路径可含空格；脚本使用 pathlib.resolve 与 subprocess 参数列表，无 shell 拼接。`--threads 4` 为默认，可改 1；`--list` 只打印计划 argv JSON，不运行、不创建目录。例：
```sh
python3 tools/demo_pack.py --executable build-release/path_tracer --resources assets --output build-release/demo-packs --profile full --list
```

quick：一般 160×100、16 spp，forest 160×90、8 spp；full：一般 640×400、128 spp，forest 640×360、64 spp。均 seed=27、depth=12、MIS、默认零孔径/瞬时快门。MIS 高采样参考实验不包含在包中；现有统计直接见 [V04_VALIDATION](V04_VALIDATION.md)，原入口 tools/validate_v04.py、tools/compare_v04_time.py 保留。

## 展示索引

| 输出名 | 展示内容 | 对应与解释 |
|---|---|---|
| cornell | 漫反射、镜面、玻璃、面灯、间接光 | 基础路径追踪/材质/灯光；玻璃为理想介质 |
| point | 点光与阴影 | 基础点光源；与 Cornell 场景灯光不同，不作单变量亮度比较 |
| wood-uv | 原创木纹及 UV | 基础自制颜色纹理/映射；原始纹理，不是 v05 低对比派生图 |
| dof-off / dof-on | 前中后球体清晰度 | 仅 lens radius 从 0 改 .22，focus distance 始终 6 |
| motion-instant / motion-exposure | 球体瞬时与移动曝光 | 仅快门从 [.5,.5] 改 [0,1]，几何匀速平移 |
| alpha-pattern | 叶片孔洞与投影 | 基础 alpha shadow，coverage 不是玻璃 |
| alpha-half / alpha-double | 一层与两层 0.5 | 层数和第二层位置改变；重叠阴影透射理论 .25，不能直接用色调映射像素读数证明 |
| hdr-0 / hdr-90 | 环境方向与镜面高亮 | 仅环境旋转 0→90°；HDR 为选做功能证据，原始线性浮点环境 |
| forest-v05 | 当前综合主视觉 | HDR 115°/1.1、暖窗、弯溪；不宣称最终画质 |

光照中的 MIS 默认启用，但单张图不证明其数值收益；使用 V04 的多 seed 线性统计解释。当前图像及测试是实现证据，不代表课程评分或全部要求最终完成。

## 现场顺序与查看方式

先加载预渲染的 forest、Cornell 和三组效果对照，解释目标、采样与限制，再现场运行 quick 或一条 manifest 中的短任务。不要现场运行高采样参考实验。文件齐全以 manifest 的 image_check 为准，画面判断还需人工查看。

程序实际输出是 ASCII PPM；可选线性 PFM 是现有渲染器能力，本展示包不默认生成 PFM。不要假定 Windows 默认看图软件支持 PPM/PFM。已有验证使用 Pillow 转换 PPM 为 PNG；Pillow 是可选的本地查看工具，不是展示脚本/渲染器依赖，不自动安装。例如在安装了 Pillow 的 Python 中：
```python
from pathlib import Path
from PIL import Image
folder = Path("实际的新展示目录")
for source in folder.glob("*.ppm"):
    target = source.with_suffix(".png")
    if target.exists():
        raise FileExistsError(target)
    Image.open(source).save(target)
```
此方法仅用于 PPM 显示图，不把线性 PFM直接当显示图片转换。无 Pillow 时可将已预转换 PNG 带到现场，或使用经本机确认支持 PPM 的查看器。

Mac quick 实测结果见下方；Windows 路径、图像查看与耗时仍待反馈。公共基线的 Windows 构建说明保持在 [WINDOWS_VALIDATION](WINDOWS_VALIDATION.md)。

## 本轮 Mac quick 实测

2026-11-19，Apple M1，使用现有 Release 可执行文件（本轮未改 C++/CMake，未重复完整构建/统计实验）：
```sh
python3 tools/demo_pack.py --executable build-release/path_tracer --resources assets --output build-release/demo-packs --profile quick
```
输出：`build-release/demo-packs/demo-quick-20261119T035906Z-d7321bf6/`。13/13 成功，总墙钟 1.990 s；每张 PPM尺寸/通道数/范围完整。进程耗时为真实 subprocess 总耗时，包含加载/构建 BVH/渲染/写图与启动，不应与旧文档的 render-only 时间直接比较。

实际查看了景深关/开、运动曝光、alpha 图案、forest-v05：中间绿球相对清晰、前球开孔径后模糊，运动球有横向曝光拖影，叶片孔洞与投影均出现。低 spp 图仍有噪声，仅适合现场短任务；正式展示优先使用已有高质量预渲染图或 full 档。PPM 转 PNG 使用本机已装 Pillow，转换文件为该目录内同名 PNG，未覆盖旧图。

`--profile full --list` 已验证仅输出命令、不创建输出目录；未完整运行 full。另用含空格的可执行文件/资源路径和输出目录实跑 quick，验证参数列表路径处理；用错误类型的 normal_preview 可执行文件故意触发失败，确认 13 个任务退出码 1、脚本退出 1，stderr 和独立 manifest 均包含场景、精确命令、错误码。该失败包是测试工具错误处理，不是渲染器回归失败。Git 缓存忽略由 `git check-ignore -v tools/__pycache__/ tools/__pycache__/render_v05.cpython-37.pyc` 验证，无缓存删除。
