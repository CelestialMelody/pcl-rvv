# RVV 多阶段优化循环

本文是 `rvv-test` 中 optimization phase loop（优化阶段循环）的唯一细则来源。它把一个 RVV topic 的持续优化定义为可恢复的循环：

```text
恢复状态 -> 写阶段计划 -> 实现 / 测试 -> 解释证据 -> 更新结果和矩阵 -> 继续或停止决策
```

它不把历史实现、某个局部正向结果或一个阶段的完成当作整个 topic 的终点。历史 topic、开源实现和本地知识库只能提供候选、风险和验证方向；新的 code shape（代码组织形态）仍可被提出，但必须通过当前 topic 的同边界正确性、性能、反汇编归属和 Evidence Doctor（证据体检）检查。

## 适用条件

以下情况默认进入 phase loop：

- 用户用短 prompt 表达“继续完善 `<topic>` 的 RVV 优化工作”或等价含义。
- topic 已有 RVV 优化，但出现新的实现族、row source、点类型、`Scalar`、布局、入口或证据缺口。
- 一个阶段结束后，计划矩阵仍有 `unblocked`（未阻塞）动作。
- topic 需要在接入 production（生产源码）前后分别补测试、bench、fallback、反汇编或板卡证据。

用户明确限定“只写计划”“只做一个指定 target”“只修一个文件”时，限定范围覆盖默认继续规则；worker 仍要记录未完成的 phase loop 状态。

## 阶段文档布局

阶段文档属于配置解析出的 topic 测试目录，不属于通用 `.agents/knowledge/`，也不属于最终 `doc-rvv` 主题文档。默认布局为：

```text
<topic-test-dir>/doc/phases/
  README.zh.md
  <phase-id>-<stable-slug>/
    plan.zh.md
    result.zh.md
    evidence-doctor.md       # 本阶段使用 Evidence Doctor 时可提交的摘要
    evidence-doctor.json     # 只有需要机器读取且已脱敏时保留
```

`<phase-id>` 使用单调、可排序的标识，例如 `000`、`010`、`020`；slug 描述阶段目标，不写易漂移的“最新”。worker 恢复时优先读取 `README.zh.md`、最近已完成阶段的 `result.zh.md`、当前未完成阶段的 `plan.zh.md` 和最近 Handoff Packet。没有阶段目录时，先创建 `000-current-state-and-gaps/plan.zh.md`，不能先写 candidate、bench 或 production。

`README.zh.md` 只维护阶段索引、状态、当前默认恢复入口和文档归属；不要复制各阶段实验流水。阶段 `plan.zh.md` 是修改前的意图和范围合同，`result.zh.md` 是完成后对计划逐项回填的事实记录。阶段结果即使被阻塞或拒绝也要保留，不能删除失败尝试来制造“已完成”假象。

## Phase Plan 最小合同

开始任何代码、测试支撑、bench、生产补丁或阶段性长文档修改前，当前阶段必须有 `plan.zh.md`。计划至少包含：

1. **阶段意图和边界**：本阶段要证明什么、不证明什么；入口、row source、点类型、`Scalar`、布局、规模、production / diagnostic 层级和不可触碰路径。
2. **当前状态清单**：已有实现、测试 target、bench target、板卡证据、反汇编、Evidence Doctor 报告、生产状态和上一阶段未完成项，并附路径或章节。
3. **假设与候选族**：允许提出新的 RVV code shape；列出历史经验作为参考而非约束，并说明要验证的 load/store、staging、reduction、FMA、ILP、LMUL 或 scalar tail 假设。
4. **优化矩阵**：至少覆盖本阶段相关的 `candidate family × row source × point type / Scalar / layout × test × bench × board × asm × doctor × decision`。
5. **实现和测试动作**：每个动作有明确产物、命令或 target、预期证据、负责人角色和完成判据。动作可以依赖其它动作，依赖必须显式写出。
6. **Evidence Doctor 和 registry 规则**：输入 manifest / summary / evidence registry 路径、预期 Errors / Warnings / Suggestions、异常时的重跑、降级、拒绝或暂停动作。
7. **阶段完成条件**：矩阵条目如何进入 `adopted`、`attempted`、`rejected`、`deferred`、`blocked` 或 `not_applicable`，哪些条目必须有同边界证据才能关闭。
8. **板卡复跑预算和决策桶**：run count、warm-up、最大复跑次数、统计口径、positive / weak-positive / neutral / negative / unstable 的判断口径，以及复跑预算耗尽后的降级或人工判断规则。
9. **继续 / 停止条件**：下一阶段默认入口、unblocked next actions、扩大权限或需要人工判断的边界。
10. **文档更新清单**：phase result、topic test 文档、evaluation、Handoff；production 行为只有在真实接入后才同步到 `doc-rvv`。

计划不是愿望清单。每个动作都必须能在 `result.zh.md` 中回填为事实、证据路径、结论和下一步。

## Optimization Matrix

复杂 topic 应维护一张可逐阶段增量更新的矩阵。最小列为：

```text
| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
```

推荐状态：

- `adopted`：当前边界下已采用，必须有足够的 correctness、性能和归属证据。
- `attempted`：已执行但证据或收益不足，必须记录结果和风险。
- `rejected`：当前边界下有证据表明不适用或不值得继续。
- `deferred`：尚未闭合，必须写恢复条件，不得伪装成完成。
- `blocked`：依赖板卡、工具、用户授权或矛盾证据，写明解除条件。
- `not_applicable`：当前 topic 边界确实不适用，必须给源码或输入语义理由。
- `planned`：仅有计划，不能在 Handoff 或 closeout 中写成已覆盖。

当 topic 存在多个 row source policy（尤其 registration topic 中常见的 `full-cloud`、`source-indexed`、`dual-indices` 和 `correspondences`）时，它们必须独立批准。一个 policy 的 adopted family 不会自动关闭其它 policy。代表性点型、`Scalar`、布局和规模也必须在矩阵中单独标出；代表性性能不能外推成全泛型生产性能。

当候选涉及公式、FMA、reduction、staging、ILP 或 LMUL 时，矩阵中必须能回到对应的数值预算、反汇编归属和板卡 A/B 证据。只有源码形式变化、没有机器码或同边界性能差异的候选，标为实现形态诊断，不能写成独立收益。

## 执行循环

worker 按下列步骤循环，直到命中停止条件：

1. **恢复**：读取 Handoff 的 `phase_loop_state`、阶段 README、当前 plan/result、optimization matrix 和相关证据；核对当前 git status 与允许路径；如果 topic 已有 `evidence_registry.json` 或等价机制，先检查是否存在 `unregistered_change`、`unregistered_file` 或 `stale_doc_pending_refresh`。
2. **建计划**：没有当前 plan 时先写；已有 plan 与当前源码或证据不一致时先修订并记录变更原因。计划必须先于本阶段任何实现或测试资产修改。
3. **冻结门禁**：完成 `phase_plan_written_before_edits`、范围、依赖、文档归属、Evidence Doctor 输入和继续 / 停止条件检查。
4. **连续推进**：按依赖顺序完成一个足够大的闭环，通常包括 candidate / test、correctness、QEMU 路径检查、bench / ablation、asm、板卡或明确的板卡阻塞处理。只完成隔离层、一个 target、一个局部 bench 或一个文档段落，不等于阶段完成。
5. **解释证据**：把实际结果、输入口径、A/B 边界、checksum、长尾、异常频率、decision bucket、rerun budget、asm attribution、目标硬件和不能证明的范围写入 `result.zh.md`，并更新矩阵状态。
6. **更新计划**：将剩余动作按 `blocked` / `unblocked` 标记；为下一阶段写默认目标或创建下一阶段 plan。计划变更必须保留原因，不得把未执行动作直接勾成完成。
7. **继续 / 停止决策**：如果存在授权且未阻塞的下一动作，默认继续同轮推进；只有命中明确 stop condition 才输出 Handoff 并停止。

worker 应优先完成能改变决策的证据链，而不是堆积无关 case。阶段大小由“是否形成可审查的决策闭环”决定，不由文件数量决定。

## Evidence Doctor 异常处理

benchmark、board summary、checksum summary、asm attribution 或 EvidenceDecision 前，必须运行 `test-rvv/script/evidence_doctor.py`，或按 `evidence-doctor.zh.md` 人工记录结果。阶段 result 至少记录输入、严重级别数量、每项异常、处理动作和对结论的影响。

- **Error**：例如 checksum 不一致、strict A/B 缺一侧、boundary / wrapper / row source / solve / timer boundary 不一致且未降级、关键 metadata 缺失。先修复并重跑，或把证据降级 / 标为 blocked；不能用未处理 Error 关闭阶段。
- **Warning**：例如 B/A 方向异常、长尾、点型偏离、环境字段缺失、asm 归属不闭合、名称与 metadata 角色冲突。必须写可能解释、最小验证动作和结论边界；未解释 Warning 时不能写成 clean pass。
- **Suggestion**：例如扩大 runs、补 binary hash、补 trace、补温度 / governor / freq 或迁移 topic-local wrapper。可以不阻塞当前阶段，但必须放入矩阵或下一阶段动作，不能无记录丢弃。

如果只有 Markdown 汇总而没有完整机器可读 metadata，结果必须标记 `metadata_incomplete`；它只能作为 reviewer aid，不能替代完整 Evidence Doctor。异常数据不能先验删除，除非证明是测量污染并保留删除理由。

## Phase Result 最小合同

阶段完成或停止前，`result.zh.md` 至少回答：

- 计划版本和实际执行范围是否一致；偏差是什么、为什么发生。
- 每个计划动作的 `done / partial / deferred / blocked` 状态、命令、证据路径和结论。
- optimization matrix 的更新，以及 adopted / attempted / rejected / deferred 的理由。
- correctness、QEMU、asm、board performance 和 production boundary 的分层结论。
- Evidence Doctor 的 Errors / Warnings / Suggestions、异常解释、重跑 / 降级 / 阻塞动作。
- evidence registry 状态：是否 fresh、是否发现人工或未登记复跑、哪些文档需要刷新。
- 板卡 rerun budget 和 decision bucket：是否用完预算、桶是否稳定、是否因此降级或需要人工判断。
- 当前阶段是否完成，未完成项是否还有 unblocked。
- `continue_stop_decision`、`stop_condition_hit` 和 `next_phase_default`。

阶段 result 不替代 topic evaluation、主题文档或 Handoff：它保存阶段探索和测试事实，长期文档只引用已经稳定的结论和证据路径。

## Continue / Stop Criteria

默认继续。以下任一条件成立才允许停止当前 worker 轮次：

1. 当前 phase plan 的完成矩阵已闭合，且没有授权、未阻塞的 next action；下一阶段已经明确标为 `not_yet_started`，并有可恢复的 plan 入口。
2. 用户明确限制本轮范围，且 worker 已完成该范围并记录剩余 loop 状态。
3. 继续需要扩大到未授权的 production 文件、public API、其它 topic、其它入口 / 点型 / `Scalar` / row source，或需要用户批准生产接入。
4. 需要板卡、工具链、远端环境或依赖，当前无法获得；已完成可运行的本地证据，并记录解除阻塞的命令与路径。
5. 板卡复跑预算已经按 plan 用完：如果 decision bucket 稳定，可以用该桶关闭当前证据动作；如果 bucket 仍摇摆，必须标为 `unstable`、降级 EvidenceDecision 或交给 reviewer / 用户判断，而不是继续自动复跑。
6. Evidence Doctor Error 未能修复，registry 显示未登记变更无法归属，或不同证据层之间矛盾，需要 reviewer / 用户判断。
7. dirty isolation 不安全，无法确认哪些文件属于当前阶段，或用户已有修改会被覆盖。
8. 当前结论已经满足 closeout 条件，且剩余方向属于另一个 topic 或明确的用户选择，而不是本阶段计划内动作。

下列情况不是合法停止理由：只补完一个 helper、一个隔离层、一个 target、一次 bench、一个 summary、一个 phase 表，或“已经有一个正向结果”。命中 `micro_stop_guard` 时必须继续当前计划的下一个 unblocked action，或明确写出上面的真实阻塞条件。

## Reviewer 早停检查

reviewer 必须从文件检查 worker 是否过早停止，而不是接受“本轮完成”这句总结。至少核对：

- 当前 plan 是否在首次编辑前存在，时间 / diff 顺序是否合理。
- plan 的每个动作是否在 result 和矩阵中有 `done / partial / deferred / blocked` 证据。
- 是否仍有 `unblocked_next_actions`，若有，worker 是否错误地停下。
- 是否把 `planned`、局部 positive、QEMU timing 或 diagnostic evidence 写成 adopted / production 结论。
- 是否为板卡复跑预设预算和 decision bucket，且没有因数字小幅波动进入无限复跑。
- 是否检查 evidence registry 或等价 freshness 状态；若有人工复跑或未登记覆盖，是否标为 stale / refresh pending。
- Evidence Doctor Warning / Error 是否被解释、重跑、降级或阻塞。
- 阶段 result 是否说明继续 / 停止决定、停止条件和下一阶段默认入口。
- Handoff 的 phase loop 状态、计划路径、结果路径和 dirty isolation 是否能让下一轮一句短 prompt 恢复。

发现早停时，reviewer 应将其列为至少 `High` finding，给出可直接转发的 prompt patch，要求 worker 回到当前 plan 的第一个 unblocked action；不要用“建议以后补”掩盖当前工作流未完成。
