# Phase 090: stick point type expansion plan

## 阶段意图和边界

本阶段验证 Phase 080 已采纳的 production RVV（RISC-V Vector，可变长向量扩展）路径在更多常见 xyz AoS（结构数组）点型上的 correctness（正确性）边界。生产实现已经使用 `RVVXYZAoSFloatLayout<PointT>` traits gate（字段特征门控），因此代码可命中范围大于 Phase 080 的 `PointXYZ` 板卡性能证据。

本阶段不修改 production 源码，不改变 public API（公开接口），不声明其它点型已有 dedicated board performance（专门板卡性能）结论。当前验证范围是 `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` 和 `PointXYZRGBNormal` 的 public entry vs Standard helper 对拍，覆盖 direct indexed `indices_`。

## 当前状态清单

| 项 | 当前状态 |
| --- | --- |
| production | Phase 080 已接入 `countWithinDistance`、`selectWithinDistance` 和 `getDistancesToModel` 三条 public entry。 |
| correctness | 阶段开始前 `run_test_compare` Std/RVV 各 10 个 gtest，主覆盖 `PointXYZ`；本阶段结束后应增长为 11 个。 |
| board performance | Phase 080 `PointXYZ` production public count/select/getDistances median 为 4.1729x / 3.3023x / 2.5883x。 |
| asm | `check_production_asm` 已证明三个 production RVV helper 有 RVV 指令。 |
| Evidence Doctor | Phase 080 production doctor 为 Errors=0、Warnings=0、Suggestions=0。 |
| 未闭合范围 | 其它满足 xyz AoS traits gate 的点型 correctness 和 dedicated performance。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| point-type-expansion | direct indexed `indices_` | `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA`、`PointXYZRGBNormal`；`Eigen::VectorXf`；xyz AoS | public count/select/getDistances | planned: public vs Standard helper for all three entries | not planned in this phase | optional board gtest smoke after QEMU pass | production helper symbols unchanged | not_applicable unless new board performance manifest is collected | planned |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| add access wrapper and typed fixtures | `src/test_sac_model_stick.cpp` | 可对 `PointXYZI` / RGB / RGBA / RGBNormal 构造同一几何输入。 |
| add point-type correctness test | `AdditionalAoSPointTypesMatchStandardPath` | 每个点型的 public count/select/getDistances 与 Standard helper 输出一致。 |
| add target alias | `Makefile` | 新增 `run_stick_point_type_tests`，只跑点型扩展 case。 |
| run correctness | `make -C test-rvv/sample_consensus/sac_model_stick run_test_compare` | Std/RVV 两个构建通过。 |
| run focused alias | `make -C test-rvv/sample_consensus/sac_model_stick run_stick_point_type_tests` | RVV 构建点型扩展 case 通过。 |
| update docs | phase result、README、correctness、overview、roadmap、matrix、`doc-rvv` | 明确 adopted-for-correctness，不外推 dedicated performance。 |

## Evidence Doctor 和 registry 规则

本阶段默认不生成新的 board performance manifest，因此 Evidence Doctor 不适用。若后续决定做 dedicated board performance，必须另写或修订 Phase 090 plan，补 point-type-specific bench label、manifest、Evidence Doctor 和 registry。

## 阶段完成条件

QEMU Std/RVV correctness 和 focused RVV alias 都通过时，本阶段可关闭为 `adopted_for_representative_point_type_correctness`。若编译失败，先判断是 traits gate、PCL 显式实例、测试构造还是 helper 可见性问题；不能把失败点型写成已覆盖。

## 板卡复跑预算和决策桶

本阶段没有 dedicated board performance 预算。允许在 correctness 通过后运行 board gtest smoke（板卡 GoogleTest 小型验证），只作为可运行性证据，不写性能 speedup。

## 继续 / 停止条件

如果点型 correctness 通过，当前 topic 的代码范围和 correctness 证据范围更一致；remaining performance expansion（剩余性能扩展）需要新 bench 设计和板卡预算，可作为后续 scope。若当前阶段失败且无法在测试资产内修复，停止并记录 blocker。

## 文档更新清单

更新 `result.zh.md`、`doc/correctness-tests.zh.md`、`doc/testing-overview.zh.md`、`doc/optimization-evidence.zh.md`、`doc/optimization-roadmap.zh.md`、`doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md` 和 `doc-rvv/sample_consensus/sac_model_stick-RVV.zh.md`。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production correctness，不是 performance。 |
| A/B boundary | public entry vs Standard helper in the same typed model。 |
| 当前决策问题 | traits-gated point type correctness coverage。 |
| diagnostic 是否可外推到 production | 本阶段直接调用 production public entry；不是 diagnostic 外推。 |
| comparison-boundary / baseline mismatch 风险 | public entry 和 Standard helper 在同一模型状态下运行，风险低。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不适用；没有性能 decision bucket。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 不适用；不做 RVV family selection（实现族选择）。 |
