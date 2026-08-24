# Phase 040 计划：generic point type expansion correctness scout

## 阶段意图和边界

本阶段在 Phase 031 adopted production behavior 之后，推进 optimization matrix 中的 `generic-point-type-expansion`。目标不是扩大已采纳的板卡性能结论，而是先证明当前 production public path 在代表 xyz AoS 点型上能正确运行。

## 验证范围

| 维度 | 本阶段范围 |
| --- | --- |
| evidence role | production public correctness scout。 |
| row source | ordered-cloud-pair。 |
| point type | `PointXYZI -> PointXYZI`、`PointXYZRGB -> PointXYZRGB`、`PointXYZI -> PointXYZRGB`、`PointXYZRGB -> PointXYZ`。 |
| Scalar | `float`。 |
| layout | `pcl::rvv::RVVXYZAoSFloatLayout<PointSource/PointTarget>` 必须为 true。 |
| size | 8192 点 correctness 样本。 |
| production source | 不修改 production；只验证当前已采纳 traits-gated dispatch。 |

不证明的范围：generic point type board performance、generic point type ASM attribution、所有 PCL xyz 点型、用户自定义点型、source-indexed / dual-indexed / correspondence、`Scalar=double`。

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| 泛型 fixture | `include/impl/tesvd_scale_support.hpp` 增加 `makePointCloudXYZ` / `transformCloudXYZTo`。 | 可以构造 source / target 不同点型的 deterministic xyz 样本。 |
| 泛型标量 reference | `include/impl/tesvd_scale_candidates.hpp` 中 `accumulateScaleStd` / `estimateScaleStd` / `estimateScalePublic` 模板化。 | public path 和同构标量 reference 可对拍不同 `PointSource` / `PointTarget`。 |
| correctness TEST | `GenericXYZPointTypesMatchReference`。 | Std/RVV 构建均通过，误差预算 `5e-4`。 |
| registry | `record_qemu_correctness_state`。 | registry 记录当前 8-test correctness。 |
| docs | phase result、matrix、roadmap、README、evaluation、correctness docs。 | 不把 correctness scout 写成 board performance adoption。 |

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production public correctness scout。 |
| A/B boundary | public overload correctness；不是 board performance A/B。 |
| 当前决策问题 | 当前 traits-gated production dispatch 是否能正确覆盖代表 xyz AoS 点型。 |
| 是否可外推到 production | yes for correctness on tested representative types；no for all point types or performance。 |
| comparison-boundary / baseline mismatch 风险 | low for correctness；performance 仍未知。 |
| weak / negative / unstable 时是否允许继续 board probe | 若 correctness 失败，不进入 board；若 correctness 通过，可另建 board/asm phase。 |
| clean adoption 是否需要 RVV-vs-RVV detail A/B | no；当前是已采纳 family 的覆盖扩展，不做 family selection。 |

## 继续 / 停止条件

若 QEMU correctness 通过，本阶段可关闭为 `correctness_scout_positive_board_pending`，下一步是 generic point type board / ASM phase。若 correctness 失败，先回退到 traits gate 或 fixture/reference 修正，不扩大 production 范围。
