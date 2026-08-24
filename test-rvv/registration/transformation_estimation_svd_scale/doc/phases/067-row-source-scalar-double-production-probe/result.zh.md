# Phase 067: Row-source Scalar Double Production Probe Result

## 当前结论

本阶段已把 row-source `Scalar=double` production public probe（生产公开探针）推进到 PI5 证据闭环，并在用户确认“当前有收益的实现可以接入”后收口为 adopted production behavior（已采纳生产行为）。

采纳范围严格限定为：

- source-indexed、dual-indexed 和 correspondence 三类 row-source public overload；
- exact `PointSource == pcl::PointXYZ` 且 `PointTarget == pcl::PointXYZ`；
- `Scalar=double`；
- dense 输入；
- `nr_points >= 16`；
- gather index / correspondence 满足既有 RVV gather gate；step=1 contiguous slice 优先复用 contiguous offset f64 fast path。

本阶段不证明 generic xyz AoS double、custom layout double、非 dense、小规模、非法 index / correspondence、sorted-copy double、stride / reverse / shuffle affine fast path，也不把 ordered double 证据外推为 row-source 以外的结论。

## 执行动作回填

| action | status | evidence / path | conclusion |
| --- | --- | --- | --- |
| PI1 范围冻结 | done | `plan.zh.md` | 范围冻结为 exact `PointXYZ -> PointXYZ` / `Scalar=double` / dense / 三类 row-source public overload。 |
| PI2 production patch | done | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` | 新增 row-source f64 widened accumulation（双精度扩宽累加）路径；非覆盖组合保持 fallback。 |
| PI3 correctness / bench support | done | `src/test_tesvd_scale.cpp`、`src/bench_tesvd_scale.cpp`、`Makefile` | 新增 public row-source double gtest、case-filter 和 QEMU / board target。 |
| PI4 QEMU smoke / Doctor / registry | done | `log/qemu/row_source_scalar_double_production_probe/evidence_doctor.md`、`log/evidence_registry.json` | QEMU 只证明路径、误差和日志形状；Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`。 |
| PI5 board repeated / decision | done | `log/board/row_source_scalar_double_production_probe_repeated/summary.md`、`log/board/row_source_scalar_double_production_probe_repeated/evidence_doctor.md` | 三类 row-source 均为 positive；用户已确认采纳。 |

## Correctness / Fallback

命令：

```bash
make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare
```

结果：Std/RVV 各 29 tests passed。

新增或相关测试：

- `ScalarDoubleRowSourceProductionProbeMatchesReference`：source-indexed、dual-indexed 和 correspondence public double path 与 selected-cloud double reference 对齐。
- `ScalarDoubleRowSourceProductionProbeFallbackBoundaries`：小规模、非 dense 和非覆盖路径保持父类 fallback。
- 既有 `ScalarDoubleProductionProbeMatchesReference` / `ScalarDoubleProductionProbeFallbackBoundaries` 仍覆盖 ordered double branch。

QEMU smoke：

```bash
make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_row_source_scalar_double_production_probe_state
```

QEMU Doctor：`Errors=0`、`Warnings=0`、`Suggestions=0`。QEMU timing 不进入性能结论。

## Board Evidence

命令：

```bash
make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_row_source_scalar_double_production_probe_repeated
```

Board repeated summary：

| case | runs B/A | median | bucket | max public error |
| --- | --- | ---: | --- | ---: |
| source-indexed 64K | `20.537, 19.869, 20.326, 20.164, 20.201` | `20.201x` | positive | `9.769963e-14` |
| dual-indexed 64K | `16.007, 11.515, 14.800, 15.517, 11.650` | `14.800x` | positive | `8.748557e-14` |
| correspondence 64K | `13.324, 12.951, 13.703, 13.647, 13.474` | `13.474x` | positive | `8.748557e-14` |

Board Doctor：`Errors=0`、`Warnings=1`、`Suggestions=0`。唯一 warning 是 dual-indexed 64K 的 `long_tail_or_variance`，max/min 为 `1.39`；由于该 case 的最小 B/A 仍为 `11.515x`，不改变 positive bucket，但长期文档和 summary 必须保留 min / median / max，不把单一均值写成全部稳定性事实。

## Diagnostic To Production Mismatch Audit

| question | result |
| --- | --- |
| evidence role | `production-public`，真实 public row-source overload。 |
| A/B boundary | Std build public double row-source fallback vs RVV build public double row-source branch。 |
| 当前决策问题 | `RVV-vs-scalar`，判断当前 public RVV path 是否快于 public scalar path。 |
| diagnostic 是否可外推到 production | 本阶段不依赖 diagnostic 外推；证据来自真实 public overload。 |
| comparison-boundary / baseline mismatch 风险 | 已按 public overload 计入 row-source dispatch、contiguous 检测、gather 和 solve；不用于 RVV-family-selection。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不适用；本阶段结果为 positive，且用户已确认采纳。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要。row-source double 此前没有已采纳 double RVV family；本阶段只回答 public RVV 是否快于 public scalar。 |

## Optimization Matrix Update

`row-source-scalar-double-production-probe` 三个 row-source slice 从 `positive_pending_user_confirmation` 更新为 `adopted-by-user`：

- source-indexed：board median `20.201x`，Doctor `0/1/0`。
- dual-indexed：board median `14.800x`，Doctor warning 保留为 variance risk。
- correspondence：board median `13.474x`，Doctor `0/1/0` 总体 warning 同上。

## Continue / Stop Decision

本阶段完成，且已由用户确认采纳。默认下一 phase 不应继续扩大本阶段结论，而应另开独立 double expansion phase：

- generic xyz AoS double production probe / diagnostic scout；
- custom layout double sampling；
- row-source double point-type expansion；
- 或新的 row-source mitigation family。

其中 generic double 和 custom layout double 都会扩大 point type / layout gate，必须重新冻结 traits、fallback、QEMU、ASM、board 和 Evidence Doctor 证据。
