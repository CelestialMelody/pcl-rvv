# Phase 040 Plan: production-integration-loop

## 阶段意图和边界

本阶段进入 PI2-PI5 production integration loop（生产接入闭环）：把 Phase 010 / Phase 030 已在板卡证明为 positive bucket 的 full-scan single polygon RVV 路径接入真实 `ExtractPolygonalPrismData<PointT>::segment`。

本阶段允许修改：

- `segmentation/include/pcl/segmentation/extract_polygonal_prism_data.h`
- `segmentation/include/pcl/segmentation/impl/extract_polygonal_prism_data.hpp`
- 当前 topic 的测试、bench、Makefile、phase / evaluation 文档

不修改 public API（公开接口），不修改 `isPointIn2DPolygon` / `isXYPointIn2DXYPolygon` 两个独立公开函数，不接入 concave hull 多 polygon XOR 的 RVV 路径。

## 候选范围

| 维度 | 本阶段接入范围 | fallback / 不覆盖范围 |
| --- | --- | --- |
| public entry | `ExtractPolygonalPrismData<PointT>::segment(PointIndices&)` | 其它公开函数不改 |
| RVV stage | `projectPoints` 后的逐点扫描段：plane distance、height mask、single polygon edge parity、保序 output compress | plane setup、`projectPoints` 自身保持标量 |
| row source | dense ordered indices 和合法 arbitrary indices gather | invalid indices、32-bit byte offset 不满足时 fallback |
| polygon | `polygons_.empty()` 或 `polygons_.size() == 1` 的 single polygon | 多 polygon XOR fallback |
| point type | `RVVXYZAoSFloatLayout<PointT>`，只读 xyz，输出 indices | 非 xyz AoS float layout fallback |
| size | `indices_->size() >= 64` | 小规模 fallback |

## TDD RED

先在 `src/test_eppd.cpp` 增加 production direct 测试。测试通过派生类调用预期的 protected `segmentStd` 与 `segmentRvv` helper：

- dense single polygon：`segmentRvv` 返回 true，输出等于 `segmentStd`。
- indexed single polygon：`segmentRvv` 返回 true，输出等于 `segmentStd`。
- concave hull 多 polygon：`segmentRvv` 返回 false，public `segment` 输出仍等于标量。

当前 production 类没有这些 helper，RED 应在 RVV 构建下编译失败。

## 实现动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| PI2 clean split | 类声明新增 protected `segmentStd` 和 `segmentRvv`；impl 中把原标量主体移入 `segmentStd` | public `segment` 只做 RVV 尝试和 Std fallback |
| RVV helper | `segmentRvv` 使用 `pcl::rvv_load` 与 `RVVXYZAoSFloatLayout<PointT>` | dense 出现 strided load；indexed 出现 gather；输出顺序与标量一致 |
| bench production mode | `bench_eppd --path production --indices dense/indexed` | Std/RVV checksum 一致，计时边界为真实 public `segment` |
| board evidence | production dense / indexed repeated board summary + Evidence Doctor | positive / weak / neutral / negative bucket 有明确结论 |

## Evidence Doctor 和决策

生产 repeated board 使用 `production-public` evidence role（证据角色）和 public overload A/B boundary（公开入口对比边界）。若 production speedup 仍为 positive，进入 PI5 `pending_user_confirmation_adopt_production`；若 neutral / negative，进入 PI5 `pending_user_confirmation_rollback`。无论正负，PI5 后都保留 patch 并等待用户确认。
