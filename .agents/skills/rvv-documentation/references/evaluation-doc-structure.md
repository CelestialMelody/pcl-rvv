# 函数级评估文档结构

函数级评估面向决策审计，强调测试矩阵、bench case、当前证据 / rerun 结果 / 历史基线、测试保留策略和最终接入判断。

推荐结构：

- 范围和目标源码。
- 函数级结论。
- 函数族评估表。
- RVV 诊断或实现设计。
- 标量流程与 RVV 流程对照。
- Traceability Map（可追踪性地图；复杂 topic 必写或引用）。
- 实现方式审计。
- 测试计划和 bench 计划。
- 当前状态。
- 验证结果。
- 增量诊断结果。
- 生产接入判断。
- 生产接入后的最终证据更新。

## 测试计划表

专项测试较多时，加入逐项表格：

```text
| 测试 | 层级 | 作用 |
```

`层级` 可使用低层 helper、production-shaped diagnostic、production direct、RVV-only 诊断、fallback、traits、标量 tail guard、local fragment 等稳定标签。

`作用` 要说明测试覆盖的对象状态、indices 形态、点类型、边界数据、fallback 或输出顺序，不要只重复测试名。

## 生产接入表述

不要把 speedup 当成单独开关。评估文档需要写出证据链：

```text
local fragment -> full diagnostic -> production case -> production decision
```

弱收益生产接入必须同时说明入口常用度、实现大小、fallback、语义风险、测试完整性、反汇编证据、板卡结果和维护成本。

生产接入判断应按 `rvv-test/references/performance-and-ablation.zh.md` 的 Production Evidence 决策优先级写清：
先判断 RVV 是否比 std（当前标量 / 标准实现基线）好，再判断静态实现质量是否更高；这两项是主要参考因素。
静态实现质量至少覆盖公式形态、指令吞吐、RAW dependency（read-after-write，写后读依赖）、寄存器压力、ILP / unroll（指令级并行 / 展开）、LMUL `m1/m2/m4`（向量寄存器分组）取舍和 asm attribution（反汇编归属）。平均情况和异常频率可作为人工判断依据，但不能替代前两项。若 benchmark、board summary、checksum 或 asm attribution 触发 Evidence Doctor（证据体检）Warning，evaluation 必须引用 doctor 结果并说明处理动作；不能只写“异常可接受”。

如果第 1 和第 2 项闭合，而平均情况或异常频率存在争议但人工决定接入，evaluation 文档必须留痕：
数据分析口径、异常值情况、可能原因分析、风险边界、为什么仍接受接入。不要只写“人工判断可接受”。

如果当前只适合 bench 诊断主题，应明确授权边界：诊断代码位于专项测试区域，上游生产入口保持不变，直到补齐 production-like 证据。

## 日期和“最新”表述

evaluation 文档会长期保留，不应把证据锚定到“最新一次”“最新日志”“截至目前”这类会随时间漂移的词。推荐写法：

- 写“本轮 board rerun 结果”“当前证据”“历史基线”“rerun 结果 supersedes 历史基线”。
- 用 `artifact_layout.board_output_subdir`、`artifact_layout.qemu_output_subdir`、summary 文件、manifest 或 run label 说明证据来源。
- 若有多轮结果，把旧结果写成“历史基线”或“rerun 前对照”，把新结果写成“rerun 结果”，不要写“最新结果”。

日期允许出现在 work log、manifest、run id、handoff / recovery path 和用户指定目录名中。不要批量改这些位置；它们承担恢复和审计语义。扫描清理时应白名单保留 ICP “最近点”、checksum、常量、指令立即数、数据规模和 run id。

如果当前结论是 partial-production-candidate（局部生产候选），评估文档必须把“有收益的诊断路径”和“可以进入 production integration loop 的范围”分开写。至少列出：

- 哪个公开入口形态或诊断路径有板卡收益。
- 哪些入口形态或数据布局已经有负向证据，必须保持标量。
- 当前还缺哪些 production direct（直接生产路径）、fallback、泛型点类型、`Scalar=double`、上游测试、反汇编归属和板卡 production bench 证据。
- 为什么本轮不直接修改 production，以及下一轮如果继续，第一步应该验证什么。

如果用户已经授权 production integration loop（生产接入闭环），评估文档不能停在 PI1 计划。PI1 只冻结
候选范围和暂停条件；只要 PI1 gate 闭合，worker 应在同一轮补写 PI2-PI5 的最终证据更新。只有用户明确要求
“只做 PI1 / 只写计划”，或生命周期暂停条件命中时，才把 PI1 作为本轮终点。

## 生产接入后的最终证据更新

PI5 后，evaluation 文档必须更新 production decision（生产接入判断），至少包含：

- `production_patch_scope`：真实改动了哪些 production 文件、helper、dispatch 和 `__RVV10__` gate。
- `covered_path`：已证明的入口、点类型、`Scalar`、数据布局、规模 gate 和目标硬件。
- `fallback_matrix`：非 RVV 构建、非覆盖点类型、`Scalar=double`、indices、correspondences、小规模和布局不满足时的回退证据。
- `production_direct_tests`：真实公开入口和 fallback 的测试命令、结果和日志路径。
- `production_asm`：反汇编中关键 RVV 指令是否能归属到 production 符号或内联范围。
- `production_board_bench`：板卡或目标硬件 production direct bench 结果；QEMU 不得写成性能结论。
- `decision_delta`：诊断阶段结论如何被生产证据确认、缩窄、推翻或回退。

最终 EvidenceDecision 必须基于 production direct 证据。如果生产证据弱于诊断证据，评估文档应降低结论
或进入 rollback/no-production，而不是沿用诊断阶段 speedup。

## 函数级门禁

函数级评估必须先回答：

- 具体可 RVV 化函数、loop 或 helper 是什么。
- 标量路径如何工作：关键循环、关键局部变量、公式、状态更新、solver 或输出写回分别做什么。
- 源码真实数据流是什么：公开入口是否已经通过 iterator（迭代器）、indices（索引）、correspondences（对应关系）、mask（掩码）、wrapper（包装层）或 dispatch（分流逻辑）把不同输入形态统一；如果统一了，必须说明统一前后各自是什么。
- 候选 RVV 路径准备如何工作：load/gather（加载/离散加载）、mask（掩码）、staging（分阶段暂存）、store/reduction（写回/规约）、scalar tail（标量尾段）和 fallback（回退路径）的职责边界。若 RVV 诊断把源码统一流重新拆成多条显式数据流，说明每条流的入口来源、访存形态、额外展开成本和 bench 计时边界。
- RVV 是否覆盖入口主成本。
- full diagnostic 或 production case 是否能在目标硬件上证明收益。
- fallback 和维护边界是否可控。
- 如果来自 bench 诊断主题，诊断问题是否代表真实入口。

## 方案取舍记录

当实现或诊断采用某个非唯一方案时，评估文档应记录取舍，而不是只记录最终代码形态。常见取舍包括：

- 使用固定 buffer、staging 结构或 `vcompress`，而不是直接在向量寄存器中完成后续计算。
- 保留 scalar tail（标量尾段），而不是做 vector reduction（向量规约）、scatter 写回或批量状态机。
- 使用或暂缓 fused multiply-add（融合乘加）intrinsic（内建函数）。
- 是否值得向量化只偶尔调用的数学函数、矩阵构造、solver 前后处理或 wrapper。

每个取舍至少写清：替代方案是什么，当前为什么选择或暂缓，语义风险是什么，性能风险是什么，需要哪些 correctness、asm、消融 bench 或板卡证据才能改变判断。

## Traceability Map

复杂 topic 的 evaluation 文档应包含或引用 Traceability Map（可追踪性地图）。它用于定位候选设计、测试资产、bench wrapper、analysis script、output summary、主题文档章节和 production 代码位置。

推荐表格：

```text
| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
```

本表不替代主题文档的长期实现说明，也不替代 output summary 的统计结果。evaluation 中的 map 主要服务决策审计：为什么这些候选被采用、尝试、暂缓或拒绝，以及 reviewer 应从哪些代码和输出复核。

如果主题文档已经有完整 map，evaluation 可以只引用路径和 anchor（章节 / 符号 / run label），再补 candidate 或 bench 决策需要的少量行。

## 实现方式审计

evaluation（函数级评估）文档应保留轻量“实现方式审计”表，用来帮助 reviewer 复核当前实现选择和证据边界。主题 RVV 文档负责长文解释“当前采用的优化方式”；evaluation 文档只记录决策矩阵，避免复制大段实现说明。

推荐表格：

```text
| 实现维度 | 当前状态 | 证据 | 边界 / 恢复条件 |
```

`当前状态` 可使用 `adopted`、`attempted`、`deferred`、`rejected`、`not_now` 或当前 EvidenceDecision。表格至少按当前 topic 风险覆盖这些维度：

- dispatch / fallback：真实 public entry（公开入口）是否命中 RVV，非覆盖路径如何回退。
- layout / traits gate：点类型、字段 offset、stride、AoS / SoA、weight、index 或 correspondence 的布局条件。
- staging / reduction：固定 buffer、`vcompress`、block reduction、vector reduction、scatter、scalar tail 或其它组织方式。
- formula / FMA：公式树、FMA contraction、误差预算、反汇编归属和是否需要消融。
- row source policy：full-cloud、source-indexed、dual-indices、correspondences 或其它入口形态是否逐 policy 独立批准。
- production scope：production direct、production-shaped diagnostic、bench 诊断主题或 no-production 的最终边界。

如果主题文档已经有完整“当前采用的优化方式”小节，evaluation 文档可以只保留表格和证据路径；不要把聊天过程或历史流水账搬进长期评估文档。候选路线、bench 统计和 output summary 应按 `document-ownership-and-traceability.zh.md` 的归属矩阵引用，避免同一段结论在主题文档、evaluation、summary 和 Handoff 中重复。

## Bench 说明要求

bench 计划和结果表不应只写 case 名。每个 case 至少说明：

- 输入数据如何构造，是否合成、随机、真实入口抽样或对抗样本。
- 测量的入口和代码路径，是否包含 setup、索引展开、权重复制、solver、输出写回或只测局部 helper。
- 证明点和不能证明的边界。
- 结果不好时的初步归因和下一步定位实验；没有定位证据时明确写成待验证假设。

建议队列主题和 bench 诊断主题都必须经过函数级评估。建议队列默认回答“是否值得生产接入”；bench 诊断主题默认回答局部诊断问题、收益归因或生产价值是否成立。

## 问题记录

实现、测试或 bench 中发现的问题不能只停留在对话里。评估文档要记录：

- 发现现象。
- 最小复现或触发条件。
- 路径命中和反汇编线索。
- 定位过程。
- 最终处理。
- 为什么不影响当前结论，或为什么导致暂缓 / 回退。

典型问题包括 std/RVV checksum 不一致、bench case 被移除或改小、fallback case 被外部已有 RVV 路径干扰、上游完整套件中出现非当前主题失败、临时诊断证伪早期推测、目标硬件性能不成立。

## 上游测试与专项测试

如果目标有对应上游原始测试，评估文档应说明是否补跑、命令入口、参数来源和结果。没有对应上游测试或上游测试覆盖范围过大时，写明不强制新增的理由，不能写成验证失败。

新增 RVV 覆盖函数时，同步补专项 test 和 bench case。板卡结果回来前，不把新增 case 写成真实性能结论。
