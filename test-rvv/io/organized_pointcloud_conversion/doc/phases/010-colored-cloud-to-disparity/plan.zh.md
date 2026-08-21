# Phase 010 Plan: colored-cloud-to-disparity

## 阶段意图和边界

本阶段扩展 `OrganizedConversion<PointT, true>::convert(cloud, ...)` 的 colored cloud -> disparity + color
路径，覆盖 `PointXYZRGB` 的 RGB image（RGB 图像）和 mono image（灰度图）两种输出。目标是判断
Phase 000 的 disparity RVV candidate 是否能在 colored specialization 中继续保持 correctness 和板卡收益。

本阶段仍是 test-only diagnostic，不修改 production。

## Phase Scope 与扩展队列

`validated_scope`：`PointXYZRGB` / `float` / organized cloud order / cloud -> disparity + RGB 或 mono。

`unvalidated_scope`：`PointXYZRGBA`、decode path、generic PointT、production `encodePointCloud`、PNG/LZF 后端、
`Scalar=double` 和 production direct。

`phase_closeout_boundary`：本阶段只关闭 colored encode diagnostic 的矩阵条目。

## 计划动作

| action | 产物 / 命令 | 完成判据 | 状态 |
| --- | --- | --- | --- |
| 写 colored failing tests | `src/test_organized_pointcloud_conversion.cpp` | 初次运行因 colored helper 缺失失败 | planned |
| 实现 PointXYZRGB diagnostic helper | `include/impl/opc_candidates.hpp`, fixtures | RGB 与 mono 输出和 PCL scalar path 完全一致 | planned |
| 扩展 bench case | `src/bench_organized_pointcloud_conversion.cpp` | RGB / mono case 输出 checksum 和时间 | planned |
| QEMU correctness | `make run_test_compare` | std/RVV correctness 通过 | planned |
| 反汇编归属 | `make dump_bench_rvv` | 仍可见 disparity RVV hot region | planned |
| 板卡 bench + Doctor | `make board_smoke`, Evidence Doctor manifest | colored case bucket 和 warning 解释写入 result | planned |

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `diagnostic` |
| A/B boundary | `test helper` |
| 当前决策问题 | colored specialization 中 RVV-vs-scalar 是否仍有局部价值 |
| diagnostic 是否可外推到 production | no；colored path 仍需 production direct 和 compression 后端稀释审计 |
| comparison-boundary / baseline mismatch 风险 | yes；candidate 可能分开处理 disparity 和 color，production 是单 loop |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 只允许在 PointXYZRGB/RGBA 分流和 fallback 可控时继续 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 若出现多个 colored candidate family，则需要同边界 A/B |

## 板卡复跑预算和决策桶

沿用 Phase 000：单次 board diagnostic 若所有 colored case 同方向且 checksum 一致，可用于继续 / 停止诊断决策；
production 结论需要 repeated board 和 production direct。

## 继续 / 停止条件

继续到 decode phase 或 production-integration-plan 的前提：colored RGB / mono 至少不出现维护成本明显高于收益的负向证据。
如果 colored path 明显退化，则只保留 PointXYZ diagnostic，并把 colored candidate 标为 rejected / deferred。
