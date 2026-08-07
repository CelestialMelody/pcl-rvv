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

进入板卡采集前，每个 baseline 和 candidate 都必须单独完成最小可运行性检查。检查至少覆盖：

- 编译目标能够生成对应 binary。
- 输入规模命中预期 RVV gate；需要回退的规模也要单独验证回退。
- 单次运行能够结束，并输出非空 checksum、accepted-point 或等价路径统计。
- 同边界 A/B 的 checksum 和 correctness 结果符合预期。

这个检查用于发现递归 wrapper、错误模板实例化、错误 layout 调用和未命中 gate。QEMU 可以完成这一步，但 QEMU timing 不作为性能结论。

QEMU timing（QEMU 计时）不作为性能结论。QEMU 只用于 correctness（正确性）、路径和日志形状。

## 对比口径

A/B 是实验设计：A 是 baseline（基线），B 是 candidate（候选）。文档和 summary 中必须写清 A/B 两侧各自调用什么路径，例如 test-only helper、public-like wrapper、真实 public overload 或 production dispatch。

候选与基线默认必须使用同一实现边界。公式消融、layout-gated helper 消融和 component no-solve 消融都应让 A/B 两侧共享同一个 wrapper、row source、gate、mask、reduction、solve 和 checksum 口径，只改变待测候选。若一次实验有意把真实 public overload 和 test-only helper 放在一起，它必须命名为 mixed-boundary cross-check（混合边界交叉检查）或等价含义，文档要说明不能作为严格 candidate-vs-baseline B/A。

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

## Production Evidence 决策优先级

判断 RVV production（生产源码）是否接入或保留时，按下列优先级组织证据：

1. RVV 实现比 std（当前标量 / 标准实现基线）更好。若没有比 std 好，不能只靠局部消融或源码形态写成可接入 production。
2. 静态实现质量更高。这一项和第 1 项是主要参考因素，必须审计公式形态、目标指令吞吐、RAW dependency（read-after-write，写后读依赖）、寄存器压力或 spill 风险、ILP / unroll（指令级并行 / 展开）、LMUL `m1/m2/m4`（向量寄存器分组）取舍，以及 asm attribution（反汇编归属，关键 RVV 指令是否归属于 production 符号或 hot path）。
3. 平均情况更好。文档必须声明使用的平均口径，例如 summary 脚本定义的 mean、median 或 repeated-board 汇总代表值。
4. 异常频率不算很高。异常值不能先验剔除，除非能证明是测量污染；异常频率应作为人工风险判断输入。

第 1 和第 2 是主门槛。若二者闭合，而第 3 或第 4 存在争议，例如平均值受少数异常点影响、个别 case 的 `B/A < 1` 频率偏高但有合理解释，人工仍可决定接入；此时 output summary、evaluation 或主题文档必须说明数据分析口径、异常值情况、可能原因、风险边界，以及为什么仍接受接入。

## 组件消融

component-only ablation（仅组件消融）只是瓶颈线索，不等于端到端 profile（剖析），也不能单独决定 production。

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

production asm attribution（生产反汇编归因）不能只硬编码一种符号形态。编译器可能把 detail helper 内联/合并到 RVV wrapper、public overload 或 clone 边界。脚本应按优先级寻找真实承载边界，并在输出表中记录 `boundary`；缺少某个 detail 符号只能触发调用链排查，不能直接写成 RVV 路径未命中。

compiler auto-vectorization（编译器自动向量化）诊断是窄用途辅助证据。S2 早期评估中，如果不确定是否需要手写 RVV，可以运行 `generate_vec_report` 判断编译器对目标循环的处理倾向。S4 证据计划中，如果结论依赖“编译器不会自动向量化”或“自动向量化不足”，应记录 missed-vectorization report（未自动向量化报告）的摘要或说明未使用原因。S8 反汇编归因仍以 objdump 和符号级 attribution 为主；missed-vectorization 报告只用于解释编译器自动向量化疑点。

板卡摘要必须记录 warm-up、run count、taskset、governor、freq 和温度；异常值不能先验剔除，除非能证明是测量污染。报告应同时给出 median / min 和 `B/A < 1` 的频率，并在必要时扩大轮数判断某个点型或候选是否稳定绑定。
