# correspondence_types RVV 阶段索引

本文是 `registration/correspondence_types` RVV topic（主题）的阶段恢复入口。阶段文档只记录测试资产、诊断候选、证据矩阵和恢复动作；当前 no-production closeout（不接入生产收尾）的长期可审计结论放在 `test-rvv/registration/correspondence_types/doc/correspondence_types-evaluation.zh.md` 和当前 phase result。只有真实 production 接入后才发布 `doc-rvv` 长期主题文档。

## 当前阶段

| phase | 状态 | 目标 | 计划 | 结果 |
| --- | --- | --- | --- | --- |
| `000-current-state-and-gaps` | `completed_diagnostic` | 建立函数级评估、轻量测试 / bench scaffold（脚手架）、QEMU 正确性和反汇编 smoke（小型路径验证），判断是否值得进入生产接入闭环 | `000-current-state-and-gaps/plan.zh.md` | `000-current-state-and-gaps/result.zh.md` |
| `010-board-diagnostic-and-production-decision` | `completed_no_production` | 在板卡上验证 index extraction（索引抽取）候选的真实性能，并决定是否进入 production integration loop（生产接入闭环） | `010-board-diagnostic-and-production-decision/plan.zh.md` | `010-board-diagnostic-and-production-decision/result.zh.md` |
| `011-production-probe-boundary-check` | `completed_probe_rolled_back` | 按历史经验补真实 production helper 边界的 production probe（生产路径探针），确认 diagnostic negative 是否会反转 | `011-production-probe-boundary-check/plan.zh.md` | `011-production-probe-boundary-check/result.zh.md` |

## 默认恢复动作

1. 当前恢复入口是 `011-production-probe-boundary-check/result.zh.md` 和 `doc/phases/optimization-matrix.zh.md`。
2. Phase 010 的 diagnostic board repeated benchmark（诊断板卡重复性能测试）为 `negative`，但不能单独禁止 production probe。
3. Phase 011 已补真实 production helper 边界的 5-run production direct probe（真实生产路径探针）；三条 case 仍全部为 `negative` bucket，Evidence Doctor 输出 Errors=3、Warnings=1。
4. 临时 production patch 已回退，当前 production 源码保持标量。
5. 默认下一动作是 `ready_for_review_no_production`：不发布 `doc-rvv`，不把诊断或已回退 probe 写成 adopted production behavior（已采用生产行为）。
6. 若后续仍要继续当前 topic，应另开 profiling / ablation（性能剖析 / 消融）小阶段，只解释负向原因，不作为默认生产接入路径。

## 文档归属

| 信息 | 主归属 | 说明 |
| --- | --- | --- |
| 阶段计划、优化矩阵、Evidence Doctor（证据体检）人工检查和继续 / 停止判断 | `doc/phases/**` | 供下一轮短 prompt 恢复，不复制到 production 长期主题文档 |
| 函数级评估、候选路线和生产接入判断 | `doc/correspondence_types-evaluation.zh.md` | S2 和后续 EvidenceDecision 的主归属 |
| `doc-rvv` 长期主题文档 | not_applicable for current no-production closeout | 当前没有 adopted production behavior；若后续进入 production integration loop 并通过 PI5，再按 `artifact_layout.topic_doc_template` 创建 |
