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

QEMU timing（QEMU 计时）不作为性能结论。QEMU 只用于 correctness（正确性）、路径和日志形状。

## 性能证据

板卡或目标硬件 benchmark 才能支撑性能结论。结论中必须写清：

- 目标硬件。
- 数据规模和输入构造。
- case 覆盖的 row source policy（行来源策略）或 production 入口。
- 计时边界是否包含 index/weight 展开、buffer 写回、solver、输出构造或 wrapper。
- speedup（加速比）来自哪条日志或分析脚本。
- 当前不能证明什么。

弱收益、退化或不同规模趋势不一致时，不要写成单一原因。必须列出可验证假设和下一轮消融条件。

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
