# Phase 030 PI1-PI5 production probe 结果

## 当前结论

Phase 030 已按 `030-pi1-production-integration-plan/plan.zh.md` 推进生产接入闭环（production integration loop，生产接入闭环）的受控探针。生产补丁采用公开入口分发形态：`getRemainingCorrespondences` 先尝试 `getRemainingCorrespondencesRVV`，失败时落回 `getRemainingCorrespondencesStandard`。这符合本阶段计划和 `rvv-implementation` 的 `Std` / `RVV` 分层要求。

当前 EvidenceDecision（证据决策）是 `rollback/no-production`。生产探针正确性通过，QEMU smoke（小型仿真验证）和反汇编路径成立，但板卡 production direct（真实生产入口直连）重复测试为负向：2048 和 8192 correspondences 两个 case 都是 5/5 退化。因此生产补丁已回滚，目标生产文件当前无本 topic diff。

## 计划动作回填

| 动作 | 状态 | 证据 | 结论 |
| --- | --- | --- | --- |
| PI2-A 标量主体拆分 | done then rolled back | 回滚前 `getRemainingCorrespondencesStandard` 存在；当前 production diff 为空 | 探针形态正确，但不保留生产改动。 |
| PI2-B RVV helper | done then rolled back | 回滚前 `getRemainingCorrespondencesRVV` 命中；asm 中可见该符号 | 板卡负向后移除。 |
| PI2-C edge work staging | done then rolled back | production-direct QEMU smoke 和 board summary | 语义可行，性能不成立。 |
| PI2-D RVV edge predicate | done then rolled back | `build/asm/riscv/bench_correspondence_rejection_poly_rvv.full.asm` 命中 `getRemainingCorrespondencesRVV` | 当前只保留为历史负向探针证据。 |
| PI3 production direct correctness | done | QEMU Std/RVV 各 8 tests passed；board 8 tests passed | 新增 fixed-seed random stress（固定种子随机压力样本）后，当前源码正确性成立。 |
| PI4 production evidence rerun | done | `log/qemu/production_direct/evidence_doctor.md`；`log/board/production_direct_repeated/*` | QEMU doctor clean；board doctor Errors=2。 |
| PI5 EvidenceDecision | done | 本文件、matrix、evaluation、Handoff | `rollback/no-production`。 |

## 测试数据说明

当前测试同时包含两类数据：

- deterministic corpus（确定性样本集）：固定合成点云、identity correspondences（同下标对应关系）、固定 edge pairs（边对）、固定 accept-rate 样本和固定 `std::srand` public entry smoke。这些样本保护 guard、NaN 拒绝、histogram / Otsu 和输出保序等明确边界。
- seeded random stress（固定种子随机压力样本）：`SeededRandomPublicEntryMatchesReference` 使用固定 `std::mt19937` seed 生成点云扰动、乱序 correspondence、不同 size / cardinality / threshold 组合，并在 reference 和 production public entry 前分别重置 `std::srand(seed)`。它是可复现压力样本，用来降低遗漏输入形态的风险。

## 生产探针证据

| 证据 | 状态 | 路径 | 边界 |
| --- | --- | --- | --- |
| QEMU correctness | pass | `test-rvv/registration/correspondence_rejection_poly/log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` | Std / RVV 构建各 8 tests passed；QEMU 不证明真实性能。 |
| board correctness | pass | `test-rvv/registration/correspondence_rejection_poly/log/board/test_smoke/run_test.log` | 板卡 8 tests passed。 |
| QEMU production-direct smoke | pass as log-shape | `test-rvv/registration/correspondence_rejection_poly/log/qemu/analyze_bench_compare_production_direct.log`、`log/qemu/production_direct/evidence_doctor.md` | Errors=0，Warnings=0，Suggestions=0；只证明探针日志和 manifest 可解析。 |
| asm attribution（反汇编归因） | pass for historical probe | `test-rvv/registration/correspondence_rejection_poly/build/asm/riscv/bench_correspondence_rejection_poly_rvv.full.asm` | 回滚前探针中 `getRemainingCorrespondencesRVV` 和 `getRemainingCorrespondencesStandard` 符号可见。当前生产源码已回滚。 |
| board production direct | negative | `test-rvv/registration/correspondence_rejection_poly/log/board/production_direct_repeated/summary.md` | 2048 median 约 0.901x，8192 median 约 0.956x；两组都是 5/5 degradation。 |
| Evidence Doctor | fail for production adoption | `test-rvv/registration/correspondence_rejection_poly/log/board/production_direct_repeated/evidence_doctor.md` | Errors=2，均为 `ba_degradation_frequency`。 |

## Evidence Doctor 处理

QEMU production-direct doctor 无发现。板卡 production-direct doctor 报告两个 Error：

- `production-direct public entry 2048 correspondences`：B/A values 为 `0.911x, 0.863x, 0.890x, 0.917x, 0.901x`，5/5 低于 1。
- `production-direct public entry 8192 correspondences`：B/A values 为 `0.958x, 0.941x, 0.955x, 0.968x, 0.956x`，5/5 低于 1。

这些 Error 不证明 RVV 代码有功能 bug；它们说明该生产探针不能作为生产性能证据。处理动作是回滚生产补丁，将 `production_edge_batch_rvv` 标为 attempted / rejected，并把证据保留为历史负向探针。

## 优化矩阵更新

| candidate family | row source policy | point type / Scalar / layout | board evidence | Evidence Doctor | decision | next action |
| --- | --- | --- | --- | --- | --- | --- |
| `production_edge_batch_rvv` | correspondences | `PointXYZ` / `float` / xyz AoS | negative：两个规模均 5/5 degradation | Errors=2 | `rollback/no-production` | 不再默认接入；若后续继续，应另开 profile / 消融或设计新 family。 |
| `edge_gather_staging` | correspondences | `PointXYZ` / `float` / test support staging | historical weak-positive diagnostic | clean | historical diagnostic | 不能替代 production direct。 |
| `accept_rate_filter` | contiguous counters | `int` counters -> `float` rates | neutral | warning in confirm | no-production | 仅 profile 指向时恢复。 |
| `histogram_otsu_scalar` | not_applicable | accept-rate values | not_applicable | manual | adopted scalar | 无默认动作。 |

## 继续 / 停止决定

本阶段完成矩阵已闭合。继续默认生产接入会再次触碰已被负向证据否决的生产路径；继续性能探索需要新的 profiling（性能剖析）或 component ablation（组件消融）假设。当前停止条件命中：生产 direct 板卡证据为 negative，Evidence Doctor 有 Error，生产补丁已回滚，topic-local 文档和 registry 已同步。

next_phase_default：`ready_for_review`。可选后续是另开窄范围 profiling / ablation phase，先解释完整 public entry 中 random sampling、edge staging、histogram / Otsu 和输出 append 的成本占比，再提出新候选。
