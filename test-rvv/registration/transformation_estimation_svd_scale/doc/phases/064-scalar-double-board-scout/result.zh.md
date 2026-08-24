# Phase 064 结果：Scalar=double board diagnostic scout

## 执行摘要

本阶段把 Phase 063 的 `Scalar=double` RVV f64 widened scout 从 QEMU smoke-only 推进到 board diagnostic（板卡诊断）性能信号。范围仍然很窄：ordered-cloud-pair、`PointXYZ -> PointXYZ`、dense 64K、test-only scalar double fallback 对 test-only RVV f64 widened accumulation。

本阶段没有修改 production 源码，也没有新增 double public dispatch。`EvidenceDecision`：`board_diagnostic_positive / scalar-double-rvv-f64-widened`。该结论只支持下一阶段进入 `scalar-double-production-probe-plan`；不能直接写成 production-ready 或 adopted。

## 动作回填

| action | 状态 | 证据 / 命令 | 结论 |
| --- | --- | --- | --- |
| A1 board target | done | `run_board_bench_scalar_double_diagnostic_scout_repeated` | 新增 scalar-double board repeated target、manifest / Doctor / registry 接入。 |
| A2 summary parser | done | `script/generate_tesvd_scale_board_repeated_summary.py` | summary 脚本识别 `scalar double diagnostic scout ordered-cloud-pair 64K` 和 `max_public_error`，evidence role 为 `scalar_double_board_diagnostic`。 |
| A3 correctness freshness | done | `make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare` | Std/RVV 各 25 tests passed。 |
| A4 board diagnostic | done | `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_scalar_double_diagnostic_scout_repeated` | 5-run board repeated positive；Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`。 |
| A5 registry freshness | done | `record_qemu_scalar_double_diagnostic_scout_state`、`record_qemu_correctness_state`、`evidence_status` | QEMU smoke、correctness 和 Phase 064 board diagnostic 均已登记；registry fresh。 |

## Correctness

```bash
make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare
```

结果：Std/RVV 各 25 tests passed。Phase 063 新增的两个 double scout TEST 仍通过：

- `ScalarDoubleAccumulationScoutMatchesPublicFallback`
- `ScalarDoubleRVVWidenedScoutMatchesScalarDoubleScout`

## Board diagnostic

```bash
make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_scalar_double_diagnostic_scout_repeated
```

输出路径：

- summary：`log/board/scalar_double_diagnostic_scout_repeated/summary.md`
- manifest：`log/board/scalar_double_diagnostic_scout_repeated/evidence_manifest.json`
- Evidence Doctor：`log/board/scalar_double_diagnostic_scout_repeated/evidence_doctor.md`

结果表：

| case | runs B/A | median | min | max | bucket | max public error | checksum |
| --- | --- | ---: | ---: | ---: | --- | ---: | --- |
| `scalar double diagnostic scout ordered-cloud-pair 64K` | `2.806, 2.851, 2.867, 2.868, 2.863` | `2.863x` | `2.806x` | `2.868x` | `positive` | `1.615e-08` | match |

Evidence Doctor：

| severity | count |
| --- | ---: |
| Errors | 0 |
| Warnings | 0 |
| Suggestions | 0 |

## QEMU smoke freshness

```bash
make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_scalar_double_diagnostic_scout_state
```

QEMU 仍只作为 smoke-only（只验证构建、路径和日志形状）：

| build | checksum | max public error | path |
| --- | ---: | ---: | --- |
| Std | `8715325090201500223` | `1.614977e-08` | `scalar-double-fallback` |
| RVV | `8715325090201500223` | `1.614987e-08` | `rvv-f64-widened` |

QEMU timing 仍不作为性能证据。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | board diagnostic + qemu_smoke_only。 |
| A/B boundary | test-only scalar double fallback vs test-only RVV f64 widened accumulation；不是 production public double dispatch。 |
| 当前决策问题 | RVV f64 widened double accumulation 是否值得进入 production probe plan。 |
| diagnostic 是否可外推到 production | no。board positive 仍没有 public overload patch、fallback matrix、ASM attribution 或 production-boundary Evidence Doctor。 |
| comparison-boundary / baseline mismatch 风险 | yes。公开 double fallback 与 test-only fused accumulation 的实现边界不同；Phase 063/064 只证明当前 deterministic ordered 输入下的数值预算和诊断性能信号。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段实际为 stable positive；因此允许下一 phase 写 PI1 计划，但不能跳过 PI1-PI5。 |
| clean adoption 是否需要同一 production boundary 内证据 | yes。必须补真实 production dispatch、fallback、ASM、board repeated、Evidence Doctor 和 PI5 用户确认。 |

## Evidence registry

```bash
make -C test-rvv/registration/transformation_estimation_svd_scale evidence_status
```

结果：`evidence registry check: fresh`。

## 仍不覆盖

- production double RVV dispatch。
- source-indexed、dual-indexed、correspondence 的 `Scalar=double`。
- generic / custom layout double。
- ASM attribution。
- fallback matrix、非法输入、NaN / Inf。

## 下一阶段恢复

默认下一步是 `scalar-double-production-probe-plan`：冻结 production double probe 的 public overload 范围、fallback / gate、ASM、QEMU smoke、board repeated 和 PI5 用户检查点。若不进入 production，则可另开 `scalar-double-row-source-scout`，但不能用本阶段 ordered board diagnostic 外推到 row-source double。
