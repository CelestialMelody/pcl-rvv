# 数值一致性、FMA 与 Reduction

本文定义 RVV 浮点测试、反汇编归属和误差预算规则。

## 数值一致性

worker（执行者）手写 RVV 公式时，必须说明标量参考链路来自哪里：

- 原 production 源码。
- test-only 标量复刻。
- libm 或 double reference（双精度参考）。
- 已有上游测试的期望值。

测试输出应记录最大误差、平均误差或 checksum（校验和）。误差阈值要说明和 production 语义的关系。

## FMA

FMA（fused multiply-add，融合乘加）不是默认禁止项。是否使用 FMA 取决于：

- production 标量编译是否已经发生 FMA contraction（融合乘加收缩）。
- RVV intrinsic（内建函数）是否改变中间舍入。
- 当前函数的误差预算是否允许。
- 是否有 same-chain（同构链路）或对抗样本测试覆盖阈值附近输入。
- 是否有反汇编归属证明 FMA 指令属于当前热点 helper。
- 是否有必要的板卡 A/B benchmark（同一板卡上的 A/B 性能测试）。

不能因为源码不是 fused（融合写法）就默认禁止 FMA。也不能只因为 FMA 可能更快就跳过误差测试。

## Reduction

vector reduction（向量规约）会改变累加树。使用或暂缓时都要写清：

- 标量路径的累加顺序。
- RVV 规约顺序和 lane（向量通道）分组。
- accepted lane 的保序要求。
- 对 `ATA/ATb`、checksum 或阈值判断的误差影响。
- 反汇编中 `vfred*` 指令是否属于当前 helper。
- 板卡 A/B 是否显示收益。

如果编译器对 scalar tail（标量尾段）自动向量化并生成 reduction 指令，文档必须记录该事实。它说明实际执行链路已经不同于源码表面形状。missed-vectorization（未自动向量化）报告可辅助解释这类现象；普通构建不默认生成该报告，需要用 `generate_vec_report` 或 `ENABLE_VEC_MISSED=1` 显式开启。

## 反汇编归属

反汇编不能只证明整个二进制里出现某条 RVV 指令。必须尽量归属到：

- 当前 RVV helper。
- production dispatch 后的真实符号范围。
- bench harness（性能测试框架）。
- Eigen/libm 或编译器自动向量化区域。
- 无关库代码。

归属不清时，结论写成“指令存在但热点归属未闭合”。
missed-vectorization 报告只能辅助解释编译器是否接管了标量循环。S8 的主证据仍来自 objdump（反汇编导出）和符号级 asm attribution（反汇编归因）。
