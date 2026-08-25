# Phase 000 plan: current state and reduction diagnostic

## 阶段意图和边界

本阶段建立 `moment_of_inertia_estimation` 的 RVV（RISC-V Vector，可变长度向量）diagnostic（诊断）闭环。目标是证明 ordered indexed cloud（按 `indices_` 顺序遍历的点云）上的逐点规约是否值得继续：均值/AABB、协方差、单轴惯性矩和 OBB（oriented bounding box，有向包围盒）投影 extrema（极值）。本阶段不修改 production（生产源码），不证明真实 `MomentOfInertiaEstimation<PointT>::compute()` 已经命中 RVV，也不覆盖 `Scalar=double`、indices 以外的公开入口或泛型点类型生产分流。

## 当前状态清单

| area | current state | path |
| --- | --- | --- |
| source | `computeMeanValue`、`computeCovarianceMatrix`、`calculateMomentOfInertia`、`computeOBB` 都有逐点规约；Eigen 求解和角度旋转是标量边界 | `features/include/pcl/features/impl/moment_of_inertia_estimation.hpp` |
| upstream test | 已有 basic feature extraction 和 invalid parameter test | `test/features/test_moment_of_inertia_estimation.cpp` |
| RVV topic assets | 当前 topic 新建中；尚无 test/bench/registry 证据 | `test-rvv/features/moment_of_inertia_estimation/` |
| screening queue | 队列表建议进行 RVV 优化，优先级 7 | `doc-rvv/library-screening/features/features-function-evaluation-queue.zh.md` |

## 假设与候选族

| hypothesis | candidate | risk | validation |
| --- | --- | --- | --- |
| xyz 字段规约可以用 RVV load + vector reduction 降低循环成本 | fused xyz reductions | 浮点规约顺序和 gather 成本可能抵消收益 | Std/RVV 对拍、反汇编、板卡 repeated bench |
| `getProjectedCloud` + projected covariance 可融合成不写临时点云的公式 | projected covariance fusion | 牵涉 eccentricity（离心率）和 Eigen 求解，不适合和 phase 000 混在一起 | phase 010 单独推导和消融 |

## 优化矩阵

见 `../optimization-matrix.zh.md`。本阶段只允许把 `fused xyz reductions` 从 `planned` 推进到 `attempted`、`adopted`、`rejected` 或 `deferred`。

## 实现和测试动作

| action | artifact / command | expected evidence | completion criteria |
| --- | --- | --- | --- |
| A1 写 test-first 红灯 | `src/test_moi.cpp` 先调用尚不存在的 `moi::computeReductionSummaryRVV` | `make run_test_rvv` 预期编译失败，证明测试能抓到缺失 candidate | RED failure 由缺少 RVV helper 引起 |
| A2 写 Std/RVV diagnostic helper | `include/moi.h`、`include/impl/moi_reductions.hpp` | Std/RVV 同输入输出；非 RVV 构建自然走 Std helper | `run_test_compare` 通过 |
| A3 写 bench diagnostic | `src/bench_moi.cpp`、`Makefile` | case `moi_reductions` 输出 checksum 和 us/iter | QEMU 只作为 log-shape smoke；性能结论等板卡 |
| A4 反汇编归属 | `make dump_bench_rvv` | filtered asm 命中当前 helper 的 RVV 指令 | asm boundary 写入 result |
| A5 板卡 repeated bench | `make run_board_moi_repeated` 或等价 board smoke + repeated summary | 5-run bounded budget，decision bucket 稳定或标 unstable | Evidence Doctor 无未处理 Error |
| A6 文档回填 | `result.zh.md`、evaluation、matrix、roadmap、Handoff | 证据边界、continue/stop decision 可恢复 | result 和 Handoff 完整 |

## Evidence Doctor 和 registry 规则

本阶段会新增 topic-local manifest script（证据清单脚本）。board summary、checksum summary、asm attribution（反汇编归属）或 EvidenceDecision（证据决策）前，必须运行 `test-rvv/script/evidence_doctor.py` 或在 result 中人工记录 Errors / Warnings / Suggestions。`log/evidence_registry.json` 尚未存在，phase 000 结束前至少记录 `evidence_registry_status`。

## 板卡复跑预算和决策桶

板卡当前由会话确认可用。本阶段预算为 5 次 repeated board run。decision bucket（决策桶）暂定：`positive` >= 1.10x，`weak-positive` 1.03x-1.10x，`neutral` 0.97x-1.03x，`negative` < 0.97x；若同一 case 跨桶摇摆则标 `unstable` 并降级结论。

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | diagnostic（诊断），test helper boundary（测试 helper 边界） |
| A/B boundary | test helper：Std reduction summary vs RVV reduction summary |
| 当前决策问题 | RVV-vs-scalar diagnostic；是否值得进入 production integration loop（生产接入闭环） |
| diagnostic 是否可外推到 production | unknown；它只模拟当前 helper 的 ordered indexed cloud 规约，不覆盖真实 `compute()` 的全部状态、projection allocation、Eigen solver 和 fallback |
| comparison-boundary / baseline mismatch 风险 | yes；diagnostic helper 的循环融合程度可能不同于现有 production helper |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes，但只在诊断结果至少 `weak-positive` 或有明确 component ablation 说明 production 可受益时考虑；否则先做 phase 010 或 no-production closeout |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes；若后续已有 adopted RVV family，必须补 production detail A/B |

## Phase scope 与扩展队列

`validated_scope`：`pcl::PointXYZ`、`float`、AoS 点云、ordered indexed cloud、diagnostic helper。

`unvalidated_scope`：泛型 PointXYZ-like traits、`PointXYZI` / `PointXYZRGBA` / `PointNormal`、真实 public `compute()` dispatch、projected cloud covariance fusion、`Scalar=double`、非标准布局、小输入 fallback。

`point_type_expansion_queue`：若 phase 000/010 支持 production probe，后续 phase 需要读取 generic point type strategy（泛型点类型策略）并为 PointXYZ-like traits gate、具体实例 fallback、asm、board 和 Evidence Doctor 建独立矩阵。

## 文档更新清单

本阶段更新 `doc/moment_of_inertia_estimation-evaluation.zh.md`、`doc/optimization-roadmap.zh.md`、`doc/phases/optimization-matrix.zh.md`、本 phase result 和 current Handoff。`doc-rvv/features/...` production 长期主题文档暂不适用。

## 继续 / 停止条件

默认继续到 A1-A6 闭环。合法停止条件只包括：测试/工具链无法构建、板卡 SSH/rsync/远端 Make target 不可用、Evidence Doctor Error 无法修复、dirty isolation 不安全，或 production 接入需要用户确认。
