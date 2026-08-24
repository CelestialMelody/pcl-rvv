# Phase 068: Generic Scalar Double Ordered Production Probe Result

## 当前结论

本阶段已把 ordered `Scalar=double` production public probe 从 exact `PointXYZ -> PointXYZ` 扩到常见 PCL xyz AoS 点型，并在用户确认“当前有收益的实现可以接入”后收口为 `adopted-by-user / generic-scalar-double-ordered-production-probe`。

采纳范围严格限定为：

- ordered-cloud-pair public overload；
- `Scalar=double`；
- source / target 满足 `RVVXYZAoSFloatLayout`；
- 当前 production 白名单内的常见 PCL 点型：`PointXYZ`、`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA`、`PointXYZL`、`PointNormal`、`PointWithRange`、`PointWithViewpoint`；
- dense 输入；
- `nr_points >= 16`。

本阶段实际 board 证据覆盖 5 个代表组合：`PointXYZI -> PointXYZI`、`PointXYZRGB -> PointXYZRGB`、`PointXYZI -> PointXYZRGB`、`PointXYZRGBA -> PointXYZRGBA`、`PointNormal -> PointXYZRGB`。它不证明 source-indexed、dual-indexed、correspondence 的 generic double，不证明 custom layout double，不证明 sorted-copy double，也不证明任意用户自定义点型全集。

## 执行动作回填

| action | status | evidence / path | conclusion |
| --- | --- | --- | --- |
| 范围冻结 | done | `plan.zh.md` | 范围冻结为 ordered public overload + common PCL xyz AoS + `Scalar=double`。 |
| production patch | done | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` | ordered double accumulation 已按 `SrcLayout` / `TgtLayout` offset 泛化；row-source double exact gate 保持 Phase 067 边界。 |
| correctness / fallback | done | `src/test_tesvd_scale.cpp`、`log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` | Std/RVV 各 31 tests passed；新增 generic double ordered correctness 和 fallback boundary。 |
| QEMU smoke / Doctor / registry | done | `log/qemu/generic_scalar_double_ordered_production_probe/evidence_doctor.md`、`log/evidence_registry.json` | QEMU 只证明路径、误差和日志形状；Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`。 |
| board repeated / Doctor | done | `log/board/generic_scalar_double_ordered_production_probe_repeated/summary.md`、`evidence_doctor.md` | 5 个 ordered generic double case 全部 positive；Doctor `Errors=0`、`Warnings=1`、`Suggestions=0`。 |

## Correctness / Fallback

命令：

```bash
make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare
```

结果：

- Std：31/31 passed。
- RVV：31/31 passed。

新增或相关测试：

- `GenericScalarDoubleOrderedProductionProbeMatchesReference`：验证 common PCL xyz AoS ordered double public path 与 double reference 对齐。
- `GenericScalarDoubleOrderedProductionProbeFallbackBoundaries`：验证小规模、non-dense 和未覆盖组合保持父类 double fallback。
- 既有 `ScalarDoubleProductionProbeMatchesReference` / `ScalarDoubleProductionProbeFallbackBoundaries` 继续覆盖 exact `PointXYZ -> PointXYZ` ordered double branch。
- 既有 `ScalarDoubleRowSourceProductionProbeMatchesReference` / `ScalarDoubleRowSourceProductionProbeFallbackBoundaries` 继续覆盖 Phase 067 row-source exact double branch。

QEMU smoke：

```bash
make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_generic_scalar_double_ordered_production_probe_state
```

QEMU Doctor：`Errors=0`、`Warnings=0`、`Suggestions=0`。5 个 label 均命中 `public-generic-double-rvv-f64-widened-probe`；QEMU timing 不进入性能结论。QEMU manifest 记录的 max public error 为 Std fallback `1.614977e-08`、RVV branch `9.858780e-14`，预算 `5e-8`。

## Board Evidence

命令：

```bash
make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_generic_scalar_double_ordered_production_probe_repeated
```

Board repeated summary：

| case | runs B/A | median | bucket | max ref error |
| --- | --- | ---: | --- | ---: |
| `PointXYZI->PointXYZI 64K` | `31.702, 30.632, 32.110, 31.085, 30.169` | `31.085x` | positive | `9.859e-14` |
| `PointXYZRGB->PointXYZRGB 64K` | `32.786, 32.006, 31.862, 32.231, 32.604` | `32.231x` | positive | `9.859e-14` |
| `PointXYZI->PointXYZRGB 64K` | `32.233, 32.195, 32.570, 30.931, 30.900` | `32.195x` | positive | `9.859e-14` |
| `PointXYZRGBA->PointXYZRGBA 64K` | `31.689, 32.799, 32.372, 32.493, 32.550` | `32.493x` | positive | `9.859e-14` |
| `PointNormal->PointXYZRGB 64K` | `24.111, 23.983, 24.158, 24.133, 24.087` | `24.111x` | positive | `9.859e-14` |

Board Doctor：`Errors=0`、`Warnings=1`、`Suggestions=0`。唯一 warning 是 `PointNormal->PointXYZRGB` 的 group_outlier：该 case median `24.111x`，低于同组约 `32.2x` 的中位收益。处理方式是按点型组合单独报告，不把其它点型约 `32x` 的收益外推给 `PointNormal->PointXYZRGB`；该 case 自身最小 B/A 仍为 `23.983x`，不改变 positive bucket。

## Diagnostic To Production Mismatch Audit

| question | result |
| --- | --- |
| evidence role | `production-public`，真实 public ordered overload。 |
| A/B boundary | Std build public generic double fallback vs RVV build public generic double branch。 |
| 当前决策问题 | `RVV-vs-scalar`，判断当前 public RVV path 是否快于 public scalar path。 |
| diagnostic 是否可外推到 production | 本阶段不依赖 diagnostic 外推；证据来自真实 public overload。 |
| comparison-boundary / baseline mismatch 风险 | 已按 public overload 计入 dispatch、layout gate、f64 widened accumulation 和 double solve；不用于 RVV-family-selection。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不适用；本阶段结果为 positive，且用户已确认当前有收益实现可以接入。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要。ordered generic double 此前没有已采纳 generic double RVV family；本阶段只回答 public RVV 是否快于 public scalar。 |

## Optimization Matrix Update

`generic-scalar-double-ordered-production-probe` 从 planned 更新为 `adopted-by-user`：

- correctness：Std/RVV 各 31 tests passed。
- QEMU smoke：5 comparisons；Doctor `0/0/0`；max public error RVV `9.858780e-14`。
- board：5 个 ordered generic double case 全部 positive，median B/A `24.111x` 到 `32.493x`。
- Doctor：board `0/1/0`；warning 只影响按点型组合解释，不阻塞本阶段 adopted decision。

## 未覆盖范围

- row-source generic double：source-indexed、dual-indexed 和 correspondence 仍只在 exact `PointXYZ -> PointXYZ` double 范围采纳。
- custom layout double：测试本地 custom layout、padding、alignment 的 double 采样未验证。
- sorted-copy double：Phase 047 的 sorted-copy branch 仍只覆盖 `Scalar=float` correspondence 窄范围。
- 非 dense、小规模、source / target size 不等、非法 index / correspondence、非 xyz AoS layout 和任意自定义点型全集。

## Continue / Stop Decision

- `continue_stop_decision`: `phase_complete_continue_available`
- `stop_condition_hit`: `none_for_phase_068`
- `next_phase_default`: 若继续 double 方向，优先另开 row-source generic double 或 custom layout double phase；二者都需要重新冻结 point type / layout gate、fallback、QEMU、board 和 Evidence Doctor。当前 phase 不能外推关闭这些方向。
