# Phase 020 Plan: Alpha M Formula Audit

## 阶段意图和边界

本阶段先审计 `alpha_m` 的公式形态，不直接修改 production（生产源码）。目标是判断当前
Eigen `AngleAxisf` / `Affine3f` 标量链路是否能用 closed-form formula（闭式公式，直接用向量代数表达）
复刻，并作为后续 RVV candidate（RVV 候选实现）的 same-chain oracle（同构链路参考）。

validated_scope：`PointXYZ` + `Normal`、float、AoS（结构数组）、非 identity pair、normal 已归一化或接近归一化。

unvalidated_scope：泛型点类型、非有限输入、非归一化 normal、`Scalar=double`、production dispatch、
`PPFRGB`/`CPPF`、真实 production direct bench。

phase_closeout_boundary：只能关闭 `alpha_m closed-form scalar audit` 的 correctness 和静态公式可行性；不能关闭
RVV `alpha_m` 性能，也不能进入 production integration loop。

## 当前状态清单

| area | 状态 |
| --- | --- |
| Phase 010 | SoA-staged pair-feature batch RVV correctness 成立，但 5-run board repeated 为 negative。 |
| `alpha_m` reference | `include/impl/ppf_reference.hpp::computeAlphaMReference` 复刻 production Eigen 链路。 |
| RVV math helper | `common/include/pcl/common/impl/rvv_math.hpp` 已有 `acos_RVV_f32m2`、`atan2_RVV_f32m2` 和有限域 `sincos_finite_domain_RVV_f32m2`，但本阶段先不写 RVV kernel。 |
| board | 本阶段默认不跑板卡，除非新增 candidate 进入 bench；只做公式可行性审计。 |

## 假设与候选族

`alpha_m` 当前把 reference normal 旋转到 x 轴，再对 `model_point - reference_point` 的旋转后 y/z 分量取
`atan2`。若用 Rodrigues rotation formula（罗德里格旋转公式）直接计算 y/z 分量，可以避免每个 pair 构造
Eigen 对象，并给后续 RVV lane（向量通道）公式提供明确输入。

风险是 `normal` 接近 `-x` 或正好平行 x 轴时公式存在分母或符号边界；本阶段必须先覆盖 parallel-to-x、
near-x、general normal 和 production fixture 样本，确认闭式公式是否和 reference 一致。

## 实现和测试动作

| 动作 | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| RED: closed-form 测试 | `src/test_ppf.cpp` 新增 `PPFAlphaM.ClosedFormMatchesEigenReference` | `make -C test-rvv/features/ppf run_test_std` 编译失败，缺少 `computeAlphaMClosedForm`。 |
| GREEN: 实现 test-only helper | `include/impl/ppf_alpha_candidate.hpp`、`include/ppf.h` | 标量闭式公式通过 Std/RVV correctness；不改 production。 |
| 数值预算 | `src/test_ppf.cpp` | 对常规样本用 `1e-5`，平行 x 轴样本用显式有限值检查；若 near `-x` 不稳定，记录为 unsupported boundary。 |
| 文档回填 | `result.zh.md`、matrix、roadmap、evaluation | 若 closed-form 不成立，拒绝 `alpha_m` RVV；若成立，把 RVV `alpha_m` candidate 作为下一 phase。 |

## Evidence Doctor 和 registry 规则

本阶段若只跑 correctness，不需要 Evidence Doctor。若临时增加 bench 或 board summary，必须生成 manifest 并运行
`evidence_doctor_repeated` 或等价 target。

## Continue / Stop 条件

默认继续到 RED/GREEN correctness。合法停止条件是 closed-form 无法在 reference 误差预算内成立、公式边界不清、
或继续需要修改 production。若 helper 成立但尚未 bench，下一阶段默认是 `030-alpha-m-rvv-candidate`；
若 helper 不成立，下一阶段默认是 structure-parity-doc-suite closeout。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic correctness。 |
| A/B boundary | test helper vs test helper。 |
| 当前决策问题 | implementation-shape。 |
| diagnostic 是否可外推到 production | no。本阶段只证明或拒绝公式等价性。 |
| comparison-boundary / baseline mismatch 风险 | yes。closed-form helper 不是 production dispatch，也不是板卡性能证据。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no。公式 correctness 不成立时先停止该候选。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes，若后续进入生产接入。 |
