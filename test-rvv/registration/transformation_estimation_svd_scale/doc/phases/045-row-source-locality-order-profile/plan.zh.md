# Phase 045 计划：row-source locality / order profile

## 阶段意图和边界

本阶段解释 Phase 043 / 044 board Evidence Doctor（证据体检）中的 long-tail / variance 和 group-outlier warning，判断 row-source public path 的收益差异是否主要来自 index / correspondence order（索引 / 对应关系顺序）、局部性或 run 顺序噪声。

验证范围：

| 维度 | 本阶段覆盖 |
| --- | --- |
| production entry | source-indexed、dual-indexed、correspondence 三类 `TransformationEstimationSVDScale` public overload |
| point type | 先覆盖 `PointXYZ -> PointXYZ` |
| `Scalar` | `float` |
| layout | dense xyz AoS |
| order patterns | contiguous、stride、reverse、deterministic shuffle |
| sizes | QEMU smoke 4K / 64K / 256K label；board repeated 同三档 |

不验证范围：

- 不修改 production 源码，不提出新 RVV family。
- 不把本阶段 profile 结果直接写成更广泛 production adoption；Phase 043 adoption 已独立成立。
- 不验证全部自定义点型、`Scalar=double`、non-dense、NaN / Inf 或非法 index / correspondence。
- 不在本阶段做排序 / staging mitigation；若 profile 显示 order 敏感，再单独建 mitigation phase。

## 当前状态清单

| area | 当前状态 |
| --- | --- |
| Phase 043 row-source patch | 已由用户确认采纳；`PointXYZ -> PointXYZ` row-source board 9 case 全 positive，Doctor `Errors=0`、`Warnings=7`。 |
| Phase 044 row-source generic | 代表点型 row-source board 9 case 全 positive，Doctor `Errors=0`、`Warnings=12`。 |
| 当前缺口 | warning 已解释为不阻塞 positive，但尚未拆分 index order / locality / run 顺序。 |
| evidence registry | 当前 `evidence_status` fresh；本阶段新增 QEMU / board profile 证据后必须登记。 |

## 假设与候选族

本阶段不是生产替代实现，而是 profile candidate（剖析候选）：

- 若 contiguous 与 stride 接近，而 reverse / shuffle 波动大，说明缓存局部性或预取方向可能主导 warning。
- 若所有 order pattern 都 positive 且 variance 相近，warning 更可能是板卡运行噪声或 summary 阈值敏感。
- 若某个 row source / size 在 shuffle 下明显退化，则后续 mitigation phase 可评估 index staging、order-preserving chunk、或文档中收窄“任意 index 分布”的表述。

## 优化矩阵

| candidate family | row source policy | order pattern / point type / Scalar / layout | correctness | bench / evidence | decision rule |
| --- | --- | --- | --- | --- | --- |
| `row-source-locality-order-profile` | source-indexed | contiguous / stride / reverse / shuffle；`PointXYZ -> PointXYZ` / `float` / dense | existing row-source correctness + QEMU smoke `max_reference_error` | board repeated + doctor | 全部 positive 则写成 profile_clear_positive；出现 weak / negative / unstable 则只降级对应 order pattern。 |
| `row-source-locality-order-profile` | dual-indexed | 同上，source / target 使用同 order family 的 pair | 同上 | 同上 | 同上。 |
| `row-source-locality-order-profile` | correspondence | 同上，query / match 使用同 order family 生成 correspondence | 同上 | 同上 | 同上。 |

## 实现和测试动作

| action | artifact / command | 完成判据 |
| --- | --- | --- |
| A1 plan freeze | 本文件 | phase 范围、order patterns、证据和停止条件冻结。 |
| A2 fixture 扩展 | `include/impl/tesvd_scale_support.hpp` 新增 reverse / shuffle index helper。 | helper 只生成合法 index，不改变 production。 |
| A3 bench 扩展 | `src/bench_tesvd_scale.cpp` 新增 `row-source-locality-order-profile` case-filter。 | QEMU smoke 输出 36 个 label：3 row source x 4 order x 3 size。 |
| A4 target / manifest 接线 | `Makefile` 和 topic-local summary / manifest wrapper。 | QEMU / board / doctor / registry target 可运行并登记。 |
| A5 QEMU 证据 | `make -C ... record_qemu_row_source_locality_order_profile_state` | QEMU Doctor clean 或 warning 已解释；只看 log shape 和误差。 |
| A6 board 证据 | `make -C ... run_board_bench_row_source_locality_order_profile_repeated` | 5-run summary / doctor / registry 完成，按 row source / order / size 报告。 |
| A7 文档同步 | result、matrix、roadmap、testing overview、benchmark/evidence、evaluation | 解释 warning 是否来自 order/locality；给出下一 phase。 |

## Evidence Doctor 和 Registry 规则

- QEMU smoke 只证明 build、case label、日志形状、`max_reference_error` 和 manifest 可解析，不作为性能结论。
- board repeated 沿用 5-run、20 iterations、5 warmup 的预算。
- Doctor warning 必须按 row source / order / size 解释；不能只给 overall average。
- registry 新增 profile QEMU 和 board 条目，doc-ref 指向本 plan/result、optimization matrix、roadmap 和 benchmark/evidence。

## 板卡复跑预算和决策桶

| 字段 | 值 |
| --- | --- |
| repeated runs | 5 |
| warmup / iterations | 5 / 20 |
| positive | 每个 case 所有 B/A > 1.20 |
| weak-positive | median >= 1.05 且 min >= 0.97 |
| neutral / negative / unstable | 沿用 summary script 口径 |
| 复跑策略 | 若 bucket 不变，不无限复跑；若出现 negative / unstable 或 Doctor error，暂停解释并降级对应 order pattern。 |

## Diagnostic-to-Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `production_public_row_source_profile`，使用真实生产公开 row-source overload，但目的只是解释已采纳路径的 order sensitivity（顺序敏感性）。 |
| A/B boundary | Std 构建父类标量公开入口 vs RVV 构建 scale 子类 row-source RVV 公开入口。 |
| 当前决策问题 | Phase 043 / 044 warning 是否暴露 order / locality 风险，以及是否需要后续 mitigation phase。 |
| diagnostic 是否可外推到 production | QEMU 不外推性能；board profile 只外推到本阶段 order pattern 和 `PointXYZ -> PointXYZ`。 |
| comparison-boundary / baseline mismatch 风险 | 有。order pattern 会改变标量父类和 RVV gather 路径两侧的缓存行为；结论必须按 pattern 分开报告。 |
| weak / negative / unstable 时是否允许 bounded production probe | Phase 043 已采纳不因单个 profile pattern 自动回滚；若 profile 出现 weak / negative / unstable，下一步是 mitigation 或范围说明。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 本阶段没有新 RVV family，不需要。若后续尝试 staging / sorting mitigation，则必须做同边界 RVV-vs-RVV A/B。 |

## 完成条件

- QEMU smoke、board repeated、Doctor 和 registry 均完成后，本阶段写 `result.zh.md`。
- 若全部 case 为 positive，且 warning 能按 order / row source 解释，decision 写 `profile_clear_positive` 或 `profile_positive_with_variance_warning`。
- 若某个 pattern 为 weak / negative / unstable，只降级该 pattern，不回推否定 Phase 043 adoption。

## 继续 / 停止条件

完成后默认检查：

1. 是否需要 `row-source-locality-mitigation`，例如 staging / sorting / chunk order 调整。
2. 是否可以扩大到 row-source generic point type locality profile。
3. 是否可以进入提交前审计。

若本阶段全部 positive 且无新的 unblocked mitigation，下一步可转入提交前审计；否则按证据新增下一 phase。
