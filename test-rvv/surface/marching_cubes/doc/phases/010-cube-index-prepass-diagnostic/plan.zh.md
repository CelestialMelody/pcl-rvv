# Phase 010 计划：cube-index prepass diagnostic

## 阶段意图和边界

本阶段继续不改 production 源码，验证 `cube-index-prepass`（批量 cube index 预扫描）是否比 Phase 000 的 edge interpolation 更有价值。候选会沿 `(x, y)` 固定后批量扫描 `z` 方向 cell，判断 8 个 leaf value 是否含 NaN、cube index 是否 active，并把 active cell 再交给标量 `emitSurfaceStd` 输出三角形。

| scope item | 本阶段范围 |
| --- | --- |
| evidence role | `production-shaped diagnostic`。 |
| A/B boundary | `test helper`：Std 是直接 scan + scalar emit；RVV 是 prepass + scalar emit。 |
| 当前决策问题 | `implementation-shape` 和 `RVV-vs-scalar`：批量 active-cell 判断是否值得继续。 |
| point type / Scalar | `PointNormal` / `float`。 |
| layout | dense contiguous grid，按 production grid index 线性化。 |
| 不覆盖范围 | production dispatch、Hoppe/RBF full public path、泛型点类型。 |

## 当前状态清单

| item | 状态 | evidence |
| --- | --- | --- |
| Phase 000 | attempted / neutral-to-weak-positive，不能进 production | `doc/phases/000-current-state-and-edge-interpolation-diagnostic/result.zh.md` |
| QEMU correctness harness | 可复用 | `run_test_compare` |
| board harness | 可用 | `board_smoke` 已完成两次 |
| Evidence Doctor manifest | 可复用并扩展 | `script/generate_marching_cubes_board_summary.py` |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| cube-index-prepass | dense-grid-cell | `PointNormal` / `float` / contiguous grid | `runPrepassCandidate` test helper | `run_test_compare` | board `run_bench_compare` | planned bounded board run | `dump_bench_rvv` | manifest + doctor | planned | implement, test, board |

## 实现和测试动作

| action | artifact / command | completion criteria |
| --- | --- | --- |
| helper | add `runPrepassCandidate` and RVV active-mask scan | RVV build compiles; Std build falls back safely. |
| correctness | extend gtest with prepass vs reference cases | counts/checksum match reference. |
| bench | add `--mode prepass` or case labels | board logs distinguish edge vs prepass. |
| asm | `dump_bench_rvv` | RVV compare/mask instructions visible. |
| board | one board smoke; if near threshold, one bounded rerun | summary and doctor explain bucket. |
| docs | result, matrix, roadmap, evaluation | decision does not exceed diagnostic evidence. |

## Evidence Doctor 和复跑预算

沿用 Phase 000：先一次 board smoke；若 median 在 `0.97x..1.10x` 或 doctor warning 影响判断，最多再跑一次同边界。`positive >= 1.10x`、`weak_positive 1.03x..1.10x`、`neutral 0.97x..1.03x`、`negative < 0.97x`。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production-shaped diagnostic`。 |
| A/B boundary | `test helper`。 |
| 当前决策问题 | `implementation-shape` / `RVV-vs-scalar`。 |
| diagnostic 是否可外推到 production | 仍为 unknown；prepass 改变 helper 组织方式，但不证明真实 public path。 |
| comparison-boundary / baseline mismatch 风险 | A/B 都输出相同三角点 checksum；候选会增加 active list staging，timer boundary 必须包含这部分成本。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 只有稳定 positive 且输出 / asm / doctor 闭合才建议 PI1；弱信号先做 full-public component audit。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 是。Phase 010 仍不能 clean adopt。 |

## 继续 / 停止条件

默认推进到 correctness、asm、board 和 Evidence Doctor。只有实现编译失败且同轮无法修、板卡不可达、doctor Error 暴露不可修证据合同，或结果需要用户确认进入 production integration loop 时停止。
