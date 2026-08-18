# 阶段索引

## 当前恢复入口

当前恢复入口是 `020-board-direction-update-diagnostic`。Phase 010 已建立 test / bench harness、QEMU correctness、QEMU smoke、filtered asm 和 Evidence Doctor；Phase 020 已完成板卡 direction-update repeated diagnostic，结果为 negative，production 源码保持不变。

## 阶段表

| phase | 状态 | plan | result | 说明 |
| --- | --- | --- | --- | --- |
| `000-current-state-and-gaps` | done / diagnostic planning | `doc/phases/000-current-state-and-gaps/plan.zh.md` | `doc/phases/000-current-state-and-gaps/result.zh.md` | 建立 topic-local scaffold、evaluation、roadmap、matrix，并同步函数评估队列状态。 |
| `010-diagnostic-scaffold-and-asm-probe` | done / diagnostic continue | `doc/phases/010-diagnostic-scaffold-and-asm-probe/plan.zh.md` | `doc/phases/010-diagnostic-scaffold-and-asm-probe/result.zh.md` | Std/RVV gtest 各 6 个通过；QEMU direction-update smoke 和 Evidence Doctor 通过；不改 production。 |
| `020-board-direction-update-diagnostic` | done / negative | `doc/phases/020-board-direction-update-diagnostic/plan.zh.md` | `doc/phases/020-board-direction-update-diagnostic/result.zh.md` | Milkv-Jupiter 5-run repeated board diagnostic 负向；默认停止生产推进。 |

## 文档归属

phase 文档记录阶段计划、完成情况和早停边界；跨阶段候选放在 `doc/optimization-roadmap.zh.md`；函数级评估和生产判断放在 `doc/bfgs-evaluation.zh.md`。

## 当前早停规则

Phase 010 不能作为 production 接入判断，因为它只有 QEMU correctness / log-shape 和二进制级 asm。Phase 020 已可达且结果 negative，因此默认停在 diagnostic closeout，不再自动排队 caller hotspot audit。
