# Benchmark 与组件消融

本文定义 benchmark（性能测试）、board（板卡）证据和 component ablation（组件消融）规则。

## Bench 输出合同

bench 输出必须可解析。至少保留：

- dataset（数据集）或 case 名。
- iterations（迭代次数）。
- 每个 case 的平均耗时。
- total time（总耗时）或等价摘要。
- checksum（校验和）。
- 构建模式、目标硬件或 QEMU 标记。
- 被测代码路径、bench wrapper、analysis script（分析脚本）和 output summary（输出摘要）的仓库相对路径；复杂 topic 应引用 Traceability Map（可追踪性地图）章节或说明 `not_required` 理由。

复杂 topic 的 bench 文档必须给出 case label（用例标签）语法或字典。读者看到一条 label 后，应能判断：

- 入口边界是真实 public overload、production helper、production-shaped test helper，还是 component no-solve helper。
- row source policy 是 full-cloud、source-indexed、dual-indices、correspondences 或其它形态。
- 点型、字段 layout、权重来源和输入规模是什么。
- 计时是否包含 setup、index/weight 展开、solver、matrix 构造或 trace 输出。
- 该 label 能证明什么，不能证明什么。

进入板卡采集前，每个 baseline 和 candidate 都必须单独完成最小可运行性检查。检查至少覆盖：

- 编译目标能够生成对应 binary。
- 输入规模命中预期 RVV gate；需要回退的规模也要单独验证回退。
- 单次运行能够结束，并输出非空 checksum、accepted-point 或等价路径统计。
- 同边界 A/B 的 checksum 和 correctness 结果符合预期。

这个检查用于发现递归 wrapper、错误模板实例化、错误 layout 调用和未命中 gate。QEMU 可以完成编译、正确性和日志形状 smoke，但默认不得在 QEMU 上执行 `run_bench_compare`、完整 bench matrix，或任何会生成 Std/RVV 数值对比表的 bench compare target；QEMU bench compare 没有性能意义且浪费时间。若确实需要 QEMU bench smoke，只运行非 compare、极小规模、少 case、少 iteration 的可运行性检查，并写清 `qemu_smoke_only`。bench 的数值分析只能在板卡或目标硬件上做。

QEMU timing（QEMU 计时）不作为性能结论。QEMU 只用于 correctness（正确性）、路径和日志形状；若历史遗留或特殊调试必须保留 QEMU bench 输出，文档必须标成 `qemu_smoke_only`，并说明它不是默认执行路径、不能进入性能排序、采纳审计或 EvidenceDecision。

## 优化采纳证据索引

复杂 topic 中每个 adopted、attempted、deferred 或 rejected 优化方式都应有证据索引。索引至少包含：

- production 代码路径或 test_support 代码路径。
- 对应 test target。
- 对应 bench target 或 board collect target。
- 当前结果和证据文件。
- 证据边界和不能外推的范围。

每个可由 `run_bench_*` 触发的 case-filter 应有对应的 `run_board_bench_*` 单次板卡 smoke target，或明确映射到 repeated board collect target。单次板卡 smoke target 必须写清输出目录和证据等级；它只证明板卡可运行、日志形状和 checksum 输出，不能写成性能结论。

如果某个优化方式没有对应 target，只能写成 `planned`、`audit`、`diagnostic gap` 或 `not_yet_covered`。不能把历史聊天结论、未提交 raw log 或未解释的 bench label 写成采纳证据。

默认综合 bench 不能单独承担范围取舍结论。若结论涉及 row source policy、点型布局、公式变体或其它候选集合，应提供专门 case-filter / make target，或在 evidence index 中把该项降级为 diagnostic gap。综合 bench 可以作为 smoke、日志形状检查或宽口径诊断。

当 dedicated diagnostic 或 repeated board summary 显示某个候选稳定正向时，应启动 adoption audit（采纳审计）。审计至少记录：

- 候选是否能接入 production public entry。
- 需要新增哪些 production direct / fallback / gate tests。
- dedicated bench target 和 board target 的名称。
- repeated board summary 的路径和结果。
- asm attribution 或等价路径证据是否已生成。
- 若不采纳，阻塞条件、退化数据或维护成本是什么。

如果正向信号来自 row-source diagnostic，审计表里要单独列出触发日志路径和最终 repeated summary 路径。触发日志只说明升级理由，不替代最终生产证据。

如果 topic 已经形成一个 adopted implementation family（已采纳实现族），新 production 接入口不能只拿旧 diagnostic helper 里第一个正向实现直接接入。worker 必须先做 implementation-family comparison（实现族比较），至少尝试当前已采纳家族迁移到新入口，或给出为什么不适用的证据化理由。这里的“已采纳家族”包括 block-reduction、fused formula、ILP code shape、layout-gated generic path、staged-gather / compressed-tail path 等。没有比较就直接接入，只能写成 provisional adoption（暂定采纳）或 deferred family comparison（实现族比较暂缓），不能写成 current best。

implementation-family comparison 可以复用同一 math kernel 或 reduction/formula helper，但 row-source ingress 必须按 full-cloud、source-indexed、dual-indices 和 correspondences 分别适配。统一 family 不等于统一证据；每个 policy 的 gather、index staging、weight source、mask 和计时边界都要单独记录。

当 diagnostic repeated summary（重复诊断摘要）为 negative（负向）但 production public
Std/RVV repeated summary（真实公开入口标量 / RVV 重复摘要）为 positive（正向）时，不要直接拒绝
production probe（生产探针），也不要直接 clean-adopt（干净采纳）新 family。worker 必须先标记
comparison-boundary / baseline mismatch（比较边界 / 基线不一致）：public Std/RVV 只回答
“当前 public RVV path 是否快于 public scalar path”，不能证明某个新 RVV family 快于已有 adopted
RVV family。若决策问题是 family selection（实现族选择），必须补同一 production boundary（生产边界）
内的 RVV-vs-RVV detail A/B，或把新 family 保留为 explicit probe（显式探针）/ experiment path（实验路径），默认路径继续使用已有 adopted family。

候选升级为 adopted 后，文档中的 case-filter 字典、细粒度 target 表、EvidenceDecision 和提交证据白名单必须同步更新。

## Evidence Doctor 证据体检

benchmark、board summary、checksum summary、asm attribution 或 EvidenceDecision 前，必须按 `evidence-doctor.zh.md` 执行 Evidence Doctor（证据体检 / 证据校验器）检查。检查可以由 `artifact_layout.evidence_doctor_script_template` 解析出的脚本读取 JSON manifest 自动完成，也可以在尚未接入脚本的 topic 中按规则人工填写；无论哪种方式，都必须把 Errors、Warnings 和 Suggestions 写入 summary、evaluation 或 Handoff Packet。

Evidence Doctor 用于阻止 worker 无解释地跳过可疑数据。典型输入应包含 case name、case kind、build、point type、size、iterations、warm-up、run count、baseline / candidate 的 boundary、wrapper、row source、formula mode、solve、checksum policy、checksum、asm boundary、RVV 指令计数、B/A values 和板卡环境字段。

- Error：例如 checksum 不一致、strict A/B 缺少两侧 metadata、严格对比两侧 boundary / wrapper / row source / solve / checksum policy / timer boundary 不一致且未降级。Error 必须先修正或降级证据角色，否则不能作为 production evidence 或严格性能结论。
- Warning：例如某点型明显偏离、`B/A < 1` 频率偏高、median 正向但长尾明显、std/RVV speedup 与 RVV-vs-RVV B/A 冲突、asm 归属不闭合、环境字段缺失或名称暗示的证据角色与 metadata 不一致。Warning 可以继续分析，但结论必须说明风险、可能原因和处理动作。
- Suggestion：例如建议扩大 runs、补 binary hash、补 per-iteration trace、补温度 / governor / freq 或把 topic-local analyzer 迁成 manifest 生成器。Suggestion 不阻塞当前结论，但应进入下一轮检查建议。

若 summary 只含 Markdown 表格而没有机器可读 metadata，Evidence Doctor 必须输出 metadata 不完整的 warning；这类轻量检查只能作为 reviewer aid，不能写成完整 doctor 通过。复杂 topic 应优先由 topic-local wrapper 生成 JSON manifest，再调用 `artifact_layout.evidence_doctor_script_template` 解析出的全局 doctor。

## 对比口径

A/B 是实验设计：A 是 baseline（基线），B 是 candidate（候选）。文档和 summary 中必须写清 A/B 两侧各自调用什么路径，例如 test-only helper、public-like wrapper、真实 public overload 或 production dispatch。

候选与基线默认必须使用同一实现边界。公式消融、layout-gated helper 消融和 component no-solve 消融都应让 A/B 两侧共享同一个 wrapper、row source、gate、mask、reduction、solve 和 checksum 口径，只改变待测候选。若 full estimate 和 component no-solve 的 sink 不同，文档必须显式写成 mixed-sink component diagnostic（混合 sink 的组件诊断）或等价含义，不能直接拿它们当严格 candidate-vs-baseline B/A。若一次实验有意把真实 public overload 和 test-only helper 放在一起，它必须命名为 mixed-boundary cross-check（混合边界交叉检查）或等价含义，文档要说明不能作为严格 candidate-vs-baseline B/A。

若使用 `B/A` 表示候选相对基线的收益，必须同时写清公式和方向，例如 `B/A = A_rvv_ms / B_rvv_ms`，其中 `>1` 表示 B 比 A 更快，`<1` 表示 B 退化。不要把每个 case 自身的 `std/RVV speedup` 当成候选相对 baseline 的收益；`std/RVV speedup = std_ms / rvv_ms` 只说明同一个 case 的标量与 RVV 构建差异。

direct diagnostic、production-shaped diagnostic 和 production direct 若同时出现，必须分表或分段报告。diagnostic 的 B/A 只能支持候选筛选或消融归因；只有真实 public overload / production dispatch 的 repeated board 结果才能作为 production performance evidence。

## 性能证据

板卡或目标硬件 benchmark 才能支撑性能结论。结论中必须写清：

- 目标硬件。
- 数据规模和输入构造。
- case 覆盖的 row source policy（行来源策略）或 production 入口。
- 计时边界是否包含 index/weight 展开、buffer 写回、solver、输出构造或 wrapper。
- speedup（加速比）来自哪条日志或分析脚本。
- 文档、测试、脚本和 output 如何互相定位；如果 summary 是 bench 结论主归属，evaluation / 主题文档只引用 summary 路径和关键结论，不复制 raw log。
- 当前不能证明什么。

弱收益、退化或不同规模趋势不一致时，不要写成单一原因。必须列出可验证假设和下一轮消融条件。

## 板卡复跑预算与决策桶

板卡性能有自然波动。worker 不应为了让每个 speedup 数字完全稳定而无限复跑；phase plan 或 repeated summary 必须先写清 bounded rerun budget（有界复跑预算）和 decision bucket（决策桶）规则。

默认策略：

- 先运行计划内 repeated board（重复板卡）采集；若 Evidence Doctor 或 summary 显示方向接近阈值、长尾严重、`B/A < 1` 频率异常或与旧文档结论冲突，最多再做一次同边界确认复跑，除非用户明确要求扩大预算。
- 使用 topic plan 定义的阈值把结果归入 `positive`、`weak_positive`、`neutral`、`negative` 或 `unstable`。没有 topic-specific 阈值时，可以把明显大于 1 的稳定结果写成 positive / weak_positive，接近 1 且跨越方向的结果写成 neutral 或 unstable；不要用单次 min / max 改变结论。
- 如果新旧 run 的精确数值不同但 decision bucket、方向、Evidence Doctor 严重级别和 EvidenceDecision 不变，长期文档只需引用当前 run label / summary；精确数字留在 summary，不在多个文档复制。
- 如果复跑预算耗尽后 decision bucket 仍摇摆，结论写成 `unstable`，并把 EvidenceDecision 降级为 blocked、no-production 或需要人工判断；不要继续自动复跑。
- summary 必须记录 run count、warm-up、taskset、governor、freq、温度、统计口径、`B/A < 1` 频率和是否用完 rerun budget。

有界复跑不是降低证据要求。它只把“持续波动”转成可审查状态：稳定桶可以停止，跨桶摇摆要降级或交给人工决策。

## Production Evidence 决策优先级

判断 RVV production（生产源码）是否接入或保留时，按下列优先级组织证据：

1. RVV 实现比 std（当前标量 / 标准实现基线）更好。若没有比 std 好，不能只靠局部消融或源码形态写成可接入 production。
2. 静态实现质量更高。这一项和第 1 项是主要参考因素，必须审计公式形态、目标指令吞吐、RAW dependency（read-after-write，写后读依赖）、寄存器压力或 spill 风险、ILP / unroll（指令级并行 / 展开）、LMUL `m1/m2/m4`（向量寄存器分组）取舍，以及 asm attribution（反汇编归属，关键 RVV 指令是否归属于 production 符号或 hot path）。
3. 平均情况更好。文档必须声明使用的平均口径，例如 summary 脚本定义的 mean、median 或 repeated-board 汇总代表值。
4. 异常频率不算很高。异常值不能先验剔除，除非能证明是测量污染；异常频率应作为人工风险判断输入。
5. bench 类结论必须来自 board 或 target hardware。QEMU bench compare 不能进入性能排序、采纳审计或 EvidenceDecision，worker 默认不得在 QEMU 上运行 `run_bench_compare` 或完整 bench matrix；只在必要调试时做非 compare 的窄范围 smoke。

第 1 和第 2 是主门槛。若二者闭合，而第 3 或第 4 存在争议，例如平均值受少数异常点影响、个别 case 的 `B/A < 1` 频率偏高但有合理解释，人工仍可决定接入；此时 output summary、evaluation 或主题文档必须说明数据分析口径、异常值情况、可能原因、风险边界，以及为什么仍接受接入。

## 组件消融

component ablation（组件消融）只是瓶颈线索，不等于端到端 profile（剖析），也不能单独决定 production。

适合拆分的组件：

- stride load（跨步加载）。
- gather（离散加载）。
- query/match 或 index/weight 展开。
- finite mask（有限值掩码）。
- `vcompress` 和 buffer 写回。
- scalar tail（标量尾段）。
- vector reduction（向量规约）。
- FMA（融合乘加）。
- solver、矩阵构造或后处理。

消融结果必须写清“拆掉了什么”和“仍包含什么”。只测局部 helper 不能外推到 production direct（真实生产路径证据）。

## 负向归因

correspondences / indexed 路径退化不能单因归因为 gather。可疑来源包括 query/match 展开、容器访问、baseline 更短、分布局部性、后段成本、`vcompress`、buffer 写回和自动 reduction。没有消融 bench 或 profile 时，这些只能写成假设。

## Fused formula 消融口径

fused formula（融合公式）候选必须拆分成独立候选再判断，不要把一个理论上的大改动直接当 production 结论。registration 类题目里，至少应把 `abc`、`d-six-term`、`d-displacement`、`abcd` 以及各自的 `ILP` 变体拆开看；LMUL `m1/m2/m4` 也应作为独立候选验证，不能默认同一机器码形态会保留收益。

对比口径必须统一到同一份 RVV binary（RVV 二进制）或同一份 log（日志）内的 RVV-vs-RVV B/A；不要拿不同 std/RVV speedup 互相比候选。`QEMU timing` 只能用于 build、correctness 和日志形状，不作为性能证据。

`vfmsac`、`vfnmsac`、`vfmacc` 等 fused intrinsic 的 operand order（操作数顺序）必须用 correctness test（正确性测试）和 asm attribution（反汇编归属）确认。不要只按源码表面符号重排操作数；若重排会改变 normal-equation、矩阵符号或 checksum，必须先修公式或放弃该候选。

fused formula 的 `block-baseline` 应代表同一 test_support 或同一 production helper 边界下的非 fused 公式。production public overload 可作为 production direct 证据，也可作为明确标注的 mixed-boundary cross-check；它不能在未说明的情况下放进 helper 消融表当作中性的 `block-baseline`。

如果某个 topic 里 full-cloud 已经采纳了更强的实现族，而 source-indexed 或其它 row source 入口还停留在旧实现，worker 必须把“当前入口为什么不适合迁移相同实现族”写成证据化说明，不能默认旧实现足够好。缺少这层说明时，evaluation 和主题文档必须把该入口标成 `implementation-family comparison pending`，而不是直接写成最终最优实现。

如果 source-indexed 当前采用 staged-gather / compressed-tail，这仍然属于 adopted family；但 evaluation 和主题文档必须同时写出它与 source-indexed block-reduction / fused-formula family 的比较状态。当前 family adopted 不等于已完成家族比较。

production asm attribution（生产反汇编归因）不能只硬编码一种符号形态。编译器可能把 detail helper 内联/合并到 RVV wrapper、public overload 或 clone 边界。脚本应按优先级寻找真实承载边界，并在输出表中记录 `boundary`；缺少某个 detail 符号只能触发调用链排查，不能直接写成 RVV 路径未命中。

compiler auto-vectorization（编译器自动向量化）诊断是窄用途辅助证据。S2 早期评估中，如果不确定是否需要手写 RVV，可以运行 `generate_vec_report` 判断编译器对目标循环的处理倾向。S4 证据计划中，如果结论依赖“编译器不会自动向量化”或“自动向量化不足”，应记录 missed-vectorization report（未自动向量化报告）的摘要或说明未使用原因。S8 反汇编归因仍以 objdump 和符号级 attribution 为主；missed-vectorization 报告只用于解释编译器自动向量化疑点。

板卡摘要必须记录 warm-up、run count、taskset、governor、freq 和温度；异常值不能先验剔除，除非能证明是测量污染。报告应同时给出 median / min、decision bucket 和 `B/A < 1` 的频率，并在预设 rerun budget 内判断某个点型或候选是否稳定绑定。板卡摘要写入 EvidenceDecision 前必须引用 Evidence Doctor 结果；若存在未解决 Warning，摘要必须说明对应可验证假设、是否重跑、是否降级证据边界、是否用完 rerun budget，以及为什么当前结论仍成立或为什么阻塞。
