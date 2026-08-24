# Phase 068: Generic Scalar Double Ordered Production Probe Plan

## 阶段意图和边界

本阶段继续 `Scalar=double` 方向，但只把 Phase 065/066 的 exact `PointXYZ -> PointXYZ` ordered double branch 扩到 ordered-cloud-pair（顺序点云对）的常见 PCL xyz AoS（xyz 结构数组）点型。

validated_scope：

- public ordered overload：`estimateRigidTransformation(source, target, Matrix4d)`。
- `Scalar=double`。
- `PointSource` 和 `PointTarget` 满足 `RVVXYZAoSFloatLayout`，即 PCL traits 注册的单个 `float x/y/z` 字段，且可按 AoS byte offset 读取。
- 本阶段代表组合：`PointXYZI -> PointXYZI`、`PointXYZRGB -> PointXYZRGB`、`PointXYZI -> PointXYZRGB`、`PointXYZRGBA -> PointXYZRGBA`、`PointNormal -> PointXYZRGB`。
- dense 输入且 `nr_points >= 16`。

unvalidated_scope：

- source-indexed、dual-indexed 和 correspondence 的 generic double。
- custom layout double、padding / alignment double 采样。
- sorted-copy double 或 row-source mitigation double。
- 非 dense、小规模、source / target size 不等、非法 index / correspondence。
- 非 `float x/y/z` 字段、非 AoS layout 或任意未审计自定义点型全集。

phase_closeout_boundary：本阶段只能关闭 ordered generic double production probe 的代表点型 correctness、QEMU smoke、board repeated 和 evidence registry 边界；不能把结论外推到 row-source generic double 或 custom layout double。

## 当前状态清单

| area | current state |
| --- | --- |
| production ordered double | Phase 065/066 已采纳 exact `PointXYZ -> PointXYZ` / `Scalar=double` / dense / `nr_points >= 16`。 |
| production row-source double | Phase 067 已采纳 exact `PointXYZ -> PointXYZ` 的三类 row-source double。 |
| float generic ordered | Phase 040/041/051/052 已证明 traits-gated xyz AoS `float` ordered public path 在多种 common PCL 点型上 positive。 |
| double generic gap | roadmap 仍写明 exact double gate 不能证明 `PointXYZI`、`PointXYZRGB`、common PCL xyz AoS 或 custom layout double。 |
| tests | 当前 Std/RVV correctness 预计从 29 tests 扩到 31 tests。 |
| evidence policy | summary-only；raw logs 不默认提交。 |

## 假设与候选族

candidate family：`generic-scalar-double-ordered-production-probe`。

假设：ordered double 当前 RVV f64 widened accumulation 只读取 source / target 的 `x/y/z` float 字段；因此可以把 exact `PointXYZ` helper 泛化为 `SrcLayout` / `TgtLayout` offsets，而不改变 solver、double reduction tree（双精度规约树）或 fallback 语义。

主要风险：

- traits gate 只证明字段和布局，不证明所有自定义点型性能。
- mixed source / target 点型 stride 可能改变 load 指令形态和收益。
- double branch 若误覆盖 non-dense、小规模或不等长输入，会破坏 fallback 边界。

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `generic-scalar-double-ordered-production-probe` | ordered-cloud-pair | common PCL xyz AoS / `Scalar=double` | public ordered overload | 新增 generic double ordered gtest；fallback 覆盖 small / non-dense / unsupported layout | 新增 `generic-scalar-double-ordered-production-probe` case-filter | 64K repeated，5 runs，positive bucket 目标 | QEMU / bench RVV asm 输入需覆盖 f64 widened accumulation 相关指令 | QEMU 和 board Doctor 需解释 Error / Warning / Suggestion | planned |

## 实现和测试动作

1. 新增 `GenericScalarDoubleOrderedProductionProbeMatchesReference`，验证 common PCL xyz AoS double public path 与 double reference 对齐。
2. 新增 `GenericScalarDoubleOrderedProductionProbeFallbackBoundaries`，覆盖 small、non-dense 和 unsupported layout fallback boundary。
3. 实现 production patch：把 ordered double exact `PointXYZ` accumulation 泛化为 `PointSource` / `PointTarget` + `SrcLayout` / `TgtLayout` offsets；保留 row-source double exact gate 不变。
4. 补 bench case-filter：新增 ordered generic double 代表组合，输出 max public error 和 path label。
5. 运行 correctness：`make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare`。
6. 运行 QEMU smoke / Doctor / registry：`record_qemu_generic_scalar_double_ordered_production_probe_state`；QEMU timing 不写性能结论。
7. 运行 board repeated：5-run 64K repeated；记录 min / median / max、bucket 和 Doctor。
8. 更新 result、optimization matrix、roadmap、README、testing/evidence docs 和长期 `doc-rvv` 的 adopted behavior 边界。

## Evidence Doctor 和 Registry 规则

- QEMU Doctor 预期 `Errors=0`；Warnings 必须解释并决定是否降级。
- Board Doctor 预期没有 checksum / boundary Error；variance 或 group-outlier warning 若出现，按点型组合保留 min / median / max。
- `make -C test-rvv/registration/transformation_estimation_svd_scale evidence_status` 必须 fresh，或在 result 中标记 stale / refresh pending。

## 板卡复跑预算和决策桶

- runs：5。
- warm-up：沿用 topic 默认 `TESVD_SCALE_BOARD_BENCH_WARMUP_ITERATIONS=5`。
- iterations：沿用 topic 默认 `TESVD_SCALE_BOARD_BENCH_ITERATIONS=20`。
- positive：所有代表组合 median B/A 明确大于 `1.20x` 且没有 checksum / boundary Error。
- weak-positive：`1.05x <= median < 1.20x`，只能作为 bounded candidate，不 clean adopt。
- negative：median < `1.00x` 或退化频率过高。
- unstable：min/max 或 below-1 run 使 decision bucket 摇摆；预算耗尽后降级，不无限复跑。

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `production-public`。 |
| A/B boundary | Std build public generic double fallback vs RVV build public generic double branch。 |
| 当前决策问题 | `RVV-vs-scalar` 和 fallback correctness。 |
| diagnostic 是否可外推到 production | 本阶段不依赖 diagnostic 外推；目标是 public overload 证据。 |
| comparison-boundary / baseline mismatch 风险 | 若 bench case 只覆盖 test helper 而非 public overload，则必须降级；计划要求 public estimator 直连。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不适用；本阶段就是 production probe。若结果弱 / 负 / 不稳定，保留 patch 等待用户判断或回滚授权。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要。generic double ordered 此前没有已采纳 RVV family；当前问题是 public RVV 是否快于 public scalar。 |

## 继续 / 停止条件

继续条件：

- production patch 后 correctness、QEMU、board 和 Doctor 能形成同边界 evidence。
- roadmap / matrix 仍有本阶段内未阻塞动作。

停止条件：

- production patch 需要扩大到 row-source generic double、custom layout double 或其它 public API。
- QEMU / board / Evidence Doctor 出现无法同轮解释的 Error。
- board 不可用且本阶段已完成本地 / QEMU 可运行证据。

## 文档更新清单

- `doc/phases/068-generic-scalar-double-ordered-production-probe/result.zh.md`。
- `doc/phases/optimization-matrix.zh.md`。
- `doc/phases/README.zh.md`。
- `doc/optimization-roadmap.zh.md`。
- topic-local README / testing / correctness / benchmark / optimization evidence / evaluation。
- `doc-rvv/registration/transformation_estimation_svd_scale-RVV.zh.md`：本阶段 production evidence positive 且用户已确认当前有收益实现可以接入时，写成 adopted behavior；否则写成 pending 边界。

## Roadmap 同步动作

本阶段若 positive：下一阶段默认候选是 row-source generic double 或 custom layout double，二者必须另开 phase。若 negative / unstable：roadmap 应把 generic double ordered 标为 attempted，并保留 exact `PointXYZ` double branch。
