# Phase 000: 当前状态与组件消融计划

## 阶段意图和边界

本阶段开启 `organized_multi_plane_segmentation` RVV topic（主题），只做 component ablation（组件消融，用测试专用 helper 拆出局部成本）。阶段目标是回答三个局部片段是否值得继续：`segment` 中的 `plane_d[i] = point dot normal` 预处理、`segment` / `segmentAndRefine` 中的 boundary cloud gather（边界点云按索引复制），以及 `projectToPlaneFromViewpoint` 投影循环。

本阶段不修改 production（生产源码），不声明 production direct（真实生产路径证据），不覆盖 `OrganizedConnectedComponentSegmentation`、`computeMeanAndCovarianceMatrix`、`eigen33` 和 `refine` 的 label growth（标签扩张）状态机。验证范围冻结为 `pcl::PointXYZ` 输入点、`pcl::Normal` 法线、`pcl::Label` 标签、`Scalar=float`、有组织点云的一维连续存储、有效 boundary indices（边界索引）。`PointNormal`、`PointXYZINormal`、泛型 normal traits（法线字段特征）、非连续 layout、`Scalar=double` 和 production dispatch 都属于未验证范围。

## 当前状态清单

| 项目 | 当前状态 | 证据 |
| --- | --- | --- |
| 目标源码 | `plane_d` 点积、boundary copy、optional projection 是本阶段候选；CCL、region covariance、`eigen33` 和 refine 保持标量边界 | `segmentation/include/pcl/segmentation/impl/organized_multi_plane_segmentation.hpp` |
| 上游测试 | 未发现直接 `OrganizedMultiPlaneSegmentation` unit test | `rg OrganizedMultiPlane ... test segmentation examples tools doc-rvv test-rvv` |
| topic 资产 | 本阶段前不存在 topic 目录 | `find test-rvv/segmentation/organized_multi_plane_segmentation` 无输出 |
| 板卡状态 | 当前会话说明板卡可用；本阶段需要 repeated board bench 才能写性能结论 | 用户目标；`test-rvv/mk/rvv-topic.mk` board target |
| production 状态 | 未修改 production；`doc-rvv` 长期主题文档暂不适用 | 当前 git status 限定路径无 diff |

## 候选族和假设

| candidate family | 入口 / row source | 点类型 / layout | 假设 | 风险 |
| --- | --- | --- | --- | --- |
| `plane_d_dot_rvv` | ordered dense point-normal arrays | `PointXYZ + Normal`，AoS stride load（结构数组跨步加载） | 三个乘加可用 `vfmacc` 合并，独立于 CCL 状态机 | 完整 `segment` 可能被 CCL 和 per-region fitting 稀释 |
| `boundary_gather_rvv` | valid boundary indices | `PointXYZ`，32-bit byte offset gather（离散加载） | 边界点多时 gather/copy 有局部收益 | 只有后处理边界，不代表公开入口总收益 |
| `viewpoint_projection_rvv` | ordered boundary cloud | `PointXYZ`，连续输出 | 投影公式按 VL chunk（可变向量长度分块）执行，`vfdiv` 和 `vfmacc` 可覆盖主算术 | 分母接近零时沿用标量语义，不在本阶段新增保护 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `plane_d_dot_rvv` | ordered dense | `PointXYZ + Normal / float / AoS` | test-only component helper | `run_test_compare` | `run_board_omps_repeated` case `plane_d_dot` | planned, 5 runs | `computePlaneDValuesRVV` | planned | planned |
| `boundary_gather_rvv` | source-indexed boundary list | `PointXYZ / float / AoS` | test-only component helper | `run_test_compare` | `run_board_omps_repeated` case `boundary_gather` | planned, 5 runs | `gatherBoundaryCloudRVV` | planned | planned |
| `viewpoint_projection_rvv` | ordered boundary cloud | `PointXYZ / float / AoS` | test-only component helper | `run_test_compare` | `run_board_omps_repeated` case `projection` | planned, 5 runs | `projectBoundaryFromViewpointRVV` | planned | planned |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| A1 写 RED test（先失败测试） | `src/test_omps.cpp`、`include/omps.h` | `make run_test_rvv` 因 RVV candidate 缺失或行为未实现而失败 |
| A2 实现 test-only Std/RVV helpers | `include/impl/omps_components.hpp` | `make run_test_compare` 通过；Std/RVV 数值误差在预算内 |
| A3 写 component bench | `src/bench_omps.cpp` | bench 输出包含 case、iterations、平均耗时、checksum 和路径标签 |
| A4 反汇编归属 | `make dump_bench_rvv` | RVV 指令能归属到三个 candidate helper 的符号或内联边界 |
| A5 板卡 repeated bench 与 Evidence Doctor | `make run_board_omps_repeated` | 5-run summary、manifest 和 doctor 报告生成；性能结论只来自板卡 |
| A6 更新 result / matrix / roadmap / evaluation / Handoff | topic-local 文档 | 每个动作回填 done / partial / deferred / blocked，写清继续或停止条件 |

## Evidence Doctor、registry 和复跑规则

本阶段使用 topic-local manifest 生成脚本 `script/generate_omps_board_evidence_manifest.py`，再调用 `test-rvv/script/evidence_doctor.py`。若 manifest 不完整，Evidence Doctor 结果只能作为 reviewer aid（审查辅助），不能当成完整通过。若 checksum 不一致或 A/B boundary 不一致，先修复或降级证据。若 summary 数字改变 decision bucket（决策桶），刷新 phase result、optimization matrix、evaluation 和 Handoff。

板卡复跑预算为初始 5 runs；若 doctor warning 显示长尾、方向接近阈值或 `B/A < 1` 频率异常，最多追加一次同边界 5-run 确认。桶规则：`speedup >= 1.15` 且方向稳定为 `positive`；`1.05 <= speedup < 1.15` 为 `weak_positive`；`0.95 <= speedup < 1.05` 为 `neutral`；`speedup < 0.95` 为 `negative`；跨桶摇摆或异常无法解释为 `unstable`。

## Diagnostic 到 production 错配审计

| question | answer |
| --- | --- |
| evidence role | diagnostic（诊断）/ component ablation，不是 production direct |
| A/B boundary | test helper，同一 wrapper 内 Std 与 RVV helper 对比 |
| 当前决策问题 | RVV-vs-scalar 局部组件是否值得继续到 production-shaped diagnostic |
| diagnostic 是否可外推到 production | 不能直接外推；它只覆盖局部循环，不包含 CCL、region fitting、refine 和公开入口对象状态 |
| comparison-boundary / baseline mismatch 风险 | 有；bench helper 会隔离组件，完整 `segment` 总成本可能不同 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 只有同一组件在 public-shaped timing 中占比可见且 fallback 可控时才允许 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要；本阶段最多支持下一阶段生产形态诊断，不支持 clean adoption |

## Phase scope 与扩展队列

`validated_scope` 只包含 `PointXYZ + Normal + Label` 的 test-only component helper。`unvalidated_scope` 包含 public `segment` / `segmentAndRefine`、`PointNormal`、`PointXYZINormal`、泛型 normal traits、`Scalar=double`、非连续 layout、invalid index fallback 和 production dispatch。

`point_type_expansion_queue`：若 Phase 000 的 `plane_d_dot_rvv` 在板卡上 positive，下一 phase 才考虑 `PointNormal` 或 `PointXYZINormal` 的 normal-layout gate；若 gather/projection positive，下一 phase 先做 public-shaped boundary timing，再决定是否进入 PI1。

## 继续 / 停止条件

默认下一阶段是 `010-production-shaped-timing`：把 positive 或 weak-positive 的组件放进更接近 `segment` / `segmentAndRefine` 的 timing 边界，量化 CCL、region fitting 和 refine 稀释。若所有组件在 repeated board 上为 neutral / negative，进入 no-production diagnostic closeout。若板卡不可达、doctor error 无法修复、或 dirty isolation 无法区分当前 topic 与无关改动，输出 blocked Handoff。

## 文档更新清单

本阶段更新 topic-local evaluation、README、phase README、optimization roadmap、optimization matrix、phase result 和 current Handoff。没有 adopted production behavior 前，不创建 `doc-rvv/segmentation/organized_multi_plane_segmentation-RVV.zh.md`。
