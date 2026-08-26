# Phase 020 PI1 Production Integration Plan

## 阶段意图和边界

本文件是 Phase 020 执行前冻结的 PI1 production integration plan（生产接入计划）。实际执行结果和当前采纳状态见
`result.zh.md`；本计划保留为审计当时的授权、范围和验收条件。

本阶段计划的作用是在用户确认可以修改 production source（生产源码）后，把 Phase 010 的
`partial-production-candidate` 转成有界 production patch（生产补丁）并采集 production direct（真实生产路径直连）证据。

默认生产补丁候选路径是 `features/include/pcl/features/impl/pfhrgb.hpp`。本计划不改变公开 API；不修改 `features/src/pfh.cpp`；不创建长期 `doc-rvv/features/pfhrgb-RVV.zh.md`，直到 PI5 证据闭环通过且用户确认采纳。

## 授权检查

| item | 状态 | 说明 |
| --- | --- | --- |
| production source 修改授权 | pending_user_confirmation | 当前 prompt 允许 topic-local 测试和文档推进，但 `AGENTS.md` 要求 production integration loop 前停在用户检查点。 |
| PI5 采纳 / 回滚授权 | not_applicable_yet | 即使 PI1-PI5 跑完，worker 也不能自动采纳或回滚生产补丁。 |
| board availability | reachable_in_phase_040 | Phase 030/040 已重新执行 `check_board_ssh` 和 5-run board repeated；PI1 开始前仍建议短探测一次。 |
| dirty isolation | pending_preflight | 开始前重新跑 `git status --short -- features/include/pcl/features/impl/pfhrgb.hpp test-rvv/features/pfhrgb`。 |

## 生产补丁候选

| aspect | plan |
| --- | --- |
| scope | 先收窄到 `PointInT == PointNT == pcl::PointXYZRGBNormal`、`PointOutT == pcl::PFHRGBSignature250`、`nr_subdiv_ == 5`、dense finite cloud。其它模板实例自然 fallback。 |
| helper split | 保留现有标量主体为 `computePointPFHRGBSignatureStd` 或等价 private/protected helper；新增 `computePointPFHRGBSignatureRVV`，公开 `computePointPFHRGBSignature` 只做短路分流。 |
| RVV work | 优先复用 Phase 030 reusable workspace 形态：邻域有向 pair 转 SoA staging，跨点复用 buffer，RVV 计算 geometry tuple 和 RGB ratio，histogram scatter 保持标量顺序。 |
| fallback | 非 RVV build、点型不匹配、小规模 `indices.size() < 4`、非默认 `nr_split`、布局 / 字段 gate 不满足、内存分配失败或 correctness 风险时走 Std。 |
| comments | production 注释仅说明 gate、fallback、staging 与 scalar scatter 边界。 |

## 测试与证据动作

| action | 产物 | 验收 |
| --- | --- | --- |
| PI1-RED | production direct gtest，证明真实 `PFHRGBEstimation::compute` 在 RVV build 会命中新生产路径并保持 descriptor 一致。 | 修改 production 前先失败，失败原因是 trace / counter / path marker 尚不存在或 RVV path 未命中。 |
| PI1-GREEN | production patch 最小接入。 | `run_test_compare` 通过；fallback case 覆盖非匹配 gate。 |
| PI1-ASM | production RVV build 反汇编归属。 | asm 能归到 production helper 或其 inline clone，不只归到 test-only helper。 |
| PI1-BOARD | board smoke + 5-run repeated。 | `public_pfhrgb_k` 成为 production-public 证据；Doctor 0 Errors，Warnings 均解释。 |
| PI5-CHECKPOINT | 保留 patch，报告 diff、命令、board / Doctor 结果和采用 / 回滚建议。 | 暂停等待用户确认。 |

## Diagnostic-To-Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | 目标是 production direct / production-public；Phase 010 只作为进入 PI1 的前置 diagnostic。 |
| A/B boundary | `PFHRGBEstimation::compute` 和真实 `computePointPFHRGBSignature` dispatch。 |
| 当前决策问题 | 当前 public RVV path 是否快于当前 public scalar path；若出现多个 RVV family，再做 RVV-family-selection。 |
| diagnostic 是否可外推到 production | Phase 010 不能直接外推；PI1 必须重跑真实生产边界。 |
| comparison-boundary / baseline mismatch 风险 | PI1 需要消除 wrapper mismatch；否则不能采纳。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 如果只有一个 RVV family，可先做 Std/RVV production-public；若引入 staging reuse / direct AoS 等多个 family，需要同边界 RVV-vs-RVV。 |

## Continue / Stop 条件

开始本阶段的前置条件是用户明确确认可以修改 `features/include/pcl/features/impl/pfhrgb.hpp`。板卡已在 Phase 030/040 恢复可达，但 PI1 前仍应短探测一次。若未确认，当前 worker 应停在 `stop at production checkpoint`，保留 topic-local diagnostic 资产。
