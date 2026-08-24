# Phase 054 计划：row-source all more-generic xyz AoS matrix

## 阶段意图和边界

本阶段补齐 Phase 051 的 5 个常见 PCL xyz AoS（结构数组）点型组合在三类 row-source public overload（公开入口）下的全交叉代表矩阵。Phase 053 已证明 3 个 row-source more-generic 代表组合，本阶段继续把同一 evidence boundary（证据边界）扩到 5 × 3 组合。

本阶段不修改 production 源码，不扩大 dispatch（分流）、fallback（回退）、correspondence sorted-copy gate（对应关系排序副本门控）或 public API。验证范围只包括 `Scalar=float`、dense、traits-gated xyz AoS 和确定性 stride indices / correspondences。

| 维度 | 本阶段覆盖 |
| --- | --- |
| production entry | source-indexed、dual-indexed、correspondence |
| point type combos | `PointXYZRGBA -> PointXYZRGBA`、`PointXYZL -> PointXYZ`、`PointNormal -> PointXYZRGB`、`PointWithRange -> PointWithRange`、`PointWithViewpoint -> PointXYZ` |
| row source | 每个点型组合分别跑 source-indexed、dual-indexed、correspondence |
| sizes | correctness 使用 4K selected pairs；QEMU / board 使用 4K、64K、256K |
| 不覆盖 | 自定义点型、异常 padding / stride、`Scalar=double`、非法 index / correspondence、NaN / Inf、新 locality mitigation |

## 当前状态清单

| area | 当前状态 | 路径 |
| --- | --- | --- |
| Phase 051/052 | 5 个 ordered more-generic 组合的 correctness / QEMU / board 已闭合。 | `doc/phases/051-more-generic-xyz-aos-point-types/result.zh.md`、`doc/phases/052-more-generic-xyz-aos-board/result.zh.md` |
| Phase 053 | 3 个 row-source more-generic 代表组合的 correctness / QEMU / board 已闭合，9 个 board case 全 positive。 | `doc/phases/053-row-source-more-generic-xyz-aos-point-types/result.zh.md` |
| 当前 correctness | Std/RVV 各 17 tests passed；本阶段预期新增 1 个 gtest，变为 18 tests。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| evidence registry | Phase 053 后 fresh；本阶段生成新 QEMU / board evidence 后必须重新登记。 | `log/evidence_registry.json` |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| `row-source-all-more-generic-xyz-aos-matrix` | 已采纳 row-source RVV path 只依赖 xyz AoS layout traits；Phase 051 的 5 个 common PCL 点型组合应在三类 row source 下保持 correctness 和 positive board bucket。 | 点型 stride、source/target 混合、row source gather 和 size 会产生不同收益；board warning 必须按组合分开解释，不能归纳成全部点型。 |

## 优化矩阵

| row source | point type / Scalar / layout | correctness | QEMU / board | decision rule |
| --- | --- | --- | --- | --- |
| source-indexed | Phase 051 五个组合 / `float` / dense xyz AoS | 新增 all-more-generic gtest | 15 source-indexed size cases 中对应 5 × 3 | 全部 correctness passed、QEMU Doctor clean、board median positive 才写 positive。 |
| dual-indexed | 同上 | 同上 | 15 dual-indexed size cases | 同上。 |
| correspondence | 同上 | 同上 | 15 correspondence size cases | 同上。 |

## 实现和测试动作

| action | artifact / command | 完成判据 |
| --- | --- | --- |
| A1 correctness 扩展 | `src/test_tesvd_scale.cpp` 新增 `RowSourceAllMoreGenericXYZAoSPointTypesMatchReference` | Std/RVV 18 tests 通过；每个组合覆盖 source-indexed、dual-indexed、correspondence。 |
| A2 bench 扩展 | `src/bench_tesvd_scale.cpp` 新增 `row-source-all-more-generic-xyz-aos-matrix` case-filter | QEMU smoke 输出 45 个 label，`max_reference_error <= 2e-3`。 |
| A3 manifest / targets | Makefile 和 summary / manifest script 支持 Phase 054 label、run-label、case-filter 和 evidence role | QEMU / board manifest 能解析 45 个比较并登记。 |
| A4 QEMU 证据 | `run_test_compare`、`record_qemu_row_source_all_more_generic_state` | correctness、QEMU Doctor、registry fresh。 |
| A5 board 证据 | `run_board_bench_row_source_all_more_generic_xyz_aos_matrix_repeated` | 5-run summary / manifest / Doctor 完成。 |
| A6 文档同步 | result、matrix、roadmap、README、testing overview、benchmark/evidence、optimization evidence、code map、evaluation、doc-rvv 边界 | 记录已验证 / 未验证边界，不写成全部自定义点型。 |

## Evidence Doctor 和 Registry

- QEMU 只证明 build、label、manifest shape 和 `max_reference_error`，不作为性能结论。
- Board repeated 采用 5 runs、20 iterations、5 warmup。
- Doctor warning 按 row source / point type / size 分开解释。
- registry 必须登记 correctness、QEMU row-source all-more-generic smoke 和 board repeated。

## 板卡复跑预算和决策桶

| 字段 | 值 |
| --- | --- |
| repeated runs | 5 |
| warmup / iterations | 5 / 20 |
| positive | 每个 case 所有 B/A > 1.20 |
| weak-positive | median >= 1.05 且 min >= 0.97 |
| neutral / negative / unstable | 沿用 summary script 口径 |
| 复跑策略 | 若 bucket 不变，不无限复跑；若出现 negative / unstable 或 Doctor error，暂停解释。 |

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production_public_row_source_all_more_generic`，真实 production public row-source overload。 |
| A/B boundary | Std 构建父类标量公开入口 vs RVV 构建 scale 子类 row-source RVV 公开入口。 |
| 当前决策问题 | Phase 051 的 5 个 more-generic 点型组合是否在 source-indexed、dual-indexed 和 correspondence 下都有同边界 correctness / board support。 |
| diagnostic 是否可外推到 production | 可外推到这 15 个具体 row-source / 点型组合；不可外推到全部自定义点型。 |
| comparison-boundary / baseline mismatch 风险 | 有。row source、点型 stride、source/target 混合和 size 成本不同，必须分 case 报告。 |
| weak / negative / unstable 时是否允许 bounded production probe | 当前已是 production public evidence；若弱 / 负 / 不稳定，只降级对应组合，不回推否定 Phase 043 / 044 / 053 adoption。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要；本阶段没有新 RVV family 选择问题。 |

## 完成条件

- `positive_row_source_all_more_generic_public_board_complete`：correctness、QEMU smoke、board repeated、Doctor 和 registry 均通过；只覆盖本阶段 15 个 row-source / 点型组合。
- `evidence-boundary-expanded`：correctness / QEMU 通过但 board 未完成；不写性能结论。
- `attempted`：某个组合 correctness / board 失败，记录原因，不回推否定已采纳主线。
- `turn_stop_deferred`：板卡或工具不可用时写清恢复命令和 stop condition。

## 继续 / 停止条件

本阶段完成后检查：

1. 是否仍需要自定义 xyz AoS / padding 采样计划。
2. `Scalar=double` 是否仍保持独立数值预算。
3. 是否有新的 locality / order mitigation idea 来自 board warning。

若 board repeated positive 且 Doctor 可解释，默认进入文档 / 提交审计；若出现新的未阻塞候选，写入 roadmap 和下一 phase。
