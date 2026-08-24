# Phase 067: Row-source Scalar Double Production Probe Plan

## 阶段意图和边界

本阶段把 Phase 066 已采纳的 ordered `Scalar=double` f64 widened accumulation（双精度扩宽累加）扩展为 row-source production probe（按行来源的生产探针）。范围只覆盖 exact `PointXYZ -> PointXYZ`、`Scalar=double`、dense 输入、`nr_points >= 16`，以及三类 row-source public overload：source-indexed、dual-indexed 和 correspondence。它不证明 generic xyz AoS double、custom layout double、非 dense、小规模、非法 index / correspondence、sorted-copy double 或全部输入分布。

本阶段是 production probe，不是 adoption closeout。若 QEMU / board 证据为 positive，阶段结果也只能停在 PI5 用户检查点，等待用户确认是否采纳。

## 当前状态清单

| area | current state | evidence / path |
| --- | --- | --- |
| ordered double | Phase 065 / 066 已接入并采纳 ordered exact `PointXYZ -> PointXYZ` / `Scalar=double`。 | `doc/phases/066-scalar-double-adoption-closeout/result.zh.md` |
| row-source float | source-indexed、dual-indexed、correspondence 的 `Scalar=float` public path 已采纳，并已有 contiguous affine fast path 与 correspondence sorted-copy 等窄分支。 | `doc/phases/043-row-source-expansion/result.zh.md`、`doc/phases/062-affine-index-fast-path-adoption-closeout/result.zh.md` |
| row-source double | 当前 production helper 的三类 row-source RVV gate 仍要求 `Scalar=float`；double 走父类 scale fallback。 | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` |
| test / bench support | 已有 ordered double production probe case-filter；缺少 row-source double public correctness、bench label、manifest / Doctor / registry target。 | `src/test_tesvd_scale.cpp`、`src/bench_tesvd_scale.cpp`、`Makefile` |

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | Phase 066 是 ordered production-public adopted evidence；本阶段是 row-source production-public probe。 |
| A/B boundary | public row-source overload；Std build 为 public double scalar fallback，RVV build 为有界 row-source double RVV branch。 |
| 当前决策问题 | `RVV-vs-scalar`：row-source double public path 是否值得接入。 |
| diagnostic 是否可外推到 production | ordered double 不能外推到 row-source；本阶段必须单独补三类 row-source correctness、QEMU smoke、ASM、board repeated 和 Evidence Doctor。 |
| comparison-boundary / baseline mismatch 风险 | 有。row-source 需要 index / correspondence 读取、contiguous 检测和 gather 边界；必须计入 public overload 的真实分流成本。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段已经是 bounded production probe；若结果 weak / negative / unstable，停在 PI5，等待用户决定保留、补测或回滚。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 不需要。row-source double 当前没有已采纳 double RVV family；本阶段只回答 public RVV 是否快于 public scalar。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `row-source-scalar-double-production-probe` | source-indexed | exact `PointXYZ -> PointXYZ` / `double` / dense | public overload with source indices and ordered selected target | 新增 gtest 覆盖 public path vs selected-cloud double reference | `scalar-double-row-source-production-probe` | 5-run board repeated；只用 64K smoke/probe case | RVV binary 应可见 f64 widen / multiply / reduction 指令 | QEMU + board Doctor 必须解释 Errors / Warnings | pending |
| `row-source-scalar-double-production-probe` | dual-indexed | exact `PointXYZ -> PointXYZ` / `double` / dense | public overload with source / target indices | 同上 | 同上 | 同上 | 同上 | 同上 | pending |
| `row-source-scalar-double-production-probe` | correspondence | exact `PointXYZ -> PointXYZ` / `double` / dense | public overload with correspondences | 同上 | 同上 | 同上 | 同上 | 同上 | pending |

## 实现和测试动作

| id | action | done criteria |
| --- | --- | --- |
| PI1 | 冻结 row-source double production probe 范围。 | 本计划写清 gate、fallback、不可外推范围和 PI5 停止点。 |
| PI2 | 在 production detail 中新增 exact `PointXYZ` f64 row-source accumulation，并让三类 row-source public overload 在 `Scalar=double` 下尝试 RVV。 | 非 RVV、非 exact 点型、非 double、非 dense、小规模和非法 index / correspondence 都自然 fallback。 |
| PI3 | 新增 row-source double production-facing gtest 和 bench case-filter。 | `run_test_compare` Std/RVV 通过；bench label 输出 row source、`max_public_error` 和 public path。 |
| PI4 | 新增 QEMU / board manifest、Doctor 和 registry target。 | QEMU smoke 只作为 correctness / log-shape；board repeated 才作为性能证据。 |
| PI5 | 运行 QEMU correctness、QEMU smoke、board repeated、Evidence Doctor 和 registry。 | result 回填证据，停在 `pending_user_confirmation_adopt_production` 或 `pending_user_confirmation_rollback`。 |

## Fallback / Gate

- `__RVV10__` 未启用时：保持父类 row-source double fallback。
- 非 exact `PointXYZ -> PointXYZ`：fallback。
- 非 `Scalar=double`：保持既有 float RVV 或 fallback 行为。
- source / target 非 dense、小于 16、source-indexed 目标数量不匹配、dual-indexed 两侧 index 数量不匹配、correspondence 为空或小于 16：fallback。
- 32-bit gather offset 不满足既有 row-source gate 时：fallback。
- generic xyz AoS double、custom layout double 和 sorted-copy double 不在本阶段范围。

## 板卡复跑预算和决策桶

- repeated runs：5。
- warm-up / iterations：沿用 `TESVD_SCALE_BOARD_BENCH_WARMUP_ITERATIONS` / `TESVD_SCALE_BOARD_BENCH_ITERATIONS`。
- positive：所有 case median B/A >= 1.20，checksum match，且 Doctor Errors 为 0。
- weak-positive：任一 case `1.05 <= median B/A < 1.20`，或 Warning 需要人工判断。
- neutral / negative：任一 case median B/A < 1.05，默认不采纳该分支。
- unstable：bucket 摇摆、退化频率高或 Doctor Error 未能修复。

## 文档更新清单

- `result.zh.md`。
- `doc/phases/README.zh.md`、`optimization-matrix.zh.md`、`doc/optimization-roadmap.zh.md`。
- topic-local `testing-overview`、`benchmark-and-evidence`、`optimization-evidence`、`test-support-code-map` 和 evaluation。
- `doc-rvv` 只有在 PI5 后用户确认采纳时才同步 adopted 状态。

## Continue / Stop Conditions

继续条件：production patch 保持在本阶段 exact row-source double 范围内、QEMU correctness 通过、board 可用且 Evidence Doctor 无阻塞 Error。停止条件：生产补丁需要扩大到泛型 double / custom layout / 其它模块，QEMU / Doctor Error 无法修复，board 不可用，证据与 correctness 矛盾，或到达 PI5 用户检查点。
