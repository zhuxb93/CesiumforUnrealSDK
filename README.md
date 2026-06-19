# CesiumforUnrealSDK

这是一个面向 UE 5.7 和 CesiumForUnreal 的独立 C++ 插件，整理地球相机控制器、UE 矢量瓦片渲染、GeoJSON 几何构建、UMG 搜索/HUD/POI 标签和通用鉴权刷新骨架。

## 亮点导航
- Camera：APawn 地球相机，支持自由飞行/跟随、平移缩放旋转倾斜、双击飞行和蓝图运镜 API。
- VectorTile：下载、LZ4 解压、FlatBuffers 解析、异步建筑/道路网格、ProceduralMesh 输出和帧率自适应节流。
- GeoJSON：解析多源 GeoJSON 并构建 UE 几何 Actor。
- UI：城市搜索面板、经纬度/视距 HUD、世界空间 POI 标签。
- Auth：只保留通用定时刷新和请求头注入骨架，不包含任何专有签名算法。

## 构建与运行
1. 在 UE 工程 Plugins 目录下放入本仓目录。
2. 先安装 CesiumForUnreal 插件，并确认项目启用 CesiumRuntime。
3. 重新生成工程文件后编译 GeoEarth 模块。
4. 示例服务地址均为 https://example.com/... 占位，请替换为自己的公开或本地测试服务。

## 工程笔记
本插件把模块名改为 GeoEarth，类前缀从历史项目名前缀迁移为 Geo。FlatBuffers 的业务 schema 生成物已改为中性 geo_flatbuffers 命名，Auth 模块也重写为公开安全的刷新示例，避免携带专有鉴权细节。

## 与其它仓库的关系
- VectorTile 是 CesiumforUnitySDK 矢量瓦片管线的 C++ 对照实现。
- Camera 与 CesiumforUnitySDK 的关键帧播放器可互相参考。
- CoordinateConverter 与 UnityGeoToolkit 的 GeoMath 可对照地理数学实现。
