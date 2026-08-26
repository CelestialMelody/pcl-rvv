# Phase 020 Plan: production-integration-plan

## 阶段意图和边界

本阶段是 PI1 production integration plan（生产接入计划）。目标是把 Phase 010 的 full-scan diagnostic（完整扫描段诊断）转成可审查的 production patch（生产补丁）范围、fallback（回退路径）矩阵和证据计划。

本阶段不修改 production 源码。PI2 会触碰 `segmentation/include/pcl/segmentation/impl/extract_polygonal_prism_data.hpp`，若选择 clean split（干净拆分，把标量主体抽成 helper），还可能需要修改 `segmentation/include/pcl/segmentation/extract_polygonal_prism_data.h` 的类声明。进入 PI2 前需要用户明确授权。

## 候选范围

| 维度 | PI1 冻结范围 | 不覆盖范围 |
| --- | --- | --- |
| public entry | `ExtractPolygonalPrismData<PointT>::segment(PointIndices&)` | `isPointIn2DPolygon`、`isXYPointIn2DXYPolygon` 独立公开函数暂不改 |
| RVV stage | plane setup 和 `projectPoints` 后的逐点扫描段 | plane fitting、`projectPoints` 自身、concave hull 多 polygon RVV |
| row source | 优先 dense ordered indices，即 `(*indices_)[i] == i`，保证 output 保序可直接压缩 lane id | arbitrary indices gather 先 fallback |
| polygon | `polygons_.empty()` 或等价 single polygon path | `polygons_` 非空的 concave hull 多 polygon XOR 先 fallback |
| point type | 默认尝试 `RVVXYZAoSFloatLayout<PointT>`，只读 xyz，输出 indices | 若 traits gate 编译或布局风险不能闭合，收窄到 exact `PointXYZ` 作为阶段例外 |
| Scalar | 当前函数固定使用 float model coefficients 与 double height limits；RVV distance 使用 float，与 Phase 010 一致 | 不声明 `Scalar=double` 泛型数学覆盖 |
| output | `output.indices` 保序压缩 | 不改变 header、init/deinit、错误路径或 public API |

## 推荐源码形态

首选 clean split：

1. 在类声明中新增私有或 protected `segmentStd(PointIndices&)`，保存原标量主体。
2. `segment` 保留 `initCompute`、hull 检查、`__RVV10__` 下的 RVV 尝试和 `segmentStd` fallback。
3. `segmentRvv` 或邻近 internal helper 只在 `__RVV10__` 下存在，返回 `bool`；失败时不改变输出或保证重新由 Std 覆盖。
4. 复用 `pcl/rvv_point_load.h` 与 `RVVXYZAoSFloatLayout<PointT>`，避免 hard-code `PointXYZ` offset。

备选 impl-only exception（只改 impl 头）：如果用户只授权 impl 文件，可以在 `segment` 内部保留原标量主体并在 `__RVV10__` 后短路返回。此形态不符合长期 clean split 质量门槛，必须在 Handoff 和 reviewer focus 里标成结构风险。

## Fallback 矩阵

| gate | fallback 条件 | 期望行为 | 必测证据 |
| --- | --- | --- | --- |
| build gate | 非 `__RVV10__` | 完全走原标量路径 | std build / test |
| traits gate | `!RVVXYZAoSFloatLayout<PointT>::value` | fallback 到 `segmentStd` | compile / fallback test 或 exact-type 阶段例外说明 |
| size gate | `indices_->size() < 64` | fallback 到 `segmentStd` | small input direct test |
| row source gate | 任意 `(*indices_)[i] != i` | fallback 到 `segmentStd` | indexed subset direct test |
| polygon gate | `polygons_` 非空或多 polygon | fallback 到 `segmentStd` | concave hull direct test |
| projected size / setup | projection 失败或 projected size 与 indices 不一致 | fallback / error 语义与 Std 一致 | direct correctness |
| offset gate | indexed gather 未实现时不使用 32-bit byte offsets | fallback | not_applicable，因本阶段只 dense |

## 生产直连测试计划

| test | 目的 | 预期 |
| --- | --- | --- |
| RVV build public `segment` single polygon dense | 真实 public entry 命中 RVV 分流 | 输出 indices 与标量参考一致 |
| non-RVV build same fixture | 验证 `__RVV10__` 关闭时不改变行为 | 输出一致 |
| small input fallback | 隔离 size gate | 输出一致且不要求 RVV 指令 |
| indexed subset fallback | 隔离 dense ordered gate | 输出一致、保序 |
| concave hull fallback | 隔离 multi-polygon gate | nested polygon XOR 与标量一致 |
| PointXYZI or PointXYZRGB compile / behavior | 若采用 traits gate，证明常见 PCL_XYZ_POINT_TYPES 可用 | 输出一致或 fallback 明确 |

## 生产 bench / asm / board 计划

| evidence | 命令 / 产物 | 判据 |
| --- | --- | --- |
| QEMU correctness | production direct test target | 只证明正确性和路径 |
| QEMU smoke | production bench small size | checksum 一致，不用计时做性能 |
| asm attribution | production RVV binary objdump | `segment` 或 RVV helper 归属到 `vlse32.v`、`vfmacc.vf`、mask、`vcompress.vm` |
| board direct repeated | public `segment` bench，5 runs + 最多 1 次确认复跑 | speedup bucket positive / neutral / negative，Evidence Doctor 解释 |
| Evidence Doctor | manifest + `evidence_doctor.py` | Errors=0；Warnings 已解释或降级 |

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | Phase 010 为 production-shaped diagnostic；PI2-PI5 才能产生 production-public / production-detail |
| A/B boundary | 当前 helper boundary；生产接入后必须改成 public overload 或 production detail helper |
| 当前决策问题 | 是否允许有界 production probe；不是 clean adoption |
| diagnostic 是否可外推到 production | partial：扫描段语义已接近，但 `projectPoints` 和真实对象状态未计入 |
| comparison-boundary / baseline mismatch 风险 | yes；diagnostic speedup 可能被真实入口前置成本稀释 |
| weak / negative 时是否允许 bounded production probe | 若 PI1 clean split 小、fallback 简单且 correctness 可闭合，仍可做显式 probe；否则不建议 |
| clean adoption 是否需要 production boundary A/B | yes；PI5 前不得写 adopted production behavior |

## 停止条件

PI1 本身可完成并停止。进入 PI2 必须满足：

- 用户明确授权 production integration loop，并说明是否允许修改 class declaration header。
- clean split 或 impl-only exception 二选一被确认。
- production direct test / bench 资产可在当前 topic 内创建，不与其它 topic dirty work 混淆。

## next_worker_action

输出 PI1 result 和 Handoff，等待用户确认是否进入 PI2 production patch。默认建议选择 clean split：允许同时修改 `segmentation/include/pcl/segmentation/extract_polygonal_prism_data.h` 和 `segmentation/include/pcl/segmentation/impl/extract_polygonal_prism_data.hpp`，随后连续推进 PI2-PI5，并在 PI5 停下等待最终采纳 / 回滚确认。
