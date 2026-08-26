# Phase 000 Plan: Current State And Gaps

## 阶段意图和边界

本阶段开启 `features/include/pcl/features/impl/cppf.hpp` 的 RVV topic（主题），先建立 CPPF（Colored Point Pair Feature，彩色点对特征）的函数级评估、测试支撑骨架和第一条 component ablation（组件消融）证据链。本阶段不修改 production（生产源码），不声明 production-ready（可接入生产），只验证测试专用候选能否复刻当前 `CPPFEstimation::computeFeature` 的 all-pairs output（所有点对输出）、identity pair（同一点对）NaN、HSV conversion（RGB 到 HSV 转换）和 `alpha_m` 语义。

| scope item | 本阶段覆盖 | 本阶段不覆盖 |
| --- | --- | --- |
| entry | `CPPFEstimation<PointXYZRGBNormal, PointXYZRGBNormal, CPPFSignature>::computeFeature` 的 caller-shaped diagnostic（调用方形态诊断） | production dispatch（生产分流）和 public API（公开接口）改动 |
| row source | 默认 full cloud sequential indices（完整点云顺序索引）和 prefix indices（前缀索引） | 自定义乱序 indices、非 dense 输入、其它 row source policy（行来源策略） |
| point type / Scalar / layout | exact `PointXYZRGBNormal`、`float`、PCL 默认 AoS（结构数组）布局 | 泛型 RGB/normal traits、`Scalar=double`、其它彩色点型组合 |
| candidate family | test-only SoA staging（测试专用分字段暂存）+ RVV pair feature / HSV / `alpha_m` 候选 | production helper、output scatter 并行化、真实上游生产直连 |

## 当前状态清单

| area | 当前事实 | 路径 |
| --- | --- | --- |
| production source | `computeFeature` 双层循环对 `indices_ × input_` 输出，每个 row 都 `push_back`；同点对写 `f1..f10/alpha_m = NaN` 并把 `is_dense=false`。非同点对调用 `computeCPPFPairFeature` 后再用 Eigen `AngleAxisf/Affine3f` 计算 `alpha_m`。 | `features/include/pcl/features/impl/cppf.hpp` |
| shared helper | `computeCPPFPairFeature` 计算距离、两个 normal-dot、normal dot normal、两端 HSV。该 helper 没有本地批量 loop，必须随 caller topic 取证。 | `features/src/cppf.cpp` |
| upstream test | 上游 `test_cppf_estimation` 依赖外部 `colored_cloud.pcd`，只抽查输出大小、row 0 NaN 和 row 2572 数值。 | `test/features/test_cppf_estimation.cpp` |
| sibling evidence | PFH/PFHRGB 已证明 direct AoS pair math + scalar scatter 可在有界 production 中成立；PPF 本地未跟踪资产提供同族结构参照，但不作为 CPPF 已提交事实。 | `doc-rvv/features/pfh-RVV.zh.md`、`doc-rvv/features/pfhrgb-RVV.zh.md`、`test-rvv/features/ppf/**` |
| evidence registry | CPPF topic 尚无 registry；本阶段先创建 manifest/doctor 入口，registry 状态为 `not_available`，用人工路径扫描补足。 | `test-rvv/features/cppf/log/evidence_registry.json` 不存在 |

## 假设与候选族

| candidate family | 假设 | 风险 | 需要证据 |
| --- | --- | --- | --- |
| `cppf-reference` | 测试专用标量 reference 能完全复刻 caller 输出语义，是后续 RVV same-chain（同构链路）基线。 | HSV 分支和 `alpha_m` 与 production 不一致会污染所有候选。 | gtest 对拍 production public entry 和 reference。 |
| `cppf-pair-hsv-batch-rvv` | 将非 identity pair 收集到 SoA staging 后，用 RVV 批量计算 `f1..f10`，`alpha_m` 先保留标量，可隔离 colored pair math 和 HSV 成本。 | staging 内存流量和标量 output 写回可能吞掉收益；HSV 分支有 lane mask（向量通道掩码）复杂度。 | QEMU correctness、RVV asm、board repeated bench 和 Evidence Doctor。 |
| `cppf-alpha-m-batch-rvv` | PPF 的 closed-form `alpha_m` 可迁移到 CPPF，减少每个 pair 构造 Eigen 旋转对象的成本。 | 公式边界、near X normal、`atan2/sin` 近似误差需要单独预算。 | closed-form 对拍、QEMU correctness、asm 和 board A/B。 |
| `output-resize-vs-push-back` | caller 当前 `push_back` 可能带来输出 staging 成本，预分配 `resize` 可作为纯标量对照。 | 只改变容器写回，不代表 RVV 算术收益；不能直接作为 production 决策。 | component bench 单独比较，若正向再决定是否进入 production probe。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `cppf-reference` | full sequential / prefix indices | `PointXYZRGBNormal`, `float`, AoS | test-only reference vs public compute | `make run_test_compare` | not_applicable | not_applicable | not_applicable | not_applicable | planned | create reference and tests |
| `cppf-pair-hsv-batch-rvv` | prefix indices | `PointXYZRGBNormal`, `float`, SoA staging | test-only candidate | `make run_test_compare` | `component_cppf_reference` vs `candidate_cppf_pair_hsv_batch_rvv` | `make board_repeated evidence_doctor_repeated` if board config works | `make dump_bench_rvv` | repeated manifest / doctor | planned | implement candidate and bench |
| `cppf-alpha-m-batch-rvv` | prefix indices | `PointXYZRGBNormal`, `float`, SoA staging | test-only candidate | `CPPFAlphaM` gtest and candidate output test | `candidate_cppf_alpha_m_batch_rvv` | same repeated board if correctness passes | `make dump_bench_rvv` | repeated manifest / doctor | planned | implement after reference |
| production integration | full public entry | exact point type only at most | production direct | not planned | not planned | not planned | not planned | not planned | deferred | only after component evidence supports bounded production probe |

## 实现和测试动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| 建立 topic scaffold | `README.zh.md`、`Makefile`、`board.mk`、`include/cppf.h`、`include/impl/*`、`src/test_cppf.cpp`、`src/bench_cppf.cpp` | `make run_test_compare` 可构建并运行。 |
| 标量 reference | `include/impl/cppf_reference.hpp` | gtest 证明 reference 与当前 public `CPPFEstimation` 对拍。 |
| RVV pair/HSV candidate | `include/impl/cppf_pair_hsv_candidate.hpp` | RVV 构建下 candidate 输出和 reference 误差在预算内；Std 构建自然落回 reference。 |
| RVV alpha candidate | `include/impl/cppf_alpha_candidate.hpp` | closed-form 与 Eigen reference 对拍；candidate 输出和 reference 对拍。 |
| bench / manifest / doctor | `src/bench_cppf.cpp`、`script/generate_cppf_evidence_manifest.py` | 可生成 Std/RVV board repeated 输入给 Evidence Doctor。 |
| phase result / docs | result、evaluation、roadmap、matrix、Handoff | 回填命令、证据、停止或继续条件。 |

## Evidence Doctor 和 registry 规则

性能结论只来自 board（板卡）或目标硬件 repeated bench。QEMU bench compare 默认不运行；若运行，只能写成 log-shape smoke（日志形状小型验证）。本阶段创建 topic-local manifest 生成脚本，并复用 `test-rvv/script/evidence_doctor.py`。CPPF 尚无 `log/evidence_registry.json`，阶段结果中必须写 `evidence_registry_status=not_available` 并列出人工检查路径。

## 板卡复跑预算和决策桶

板卡当前由用户说明可用。本阶段板卡性能预算为 `REPEATED_BOARD_RUNS=5`。决策桶：mean 或 median `>=1.10x` 且无 checksum mismatch 为 `positive`；`1.03x-1.10x` 为 `weak-positive`；`0.97x-1.03x` 为 `neutral`；`<0.97x` 为 `negative`；方向跨桶且预算耗尽为 `unstable`。若 board target 或 ssh 配置失败，停止条件是工具 / 板卡配置不可用，而不是“需要板卡验证”。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `diagnostic` 和 `production-shaped diagnostic`，均为测试专用。 |
| A/B boundary | `test helper` 和 caller-shaped helper；不是 production public dispatch。 |
| 当前决策问题 | 先回答 RVV-vs-scalar component 是否值得继续，再判断是否允许 bounded production probe。 |
| diagnostic 是否可外推到 production | unknown；all-pairs loop 与 public entry 相同，但 staging、output resize、candidate helper 都不在 production。 |
| comparison-boundary / baseline mismatch 风险 | yes；reference/candidate 可能预分配输出，而 production 当前 `push_back`，bench 必须写清计时边界。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 仅在 component 结果至少 weak-positive、correctness/asm/doctor 无 Error 且生产补丁范围能限定到 exact point type 时才允许；negative 时不建议直接 production probe。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes；若之后已有生产 RVV family 或多个候选，都要同一 production boundary 内比较。 |

## 继续 / 停止条件

默认下一步是执行本阶段 scaffold、reference、candidate、correctness、asm、board repeated 和 Evidence Doctor。合法停止条件包括：构建环境缺失、板卡配置失败、Evidence Doctor Error 未处理、checksum 不一致、继续需要修改 production 或扩大到泛型点型 / public API。只完成 scaffold、一个 gtest 或一次 bench 不构成停止条件。

## 文档更新清单

本阶段更新 topic-local evaluation、phase README、optimization roadmap、optimization matrix 和最终 Handoff。没有 adopted production behavior，因此 `doc-rvv/features/cppf-RVV.zh.md` 为 `not_applicable`，本阶段不创建 production 长期主题文档。
