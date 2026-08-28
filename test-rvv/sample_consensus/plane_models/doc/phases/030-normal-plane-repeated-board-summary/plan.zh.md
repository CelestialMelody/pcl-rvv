# Normal-plane Repeated Board Summary Phase Plan

## 阶段意图和边界

本阶段只增强当前 protected helper hot path（受保护 helper 热点路径）的 board evidence（板卡证据）。目标是把 phase 000 的单次 `run_board_bench_compare` 结果扩展为 5-run repeated summary（重复摘要），并接入 manifest / Evidence Doctor / registry 刷新。

本阶段要证明：

- `run_board_bench_compare_repeated` 能在板卡上用同一 bench wrapper 连续采集 5 组 Std/RVV compare。
- `analyze_board_bench_compare_repeated` 能从 run-labelled logs 生成 `summary.md`。
- repeated manifest / Evidence Doctor / registry 能记录 5 个 B/A values（候选相对基线的加速值），并暴露长尾或低 run count 问题。

本阶段不证明：

- 完整公开入口计时；公开入口 dispatch / fallback 仍由 GTest 覆盖。
- 泛型点类型、`Scalar=double` 或新的 RVV 实现族选择。
- raw logs 默认可提交；仍采用 summary-only 策略。

## 当前状态清单

| 对象 | 当前事实 | 路径 |
| --- | --- | --- |
| 单次 board compare | 三项均为 positive bucket。 | `log/board/analyze_bench_compare.log` |
| Phase 020 registry | manifest / doctor / registry target 已关闭，`evidence_status` fresh。 | `doc/phases/020-normal-plane-evidence-registry-target-alias/result.zh.md` |
| repeated analyzer | 全局脚本已存在，可消费多个 `analyze_bench_compare.log`。 | `test-rvv/script/analyze_bench_repeated.py` |
| repeated target | 当前 topic 尚无 5-run board target。 | `Makefile` |

## 优化矩阵

| candidate family | scope | correctness target | bench / evidence target | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- |
| repeated board summary | protected helper hot path | not_applicable | `run_board_bench_compare_repeated`、`analyze_board_bench_compare_repeated` | repeated manifest / doctor | planned | 补 Makefile target 并跑板卡 |
| repeated evidence registry | summary artifact freshness | not_applicable | `record_repeated_board_evidence_state`、`repeated_evidence_status` | doctor output must be interpreted | planned | 登记 repeated summary / manifest / doctor |
| generic point-type expansion | production gate expansion | not in this phase | not in this phase | not_applicable | deferred | repeated summary 后另行判断 |

## 实现和测试动作

| action | 产物 | 命令 / 证据 | 完成判据 |
| --- | --- | --- | --- |
| A1 phase plan | 本文件 | path-limited `git status` | plan 先于 Makefile / script 修改存在。 |
| A2 board repeated targets | `Makefile` | `make -C test-rvv/sample_consensus/plane_models run_board_bench_compare_repeated` | 生成 5 个 run-labelled board compare 目录并抓回本机。 |
| A3 repeated summary | `log/board/normal-plane-phase030-repeated-board/summary.md` | `make -C test-rvv/sample_consensus/plane_models analyze_board_bench_compare_repeated` | summary 包含三条 helper 的 runs=5、median/min/max/p10/p90。 |
| A4 repeated manifest / doctor | script / Makefile | `make -C test-rvv/sample_consensus/plane_models run_repeated_board_evidence_doctor` | doctor 输出必须无 Error；Warning 需解释或降级。 |
| A5 registry | `log/evidence_registry.json` | `make -C test-rvv/sample_consensus/plane_models record_repeated_board_evidence_state`、`make ... repeated_evidence_status` | registry check 输出 fresh。 |
| A6 docs sync | README、testing / benchmark / optimization docs、phase result、matrix、roadmap、evaluation、doc-rvv、queue | stale scan、`git diff --check` | 文档引用 run label、summary、doctor 和 registry 状态一致。 |

## Evidence Doctor 和 Registry 规则

repeated evidence 使用 run-labelled 目录：

```text
log/board/normal-plane-phase030-repeated-board/run_01/analyze_bench_compare.log
...
log/board/normal-plane-phase030-repeated-board/run_05/analyze_bench_compare.log
log/board/normal-plane-phase030-repeated-board/summary.md
log/board/normal-plane-phase030-repeated-board/evidence-manifest.json
log/board/normal-plane-phase030-repeated-board/evidence-doctor.md
log/board/normal-plane-phase030-repeated-board/evidence-doctor.json
```

registry 只登记 repeated summary、manifest 和 doctor summary artifact，不默认登记或提交 raw `run_bench_*.log`。若 Evidence Doctor 报告 long tail（长尾）或 run-to-run fluctuation（运行间波动），phase result 必须解释是否影响 positive bucket。

## 板卡复跑预算和决策桶

有界复跑预算：5 runs，每个 run 沿用 bench binary 的 50 iterations。若三条 helper 的 repeated median speedup 均大于 1.2x，且 min speedup 均大于 1.0x，本阶段性能 bucket 记为 `positive-stable`。若 median 大于 1.2x 但存在单次小于等于 1.0x，记为 `weak-positive` 并需要解释。若任一 helper median 在 0.95x 到 1.2x 之间，记为 `neutral`。若任一 helper median 小于 0.95x，记为 `negative`。若目标失败、日志不完整或结果方向摇摆且预算耗尽，记为 `unstable` 或 `blocked`。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic；bench 调用 protected helper hot path。 |
| A/B boundary | Std/RVV protected helper bench wrapper，5-run repeated board summary。 |
| 当前决策问题 | 稳定性增强；验证 phase 000 positive bucket 是否经 repeated board 仍成立。 |
| diagnostic 是否可外推到 production | 不直接外推。它增强 helper hot path 性能证据；公开入口 dispatch / fallback 仍由 GTest 证明。 |
| comparison-boundary / baseline mismatch 风险 | 每个 run 使用同一 board target、同一 PCD、同一 iterations；run-labelled output 防止覆盖旧日志。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前已有 production patch；若 repeated bucket 弱或负，先降级本阶段性能证据并等待用户判断是否继续或回滚，不自动回滚。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 不需要；本阶段未比较新 RVV family。 |

## 继续 / 停止条件

继续条件：repeated target、summary、manifest、doctor、registry 或文档同步任一项未关闭且没有真实工具 / 板卡阻塞。停止条件：板卡不可达、repeated budget 耗尽且 bucket 不稳定、Evidence Doctor Error 无法修复、registry 显示未登记变化无法归属、dirty isolation 不安全，或继续需要扩大到泛型点类型 / production gate 变更。

默认下一 phase：若 repeated summary positive-stable，则进入 `ready_for_review_validity_check`，再判断是否需要点类型扩展或停在用户确认边界。
