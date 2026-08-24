# Phase 044 计划：row-source generic xyz point type expansion

## 阶段意图和边界

本阶段在 Phase 043 已采纳的 row-source production patch 基础上，补 `row-source-direct-fused-scale-accum` 的 representative generic xyz AoS（代表性泛型 xyz 数组结构）证据。

验证范围：

| 维度 | 本阶段覆盖 |
| --- | --- |
| production entry | source-indexed、dual-indexed、correspondence 三类 `TransformationEstimationSVDScale` public overload |
| row source | source-indexed、dual-indexed、correspondence |
| point type | `PointXYZI -> PointXYZI`、`PointXYZRGB -> PointXYZRGB`、`PointXYZI -> PointXYZRGB` |
| `Scalar` | `float` |
| layout | `RVVXYZAoSFloatLayout` traits-gated dense xyz AoS |
| sizes | QEMU smoke 4K / 64K / 256K label；board repeated 同三档 |

不验证范围：

- 全部 xyz AoS 点型和用户自定义点型。
- `Scalar=double`。
- non-dense、NaN / Inf、非法 index、越界 correspondence。
- locality / order profile（索引局部性 / 顺序剖析）和 long-tail mitigation（长尾缓解）。

## 当前状态清单

| area | 当前状态 |
| --- | --- |
| Phase 043 row-source patch | 用户已确认“当前有收益的实现可以接入”；本阶段把 Phase 043 从 `positive_pending_user_confirmation` 收口为 adopted 范围。 |
| correctness | Phase 043 当前 Std/RVV 为 11 tests；本阶段新增 `RowSourceGenericXYZPointTypesMatchReference` 后应成为 12 tests。 |
| bench | 已有 `row-source-scale` case-filter；本阶段新增 `row-source-generic-xyz-point-types`。 |
| QEMU smoke / board target | 本阶段新增 `record_qemu_row_source_generic_state` 和 `run_board_bench_row_source_generic_xyz_point_types_repeated`。 |
| Evidence Doctor / registry | 新证据必须登记到 `log/evidence_registry.json`，并在 `evidence_status` 中 fresh。 |

## 假设与候选族

本阶段假设 production row-source helper 已经使用 `RVVXYZAoSFloatLayout<PointSource>` 和 `RVVXYZAoSFloatLayout<PointTarget>` 分别 gate source / target，因此代表点型扩展不需要新 production code shape。需要验证的是：

- indexed gather 使用当前 `PointSource` / `PointTarget` 的 POD stride 和 xyz offset，而不是 `PointXYZ` 写死 offset。
- source-indexed 的 target ordered selected cloud 在泛型 target 上仍与标量 reference 对齐。
- dual-indexed 和 correspondence 的 source / target 双 gather 在混合点型上保持数值一致。
- board repeated 的收益不因 `PointXYZI` / `PointXYZRGB` stride 改变而退化到 neutral 或 negative。

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | correctness | bench / evidence | decision rule |
| --- | --- | --- | --- | --- | --- |
| `row-source-direct-fused-scale-accum` | source-indexed | `PointXYZI -> PointXYZI` / `float` / dense xyz AoS | `RowSourceGenericXYZPointTypesMatchReference` | QEMU smoke + board repeated | correctness passed、QEMU Doctor clean、board median positive 才能写 positive。 |
| `row-source-direct-fused-scale-accum` | dual-indexed | `PointXYZRGB -> PointXYZRGB` / `float` / dense xyz AoS | 同上 | 同上 | 同上。 |
| `row-source-direct-fused-scale-accum` | correspondence | `PointXYZI -> PointXYZRGB` / `float` / dense xyz AoS | 同上 | 同上 | 同上。 |

## 实现和测试动作

| action | artifact / command | 完成判据 |
| --- | --- | --- |
| A1 correctness 扩展 | `src/test_tesvd_scale.cpp` 新增代表点型 row-source gtest | Std/RVV gtest 通过。 |
| A2 bench 扩展 | `src/bench_tesvd_scale.cpp` 新增 `row-source-generic-xyz-point-types` case-filter | QEMU smoke 输出 9 个 label，`max_reference_error <= 2e-3`。 |
| A3 target 接线 | `Makefile` 新增 QEMU / board / doctor / registry target | `make -n` 或实际 target 可解析。 |
| A4 QEMU 证据 | `make -C ... run_test_compare_recorded record_qemu_row_source_generic_state evidence_status` | correctness、doctor、registry fresh。 |
| A5 board 证据 | `make -C ... run_board_bench_row_source_generic_xyz_point_types_repeated evidence_status` | 5-run summary / doctor / registry 完成。 |
| A6 文档同步 | result、matrix、roadmap、evaluation、testing overview、benchmark/evidence、README | 当前状态不再写 Phase 043 pending；Phase 044 结果按证据填写。 |

## Evidence Doctor 和 Registry 规则

- QEMU smoke 只证明 build、case label、日志形状、`max_reference_error` 和 manifest 可解析，不作为性能结论。
- board repeated 采用 5-run、20 iterations、5 warmup 的既有预算。
- Doctor 若有 warning，按 row source / point type / size 分开解释；不能无解释写 clean generic adoption。
- registry 必须记录 QEMU correctness、QEMU row-source generic smoke 和 board row-source generic repeated。

## 板卡复跑预算和决策桶

| 字段 | 值 |
| --- | --- |
| repeated runs | 5 |
| warmup / iterations | 5 / 20 |
| positive | 每个 case 所有 B/A > 1.20 |
| weak-positive | median >= 1.05 且 min >= 0.97 |
| neutral / negative / unstable | 沿用 summary script 口径 |
| 复跑策略 | 若 bucket 不变，不无限复跑；若出现 negative / unstable 或 Doctor error，暂停解释。 |

## Diagnostic-to-Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `production_public_row_source`，本阶段使用真实生产公开入口。 |
| A/B boundary | Std 构建父类标量公开入口 vs RVV 构建 scale 子类 row-source RVV 公开入口。 |
| 当前决策问题 | Phase 043 row-source patch 的 traits-gated generic xyz AoS 代表点型是否有同边界 correctness / board support。 |
| diagnostic 是否可外推到 production | QEMU smoke 不外推性能；board repeated 支撑当前 public row-source generic representative boundary。 |
| comparison-boundary / baseline mismatch 风险 | 有。不同 row source 和点型 stride 成本不同，必须分 case 报告。 |
| weak / negative / unstable 时是否允许 bounded production probe | 当前已是 bounded production evidence；若 weak / negative / unstable，降级为代表点型不充分，不回推否定 PointXYZ row-source patch。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要；当前没有同 row-source generic 的既有 RVV family 选择问题。 |

## 完成条件

- 若 correctness、QEMU smoke、board repeated、Doctor 和 registry 均通过，写为 `positive_row_source_generic_public_board_complete`，但只覆盖代表点型。
- 若 QEMU correctness 失败，暂停修复，不能跑 board。
- 若 board 为 weak / negative / unstable，写为 attempted / deferred，并保留 row-source `PointXYZ` adopted 结论。

## 继续 / 停止条件

完成后优先检查：

1. `row-source-locality-order-profile` 是否仍因 Phase 043/044 Doctor warning 成为未阻塞下一 phase。
2. 是否需要扩大更多 xyz AoS 点型。
3. 是否可以进入提交前审计。

若 Phase 044 positive 但仍有 locality warning，下一 phase 默认是 `row-source-locality-order-profile`，除非证据干净到可直接提交。
