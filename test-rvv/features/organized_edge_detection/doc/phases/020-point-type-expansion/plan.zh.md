# Phase 020 Point Type Expansion Plan

## 阶段意图和边界

本阶段不修改 production 源码，只验证 Phase 010 已采纳的 depth label RVV production path
（生产路径）在更多满足 `RVVXYZAoSFloatLayout<PointT>` 的点型上是否可以闭合 evidence
（证据）。Phase 010 的 production patch 已经用 traits gate（字段布局准入）允许不止 `PointXYZ` 的
`PointT` 命中 RVV helper；当前缺口是 correctness（正确性）、asm attribution（反汇编归属）和 board
（板卡）性能证据只覆盖了 `PointXYZ + pcl::Label`。

validated_scope（本阶段准备证明）：

- 入口：真实 `OrganizedEdgeBase<PointT, pcl::Label>::compute()` / `extractEdges()` depth label path。
- 点型：`PointXYZI`、`PointXYZRGB`、`PointXYZRGBNormal`，均只使用 x/y/z 字段驱动 depth label。
- `PointLT`：继续固定为 `pcl::Label`。
- 数据布局：`RVVXYZAoSFloatLayout<PointT>` 成立的 AoS 单 float xyz 布局。
- case：finite 320x240 / selected tail 或 NaN boundary case，按 board budget 裁剪。

unvalidated_scope（本阶段不证明）：

- 泛型 `PointLT` label field gate；production 当前仍明确收窄到 `pcl::Label`。
- RGB / normal Canny 派生入口；它们属于后续 `030-rgb-normal-derived-entries`。
- `assignLabelIndices()` RVV 化；只有 production profile 显示它成为主成本时才恢复。
- 自定义点型、非标准 AoS、非 float z、indices/correspondences 或真实数据集性能。

phase_closeout_boundary：本阶段只能把已测试点型在真实 public `compute()` depth path 下标为
`covered_by_phase020` 或 `not_worth_expanding`；不能把结果外推成所有 PointXYZ-like 点型均已证明。

## 当前状态清单

| 项 | 当前状态 | 路径 |
| --- | --- | --- |
| production depth RVV | Phase 010 已采纳；`extractEdges()` 先尝试 `organizedEdgeDepthLabelsRVV()`，失败回 Std helper。 | `features/include/pcl/features/impl/organized_edge_detection.hpp` |
| correctness | Phase 010 `run_test_compare` Std/RVV 各 4/4 pass，只含 `PointXYZ + Label` production compute 对拍。 | `test-rvv/features/organized_edge_detection/src/test_organized_edge_detection.cpp` |
| production bench | Phase 010 production-public board summary 为 `6.194x` / `5.521x` / `3.016x`，checksum match。 | `test-rvv/features/organized_edge_detection/log/board/production-repeated-summary.md` |
| Evidence Doctor | Phase 010 为 `0E/0W/3S`，Suggestions 为环境 metadata 缺失。 | `test-rvv/features/organized_edge_detection/log/board/production-evidence_doctor.md` |
| roadmap | 默认恢复队列第一项为 `020-point-type-expansion`。 | `test-rvv/features/organized_edge_detection/doc/optimization-roadmap.zh.md` |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| `depth_labels_point_type_expansion` | depth helper 只读取 z 字段并写 `pcl::Label`，因此满足 xyz AoS gate 的常见点型应保持相同标签语义。 | 更大 point stride 可能降低内存吞吐；RGBNormal 等点型的 board speedup 可能弱于 `PointXYZ`。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `depth_labels_point_type_expansion` | organized-grid internal pixels | `PointXYZI`, `PointXYZRGB`, `PointXYZRGBNormal`, `pcl::Label`, float z, AoS | public `OrganizedEdgeBase<PointT, Label>::compute()` depth path | extend `run_test_compare` production typed cases | new point-type production bench cases | planned 5-run repeated if board remains available | existing `check_production_rvv_asm` plus point-type bench binary | planned manifest + Doctor | planned |

## 实现和测试动作

| 动作 | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| A1 typed production correctness | 扩展 `src/test_organized_edge_detection.cpp`，对 `PointXYZI` / `PointXYZRGB` / `PointXYZRGBNormal` 调真实 `compute()`。 | Std/RVV `run_test_compare` 均通过；新增测试失败会暴露点型布局或 fallback 语义问题。 |
| A2 point-type production bench | 扩展 `src/bench_organized_edge_detection_production.cpp`，新增 point-type case label。 | QEMU smoke 可运行；QEMU timing 只作为日志形状。 |
| A3 manifest / registry | 扩展 topic-local manifest script 和 Makefile target，独立生成 Phase 020 point-type summary。 | Evidence Doctor 可读，registry fresh。 |
| A4 asm | 复用 `check_production_rvv_asm`。 | 生产 helper 仍含预期 RVV 指令。 |
| A5 board repeated | 新增或运行 point-type repeated board target。 | 5-run summary checksum match；decision bucket 稳定或按预算降级。 |
| A6 docs | 更新 phase result、matrix、roadmap、evaluation 和 `doc-rvv`。 | 当前 truth 和 evidence path 一致，不把未测点型写成已覆盖。 |

## Evidence Doctor 和 Registry 规则

- Phase 020 使用独立 summary：`log/board/point-type-repeated-summary.md`。
- Manifest：`log/board/point-type-evidence_manifest.json`。
- Doctor：`log/board/point-type-evidence_doctor.md`。
- Registry run label：`board-organized-edge-detection-point-type-expansion-phase020`。
- 预期：checksum match；Errors=0。若某点型 speedup 低于 1 或 Doctor Warning 指向边界不一致，不扩大 production 文档的 adopted scope，只把该点型标为 attempted / weak / rejected。

## 板卡复跑预算和决策桶

- run count：5。
- iterations / warmup：沿用 topic production bench 默认或 board target 传入参数。
- decision bucket：
  - `positive`：每个新增点型 mean 和 median 均大于 `1.05x`，且 `min B/A >= 1.0`。
  - `weak-positive`：mean / median 大于 `1.05x`，但出现少量 run 低于 1 或环境 metadata 不足。
  - `neutral`：mean / median 在 `0.95x-1.05x`。
  - `negative`：mean / median 小于 `0.95x` 或 `B/A < 1` 稳定出现。
  - `unstable`：5-run 内方向摇摆，且无法用当前 metadata 解释。
- 预算耗尽后不无限复跑；若 bucket 不稳定，记录为风险或人工判断项。

## Diagnostic-to-Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `production-public`。 |
| A/B boundary | public `OrganizedEdgeBase<PointT, pcl::Label>::compute()`。 |
| 当前决策问题 | scope expansion：当前 adopted RVV family 是否可扩到更多 `PointT`。 |
| diagnostic 是否可外推到 production | 不使用 diagnostic 外推；本阶段直接跑 production public。 |
| comparison-boundary / baseline mismatch 风险 | Std/RVV 是同一 bench wrapper、同一点型、同一 synthetic organized grid。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前 production patch 已存在；弱 / 负只限制文档 adopted scope，不自动回滚 Phase 010 `PointXYZ` 采纳。 |
| clean adoption 是否需要 RVV-vs-RVV detail A/B | 不需要；本阶段不是新 RVV family 选择，而是同一 family 的点型覆盖扩展。 |

## 继续 / 停止条件

继续条件：

- A1-A5 均可执行，且板卡可用。
- 新增点型至少有 correctness、asm 和 board / Doctor 证据可闭合。

停止条件：

- 某点型无法编译或 `RVVXYZAoSFloatLayout` 不成立；该点型标为 rejected / not applicable。
- board 不可用或 repeated target 失败三次以上；记录 blocked evidence，不把范围扩展写成 adopted。
- Doctor Error 无法修复；本阶段不扩大 adopted scope。

## 文档更新清单

- `020-point-type-expansion/result.zh.md`
- `doc/phases/README.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/organized_edge_detection-evaluation.zh.md`
- `doc-rvv/features/organized_edge_detection-RVV.zh.md`

## roadmap 同步动作

若 Phase 020 positive，把 `020-point-type-expansion` 从默认恢复队列移到 covered / adopted-with-evidence，
并把剩余泛型 `PointLT`、RGB/normal 派生入口和 `assignLabelIndices` 分开保留。若收益弱或负，roadmap
记录点型 stride / cache 风险，后续不建议继续扩大 input point type，除非真实 workload 需要。
