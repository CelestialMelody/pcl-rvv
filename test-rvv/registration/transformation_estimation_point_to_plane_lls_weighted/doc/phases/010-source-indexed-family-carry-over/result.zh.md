# Phase 010: source-indexed 实现族迁移审计结果

## Phase 030 更新说明

本文件记录的是 Phase 010 当时的执行事实。Phase 030 重新分析同一
`run_board_bench_source_indexed_family` 日志后，发现该日志为 `Warmup Iterations: 0`，
并新增了 component/full sink 与 solve-delta 异常规则。当前 doctor 结果已刷新为
`2 Errors / 30 Warnings / 26 Suggestions`。因此本文件中 `5 Errors / 16 Warnings / 1 Suggestion`
和 `attempted / rejected for replacement` 只能作为 historical diagnostic；不能把旧 no-warmup
single-run smoke 写成永久拒绝 `block-fused-abcd-ilp` 生产候选的证据。

## 实际执行范围

本阶段只在 `test-rvv` 里审计 source-indexed（源索引路径）是否需要尝试 full-cloud（全云顺序扫描）已采纳的 block-reduction（分块规约）/ A/B/C/N block groups / fused-abcd-ilp（融合公式与指令级并行源码形态）family。生产源码保持不变。

实际范围与计划一致：新增 PointNormal-first 的 source-indexed family candidate、gtest filter、bench case-filter、QEMU correctness、QEMU bench shape、单次板卡 smoke，以及诊断 manifest / Evidence Doctor（证据体检）边界。未做 production replacement、未做 source-indexed repeated family board、未做 source-indexed-specific asm attribution。

## 完成矩阵

| 动作 | 状态 | 证据 / 产物 | 结论 |
| --- | --- | --- | --- |
| B1 写 phase 010 计划 | done | `010-source-indexed-family-carry-over/plan.zh.md` | 计划先于本阶段 candidate / target 修改存在。 |
| B2 增加 source-indexed block/fused PointNormal candidate | done | `include/impl/teptplw_candidate_row_sources.hpp`、`teptplw_candidate_estimates.hpp` | 新 candidate 只在 test-rvv；source gather、target stride load、连续 weight、finite mask 和 A/B/C/N block-reduction 同边界。 |
| B3 增加同族 correctness target | done | `src/test_teptplw_row_sources.cpp`、`Makefile` 中 `run_test_source_indexed_family_compare` | std / RVV QEMU 均 3 tests passed。 |
| B4 增加 same-boundary bench target | done | `include/impl/teptplw_bench_cases.hpp`、`Makefile` 中 `run_bench_source_indexed_family_compare` / `run_board_bench_source_indexed_family` | label 区分 `staged-gather`、`block-baseline`、`block-fused-abcd-ilp` 和 component no-solve。 |
| B5 运行本地可得验证 | done | `log/qemu/run_test_source_indexed_family_*.log`、`log/qemu/analyze_bench_compare_source_indexed_family.log`、`log/board/run_board_bench_source_indexed_family/analyze_bench_compare.log` | correctness 通过；QEMU 只作 shape；板卡 smoke 混合，不能支撑 production replacement。 |
| B6 Evidence Doctor / manifest 边界 | done | `script/generate_teptplw_evidence_manifest.py --kind source-indexed-family-diagnostic`、`log/board/run_board_bench_source_indexed_family/evidence_manifest.json`、`evidence_doctor.md` | Phase 010 当时 doctor 为 5 Errors / 16 Warnings / 1 Suggestion；Phase 030 刷新后为 2 Errors / 30 Warnings / 26 Suggestions。本数据只能作为 historical diagnostic。 |
| B7 更新 phase result、README 和 topic docs | done | 本文件、`doc/phases/README.zh.md`、topic docs | 当前 production 仍采用 source-indexed staged-gather / compressed-tail；新 family 为 attempted / diagnostic。 |

## Correctness 与 bench 结果

QEMU correctness：

| target | 结果 | 证据 |
| --- | --- | --- |
| `run_test_source_indexed_family_std` | 3 tests passed | `log/qemu/run_test_source_indexed_family_std.log` |
| `run_test_source_indexed_family_rvv` | 3 tests passed | `log/qemu/run_test_source_indexed_family_rvv.log` |

QEMU bench shape（QEMU 计时不作为性能结论）：

| case | 65536 | 262144 | 边界 |
| --- | ---: | ---: | --- |
| `staged-gather` | `0.46x` | `0.46x` | 只说明 QEMU 可运行和日志形状。 |
| `block-baseline` | `0.33x` | `0.33x` | 不写性能结论。 |
| `block-fused-abcd-ilp` | `0.34x` | `0.34x` | 不写性能结论。 |

单次板卡 smoke：

| case | 65536 | 262144 | 结论边界 |
| --- | ---: | ---: | --- |
| `staged-gather` full estimate | `1.12x` | `1.00x` | current production family 的 test-rvv 对照，不替代 repeated production summary。 |
| `block-baseline` full estimate | `1.16x` | `1.02x` | 有弱正向信号，但靠近阈值且只有单次 smoke。 |
| `block-fused-abcd-ilp` full estimate | `1.06x` | `0.59x` | 262144 明显退化；不能进入 production replacement。 |
| `block-baseline` component no-solve | `0.93x` | `0.99x` | component 层不稳定。 |
| `block-fused-abcd-ilp` component no-solve | `0.57x` | `1.11x` | 方向冲突，需要 profile / repeated board 才能归因。 |

## Evidence Doctor 处理

| 证据 | doctor 结果 | 处理动作 |
| --- | --- | --- |
| source-indexed family single-run board diagnostic | Phase 030 刷新为 `2 Errors / 30 Warnings / 26 Suggestions` | Errors 来自 B/A 退化频率；Warnings 现在还包含 zero-warmup、component/full 关系和 solve-delta outlier。结论降级为 historical diagnostic，不作为 production evidence。 |
| source-indexed production repeated summary | `0 Errors / 7 Warnings / 6 Suggestions` | 保持 source-indexed current family adopted；Warnings 继续限制 clean-pass 表述，尤其 source-indexed-specific asm 和 binary identity。 |
| row-source diagnostic trigger | `0 Errors / 0 Warnings / 0 Suggestions` | 只作为 pre-production diagnostic；不替代 production direct。 |

`source-indexed-family` 新增的 manifest / doctor 是 summary-only evidence（摘要证据）。raw board logs 和 board 环境日志仍不进入默认提交边界。

## Optimization Matrix 更新

| candidate family | row source policy | point type / Scalar / layout | test | bench / board | asm | doctor | decision | next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| block-reduction + fused-abcd-ilp | full-cloud | 三类代表点型 / `Scalar=float` / f32 AoS gated | 已覆盖 | repeated board 正向 | 已有 production asm | `0E / 0W / 3S` | adopted | 保持生产边界，不外推。 |
| staged-gather / compressed-tail | source-indexed | 三类代表点型 / `Scalar=float` / valid source index | 已覆盖 production direct | repeated board 正向 | source-indexed-specific asm missing | `0E / 7W / 6S` | adopted for current family | 保持当前 production。 |
| block-baseline carry-over | source-indexed | `PointNormal` first / `Scalar=float` / source gather + target normal f32 AoS | `run_test_source_indexed_family_compare` 通过 | QEMU shape 0.33x；board smoke 1.16x / 1.02x | missing | source-indexed-family doctor has Errors / Warnings | attempted | 需要 repeated board + asm 才能继续；暂不替换 production。 |
| block-fused-abcd-ilp carry-over | source-indexed | 同上 | `run_test_source_indexed_family_compare` 通过 | QEMU shape 0.34x；board smoke 1.06x / 0.59x | missing | source-indexed-family doctor has Errors / Warnings | attempted / reopened diagnostic | 当前不替换 production；若要继续，先补带 warmup repeated board / asm，不能用旧 smoke 永久拒绝。 |
| block/fused family carry-over | dual-indices | `PointNormal` first / `Scalar=float` / 双 gather | missing | missing | missing | missing | planned | Phase 020 先补 test-rvv candidate / test / bench。 |
| block/fused family carry-over | correspondences | `PointNormal` first / `Scalar=float` / query/match/weight 展开 + 双 gather | missing | missing | missing | missing | planned | Phase 020 先补 test-rvv candidate / component ablation。 |

## Pre / Post Production 分界

`run_test_source_indexed_family_compare`、`run_bench_source_indexed_family_compare` 和 `run_board_bench_source_indexed_family` 属于 pre-production diagnostic（接入生产前诊断）。它们只能回答“full-cloud adopted family 是否值得在 source-indexed 上继续审计”。

`run_test_source_indices_compare`、`run_bench_production_source_indices` 和 `collect_board_production_source_indices_repeated` 属于 post-production direct（接入生产后真实路径证据）。当前 source-indexed production adopted 仍由这些证据支撑，而不是由本阶段新的 family smoke 支撑。

## Production Decision

本阶段不修改 production。source-indexed production 继续采用 staged-gather / compressed-tail family。block-baseline 和 block-fused-abcd-ilp 的 source-indexed carry-over 已完成 PointNormal-first correctness 与 board smoke 审计，但证据不支持替换 production：

- block-baseline 只有单次板卡弱正向，doctor 提示 low run count 和近阈值风险。
- block-fused-abcd-ilp 在 262144 full estimate 上 `0.59x`，并触发退化频率 Errors；Phase 030 后这只表示旧 no-warmup smoke 不支持替换 production，不表示永久拒绝该 family。
- 缺 source-indexed family repeated board、source-indexed-specific asm attribution 和 generic representative candidate。

## Continue / Stop Decision

Phase 010 完成。合法停止条件是：本阶段计划矩阵已闭合，source-indexed production replacement 需要新的 repeated board / asm / production 授权，且下一阶段已创建可恢复计划。

下一阶段默认入口：

```text
test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/doc/phases/020-dual-indices-correspondences-family-carry-over/plan.zh.md
```

Phase 020 应先在 test-rvv 中补 dual-indices / correspondences 的 block/fused family candidate、correctness、bench 和 board smoke / doctor 边界。不要直接 production。
