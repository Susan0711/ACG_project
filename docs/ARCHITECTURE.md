# 场景层解耦：第一阶段

当前工作树含 v0.5 与展示包。本阶段只整理依赖和工厂位置，未新增渲染功能。

## 实际依赖

```text
path_tracer / 使用场景的测试
  -> scene_library（静态注册表、具体场景工厂、原位置森林文件）
     -> tracer（相机/Film/BSDF、积分、BVH、HDR、alpha、render、资源读取、World 装配）
        -> geometry + Threads::Threads
normal_preview -> geometry
pipeline_tests -> tracer
```

核心 tracer 不链接 scene_library、不调用具体演示场景。World::add_area 已逐字机械迁移到 src/world.cpp，继续同时装配几何、材质和灯光关联。src/scenes.cpp 只保留 Cornell/点光工厂；forest*.cpp 原地不动，但属于 scene_library 目标。

src/scenes/catalog.h/.cpp 提供 SceneEntry（名字、描述、工厂函数指针）、scene_catalog/find_scene/create_scene/write_scene_names。函数内静态表只保存数据，没有全局自注册或插件。CLI 帮助列表、名称校验、构造派发全走该表。texture、sampling、效果场景分别在 scenes/texture_scene.cpp、sampling_scene.cpp、effect_scenes.cpp；effects.cpp 和 integrator.cpp 仅机械移除了工厂，算法未改。

## 两人添加场景的步骤

1. 在场景侧实现 `World factory(const std::string& resource_root)`，使用已有核心接口；可用 catalog.h 声明，或建立自己的场景头。
2. 在 catalog.cpp 表中增加一个名称唯一、带描述的注册项。需要额外选项的旧工厂可用无捕获 lambda 适配。无需修改 CLI 名称列表或 if 派发。
3. 新 cpp 登记到 CMake 的 scene_library 源文件；链接场景的测试显式依赖 scene_library。若确实新增公开名称，扩展 catalog_tests 的旧名称保护清单，并验证资源路径和图像。

资源根目录仍指 assets，不自行改变当前工作目录。CLI 先构造工厂，再覆盖用户指定 HDR/相机参数，最后按快门范围建 BVH；此顺序未变。场景默认名称仍 cornell。

## 刻意保留的耦合

geometry 中 normal_preview 的小型 preview_scene 辅助函数也暂留；不把本阶段扩大为全部几何文件重组。

tracer.h 仍含历史 make_world/make_effect_scene/make_sampling_scene/forest 工厂声明，旧调用可继续编译，但调用方必须链接 scene_library。本阶段不大拆公共头。forest_v05.h 也保留原入口。材质索引、灯光索引、几何数组和 BVH 生命周期仍由 World 暴露管理。部分历史工厂会自行建 BVH，CLI 随后还会建一次；这是原有语义，本轮没有优化。

后续需两人另行讨论：将 Material/BSDF 采样协议从综合头中分离的边界、纹理/材质所有权、Scene 与加速结构构建/失效协议，以及是否统一工厂建树策略。这里记录待讨论事项，不预先改变接口，不假定队友未提交内容。

## 本次行为验证

见 [SCENE_DECOUPLING_VALIDATION](SCENE_DECOUPLING_VALIDATION.md)。基于本次实际工作树新建 Release 基线，非复用旧图；前后 8 组 PPM/PFM 与 normal_preview 均字节一致。注册表测试覆盖全部 19 名称。Windows/MinGW 旧提交的验证结果不能覆盖本轮；Windows 与 MSVC 均需队友重新验证。展示包和 Midterm 文档保留。
