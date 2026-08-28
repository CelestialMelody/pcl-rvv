# Phase 020 Plan: point type expansion

## 阶段意图和边界

本阶段验证 `SampleConsensusModelPlane<PointT>` 当前 production RVV（生产 RVV）分流在更多
registered single-float xyz AoS（已注册单精度 xyz 结构数组）点型上的 correctness（正确性）。
算法只读取 `x/y/z` 并输出 index、count 或 distance，不构造完整 `PointT` 输出，因此 intensity、
RGB、normal 等额外字段不参与当前距离语义。

本阶段不改变 public API，不修改 production helper，不扩大到 correspondence row source、
`Scalar=double`、非 AoS layout、其它 SAC 模型或真实自定义点类型全集。

## 当前状态

Phase 000/010 已证明：

- `PointXYZ` 有 QEMU correctness、反汇编和板卡性能证据。
- `PointXYZI` 已有 public entry correctness。
- 生产代码使用 `RVVXYZAoSFloatLayout<PointT>` gate，不是 exact `PointXYZ` gate。

未闭合项是更多常见 PCL xyz 点型的 dispatch/correctness 证据，例如 `PointXYZRGB`、
`PointXYZRGBA` 和带 normal 的 xyz 点型。

## validated_scope / unvalidated_scope

| field | scope |
| --- | --- |
| validated_scope | `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA`、`PointXYZINormal` 的 public entry 与 Standard path 对拍；direct indexed shuffled 与 identity 两类 indices。 |
| unvalidated_scope | 自定义点型全集、非 AoS layout、非 float xyz、`Scalar=double`、其它 row source 和这些点型的 dedicated board performance。 |
| point_type_expansion_queue | 若 correctness 通过，再决定是否给 `PointXYZRGB/RGBA` 或 `PointXYZINormal` 增加 dedicated board bench；若编译失败，回退到 traits gate / field access 审计。 |
| phase_closeout_boundary | 本阶段只能关闭代表性点型 correctness / dispatch，不能把性能结论外推到所有点型。 |

## 优化矩阵

| candidate family | row source | point type / layout | correctness | bench / board | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| AoS xyz point type expansion | shuffled direct cloud | `PointXYZRGB` / `PointXYZRGBA` / `PointXYZINormal` | planned: public entry vs Standard | not planned unless correctness passes and board budget is assigned | compile/path evidence from RVV build | not_applicable for correctness-only | pending |
| identity select/count dispatch on wider point types | identity direct cloud | same representative AoS types | planned: public entry vs Standard | deferred performance | compile/path evidence from RVV build | not_applicable for correctness-only | pending |

## 实现和测试动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| add representative point fixtures | `src/test_sac_model_plane.cpp` | 点云构造可生成 `PointXYZRGB`、`PointXYZRGBA`、`PointXYZINormal`。 |
| add correctness tests | `src/test_sac_model_plane.cpp` | shuffled 和 identity 两类 case 都与 Standard path 对拍。 |
| QEMU correctness | `run_test_compare` | Std/RVV 两个构建全部 gtest 通过。 |
| board correctness smoke | `board_smoke` 或 `run_board_base_plane_public_tests` | 若板卡可用，至少跑 RVV gtest；bench 数字不作为本阶段性能结论。 |
| docs / matrix update | phase result、roadmap、matrix、evaluation | 明确代表点型 correctness 与未验证性能范围。 |

## Evidence Doctor 和 registry

本阶段默认 correctness-only（只验证正确性），不生成新的性能 summary，不要求 Evidence Doctor。
如果追加 dedicated board bench，必须先扩展 bench case label、manifest 和 Evidence Doctor 输入。

## 板卡复跑预算和决策桶

Correctness smoke 不使用性能 decision bucket。若后续升级为 point-type performance phase，初始预算为
每个代表点型 5-run repeated board，positive / weak / neutral / negative 阈值另写新 plan。

## 继续 / 停止条件

若 QEMU 或板卡 correctness 失败，先修复测试或 traits gate。若 correctness 通过但尚未做 dedicated
board performance，只能写成代表点型 correctness adopted，不能写成完整泛型性能 adopted。
