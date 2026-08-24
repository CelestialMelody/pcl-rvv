# Phase 053 计划：row-source more-generic xyz AoS point types

## 阶段意图和边界

本阶段把 Phase 051 / 052 的更多常见 PCL xyz AoS（结构数组）点型证据，扩展到已采纳的 row-source public overload（按索引 / 对应关系公开入口）。本阶段不修改 production 源码，只验证现有 traits-gated `float` / dense xyz AoS gate 在更多具体 row-source 点型组合下的 correctness（正确性）、QEMU smoke（QEMU 冒烟）和 board repeated（板卡重复性能）。

验证范围：

| 维度 | 本阶段覆盖 |
| --- | --- |
| production entry | source-indexed、dual-indexed、correspondence 三类 `TransformationEstimationSVDScale` public overload |
| row source | source-indexed、dual-indexed、correspondence |
| point type | source-indexed `PointXYZRGBA -> PointXYZRGBA`；dual-indexed `PointNormal -> PointXYZRGB`；correspondence `PointWithViewpoint -> PointXYZ` |
| `Scalar` | `float` |
| layout | `RVVXYZAoSFloatLayout` traits-gated dense xyz AoS |
| sizes | correctness 4K selected pairs；QEMU / board 4K、64K、256K |
| production | 不扩大 dispatch、fallback、API 或 sorted-copy gate；只补现有公开入口证据。 |

不验证范围：

- Phase 051 的全部 5 个点型组合在每一种 row source 下的全交叉矩阵。
- 全部自定义 xyz AoS 点型、异常 padding / stride 点型。
- `Scalar=double`。
- non-dense、NaN / Inf、非法 index、越界 correspondence。
- 新的 locality mitigation（局部性缓解）或 row-source order profile；本阶段使用既有 stride indices。

## 当前状态清单

| area | 当前状态 | 路径 |
| --- | --- | --- |
| production row-source | Phase 043 已采纳 source-indexed、dual-indexed、correspondence public overload；Phase 047 另采纳 correspondence sorted-copy 窄 gate。 | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` |
| row-source representative generic | Phase 044 已覆盖 `PointXYZI` / `PointXYZRGB` 代表组合的 correctness、QEMU smoke、board repeated。 | `doc/phases/044-row-source-generic-xyz-point-type-expansion/result.zh.md` |
| more-generic ordered public | Phase 051 / 052 已覆盖 5 个常见 PCL xyz AoS ordered public 点型组合；不外推到 row source。 | `doc/phases/051-more-generic-xyz-aos-point-types/result.zh.md`、`doc/phases/052-more-generic-xyz-aos-board/result.zh.md` |
| current correctness | Phase 052 后 Std/RVV 各 16 tests passed；本阶段预期新增 1 个 gtest，变为 17 tests。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| evidence registry | Phase 052 后 fresh；本阶段覆盖生成日志后必须重新登记 correctness、QEMU smoke 和 board repeated。 | `log/evidence_registry.json` |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| `row-source-more-generic-xyz-aos-point-types` | 现有 row-source helper 通过 `RVVXYZAoSFloatLayout<PointSource>` / `PointTarget` 读取 x/y/z，不依赖 `PointXYZI` / `PointXYZRGB` 的具体 stride。 | 更大的点型 stride 和混合 source/target 可能改变 gather 成本；代表组合不能外推到所有 PCL 或自定义点型。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | correctness | bench / evidence | board evidence | decision rule |
| --- | --- | --- | --- | --- | --- | --- |
| `row-source-more-generic-xyz-aos-point-types` | source-indexed | `PointXYZRGBA -> PointXYZRGBA` / `float` / dense xyz AoS | 新增 gtest 对 selected-cloud scalar reference | `row-source-more-generic-xyz-aos-point-types` QEMU smoke | 4K / 64K / 256K repeated | correctness passed、QEMU Doctor clean、board median positive 才写 positive。 |
| `row-source-more-generic-xyz-aos-point-types` | dual-indexed | `PointNormal -> PointXYZRGB` / `float` / dense xyz AoS | 同上 | 同上 | 同上 | 同上。 |
| `row-source-more-generic-xyz-aos-point-types` | correspondence | `PointWithViewpoint -> PointXYZ` / `float` / dense xyz AoS | 同上 | 同上 | 同上 | 同上。 |

## 实现和测试动作

| action | artifact / command | 完成判据 |
| --- | --- | --- |
| A1 correctness 扩展 | `src/test_tesvd_scale.cpp` 新增 `RowSourceMoreGenericXYZAoSPointTypesMatchReference` | Std/RVV gtest 通过；traits static_assert 命中。 |
| A2 bench 扩展 | `src/bench_tesvd_scale.cpp` 新增 `row-source-more-generic-xyz-aos-point-types` case-filter | QEMU smoke 输出 9 个 label，`max_reference_error <= 2e-3`。 |
| A3 manifest / target | `script/generate_tesvd_scale_qemu_evidence_manifest.py`、`Makefile`、board summary script | QEMU / board manifest 能识别本阶段边界和 label。 |
| A4 QEMU 证据 | `make -C ... run_test_compare`、`record_qemu_row_source_more_generic_state` | correctness、QEMU Doctor 和 registry fresh。 |
| A5 board 证据 | `run_board_bench_row_source_more_generic_xyz_aos_point_types_repeated` | 5-run summary / manifest / Doctor 完成。 |
| A6 文档同步 | result、matrix、roadmap、README、testing overview、correctness、benchmark/evidence、optimization evidence、test-support map、evaluation、doc-rvv 边界 | 记录已验证 / 未验证边界，不把本阶段外推成全部点型。 |

## Evidence Doctor 和 Registry

- QEMU smoke 只证明 build、label、checksum、manifest metadata 和 `max_reference_error`；QEMU timing 不作为性能结论。
- Board repeated 采用既有 5-run、20 iterations、5 warmup 预算。
- Doctor 若出现 warnings，按 row source / point type / size 分开解释；不能把 source-indexed 的收益外推到 correspondence。
- registry 必须登记 QEMU correctness、QEMU row-source more-generic smoke 和 board repeated。

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
| evidence role | `production_public_row_source_more_generic`，本阶段使用真实 production public row-source overload。 |
| A/B boundary | Std 构建父类标量公开入口 vs RVV 构建 scale 子类 row-source RVV 公开入口。 |
| 当前决策问题 | 已采纳 row-source traits-gated xyz AoS gate 是否有更多常见 PCL 点型组合的同边界 correctness / board support。 |
| diagnostic 是否可外推到 production | correctness 和 board 只支撑这 3 个 row-source / 点型组合；不外推到全部点型。 |
| comparison-boundary / baseline mismatch 风险 | 有。row source、点型 stride、source/target 混合和 size 成本不同，必须分 case 报告。 |
| weak / negative / unstable 时是否允许 bounded production probe | 当前已是 production public evidence；若弱 / 负 / 不稳定，只降级本阶段代表组合，不回推否定 Phase 043 / 044 adoption。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要；当前没有新 RVV family 选择问题。 |

## 完成条件

- `positive_row_source_more_generic_public_board_complete`：correctness、QEMU smoke、board repeated、Doctor 和 registry 均通过；只覆盖本阶段 3 个具体 row-source / 点型组合。
- `evidence-boundary-expanded`：correctness / QEMU smoke 通过但本轮未完成 board；不写性能结论，board 作为默认下一阶段。
- `attempted`：某个点型 traits gate 不满足或 correctness / board 失败，记录原因，不回推否定已采纳 row-source 主线。
- `turn_stop_deferred`：板卡不可用或外部依赖阻塞时，写清恢复命令和 stop condition。

## 继续 / 停止条件

本阶段完成后检查：

1. 是否还需要 Phase 051 全部 5 个点型组合在 row-source 下的扩展矩阵。
2. 是否需要自定义 xyz AoS / padding 采样计划。
3. `Scalar=double` 是否仍保持独立数值预算。

若 board repeated positive 且 Doctor 可解释，默认进入文档 / 提交审计；若仍有未阻塞扩展，写入 roadmap 而不声明 topic closeout。

## 文档更新清单

更新 `result.zh.md`、`optimization-matrix.zh.md`、`optimization-roadmap.zh.md`、`README.zh.md`、`doc/correctness-tests.zh.md`、`doc/benchmark-and-evidence.zh.md`、`doc/optimization-evidence.zh.md`、`doc/testing-overview.zh.md`、`doc/test-support-code-map.zh.md`、`doc/transformation_estimation_svd_scale-evaluation.zh.md` 和适用的长期 `doc-rvv` 边界说明。
