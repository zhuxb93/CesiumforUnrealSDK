# UMG 搜索与标签

## 解决什么问题
地球应用需要把搜索面板、经纬度 HUD 和 POI 标签叠加在 3D 场景上。

## 实现思路
UMG 面板负责输入与列表，Actor/WidgetActor 负责世界空间挂载，HTTP 端点统一使用占位配置。

## 基本用法
替换 https://example.com/... 为自己的服务，或只使用本地合成数据测试 UI 结构。

## 依赖
UMG、HTTP、Json。
