# 阶段索引

## 当前恢复入口

当前恢复入口是 Phase 060 dual-indices / correspondences production integration（双索引 / 对应关系生产接入）闭环之后的生产 closeout（收尾）与文档刷新：

- 已完成阶段：`doc/phases/000-current-state-and-gaps/result.zh.md`
- 已完成阶段：`doc/phases/010-board-and-asm-diagnostic/result.zh.md`
- 已完成阶段：`doc/phases/020-pi1-production-integration-plan/result.zh.md`
- 已完成阶段：`doc/phases/030-row-source-audit/result.zh.md`
- 已完成阶段：`doc/phases/040-source-indexed-production-integration/result.zh.md`
- 已完成阶段：`doc/phases/050-dual-indices-correspondences-audit/result.zh.md`
- 已完成阶段：`doc/phases/060-dual-indices-correspondences-production-integration/result.zh.md`
- 默认下一动作：刷新 topic-local docs、evidence registry 和长期 `doc-rvv` 收尾；不要再把 `Scalar=double` 拉回当前 production 范围。

## 阶段表

| phase | 状态 | 目标 | 默认下一步 |
| --- | --- | --- | --- |
| `000-current-state-and-gaps` | done | 建立 dense ordered-cloud-pair fused candidate、correctness、bench smoke 和 S2 evaluation。 | none |
| `010-board-and-asm-diagnostic` | done | 生成 ordered-cloud-pair board repeated summary、manifest 和 Evidence Doctor；bench asm 输入已生成。 | result 已回填。 |
| `020-pi1-production-integration-plan` | done / production-ready | PI1-PI5：ordered-cloud-pair 生产补丁、production direct tests、asm、board repeated 和 EvidenceDecision。 | 进入 source-indexed audit。 |
| `030-row-source-audit` | done | source-indexed candidate correctness、bench、asm 和 board diagnostic。 | 进入 source-indexed production integration。 |
| `040-source-indexed-production-integration` | done / production-ready | source-indexed 生产补丁、fallback、production direct tests、asm、board repeated 和 EvidenceDecision。 | 默认进入 dual-indices / correspondences audit。 |
| `050-dual-indices-correspondences-audit` | done | dual-indices / correspondences row-source audit、diagnostic board 和 EvidenceDoctor。 | 进入 060 production integration。 |
| `060-dual-indices-correspondences-production-integration` | done / production-ready | dual-indices / correspondences 生产补丁、fallback、production direct tests、asm、board repeated 和 EvidenceDecision。 | 进入文档刷新和 closeout。 |

## 文档归属

Phase plan/result 记录阶段动作和证据缺口；`doc/optimization-roadmap.zh.md` 记录跨阶段候选搜索空间；`doc/transformation_estimation_svd-evaluation.zh.md` 记录 S2 evaluation、Traceability Map 和生产接入判断；`doc-rvv/registration/transformation_estimation_svd-RVV.zh.md` 只记录已采用生产行为。

## 当前早停规则

本 topic 不能停在 scaffold、positive diagnostic 或单个 row source summary。Phase 020/040/060 已完成 ordered-cloud-pair、source-indexed-cloud-pair、dual-indices-cloud-pair 和 correspondence-pair production direct；QEMU timing 仍不能写成性能结论。`Scalar=double` 仍保持标量 fallback，不再自动回到当前 production 范围。
