# Phase 000: cylinder count/select production-shaped diagnostic 计划

## 阶段意图和边界

本阶段只为 `SampleConsensusModelCylinder<PointT, PointNT>::countWithinDistance` 和
`selectWithinDistance` 建立 production-shaped diagnostic（生产形态诊断，测试专用代码模拟真实公开入口的数据形态）。
目标是证明 cylinder 的 indexed xyz/normal AoS gather（按索引离散加载结构数组字段）、轴向投影、半径误差、
normal angle（法线夹角）和 `select` 保序输出可以先在测试专用 candidate 中与公开标量入口一致。

本阶段不修改 production（生产源码），不新增 public API（公开接口），不覆盖 `getDistancesToModel`、
`optimizeModelCoefficients`、`projectPoints`、`doSamplesVerifyModel`、泛型点型全集、`Scalar=double` 或真实 RANSAC 上游性能。

## 当前状态清单

| area | 当前事实 |
| --- | --- |
| 源码 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_cylinder.hpp` 三个距离入口均为标量循环；count/select 在 `weighted_euclid_dist > threshold` 时 early continue（早停）。 |
| 筛选依据 | `doc-rvv/library-screening/sample_consensus/sample_consensus-retained-candidate-rescreen.zh.md` 将 cylinder 列为第一条建议启动函数级评估主题。 |
| 测试资产 | 本 topic 新建 `test-rvv/sample_consensus/sac_model_cylinder`，采用 `src/`、`include/` 与 `doc/phases/` 结构。 |
| 生产状态 | 未接入 RVV；`doc-rvv` 长期生产主题文档当前不适用。 |
| 板卡 | 当前会话说明板卡可用；本阶段若补齐 bench，将按 5-run bounded rerun budget（有界复跑预算）执行。 |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| count/select indexed-gather + radial-norm + normal-angle | `count` 可用 mask popcount（掩码计数），`select` 可用 `vcompress`（向量压缩）保序写回。 | 每点 `dir.norm()` 与 `getAngle3D` 都需要 `sqrt/acos`，收益可能被数学函数成本或近阈值误差吞掉。 |
| getDistances dense writeback | 后续可尝试 full-RVV sqrt + double store（向量平方根和 double 写回）。 | 本阶段不覆盖，避免把 dense output 与 select/count early gate 混在一起。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| count-select diagnostic | direct indexed `indices_` | `PointXYZ + Normal`, float xyz/normal AoS, `Eigen::VectorXf` | test-only candidate for count/select | `run_test_compare`; RED 先证明缺 candidate 会失败 | planned | planned after bench scaffold | planned candidate helper asm | planned | planned | 写 candidate helper、bench 和 board summary |

## 实现和测试动作

| 动作 | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| RED test | `src/test_sac_model_cylinder.cpp`，运行 `make run_test_rvv` | 因缺少 `SampleConsensusModelCylinderDiagnostic` 或 candidate helper 编译失败。 |
| GREEN helper | `include/impl/sac_model_cylinder_diagnostic.hpp` 与聚合头 | `run_test_compare` 通过，证明 Std/RVV 构建下 candidate 与公开入口一致。 |
| bench scaffold | `src/bench_sac_model_cylinder.cpp` 和 Makefile target | 输出 Dataset、Iterations、Warmup、Build、Checksum 和 count/select 行。 |
| QEMU / asm | `run_test_compare`、`dump_bench_rvv` | QEMU correctness 通过；反汇编能归属到 candidate RVV helper，或明确降级。 |
| board / doctor | repeated board 5-run + manifest + Evidence Doctor | 只把板卡 repeated summary 写成性能证据；QEMU timing 不进入结论。 |

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | test helper；baseline 是公开入口标量语义，candidate 是测试专用 helper |
| 当前决策问题 | RVV-vs-scalar 候选是否值得进入后续 production integration plan |
| diagnostic 是否可外推到 production | 只能部分外推公式、访存和输出顺序；不能证明真实 production dispatch、fallback 或 public overload 性能。 |
| comparison-boundary / baseline mismatch 风险 | 有。公开入口仍是生产标量路径，candidate 是测试专用 helper；最终 production 取舍必须另跑 production direct。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 只有 correctness、asm、板卡 summary 和 Evidence Doctor 无 Error，且实现范围不扩大到 production 时才继续诊断；负向不能直接拒绝有界 production probe。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前没有已采纳 cylinder RVV family；若后续接 production，仍需 production public Std/RVV 与必要 family comparison。 |

## Phase scope 与扩展队列

`validated_scope`：本阶段计划覆盖 `countWithinDistance` / `selectWithinDistance`、direct indexed `indices_`、
`PointXYZ + pcl::Normal`、float AoS 布局、`Eigen::VectorXf` 系数、乱序 indices、synthetic cylinder cloud。

`unvalidated_scope`：`getDistancesToModel`、`optimizeModelCoefficients`、`projectPoints`、空 indices、
默认整云 identity indices、`PointXYZI` / RGB / RGBA / 自定义点型、normal-like 泛型点型、`Scalar=double`、
production fallback、真实 RANSAC 上游路径和真实 production dispatch。

`point_type_expansion_queue`：若本阶段 positive，下一轮只能创建独立 point-type expansion phase，
先补 traits / layout gate、fallback tests、dedicated bench、QEMU / asm、repeated board 和 Evidence Doctor。

## 板卡复跑预算和决策桶

本阶段默认 5-run repeated board。若 Evidence Doctor 暴露长尾或方向接近阈值，最多追加一次同边界确认复跑。
decision bucket（决策桶）暂定为：median speedup >= 1.20 且无 Error 为 positive；1.05-1.20 为 weak_positive；
0.95-1.05 为 neutral；低于 0.95 为 negative；跨桶摇摆或 Error 未解为 unstable / blocked。

## 继续 / 停止条件

默认下一动作是执行 RED test，然后补 GREEN diagnostic helper。只有继续会触碰 production、public API、
其它 topic、dirty isolation 不安全、工具链 / 板卡不可用、Evidence Doctor Error 未解或证据互相矛盾时，才停止。
