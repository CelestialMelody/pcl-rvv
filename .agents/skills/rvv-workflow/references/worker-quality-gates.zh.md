# RVV Worker 质量门禁

本文是短 prompt worker（执行者）的轻量质量门禁。它不替代各 skill（技能）的详细规则，
只规定 worker 在写代码、文档、bench 或 handoff 前必须产出的最小结构。

## 何时读取

- 短 prompt 进入普通 topic 后、开始写 `test-rvv/`、`doc-rvv/` 或 production 前读取。
- 如果 topic 涉及 staging（分阶段暂存）、gather（离散加载）、`vcompress`、scalar tail（标量尾段）、
  vector reduction（向量规约）、FMA（融合乘加）、目标硬件性能或 no-production closeout，
  必须继续读取本文列出的详细 reference。
- 如果 topic 从 `partial-production-candidate`（局部生产候选）继续到 PI1 production integration plan
  （生产接入计划），必须读取 `rvv-implementation/SKILL.md`、`point-load-store.md`、
  `fallback-and-dispatch.md`；若生产入口是模板点类型或要从 `PointNormal` 诊断扩展到泛型入口，
  必须读取 `doc-rvv/rvv/RVV Generic Point Type Strategy.zh.md`。

## 最小门禁

### 1. 标量路径重建

S2 evaluation（函数级评估）不能只写函数名或数学名词。worker 必须写清：

- public entry（公开入口）和 wrapper / iterator / dispatch（包装层 / 迭代器 / 分流逻辑）如何进入目标 helper；
- 关键循环、有限值检查、核心公式、状态更新、solver（求解器）和输出构造；
- 哪些阶段在逐点热点循环内，哪些阶段每次调用只执行一次；
- RVV 计划覆盖哪一段标量路径，哪些阶段保留标量以及原因。

详细规则见 `rvv-documentation/references/evaluation-doc-structure.md`。

### 2. Production 数据流与诊断数据流映射

如果 production 源码通过 iterator、indices、correspondences、wrapper 或 dispatch 把多种入口统一，
而 diagnostic（诊断代码）为了 RVV 显式拆成 full-cloud（全云顺序扫描）、gather、scatter 或 staging，
worker 必须写清映射关系：

- 每条 diagnostic 路径来自哪个公开入口；
- 额外 index/weight 展开、offset 计算、buffer 写回是否计入 bench；
- 这些拆分能证明什么，不能证明什么。

详细规则见 `rvv-diagnostics/SKILL.md` 和 `rvv-documentation/references/topic-doc-structure.md`。

### 3. 主题文档质量

主题 RVV 文档至少要包含：

- 函数入口作用和标量路径；
- RVV 数据流、VL chunk（可变向量长度分块）、mask（掩码）、staging、tail 和 fallback（回退路径）；
- 实现选择审计，例如 buffer、`vcompress`、scalar tail、FMA、vector reduction 或数学函数向量化；
- bench case 的输入构造、计时边界、证明点和不能证明的边界；
- QEMU、反汇编、板卡证据分别支持什么；
- no-production 时的受证据约束归因和后续消融条件；
- partial-production-candidate（局部生产候选）时的生产直连缺口，例如真实公开入口 direct test、fallback、点类型 traits、`Scalar=double`、indices / correspondences 策略、生产 bench 重跑和人工确认点。

如果当前模块已有最近通过 reviewer 的 sibling topic（同模块相邻主题）文档，worker 应把它作为质量标杆读取或抽样对照。对照目标是结构深度、解释粒度和证据边界，不是复制 topic 特有结论、参数或性能数字。

详细规则见 `rvv-documentation/SKILL.md`、`topic-doc-structure.md` 和 `evaluation-doc-structure.md`。

### 4. Test-rvv / diagnostic 注释

`test-rvv` 和 diagnostic 代码必须面向 reviewer（审查者）可读：

- 长文件有“本文件做什么”和阅读提示；
- 非平凡 helper 说明作用、调用者、production 语义映射和证据角色；
- 每个非显然 `TEST` 或 bench case 前说明验证什么、为什么需要、失败代表哪条证据断了；
- 专业英文术语首次出现时解释其在当前 topic 中的工程含义；
- production 注释保持克制，只解释维护边界、fallback、dispatch、数据布局和数值风险。

详细规则见 `rvv-workflow/references/reviewability-and-language.zh.md`。

### 5. 证据和归因

worker 必须分开写：

- correctness（正确性）证据；
- QEMU path/log-shape（QEMU 路径 / 日志形状）证据；
- disassembly（反汇编）证据；
- board/target performance（板卡 / 目标硬件性能）证据；
- negative evidence（负向证据）；
- unsupported claims（当前证据不能支持的说法）。

QEMU 不能作为性能结论。板卡退化只能支持 no-production，不能自动证明 gather、buffer、
FMA 或 tail 是单一主因；没有消融 bench 或 profile 时必须写成假设。

详细规则见 `rvv-benchmarking/SKILL.md` 和 `rvv-benchmarking/references/qemu-board-disassembly.md`。

### 6. PI1 生产接入计划门禁

当 worker 继续一个 `partial-production-candidate` topic 并进入 PI1 时，先产出 production integration plan，
再考虑生产补丁。若用户目标是“进入 / 推进 production integration loop（生产接入闭环）”，PI1 是同轮
PI2-PI5 的前置 gate（门禁），不是默认终点；PI1 闭合且未命中暂停条件时，worker 应继续生产补丁、
生产直连测试、生产证据重跑和 PI5 再决策。只有用户明确要求“只做 PI1 / 只写计划”，或 PI1 发现范围、
fallback、测试或泛型策略不能闭合，才停在 PI1。PI1 至少写清：

- 候选范围：入口形态、点类型、`Scalar`、数据布局、规模 gate、目标硬件和已证明收益。
- 不接入范围：indices、correspondences、泛型点类型、`Scalar=double` 或其它未闭合路径是否保持标量。
- dispatch / fallback：`__RVV10__` 关闭、小规模、VLEN、字段布局、点类型 traits、非覆盖入口如何回退。
- production direct test：真实公开入口如何证明命中 RVV 分流，fallback 如何单独证明。
- 泛型点类型策略：如果 production 模板入口不应收窄，使用 traits / offset / POD / standard-layout gate；如果只接 `PointNormal`，必须写明这是有意收窄且其它类型 fallback。
- 反汇编和板卡计划：如何按 production 符号范围确认 RVV 指令，如何重跑目标硬件 bench。

PI1 中不要把诊断路径 speedup 写成 production-ready。只有 PI2-PI5 后 production direct 证据闭合，才能升级 EvidenceDecision。

### 7. PI2-PI5 连续推进门禁

当用户用短 prompt 授权继续 production integration loop（生产接入闭环），且最近 Handoff Packet 的
`next_worker_action_if_review_passes` 已给出 PI2 范围时，worker 可以同轮连续推进 PI2-PI5。连续推进前必须冻结：

- `pi2_scope`：入口、点类型、`Scalar`、数据布局、规模 gate 和不可触碰路径。
- `forbidden_expansion`：不得扩大到 PI1 未授权的泛型、indices、correspondences、public API 或公共 helper 变更。
- `fallback_matrix`：非 RVV 构建、非覆盖点类型、`Scalar=double`、小输入、VLEN/buffer、indices、correspondences 等回退项。
- `evidence_commands`：PI3/PI4 需要运行的 test、bench、asm 和 board 命令。
- `pause_conditions`：命中 `topic-lifecycle.zh.md` 中连续推进暂停条件时停止并输出 Handoff Packet。

PI2-PI5 结束后，Handoff Packet 必须新增或等价覆盖：

- `production_patch_summary`：生产补丁范围和未触碰路径。
- `production_direct_results`：真实公开入口命中 RVV 的测试结果。
- `fallback_results`：每个 fallback gate 的测试或构建证据。
- `asm_hotspot_attribution`：按 production 符号范围归属关键 RVV 指令。
- `board_production_results`：目标硬件 production direct bench 结果；若未运行，写明阻塞原因。
- `pi5_evidence_decision`：基于 production direct 证据的新 EvidenceDecision。

PI2-PI5 结束后必须继续完成 S11 文档 closeout（收尾文档）。worker 不能只更新代码和日志后结束。
最终文档门禁至少覆盖：

- `production_doc_scope`：主题文档和 evaluation 文档是否写清真实生产补丁范围、覆盖入口、点类型、`Scalar`、数据布局和目标硬件。
- `production_doc_fallbacks`：是否用表格或等价结构列出 fallback 矩阵，包含非 RVV 构建、非覆盖点类型、`Scalar=double`、indices、correspondences、小规模和布局不满足路径。
- `production_doc_evidence`：是否把 production direct test、fallback test、asm 符号归属和 board production bench 分别写清，而不是沿用诊断 bench 结论。
- `production_doc_decision_delta`：是否说明诊断阶段结论如何被生产证据确认、缩窄、推翻或回退。
- `production_doc_remaining_scope`：是否写清仍保持标量或未覆盖的入口，以及下一轮扩展必须补的证据。

### 8. 窄范围结论后的后续路径门禁

当当前结论不是“整个模板入口都完成”，而是 `narrow`、`partial`、`bench-only`、`no-production`
或带有明确 fallback / 未覆盖范围时，worker 不能只写“进入下一个 topic”。必须在最终输出和
Handoff Packet 中给出 `followup_options_for_user`：

- `recommended_default`：默认建议，例如“评审通过后进入下一个 topic”或“先补一个 fallback 测试”。
- `continue_current_topic`：如果继续当前 topic，最值得做的扩展是什么，范围必须窄到入口、点类型、`Scalar` 和证据。
- `separate_followup_topic`：哪些方向应该另开 follow-up topic 或消融 topic，例如 indexed / correspondences 退化归因。
- `not_recommended_now`：哪些方向当前不建议直接做，以及阻塞证据是什么。

对 production-ready/narrow 结论，worker 必须主动回答：

- 这个窄范围是否漏掉了常见泛型入口。
- 如果要扩大到泛型点类型，应读取哪些策略文档、需要哪些 traits / offset / fallback / board 证据。
- 哪些入口虽然“看起来相近”，但因为负向性能、语义风险或测试缺口不能一起扩大。

`next_worker_action_if_review_passes` 仍只保留一个默认动作；其它重要选择放在 `followup_options_for_user`，
避免用户只能靠人工复查发现下一步。

## 开始写文件前的自查

worker 在创建或更新 topic 产物前，先确认：

```text
scalar_path_ready:
production_to_diagnostic_mapping_ready:
doc_quality_refs_loaded:
test_comment_strategy_frozen:
bench_timing_boundary_defined:
alternative_designs_listed:
evidence_model_defined:
stop_condition_defined:
pi1_production_scope_ready:
generic_point_type_strategy_ready:
fallback_dispatch_strategy_ready:
production_direct_test_plan_ready:
pi2_scope_ready:
fallback_matrix_ready:
pi2_to_pi5_pause_conditions_ready:
production_direct_results_ready:
fallback_results_ready:
asm_hotspot_attribution_ready:
board_production_results_ready:
pi5_evidence_decision_ready:
production_doc_closeout_ready:
followup_options_ready:
```

若其中任一项为 false，先补评估或设计，不要直接开始堆代码和结论。

## 交接前的证据化门禁

最终 Handoff Packet（交接数据包）里的 `worker_quality_gate_check` 不能只写 true / false。worker 必须把上面的自查改写成可复核表格：

```text
| gate（门禁项） | status（状态） | evidence（文件 / 章节 / 行或段落） | missing_items（缺口） |
```

要求：

- `status` 可写 `pass`、`partial`、`fail` 或 `not_applicable`；不要用没有证据的 `true`。
- `evidence` 至少指向当前 topic 的 evaluation、主题文档、test-rvv 注释、bench 说明、证据日志或 Handoff 段落。
- `missing_items` 必须写成陈述句；没有缺口时写 `none`。
- 若 `language_check` 声称通过，必须能在同一张表或相邻段落中指出诊断代码、测试、bench 和文档的术语 / 中文注释证据。
- 若当前结论强于 no-production，例如 `partial-production-candidate`，表格必须额外列出 production direct 尚未闭合的证据项，避免把诊断收益误写成 production-ready。
- 若本轮进入 PI1，表格必须额外列出 `pi1_production_scope_ready`、`generic_point_type_strategy_ready`、
  `fallback_dispatch_strategy_ready` 和 `production_direct_test_plan_ready`。不适用时写明原因，不能省略。
- 若本轮连续推进 PI2-PI5，表格必须额外列出 PI2 scope、fallback matrix、暂停条件、production direct
  results、fallback results、asm hotspot attribution、board production results、PI5 EvidenceDecision
  和 production doc closeout。
- 若当前结论是窄范围 production-ready、partial-production-candidate、bench-only/no-production 或保留重要未覆盖范围，
  表格必须列出 `followup_options_ready`，并指向 Handoff Packet 或文档中的可选后续路径。

这张表是给 reviewer 复核的，不是为了加长最终回复。证据可以用章节名或稳定路径摘要，不需要复制长文档内容。
