# Phase 020 Viewpoint Histogram RVV Diagnostic Plan

## 阶段意图和边界

本阶段继续 VFH topic 的 test-only diagnostic（测试专用诊断）。Phase 000 已证明
centroid-to-point SPFH-like pair math（从质心到点的简化点特征直方图点对数学）有稳定板卡收益，
但 public baseline（公开入口基线）仍在 1x 附近。本阶段只把 viewpoint histogram（视点直方图）的
normal dot + bin preparation（法线点积与分箱准备）移到 RVV，histogram scatter（直方图离散累加）
仍保持标量顺序，用来判断第二条 O(N) 扫描是否是稀释来源。

不修改 `features/include/pcl/features/impl/vfh.hpp`，不创建 `doc-rvv/features/vfh-RVV.zh.md`。
覆盖范围仍是 dense finite `PointNormal -> VFHSignature308`、`float`、默认 45/128 bin、full-cloud
sequential indices。

## 当前状态清单

| item | 当前事实 |
| --- | --- |
| Phase 000 | candidate 2.61x-2.70x positive diagnostic。 |
| PI1 | 计划已建立，但 production patch 需要用户明确确认。 |
| 当前缺口 | public baseline 0.97x-1.01x；需要拆分 viewpoint 扫描是否值得纳入后续 production probe。 |
| 板卡 | 当前可用；本阶段若 QEMU / asm 通过，应继续 5-run repeated。 |

## 优化矩阵

本阶段新增 `vfh-viewpoint-histogram-rvv` 条目。它只能关闭 test-only helper 边界，不能关闭
production dispatch。

## 实现和测试动作

| action | 产物 | 验收 |
| --- | --- | --- |
| RED-1 | `src/test_vfh.cpp` 新增 `SPFHAndViewpointRVVMatchesReference`。 | helper 缺失时 `run_test_rvv` 编译失败，证明测试先行。 |
| GREEN-1 | `include/impl/vfh_reference.hpp` 新增 combined candidate。 | RVV 构建通过，Std 构建 fallback 返回 false。 |
| BENCH-1 | `src/bench_vfh.cpp` 新增 `candidate_vfh_spfh_viewpoint_rvv` case。 | board repeated 能同时比较 Phase 000 和 Phase 020 case。 |
| ASM-1 | `dump_bench_rvv`。 | 反汇编中可见 viewpoint normal dot 的 RVV load / arithmetic / store。 |
| BOARD-1 | `board_repeated` + `evidence_doctor_repeated`。 | 5-run repeated 用于判断 combined candidate 是否强于 Phase 000 candidate。 |

## Evidence Doctor 和决策桶

沿用 Phase 000 的 5-run、warmup 2、iterations 8。若 combined candidate 相对 Std 的 speedup 仍稳定
大于 1.15，且相对 Phase 000 candidate 没有明显退化，则标为 positive diagnostic；若相对 Phase 000
收益小于 3% 或 doctor 出现未解释退化，标为 attempted / neutral and not production decisive。

## 继续 / 停止条件

如果本阶段 positive，更新 PI1 计划，把 viewpoint RVV 纳入可选 production probe 范围；若中性或退化，
保留 Phase 000 candidate，viewpoint 路线标为 attempted / rejected。任何 production patch 仍需用户确认。
