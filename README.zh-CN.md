# CesiumforUnrealSDK

[English](README.md)

CesiumforUnrealSDK 是一份面向 Unreal Engine 5.7 与 CesiumForUnreal 的独立 C++ 插件技术积累，整理了三维地球相机控制器、UE 端矢量瓦片渲染、GeoJSON 几何构建、UMG 搜索/HUD/POI 标签，以及通用鉴权刷新骨架。

仓库已重组为 `GeoEarth` 插件模块，适合放入 UE 工程的 `Plugins/` 目录中独立编译。它不包含 CesiumForUnreal 插件本体、商业美术资源、sqlite 缓存、构建产物、真实服务地址或专有鉴权算法。

## 亮点导航

- `Source/GeoEarth/Camera/`：`APawn` 地球相机，支持自由飞行/跟随、平移、缩放、旋转、倾斜、双击飞行和蓝图运镜 API。
- `Source/GeoEarth/VectorTile/`：矢量瓦片下载、LZ4 解压、FlatBuffers 解析、异步建筑/道路网格生成、ProceduralMesh 输出和帧率自适应节流。
- `Source/GeoEarth/GeoJSON/`：GeoJSON 解析、几何构建和多源加载。
- `Source/GeoEarth/UI/`：城市搜索面板、经纬度/视距 HUD、世界空间 POI 标签和搜索面板结构。
- `Source/GeoEarth/Auth/`：通用定时刷新与请求头/URL 占位注入骨架，不包含任何真实签名算法。
- `Docs/`：相机控制器、矢量瓦片和 UMG 结构的中文工程说明。

## 安装与依赖

1. 将本仓目录放入 UE 工程的 `Plugins/CesiumforUnrealSDK/`。
2. 安装并启用 CesiumForUnreal，确保项目可引用 `CesiumRuntime`。
3. 启用 `ProceduralMeshComponent`、`UMG`、`HTTP`、`Json` 等插件/模块依赖。
4. 重新生成工程文件并编译 `GeoEarth` 模块。
5. 示例 URL 均使用 `https://example.com/...` 占位，请替换为自己的公开测试服务或本地服务。

## 使用建议

建议先在一个空 UE 工程中验证相机控制器：创建包含 `CesiumGeoreference` 的关卡，把 `AGeoCameraController` 设为默认 Pawn。确认相机可用后，再接入合成 GeoJSON 或本地测试瓦片验证 `VectorTile` 与 `UI` 模块。

`Auth` 模块只是安全公开的示例骨架。如果要接入真实鉴权，请把密钥和签名逻辑放在服务端，客户端只保存短期访问凭据。

## 脱敏与许可

- 模块名已从历史项目名迁移为 `GeoEarth`，类前缀已改为 `Geo`。
- 私有品牌、内网地址、真实城市服务端点、专有签名算法、缓存和商业资源均已移除。
- `LICENSE` 仅覆盖本人原创和改写部分。
- CesiumForUnreal、Google FlatBuffers、Triangle(C) 和 Unreal Engine 相关部分按各自许可使用，详情见 `THIRD_PARTY_NOTICES.md`。
- Triangle(C) 涉及非商用许可边界，已在 `待本人确认清单.md` 中列为公开前确认项。
- 复核记录见 `脱敏复核报告.md`。

## 与其它仓库的关系

- `VectorTile` 可与 `CesiumforUnitySDK` 的 C# 矢量瓦片管线对照阅读。
- `CoordinateConverter` 可与 `UnityGeoToolkit` 的 `GeoMath` 对照阅读。
- 地球相机控制器可与 Unity 版相机关键帧播放器互相参考。

## 当前状态

本仓已完成插件重组、中文模块说明、英文入口说明、第三方许可清单和脱敏复核。尚未在 Unreal Editor 中完成真实导入编译，公开使用前建议先在 UE 5.7 工程中跑一轮插件编译验证。
