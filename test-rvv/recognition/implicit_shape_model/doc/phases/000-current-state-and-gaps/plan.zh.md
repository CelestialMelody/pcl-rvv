# Phase 000: current state and ISM local formula diagnostic plan

## 阶段意图和边界

本阶段把 `implicit_shape_model.hpp` 从筛选清单推进到可执行 topic。阶段只证明三个
local formula diagnostic（局部公式诊断）是否值得继续：descriptor-to-cluster distance
（描述子到聚类中心距离）、`calculateSigmas()` 的 pairwise max-dot（两两点积最大值）
和 vote density Gaussian sum（投票密度高斯加权求和）。不修改 production 源码，不改变
公开 API，不证明完整 `trainISM()` / `findObjects()` 的收益。

## 当前状态清单

| area | 当前状态 | 证据 / 路径 |
| --- | --- | --- |
| production source | 尚无 RVV patch | `recognition/include/pcl/recognition/impl/implicit_shape_model.hpp` |
| upstream test | `test_recognition_ism.cpp` 覆盖 train/findObjects 功能，但不提供 profile | `test/recognition/test_recognition_ism.cpp` |
| topic scaffold | 本 phase 新建 | `test-rvv/recognition/implicit_shape_model/` |
| doc-rvv | 不适用 | 没有 adopted production behavior |
| board | 配置可用但未对本 topic 取证 | `test-rvv/config.mk` 只作为本机配置，不提交 |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| descriptor cluster distance | `FeatureSize=153` 的 L2 distance 可用 RVV sum reduction 降低逐 cluster 成本 | 完整入口里 feature estimation 和 KMeans 可能主导 |
| sigma pairwise max-dot | 内层 `j` loop 可用跨步加载和 max reduction | 原源码公式是点积后 sqrt，不是欧氏距离；只按现有语义复刻 |
| vote density Gaussian sum | `expf_RVV_f32m2` 可替代局部 `std::exp` 近似诊断 | production 使用 double `std::exp`，float helper 只能作为诊断候选，不能直接接入 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | test | bench / board | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| descriptor cluster distance | single descriptor vs contiguous centers | synthetic float descriptor / `FeatureSize=153` / contiguous | `run_test_compare` | `board_repeated` | `check_ism_rvv_asm` | `evidence_doctor_repeated` | planned |
| sigma pairwise max-dot | single training cloud pairwise points | synthetic PointXYZ-like AoS float | `run_test_compare` | `board_repeated` | `check_ism_rvv_asm` | `evidence_doctor_repeated` | planned |
| vote density Gaussian sum | radiusSearch output distances + strengths | synthetic float arrays | `run_test_compare` | `board_repeated` | `check_ism_rvv_asm` | `evidence_doctor_repeated` | planned |

## 实现和测试动作

1. 新建 `include/ism.h` 和 `include/impl/ism_diagnostics.hpp`，只放测试专用 diagnostic helper。
2. 新建 `src/test_ism.cpp`，对拍 Std/RVV 同构链路。
3. 新建 `src/bench_ism.cpp`，输出共享 analyzer 可解析的 bench 行和 checksum。
4. 新建 `Makefile`、`board.mk` 和 `script/generate_ism_evidence_manifest.py`。
5. 运行 `make -C test-rvv/recognition/implicit_shape_model run_test_compare`。
6. 运行 `make -C test-rvv/recognition/implicit_shape_model check_ism_rvv_asm`。
7. 板卡可达时运行 `board_repeated`、生成 manifest、Evidence Doctor 和 registry。
8. 更新 phase result、evaluation、roadmap、matrix、README 和筛选状态。

## Evidence Doctor 和 registry 规则

Repeated board summary 位于 `log/board/repeated_phase000_ism_local_formula_diagnostic/summary.md`；
manifest、doctor 和 registry 分别位于同目录 `evidence_manifest.json`、`evidence_doctor.md` /
`evidence_doctor.json` 和 `log/evidence_registry.json`。任何 Error 必须先修复或降级；Warning
必须解释，Suggestion 可进入 roadmap。

## 板卡复跑预算和决策桶

默认 `REPEATED_BOARD_RUNS=5`，`iterations=100`，`warmup_iterations=5`。decision bucket
使用 manifest 脚本规则：median `>=1.20x` 且无退化为 positive，`1.05x-1.20x` 为 weak_positive，
`0.95x-1.05x` 为 neutral，`<0.95x` 为 negative，否则 unstable。预算耗尽后若桶稳定即关闭本阶段证据；
若桶摇摆，标为 unstable 并暂停生产接入判断。

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | diagnostic |
| A/B boundary | test helper |
| 当前决策问题 | implementation-shape 和 RVV-vs-scalar 局部诊断 |
| diagnostic 是否可外推到 production | no；完整入口包含 feature estimator、VoxelGrid、KMeans、radiusSearch、`nth_element` 和状态更新 |
| comparison-boundary / baseline mismatch 风险 | yes；局部 synthetic 输入不能代表真实 train/findObjects 分布 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no；ISM 筛选清单要求先证明 profile 条件 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes；如果未来进入 production，还需要真实公开入口和 production direct board 证据 |

## Phase scope 与扩展队列

- validated_scope：测试专用 synthetic float、`PointXYZ`-like AoS、`FeatureSize=153` 局部公式。
- unvalidated_scope：真实 FPFH descriptor、真实 training objects、KMeans label 分布、`PointT` 泛型、`NormalT`、
  `Scalar=double`、完整 `trainISM()`、完整 `findObjects()`、vote list tree state。
- point_type_expansion_queue：当前 not_applicable；尚无 production patch。
- phase_closeout_boundary：只能关闭局部公式诊断矩阵，不能关闭 topic-level production 结论。

## 继续 / 停止条件

如果局部公式在板卡 repeated 中至少一个稳定 positive，且 checksum / asm / doctor 闭合，继续
`010-production-shaped-ism-subkernel-diagnostic`，用真实入口形态或 profile 证明子核占比。如果全部
neutral / negative，或 Evidence Doctor / 板卡显示证据不可靠，则暂停并整理 no-production diagnostic
closeout。
