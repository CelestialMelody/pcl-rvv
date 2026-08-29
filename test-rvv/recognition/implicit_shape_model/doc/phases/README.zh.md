# implicit_shape_model 阶段索引

## 当前恢复入口

默认恢复入口是 Phase 020 result：`findObjects()` public-entry descriptor assignment 已完成生产接入、
公开入口 correctness、反汇编、production direct board repeated、Evidence Doctor 和文档 closeout。
当前 topic 在这个窄范围内可以进入 reviewer 审查；若继续优化，应新建 Phase 030，并先冻结新的入口、
点型 / `Scalar`、layout、规模和证据边界。

## 阶段表

| phase | status | 默认恢复动作 | 说明 |
| --- | --- | --- | --- |
| `000-current-state-and-gaps` | completed | 作为历史 diagnostic 读取 | 建立 topic-local doc suite 和局部公式 diagnostic；未修改 production。 |
| `010-production-shaped-ism-subkernel-diagnostic` | completed | 作为 Phase 020 输入读取 | 证明 descriptor batch 形态保住正向收益；未修改 production。 |
| `020-production-integration-findobjects-public-entry` | completed / adopted narrow | 读取 `result.zh.md` 和 `doc-rvv/recognition/implicit_shape_model-RVV.zh.md` | 接入 `findObjects()` descriptor assignment 生产 helper，板卡 weak positive 后采纳。 |

## 文档归属

- `plan.zh.md`：本阶段计划、边界和停继续条件。
- `result.zh.md`：本阶段事实回填、证据解释和下一步。
- `optimization-matrix.zh.md`：跨阶段候选、证据状态和恢复条件。
- `../optimization-roadmap.zh.md`：候选搜索空间和默认下一 phase。
- `../implicit_shape_model-evaluation.zh.md`：函数级评估和生产接入判断主归属。
- `doc-rvv/recognition/implicit_shape_model-RVV.zh.md`：已采纳生产行为的长期文档。

## 当前早停规则

Phase 020 同 scope 没有继续自动推进的未阻塞优化动作。继续会扩大到新入口、训练 path、数学语义或
泛型扩展；这些方向需要新的 phase plan 和对应 evidence boundary，不应静默混入本次 closeout。
