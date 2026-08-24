# Phase 065 Result: Scalar Double Production Probe

## 结论

本阶段已把 Phase 063 / 064 的 `Scalar=double` RVV f64 widened diagnostic 推进为有界 production public probe（生产公开探针）。真实 public ordered overload 现在在 RVV 构建中对 `PointXYZ -> PointXYZ`、`Scalar=double`、dense、点数不少于 16 的 ordered-cloud-pair 输入尝试 RVV f64 widened accumulation；其它 double 范围继续 fallback。

`EvidenceDecision`：`pending_user_confirmation_adopt_production / scalar-double-production-probe`。证据支持保留当前 production patch，但 PI5 要求先停在用户检查点；用户确认前不能把它写成 adopted production behavior，也不更新长期 `doc-rvv` 为 double adopted。

## 实际执行范围

| item | planned | actual |
| --- | --- | --- |
| public entry | ordered-cloud-pair public overload | done：只触碰 ordered overload 的 RVV helper 分流。 |
| point type / Scalar | `PointXYZ -> PointXYZ` / `Scalar=double` | done：exact gate；不扩大到泛型 xyz AoS double。 |
| fallback | 非 RVV、非 dense、小规模、非 exact point type、row-source double fallback | done：新增测试覆盖小规模 / non-dense double fallback；row-source double 未触碰。 |
| QEMU correctness | Std/RVV 全测试 | done：Std/RVV 各 27/27 passed。 |
| QEMU smoke / asm input | `scalar-double-production-probe` smoke + `dump_bench_rvv` | done：Doctor `0/0/0`；asm 摘要可见 `vfwcvt.f.f.v` / `vfredosum.vs` 等 f64 widened 路径特征。 |
| board repeated | 5-run production public probe | done：5-run B/A 全 positive，Doctor `0/0/0`。 |

## Production Diff 摘要

- `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp`
  - 新增 `TransformationEstimationSVDScaleD64Accumulation`。
  - 新增 ordered `PointXYZ -> PointXYZ` double RVV accumulation helper：从 float xyz strided load 扩宽到 f64，使用 f64 vector multiply / reduction。
  - 新增 double solve helper，保留 Eigen `Matrix3d` SVD 后段。
  - ordered public RVV dispatch 新增 exact `PointXYZ -> PointXYZ` / `Scalar=double` gate；既有 `Scalar=float` traits-gated 路径保持原样。

## Correctness

命令：

```bash
make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare
make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_correctness_state
```

结果：

- Std：27/27 passed。
- RVV：27/27 passed。
- 新增测试：
  - `ScalarDoubleProductionProbeMatchesReference`
  - `ScalarDoubleProductionProbeFallbackBoundaries`

double public probe 与 scalar double fused reference 的预算：

- Std public double fallback：`max_public_error = 1.614977e-08`。
- RVV public double probe：`max_public_error = 9.858780e-14`。

## QEMU Smoke / ASM

命令：

```bash
make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_scalar_double_production_probe_state
```

证据：

- `log/qemu/scalar_double_production_probe/analyze_bench_compare.log`
- `log/qemu/scalar_double_production_probe/evidence_manifest.json`
- `log/qemu/scalar_double_production_probe/evidence_doctor.md`

QEMU 只证明构建、路径命中、日志形状和 asm 输入，不作为性能结论。Evidence Doctor：

- Errors=0
- Warnings=0
- Suggestions=0

反汇编摘要中可见 `vfwcvt.f.f.v`、`vfmul.vv`、`vfredosum.vs` 和 `vsetvli e64,m1` 等 f64 widened accumulation 相关指令。当前 asm 只作为 probe 归属输入；若用户确认采纳，closeout 再把它归入长期证据链。

## Board Repeated Evidence

命令：

```bash
make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_scalar_double_production_probe_repeated
```

证据：

- `log/board/scalar_double_production_probe_repeated/summary.md`
- `log/board/scalar_double_production_probe_repeated/evidence_manifest.json`
- `log/board/scalar_double_production_probe_repeated/evidence_doctor.md`

结果：

| case | B/A values | median | bucket | max public error | checksum |
| --- | --- | ---: | --- | ---: | --- |
| `scalar double production probe ordered-cloud-pair 64K` | `33.955, 33.664, 33.781, 34.077, 33.792` | `33.792x` | `positive` | `9.858780e-14` | match |

Evidence Doctor：

- Errors=0
- Warnings=0
- Suggestions=0

B/A 边界是 Std public double fallback ms / RVV public double production probe ms。它不同于 Phase 064 的 test-only scalar double fallback vs RVV f64 widened diagnostic，所以数值不能直接与 Phase 064 的 `2.863x` 横向比较。

## Diagnostic To Production Mismatch Audit 回填

| question | result |
| --- | --- |
| evidence role | production-public probe。 |
| A/B boundary | 真实 public ordered overload；Std 构建是 public double fallback，RVV 构建是 public double RVV probe。 |
| 当前决策问题 | `RVV-vs-scalar`：ordered public double path 是否值得保留。 |
| diagnostic 是否可外推 | Phase 064 不能直接外推；本阶段已用 production public evidence 重跑。 |
| baseline mismatch 风险 | 已降低：bench label 调用真实 public estimator；日志输出 `path: public-double-rvv-f64-widened-probe`。 |
| weak / negative 时 production probe 条件 | 不适用本结果；board repeated 为 stable positive。 |
| clean adoption 是否还需 RVV-vs-RVV detail A/B | 不需要；double ordered public path 没有既有 adopted RVV family。 |

## 未覆盖范围

- row-source double：source-indexed、dual-indexed 和 correspondence 仍 fallback；不能继承本阶段结论。
- 泛型 xyz AoS double：`PointXYZI`、`PointXYZRGB`、custom layout 等没有 double production direct 证据。
- 非 dense、小规模、非法输入、退化输入：只证明 fallback correctness，不证明 RVV。
- 长期 `doc-rvv`：用户确认采纳前不更新为 double adopted。

## PI5 用户检查点

当前 patch 保留在工作区，证据支持采纳，但按 production integration loop 规则必须等待用户确认：

- 若用户确认保留 / 采纳：下一阶段进入 `066-scalar-double-adoption-closeout`，刷新长期 `doc-rvv`、evaluation、matrix、roadmap 和提交边界。
- 若用户不确认采纳或要求更多证据：保留 patch，按用户指定补测或降级为 pending。
- 若用户要求回滚：另开 rollback phase，回收 production patch，并把本阶段证据写成 historical positive probe。

## continue_stop_decision

- `continue_stop_decision`: `turn_stop_deferred`
- `stop_condition_hit`: `PI5 production_adoption_requires_user_authorization`
- `next_phase_default`: 等待用户确认；确认采纳后进入 `066-scalar-double-adoption-closeout`。
