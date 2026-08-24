# Phase 063 结果：Scalar=double diagnostic scout

## 执行摘要

本阶段把 `Scalar=double` 从 `not_applicable / 未验证` 推进到 `diagnostic_correctness_scout`。范围保持很窄：ordered-cloud-pair、`PointXYZ -> PointXYZ`、dense input、公开 double fallback 作为 reference。

本阶段没有修改 production 源码，也没有把 double RVV 接入公开 overload。新增代码全部在 `test-rvv/registration/transformation_estimation_svd_scale` 的 test support / gtest / bench smoke 内。

`EvidenceDecision`：`diagnostic_correctness_scout_complete / scalar-double-rvv-f64-widened`。

## 动作回填

| action | 状态 | 证据 / 命令 | 结论 |
| --- | --- | --- | --- |
| A1 标量 double scout | done | `ScaleAccumulationD64`、`accumulateScaleStdDouble`、`solveScaleFromAccumulationDouble`、`estimateScaleStdDouble` | test-only double fused accumulation 与公开 double fallback 对齐。 |
| A2 RVV f64 widened scout | done | `estimateScaleRVVDouble` / `accumulateScaleRVVDouble` | RVV 构建从 float xyz load 扩宽到 double 规约；Std 构建保留 scalar double fallback。 |
| A3 correctness guard | done | `ScalarDoubleAccumulationScoutMatchesPublicFallback`、`ScalarDoubleRVVWidenedScoutMatchesScalarDoubleScout` | `5e-8` 数值预算内通过。 |
| A4 bench smoke入口 | done | `--case-filter scalar-double-diagnostic-scout` | 输出 64K ordered double scout label、checksum、`max_public_error` 和 path；QEMU timing 只作 smoke。 |
| A5 production guard | done | `UncoveredPublicEntriesStayCorrect` 仍覆盖 `Scalar=double` 父类 fallback | 真实 production public path 没有因本阶段进入 double RVV。 |

## Correctness

```bash
make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare
```

结果：Std/RVV 各 25 tests passed。

新增 TEST：

- `ScalarDoubleAccumulationScoutMatchesPublicFallback`：test-only double fused accumulation 对公开 `TransformationEstimationSVDScale<..., double>` fallback，预算 `5e-8`。
- `ScalarDoubleRVVWidenedScoutMatchesScalarDoubleScout`：RVV f64 widened scout 对 scalar double scout，预算 `5e-8`；Std 构建自然 fallback。

## QEMU smoke

第一次直接运行 `run_bench_compare` 时，Makefile guard 按预期阻止 QEMU bench compare，并提示必须显式声明 smoke-only：

```bash
make -C test-rvv/registration/transformation_estimation_svd_scale run_bench_compare BENCH_ARGS="--case-filter scalar-double-diagnostic-scout --iterations 3 --warmup-iterations 1"
```

结果：guard failed，原因是 `ALLOW_QEMU_BENCH_COMPARE=1` 未设置。

随后按 smoke-only 口径运行：

```bash
make -C test-rvv/registration/transformation_estimation_svd_scale run_bench_compare ALLOW_QEMU_BENCH_COMPARE=1 BENCH_ARGS="--case-filter scalar-double-diagnostic-scout --iterations 3 --warmup-iterations 1"
```

输出事实：

| build | label | checksum | max_public_error | path |
| --- | --- | ---: | ---: | --- |
| Std | `scalar double diagnostic scout ordered-cloud-pair 64K` | `8715325090201500223` | `1.614977e-08` | `scalar-double-fallback` |
| RVV | `scalar double diagnostic scout ordered-cloud-pair 64K` | `8715325090201500223` | `1.614987e-08` | `rvv-f64-widened` |

QEMU summary 显示 Std `6.8722 ms/iter`、RVV `8.6421 ms/iter`、speedup `0.80x`。这不是性能结论，只证明 build、case-filter、checksum、误差字段和 RVV path label 可复现。double RVV 是否值得生产接入仍需要目标硬件 repeated benchmark、ASM attribution、Evidence Doctor 和 production-boundary probe。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic correctness scout + qemu_smoke_only。 |
| A/B boundary | public double fallback / scalar double fused accumulation / test-only RVV f64 widened accumulation；不是 production public RVV-vs-scalar。 |
| 当前决策问题 | double 数值预算和 RVV f64 code shape 是否可继续。 |
| diagnostic 是否可外推到 production | no。本阶段没有 production dispatch、fallback matrix、ASM 或 board repeated。 |
| comparison-boundary / baseline mismatch 风险 | yes。公开 double fallback 走父类矩阵路径；test-only scout 走 fused accumulation，累加树不同。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前只允许继续诊断或另开 production probe plan；不能直接接 production。 |
| clean adoption 是否需要同一 production boundary 内证据 | yes。需要 public overload patch、fallback guard、QEMU smoke、ASM、board repeated 和 Evidence Doctor。 |

## 仍不覆盖

- source-indexed、dual-indexed、correspondence 的 `Scalar=double`。
- 所有 generic xyz AoS 点型或 custom layout 的 double 路径。
- 真实 production RVV f64 dispatch 和 fallback。
- RVV f64 目标硬件性能。
- 非 dense、NaN / Inf、非法 index / correspondence。

## 下一阶段恢复

默认不把 Phase 063 直接接 production。可继续的独立方向：

1. `scalar-double-board-scout`：只做 test-only RVV f64 widened vs scalar double 的目标硬件 repeated diagnostic，先判断 f64 吞吐是否值得 production probe。
2. `scalar-double-production-probe-plan`：若用户明确要接 production，再冻结 `Scalar=double` public overload、fallback、ASM、board 和 Doctor gate。
3. `scalar-double-row-source-scout`：在 ordered double 数值预算稳定后，另开 row-source double correctness scout。
