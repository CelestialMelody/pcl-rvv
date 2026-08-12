# Phase 030 Result：source-indexed bench evidence calibration

## 阶段摘要

本阶段把 source-indexed-family 的 bench 证据口径重新校准到“有 warm-up、sink-aware、可复跑”的状态，并完成
5-run board repeated summary（重复板卡汇总）和 Evidence Doctor（证据体检）审计。

当前已经补齐：

- source-indexed-family 的 dedicated repeated board target。
- bench / board 文档中对 warm-up、sink 和历史 no-warmup smoke 的边界说明。
- source-indexed-family 旧 single-run board smoke 的 historical diagnostic 降级说明。
- 新的 `log/board/source_indexed_family_repeated/summary.md`、`evidence_manifest.json`、`evidence_doctor.md` 和
  `evidence_doctor.json`。

本阶段结论：

- `block-fused-abcd-ilp` full estimate（完整估计）的 median 为 `0.90x`，5-run 中 `4/5` 低于 `1.0x`。
- Evidence Doctor 给出 `3 Errors / 6 Warnings / 13 Suggestions`，其中 `block-fused-abcd-ilp` 的 full estimate
  和 component no-solve（组件级不求解）都有高频退化信号。
- 当前 evidence bucket（证据决策桶）是 `negative_with_variance`：不支持把 source-indexed
  `block-fused-abcd-ilp` 推进 production-candidate investigation。旧 no-warmup smoke 仍只是历史诊断；
  新 repeated summary 才是当前 Phase 030 truth。

## 计划 vs 实际

计划范围已经闭合：

1. 验证入口、文档和脚本对齐完成。
2. dedicated repeated board run 在 Milkv-Jupiter 上完成，默认参数为 `size=262144`、`runs=5`、`iterations=20`、
   `warmup=5`。
3. summary 生成后运行 `doctor_board_source_indexed_family_repeated`，manifest 和 Evidence Doctor 输出已生成。
4. `block-fused-abcd-ilp` 没有进入 production-candidate；当前 production adopted 状态保持不变。

## 完成矩阵

| 动作 | 状态 | 证据 / 产物 | 结论 |
| --- | --- | --- | --- |
| C1 补 agent asset 规则 | done | `.agents/skills/rvv-test/SKILL.md`、`performance-and-ablation.zh.md`、`evidence-doctor.zh.md` 已具备 warm-up、QEMU bench 边界和 mixed-sink 规则。 | bench 数值默认带 warm-up，QEMU bench 只作 smoke。 |
| C2 补 bench harness / analyzer / Makefile 对齐 | done | `include/impl/teptplw_bench_harness.hpp`、`include/impl/teptplw_bench_cases.hpp`、`Makefile`、`script/analyze_bench_compare.py`。 | filtered bench / board target 已传 `--warmup-iterations 5`；source-indexed full estimate sink-aware。 |
| C3 刷新 source-indexed-family diagnostic 边界 | done | `log/board/run_board_bench_source_indexed_family/analyze_bench_compare.log`、`evidence_doctor.md`。 | 旧 doctor 为 `2E / 30W / 26S`，旧 no-warmup smoke 只能作 historical diagnostic。 |
| C4 增加 dedicated repeated board target | done | `collect_board_source_indexed_family_repeated`、`log/board/source_indexed_family_repeated/summary.md`。 | 5-run repeated summary 已生成；raw compare logs 默认临时清理。 |
| C5 更新 topic docs | done | `README.zh.md`、`doc/testing-overview.zh.md`、`doc/benchmark-and-evidence.zh.md`、`doc/optimization-evidence.zh.md`、`doc/transformation_estimation_point_to_plane_lls_weighted-evaluation.zh.md`。 | 文档已改成真实 repeated summary / doctor 结论。 |
| C6 写 Phase 030 result 和刷新 phase README | done | `doc/phases/030-source-indexed-bench-evidence-calibration/result.zh.md`、`doc/phases/README.zh.md`。 | 恢复入口记录 Phase 030 已闭合；不默认进入 production-candidate。 |
| C7 刷新 current handoff | done | `tmp/rvv-work-logs/registration/transformation_estimation_point_to_plane_lls_weighted/current-handoff/*` | handoff 记录 summary、doctor、dirty isolation 和下一步边界。 |
| C8 补 repeated summary manifest / doctor wrapper | done | `script/generate_teptplw_evidence_manifest.py --kind source-indexed-family-repeated`、`doctor_board_source_indexed_family_repeated`。 | 已生成 `evidence_manifest.json`、`evidence_doctor.md`、`evidence_doctor.json`。 |
| C9 证据 allowlist | done | `test-rvv/.gitignore`；topic-local `.gitignore` 本机辅助 | source-indexed-family repeated summary / manifest / doctor 作为 summary evidence（摘要证据）进入可审查边界；raw logs 仍默认忽略。 |

## Optimization Matrix

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| block-reduction / A/B/C/N / fused-abcd-ilp | full-cloud | representative point types / `float` / contiguous weights | production full-cloud public overload | `run_test_production_direct_compare`、`run_test_candidates_compare` | `collect_board_production_dispatch_repeated` | adopted repeated board | production-default asm exists | `0E / 0W / 3S` | adopted | none |
| staged-gather / compressed-tail | source-indexed | representative point types / `float` / source indices + contiguous weights | production source-indexed public overload | `run_test_source_indices_compare` | `collect_board_production_source_indices_repeated` | adopted repeated board | source-indexed-specific asm missing warning | `0E / 7W / 6S` | adopted with warnings | source-indexed-specific asm deferred |
| staged-gather / block-baseline / block-fused-abcd-ilp | source-indexed | `PointNormal` / `float` / test-rvv family diagnostic | `run_bench_source_indexed_family`、`run_board_bench_source_indexed_family` | `run_test_source_indexed_family_compare` | `collect_board_source_indexed_family_repeated` | `source_indexed_family_repeated/summary.md`：staged median `1.04x`、block-baseline median `1.05x`、block-fused median `0.90x` | missing / deferred | `source_indexed_family_repeated/evidence_doctor.md`：`3E / 6W / 13S` | block-fused no-production；baseline/staged remain diagnostic only | no production-candidate by default |
| staged/block/fused family | dual-indices | `PointNormal` / `float` / dual source-target indices | test-rvv diagnostic only | `run_test_dual_correspondence_family_compare` | `run_board_bench_dual_correspondence_family` | diagnostic negative | missing | `20E / 21W / 0S` | no-production | none |
| staged/block/fused family | correspondences | `PointNormal` / `float` / correspondences query/match | test-rvv diagnostic only | `run_test_dual_correspondence_family_compare` | `run_board_bench_dual_correspondence_family` | diagnostic negative | missing | `20E / 21W / 0S` | no-production | none |

## Evidence Doctor / Manifest

历史 source-indexed-family diagnostic 仍保留旧日志：

- `log/board/run_board_bench_source_indexed_family/analyze_bench_compare.log`
- `log/board/run_board_bench_source_indexed_family/evidence_manifest.json`
- `log/board/run_board_bench_source_indexed_family/evidence_doctor.md`

旧 doctor 结果为 `2 Errors / 30 Warnings / 26 Suggestions`。这些 Errors 来自 `ba_degradation_frequency`，
Warnings 主要来自 zero-warmup、component/full sink 不一致和 solve-delta 离群。它们共同说明：旧单次
smoke 不能再当 current truth。

新的 `collect_board_source_indexed_family_repeated` 已产出当前 summary：

- `log/board/source_indexed_family_repeated/summary.md`
- `log/board/source_indexed_family_repeated/evidence_manifest.json`
- `log/board/source_indexed_family_repeated/evidence_doctor.md`
- `log/board/source_indexed_family_repeated/evidence_doctor.json`

新 doctor 结果为 `3 Errors / 6 Warnings / 13 Suggestions`。关键异常：

- `block-fused-abcd-ilp` full estimate：values `0.83x, 0.93x, 0.90x, 1.20x, 0.76x`，median `0.90x`，
  `4/5` 低于 `1.0x`。
- `block-fused-abcd-ilp` component no-solve：values `1.18x, 0.98x, 0.93x, 1.03x, 0.85x`，median `0.98x`，
  `3/5` 低于 `1.0x`。
- staged-gather diagnostic：median `1.04x`，但 `2/5` 低于 `1.0x`，且 near-threshold。
- block-baseline diagnostic：median `1.05x`，但 min/max 为 `0.92x / 1.38x`，长尾明显。

这些 wrapper 仍是 pre-production diagnostic 边界。它们可以关闭本阶段的 source-indexed family
calibration，但不能替代 production direct、fallback、source-indexed-specific asm 或 production bench。

## Validation

本轮实际跑过的验证：

| 检查 | 状态 | 证据 |
| --- | --- | --- |
| `py_compile` | pass | `python3 -m py_compile` 覆盖 `analyze_bench_compare.py`、`evidence_doctor.py`、`collect_teptplw_board_compare_repeated.py`、`generate_teptplw_evidence_manifest.py`。 |
| `run_bench_source_indexed_family_compare -n` | pass | 展示 `--warmup-iterations 5` 已进入 compare target。 |
| `run_board_bench_source_indexed_family -n` | pass | 展示板卡 smoke 默认 `--warmup-iterations 5`。 |
| `collect_board_source_indexed_family_repeated -n` | pass | 展示 dedicated repeated board target 使用 `source-indexed-family`、`size=262144`、`runs=5`、`iterations=20`、`warmup=5`。 |
| `doctor_board_source_indexed_family_repeated -n` | pass | 展示 summary 存在后会先生成 `source-indexed-family-repeated` manifest，再跑 Evidence Doctor 输出 markdown/json。 |
| synthetic repeated summary wrapper smoke | pass | 临时 sample summary 能通过 manifest 生成器和 Evidence Doctor；未写入 repo。 |
| `collect_board_source_indexed_family_repeated` | pass | Milkv-Jupiter 上生成 `log/board/source_indexed_family_repeated/summary.md`；5 runs、20 iterations、5 warm-up。 |
| `doctor_board_source_indexed_family_repeated` | pass | 生成 `evidence_manifest.json`、`evidence_doctor.md`、`evidence_doctor.json`；doctor 结果 `3E / 6W / 13S`。 |

## Current Decision

当前结论是：

```text
source-indexed bench evidence calibration complete; current repeated board diagnostic does not support block-fused-abcd-ilp production-candidate promotion
```

这不是 production replacement 结论。当前 production source-indexed adopted family 仍是 staged-gather /
compressed-tail；`block-fused-abcd-ilp` 在这次 source-indexed family diagnostic 中表现为
`negative_with_variance`，不应进入新的 production-candidate phase。该结论只覆盖当前 `PointNormal`、
`float`、`source-indexed-family`、`262144` 点和当前 5-run board 口径；不写成对所有未来 source-indexed
formula variant 的永久拒绝。

## Next Step

默认不再进入 source-indexed `block-fused-abcd-ilp` production-candidate。若用户要继续做 root-cause
analysis（根因分析），建议另开后续 phase，先补：

- 20-run / 50-run repeated board，带 `taskset`、governor、freq、temperature 和 binary hash。
- `--raw-dir` 保留 raw compare logs，用于逐轮 trace 和 run 顺序分析。
- source-indexed-specific asm attribution（反汇编归因）。
- 若仍要比较 component 与 full estimate，先把 checksum sink 对齐，或继续把它写成 mixed-sink component diagnostic。
