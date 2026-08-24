# Phase 043 结果：row-source expansion

## 结论

本阶段已把 `row-source-expansion`（行来源扩展）从优化矩阵中的
`phase_deferred + unblocked_after_adoption` 推进为：

```text
adopted-by-user /
row-source-direct-fused-scale-accum /
source-indexed + dual-indexed + correspondence /
PointXYZ -> PointXYZ / Scalar=float / dense xyz AoS
```

生产源码中已经新增 source-indexed、dual-indexed 和 correspondence 三类公开入口的
scale-aware fused accumulation（带尺度估计的融合累加）路径；gate 不满足时仍回父类
`TransformationEstimationSVD` 的 scale 标量路径。QEMU correctness、row-source QEMU smoke、
board repeated summary、Evidence Doctor 和 registry 均已完成。用户后续已明确确认“当前有收益的实现可以接入”，
因此本阶段现在收口为 adopted production behavior（已采纳生产行为）。采纳范围仍只覆盖本阶段矩阵列出的
`PointXYZ -> PointXYZ` / `Scalar=float` / dense xyz AoS row-source public path。

## 实际变更

| area | 实际变更 | 位置 |
| --- | --- | --- |
| production declaration | `TransformationEstimationSVDScale` 新增 source-indexed、dual-indexed 和 correspondence override。 | `registration/include/pcl/registration/transformation_estimation_svd_scale.h` |
| production implementation | 新增 source-indexed、dual-indexed、correspondence 的 RVV gather 累加 helper，并复用 scale 专用 solve helper。 | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` |
| test support | 新增 deterministic indices、selected cloud 和 correspondence fixture。 | `include/impl/tesvd_scale_support.hpp` |
| correctness | 新增 `SourceIndexedScaleMatchesReference`、`DualIndexedScaleMatchesReference`、`CorrespondenceScaleMatchesReference`。 | `src/test_tesvd_scale.cpp` |
| bench | 新增 `row-source-scale` case-filter，覆盖 3 类 row source x 3 个规模。 | `src/bench_tesvd_scale.cpp` |
| evidence target | 新增 QEMU row-source smoke、board row-source repeated、doctor 和 registry target。 | `Makefile` |
| evidence wording | board summary wrapper 对 `production_public_row_source` 输出生产公开 row-source 证据语义。 | `script/generate_tesvd_scale_board_repeated_summary.py` |

## 计划回填

| 计划项 | 结果 | 证据 |
| --- | --- | --- |
| production overload | 完成。三类 row source 命中 RVV gate 时直接走 scale RVV 累加；否则显式回父类 overload。 | 源码 diff；`run_test_compare_recorded`。 |
| correctness | 完成。Std/RVV 各 11 个 gtest 全通过。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log`。 |
| QEMU smoke | 完成。`row-source-scale` case-filter 可解析，QEMU timing 只作日志形状证据。 | `log/qemu/row_source_scale/analyze_bench_compare.log`、`log/qemu/row_source_scale/evidence_doctor.md`。 |
| board repeated | 完成。5-run repeated 覆盖 9 个 row-source case，全部 median bucket 为 `positive`。 | `log/board/row_source_scale_repeated/summary.md`。 |
| Evidence Doctor | 完成。QEMU row-source smoke 为 `Errors=0`、`Warnings=0`；board row-source 为 `Errors=0`、`Warnings=7`。 | `log/qemu/row_source_scale/evidence_doctor.md`、`log/board/row_source_scale_repeated/evidence_doctor.md`。 |
| registry | 完成。`evidence_status` fresh。 | `log/evidence_registry.json`。 |

## Row-source 结果矩阵

| row source policy | public entry / timing boundary | QEMU correctness | QEMU smoke | board median B/A | board doctor | decision |
| --- | --- | --- | --- | --- | --- | --- |
| source-indexed | `cloud_src + indices_src + ordered selected target` public overload | passed | clean | 4K `12.571x`，64K `16.309x`，256K `16.025x` | group-outlier warnings；所有 case 仍 positive | `adopted-by-user` |
| dual-indexed | `cloud_src + indices_src + cloud_tgt + indices_tgt` public overload | passed | clean | 4K `9.644x`，64K `9.726x`，256K `11.089x` | 256K long-tail warning；所有 case 仍 positive | `adopted-by-user` |
| correspondence | `cloud_src + cloud_tgt + correspondences` public overload | passed | clean | 4K `9.729x`，64K `6.171x`，256K `8.395x` | 64K/256K long-tail 和 64K group-outlier warning；所有 case 仍 positive | `adopted-by-user` |

Board summary 中的 `log checksum` 为 mismatch 是预期风险：RVV reduction tree（规约树）与标量路径不同，正确性以 gtest 和 `max_reference_error <= 2e-3` 为准。row-source board 最大 `max_reference_error` 为 `3.040e-05`，低于当前预算。

## Evidence Doctor 处理

QEMU row-source smoke doctor 为 `Errors=0`、`Warnings=0`、`Suggestions=0`。

Board row-source doctor 为 `Errors=0`、`Warnings=7`、`Suggestions=0`。这些 warning 不阻塞当前
positive bucket，但必须限制结论边界：

- source-indexed 4K/64K/256K 是组内高收益离群，不把它外推到 dual-indexed 或 correspondence。
- correspondence 64K/256K 有长尾，结论使用 median/min/max 全量报告，不剔除低值 run。
- dual-indexed 256K 有轻微 long-tail，仍按该 row source 自己的 repeated 结果判断。

因此本阶段只证明这 9 个 public row-source case 在当前板卡 repeated 预算内均为 positive，不证明全部点型、全部 index 分布、非法 index / correspondence、`Scalar=double` 或非 dense 输入。

## Diagnostic-to-Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `production_public_row_source`。生产源码补丁触碰真实公开入口，且用户已确认采纳当前有收益实现。 |
| A/B boundary | Std 构建为父类 scale 标量公开入口；RVV 构建为 scale 子类 row-source RVV 公开入口。 |
| 当前决策问题 | public RVV path 是否在 source-indexed、dual-indexed 和 correspondence 上快于 public scalar path。 |
| diagnostic 是否可外推到 production | QEMU smoke 不可外推性能；board repeated 支撑当前 public row-source production patch 的性能判断。 |
| comparison-boundary / baseline mismatch 风险 | 有。source-indexed、dual-indexed 和 correspondence 的 gather / correspondence 读取成本不同，必须分 row source 和 size 报告。 |
| weak / negative / unstable 时是否允许 bounded production probe | 本阶段就是有界 production probe；当前结果为 positive，并已在用户确认后采纳。未来新 row-source family 若出现 weak / negative / unstable，只影响对应新 family。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前 row-source scale 子类没有既有 adopted RVV family；不需要 RVV-vs-RVV 选择，但后续若出现 staged gather / locality sort 等同 row source 新 family，则需要同边界 A/B。 |

## 后续候选

| candidate | why now | state | resume condition |
| --- | --- | --- | --- |
| `row-source-generic-point-type-expansion` | 当前 row-source board 只覆盖 `PointXYZ -> PointXYZ`。 | `completed_positive_in_phase_044` | Phase 044 已按 `PointXYZI` / `PointXYZRGB` 代表点型补同边界 correctness / board。 |
| `row-source-locality-order-profile` | correspondence 64K/256K long-tail 说明 index / correspondence 局部性可能影响稳定性。 | `phase_deferred + unblocked` | 新增 locality/order profile phase，解释 warning 来源和是否需要后续缓解。 |
| `row-source-invalid-input-fallback` | 当前测试不覆盖非法 index / correspondence；生产 gate 依赖父类语义。 | `phase_deferred_after_locality_profile` | 若准备最终提交前扩大边界，可补非法输入语义审计或明确保持父类边界。 |

## 验证命令

```bash
make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare_recorded
make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_row_source_state
make -C test-rvv/registration/transformation_estimation_svd_scale record_board_row_source_state
make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_matrix_local_scale_smoke_state
make -C test-rvv/registration/transformation_estimation_svd_scale evidence_status
```

本阶段最后一次 registry 检查为 fresh。

## 停止 / 继续判断

用户已确认采纳本阶段 row-source patch，因此 Phase 043 用户检查点已关闭。Phase 044 已补代表泛型点型同边界证据；
optimization matrix 仍保留 `row-source-locality-order-profile` 作为当前授权范围内的 `phase_deferred + unblocked`
下一动作，用来解释 Phase 043 / 044 的 long-tail 和 group-outlier warning。
