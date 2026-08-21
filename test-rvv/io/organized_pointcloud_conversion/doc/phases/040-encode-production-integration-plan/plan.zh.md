# Phase 040 Plan: encode-production-integration-plan

## 阶段意图和边界

本阶段进入 encode-only production probe（仅 encode 侧生产探针）。只考虑
`OrganizedConversion<PointT, false>::convert(cloud, ...)` 和
`OrganizedConversion<PointT, true>::convert(cloud, ...)` 这两个 cloud -> disparity / RGB / mono 入口。

不接入 disparity/depth -> cloud decode overloads。Phase 030 的 decode v0 正确但 0.85x-0.91x 退化，已被
拒绝为 production candidate。

## 当前证据

| area | evidence |
| --- | --- |
| PointXYZ encode diagnostic | 1.96x-2.00x，checksum match |
| PointXYZRGB fused colored diagnostic | RGB 1.48x-1.49x，mono 1.81x，checksum match |
| decode diagnostic | 0.85x-0.91x，checksum match，rejected |
| Evidence Doctor | `Errors=3, Warnings=16`；Errors 只来自 decode v0 |
| production | 未修改 |

## Production 设计

生产补丁采用小型分发层：

- 把现有标量主体抽成 `convertCloudToDisparityStd` / `convertCloudToDisparityColorStd` 等邻近 helper。
- `__RVV10__` 下新增 encode-only RVV helper，复用公共 `pcl/rvv_point_load.h` 的 PointXYZ-like
  strided xyz load；颜色写出先保持逐 lane 标量写。
- 公开 `convert(cloud, ...)` 入口只做短路 RVV 尝试和 Std fallback（标量回退）。
- 不改变公开 API，不改变 decode overload。

## Gate 与 fallback

首个 production probe 默认采用 PointXYZ-like traits gate（点类型字段特征门控）：

- `x/y/z` 均为单个 `float` 字段。
- 使用 PCL traits 取得 offset，使用公共 strided load wrapper。
- 输入 work item count 足够大才走 RVV；小规模 fallback Std。
- RVV helper 内部仍按 `pcl::isFinite` 等价语义处理 x/y/z finite mask；不覆盖其它非标准字段语义。

若 traits gate 在当前 header 模板上下文中编译或可维护性风险过高，允许阶段性收窄到已证明的 `PointXYZ`
和 `PointXYZRGB` exact-type gate，但必须在 result 写成 phase-local exception，并列入 point type expansion queue。

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| production direct RED | topic gtest 改为直接调用 PCL production RVV build 并要求 RVV path metadata / checksum | 修改 production 前失败或无法命中 RVV |
| 抽 Std helper | `io/include/pcl/compression/organized_pointcloud_conversion.h` | 非 RVV build 语义保持 |
| 接 RVV helper | 同上 | RVV build correctness 与 scalar 对拍 |
| production bench | topic bench 增加 production-direct label 或切换 existing label 到 production path | board checksum match |
| Evidence Doctor | 更新 manifest | production direct warnings / errors 可解释 |

## 完成条件

production probe 只有在 correctness、asm、board 和 Evidence Doctor 都闭合后，才能进入 production closeout。
若 production direct 明显低于 diagnostic、或 Doctor 出现无法解释的 production boundary Error，必须回收 production
默认路径或把补丁降级为 diagnostic-only。
