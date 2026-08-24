# Phase 065: Scalar Double Production Probe Plan

## 阶段意图和边界

本阶段把 Phase 063 / 064 的 `Scalar=double` RVV f64 widened diagnostic（双精度 RVV 扩宽诊断）推进到有界 production probe（生产探针）。范围只覆盖 ordered-cloud-pair（顺序点云对）公开入口、`PointXYZ -> PointXYZ`、`Scalar=double`、dense 输入和当前 64K board case。它不证明 row-source double、泛型 xyz AoS double、自定义 layout double、非法输入、非 dense 输入或全部规模。

PI5 之后必须停在用户检查点。即使 production public evidence 为 positive，也只能进入 `pending_user_confirmation_adopt_production`，不能自动写成 adopted production behavior。

## 当前状态清单

| area | current state | evidence / path |
| --- | --- | --- |
| diagnostic correctness | Phase 063 已通过 `ScalarDoubleAccumulationScoutMatchesPublicFallback` 和 `ScalarDoubleRVVWidenedScoutMatchesScalarDoubleScout`；Std/RVV 25 tests passed。 | `doc/phases/063-scalar-double-diagnostic-scout/result.zh.md` |
| board diagnostic | Phase 064 5-run B/A `2.806, 2.851, 2.867, 2.868, 2.863`，median `2.863x`，Doctor `0/0/0`。 | `doc/phases/064-scalar-double-board-scout/result.zh.md` |
| production state | 当前 production ordered RVV gate 只支持 `Scalar=float`；`Scalar=double` fallback 到父类。 | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` |
| test / bench support | 已有 test-only double helper 和 `scalar-double-diagnostic-scout` case-filter；缺少 production public double probe label、manifest / Doctor / registry wrapper。 | `src/test_tesvd_scale.cpp`、`src/bench_tesvd_scale.cpp`、`Makefile` |

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | Phase 063/064 是 diagnostic；本阶段目标是 production-public probe。 |
| A/B boundary | 从 test helper A/B 迁移到 public ordered overload。 |
| 当前决策问题 | `RVV-vs-scalar`：公开 double ordered path 是否值得有界接入。 |
| diagnostic 是否可外推到 production | 不能直接外推；本阶段必须补真实 dispatch、fallback、QEMU smoke、ASM 和 board repeated。 |
| comparison-boundary / baseline mismatch 风险 | 有。diagnostic candidate 绕过 public dispatch；production probe 必须计入 `Scalar=double` gate、dense / size fallback 和真实 public estimator。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | Phase 064 为 positive，所以允许。若 production board 为 weak / negative / unstable，则停在 PI5，等待用户决定保留补测或回滚。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 不需要。当前 double production 没有已 adopted RVV family，本阶段回答 public RVV vs public scalar。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `scalar-double-production-probe` | ordered-cloud-pair | `PointXYZ -> PointXYZ` / `double` / dense | 真实 public ordered overload；RVV 构建尝试 f64 widened accumulation，Std 构建 fallback | 新增 production-facing gtest；`run_test_compare` | 新增 `scalar-double-production-probe` QEMU smoke / board repeated | 5-run repeated，positive / weak / neutral / negative / unstable bucket | `dump_bench_rvv` 输入；确认 f64 vector instruction 出现在 RVV binary | QEMU + board Doctor 必须 `Errors=0` 才能进入 PI5 positive | pending |

## 实现和测试动作

| id | action | done criteria |
| --- | --- | --- |
| PI1 | 冻结生产接入范围：exact `PointXYZ -> PointXYZ`、`Scalar=double`、dense、ordered only。 | plan 写清 fallback 和不可扩大范围。 |
| PI2 | 在 production detail 中新增 double accumulation / solve helper，并让 ordered public overload 在窄 gate 下尝试 RVV。 | 非 RVV 构建自然保持父类 fallback；float 既有路径不变。 |
| PI3 | 新增 production-facing double gtest 和 bench case-filter。 | `run_test_compare` Std/RVV 通过；bench label 输出 `max_public_error` 和 public path。 |
| PI4 | 新增 QEMU / board manifest、Doctor 和 registry targets；运行 QEMU correctness、QEMU smoke、board repeated。 | registry fresh；Doctor Errors 为 0。 |
| PI5 | 写 result，展示 production diff、证据和用户检查点。 | `continue_stop_decision=pending_user_confirmation_adopt_production` 或 `pending_user_confirmation_rollback`。 |

## Fallback / Gate

- `__RVV10__` 未启用时：保持父类 public ordered double path。
- 非 `PointXYZ -> PointXYZ`：fallback。
- 非 `Scalar=double`：保持既有 float RVV 或 fallback 行为。
- source / target size 不相等、非 dense 或少于 16 点：fallback。
- row-source overloads：本阶段不触碰，继续保持既有 `Scalar=float` RVV 或 double fallback。

## 板卡复跑预算和决策桶

- repeated runs：5。
- warm-up / iterations：沿用 topic Makefile 默认 `TESVD_SCALE_BOARD_BENCH_WARMUP_ITERATIONS` / `TESVD_SCALE_BOARD_BENCH_ITERATIONS`。
- positive：median B/A >= 1.20 且没有 checksum mismatch / Doctor Error。
- weak-positive：1.05 <= median B/A < 1.20，需要用户判断。
- neutral / negative：median B/A < 1.05 或多 run 低于 1，默认不采纳。
- unstable：bucket 摇摆或退化频率高，降级为需要用户判断。

## 文档更新清单

- 本阶段 `result.zh.md`。
- `doc/phases/README.zh.md`、`optimization-matrix.zh.md`、`doc/optimization-roadmap.zh.md`。
- topic-local `testing-overview`、`benchmark-and-evidence`、`optimization-evidence`、`test-support-code-map` 和 evaluation。
- `doc-rvv` 只有在用户确认采纳后才同步；本阶段 PI5 前不把 double 写成 adopted。

## Continue / Stop Conditions

继续条件：PI1 gate 可闭合、production patch 范围不扩大、QEMU correctness 通过、board 可用。停止条件：生产补丁超出本计划、QEMU / Doctor 出 Error 无法修复、board 不可用、production evidence 与 diagnostic evidence 矛盾，或到达 PI5 用户检查点。
