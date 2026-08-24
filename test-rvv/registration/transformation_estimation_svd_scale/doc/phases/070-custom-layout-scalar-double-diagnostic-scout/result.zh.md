# Phase 070 Result: custom layout `Scalar=double` diagnostic scout

## 结论

EvidenceDecision（证据决策）：`positive_pending_user_confirmation`。

本阶段在 Phase 069 仍为 `positive_pending_user_confirmation` 的前提下，单独侦察测试本地 registered custom xyz AoS layout（已注册自定义 xyz 结构数组布局）能否命中 `Scalar=double` 的生产公开入口候选。结果为 positive：ordered、source-indexed、dual-indexed 和 correspondence 四个 public overload 均可命中 f64 widened RVV accumulation（双精度扩宽 RVV 累加），correctness（正确性）、QEMU smoke（QEMU 小型路径验证）、board repeated（板卡重复性能测试）和 Evidence Doctor（证据体检）均完成。

该结果只支持 `LocalSVDScalePaddedXYZSource -> LocalSVDScaleWideXYZTarget` 这个 custom layout sample 的 bounded production-public candidate（有界生产公开候选）。它不把 Phase 069 写成 adopted，也不把全部自定义点型、packed unaligned float、异常 alignment 全集或 sorted-copy double 写成已覆盖。

## 执行范围

本阶段实际覆盖：

- ordered-cloud-pair、source-indexed、dual-indexed 和 correspondence public overload。
- `LocalSVDScalePaddedXYZSource -> LocalSVDScaleWideXYZTarget`。
- `Scalar=double`、dense、64K board case、合法 index / correspondence。
- QEMU smoke 只作为 build / path / log-shape evidence（构建、路径和日志形状证据），不作为性能结论。

本阶段不覆盖：

- Phase 069 adoption closeout；用户未明确确认前 Phase 069 仍保持 `positive_pending_user_confirmation`。
- 全部自定义 xyz AoS layout、packed unaligned float、异常 padding / alignment 全集。
- sorted-copy double、affine fast path double 或新的 RVV-family-selection（RVV 实现族选择）。
- 非 dense、小规模、非法 index / correspondence 或更广规模矩阵。

## TDD 红绿记录

先补测试，再放宽 production dispatch（生产分流逻辑）。

| 步骤 | 命令 / 现象 | 结论 |
| --- | --- | --- |
| red | `make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare`：Std 35/35 passed，RVV 34/35；新增 custom layout double dispatch-hit case 失败。 | 证明旧 common PCL whitelist 阻止 custom layout double 命中 RVV。 |
| green | 放宽 double dispatch 后重跑 `run_test_compare`：Std/RVV 35/35 passed。 | custom layout double correctness、detail-hit 和 fallback boundaries 闭合。 |

新增 gtest：

- `CustomLayoutScalarDoubleProductionScoutMatchesReference`
- `CustomLayoutScalarDoubleProductionScoutFallbackBoundaries`

## 实现变更

production header 的 double dispatch 从 common PCL xyz AoS whitelist 收窄条件，改为：

```text
std::is_same_v<Scalar, double> && SrcLayout::value && TgtLayout::value
```

这让 source / target 分别满足 `RVVXYZAoSFloatLayout` 的 registered xyz AoS layout 可以进入 f64 widened helper。dense、size、gather byte-offset、index / correspondence 合法性和 fallback gate 均保持不变。

本阶段没有改 public API（公开接口），也没有把 sorted-copy double 接入 production。

## Correctness

`make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare`：

- Std：35/35 passed。
- RVV：35/35 passed。

新增 custom layout double tests 覆盖：

- ordered public path 与 public double fallback reference 对齐。
- source-indexed、dual-indexed 和 correspondence row-source public path 与 reference 对齐。
- RVV 构建下 detail dispatch 命中。
- small / non-dense / unsupported fallback boundaries 保持标量路径。

## QEMU Smoke

Target：`record_qemu_custom_layout_scalar_double_diagnostic_scout_state`。

- case-filter：`custom-layout-scalar-double-diagnostic-scout`
- comparisons：4
- RVV path 命中：`public-generic-double-rvv-f64-widened-probe` / row-source generic double variant
- QEMU Evidence Doctor：`Errors=0`、`Warnings=0`、`Suggestions=0`
- 证据路径：
  - `test-rvv/registration/transformation_estimation_svd_scale/log/qemu/custom_layout_scalar_double_diagnostic_scout/analyze_bench_compare.log`
  - `test-rvv/registration/transformation_estimation_svd_scale/log/qemu/custom_layout_scalar_double_diagnostic_scout/evidence_manifest.json`
  - `test-rvv/registration/transformation_estimation_svd_scale/log/qemu/custom_layout_scalar_double_diagnostic_scout/evidence_doctor.md`

QEMU timing 不作为性能结论。

## Board Repeated

Target：`run_board_bench_custom_layout_scalar_double_diagnostic_scout_repeated`。

Board repeated 覆盖 4 个 64K case，全部 `positive`，checksum match，最大 reference error 约 `9.859e-14`。

| row source | point type pair | median B/A | min | max | bucket |
| --- | --- | ---: | ---: | ---: | --- |
| ordered-cloud-pair | `LocalPaddedXYZSource->LocalWideXYZTarget` | `27.497x` | `27.081x` | `27.710x` | positive |
| source-indexed | `LocalPaddedXYZSource->LocalWideXYZTarget` | `15.948x` | `15.787x` | `16.274x` | positive |
| dual-indexed | `LocalPaddedXYZSource->LocalWideXYZTarget` | `11.017x` | `10.043x` | `11.771x` | positive |
| correspondence | `LocalPaddedXYZSource->LocalWideXYZTarget` | `9.966x` | `9.229x` | `10.230x` | positive |

Board Evidence Doctor：`Errors=0`、`Warnings=1`、`Suggestions=0`。

唯一 warning 是 dual-indexed `LocalPaddedXYZSource->LocalWideXYZTarget` 64K 的 `long_tail_or_variance`：min `10.043x`、median `11.017x`、max `11.771x`，max/min `1.17`。处理方式是保留 min / median / max，不剔除异常；该 warning 不改变 4 个 case 的 positive bucket。

证据路径：

- `test-rvv/registration/transformation_estimation_svd_scale/log/board/custom_layout_scalar_double_diagnostic_scout_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_svd_scale/log/board/custom_layout_scalar_double_diagnostic_scout_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_svd_scale/log/board/custom_layout_scalar_double_diagnostic_scout_repeated/evidence_doctor.md`

## Evidence Registry

`log/evidence_registry.json` 已登记本阶段 QEMU 和 board evidence。当前登记项的 doc refs 指向：

- `doc/benchmark-and-evidence.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc/phases/070-custom-layout-scalar-double-diagnostic-scout/result.zh.md`

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production_public_custom_layout_scalar_double_scout`，真实公开入口候选侦察证据。 |
| A/B boundary | public overload；Std 为 public double fallback，RVV 为当前 production patch 下的 generic double RVV path。 |
| 当前决策问题 | RVV-vs-scalar：该 custom layout sample 的 public double RVV path 是否快于 public double scalar fallback。 |
| 是否可外推到 production | 只能外推到本阶段列出的 custom layout sample；不能外推到全部自定义 layout 或异常 alignment 全集。 |
| comparison-boundary / baseline mismatch 风险 | 低；board summary 的 Std/RVV 对比使用同一 public case-filter。但该证据不是 RVV-family-selection。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段本身就是 bounded production-public scout；若后续更广 custom layout 出现弱 / 负 / 不稳定，只能按对应 layout sample 降级。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 本阶段没有选择新 RVV family，只放宽 layout gate；若要 clean-adopt 这个 gate，仍需要用户明确确认，并把长期文档写清代表性证据边界。 |

## 继续 / 停止判断

本阶段计划内动作已闭合，但不能自动采纳。当前停止在 `positive_pending_user_confirmation`：

- Phase 069 仍是 `positive_pending_user_confirmation`，未进入 adopted。
- Phase 070 是独立 custom layout double candidate，也停在 `positive_pending_user_confirmation`。
- 若用户确认采纳 Phase 069 或 Phase 070，应另开 adoption closeout phase，刷新长期 `doc-rvv`、matrix、roadmap 和 topic-local docs。
- 若用户暂不采纳，保留 pending candidate，下一条可评估路线仍是 sorted-copy double、更多 custom layout double 取样或其它 roadmap 中未关闭方向。

## 文档同步状态

已同步：

- `doc/phases/README.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc/transformation_estimation_svd_scale-evaluation.zh.md`
- `doc-rvv/registration/transformation_estimation_svd_scale-RVV.zh.md`
- `tmp/rvv-work-logs/registration/transformation_estimation_svd_scale/current-handoff/`

同步边界：这些文档只把 Phase 070 写成 pending candidate，不写成 adopted production behavior。
