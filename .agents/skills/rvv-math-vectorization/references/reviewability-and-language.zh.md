# RVV 数学函数原型的术语补充

通用的术语解释、配置解析出的测试资产注释密度、Makefile/Python 注释和 reviewer 汇报规则见：

```text
../../rvv-workflow/references/reviewability-and-language.zh.md
```

本文只补充 RVV 数学函数拟合中容易误读的术语。文档、注释和回复中，下面术语首次出现时也要解释；中文主导时给中文解释，英文主导时也要给 plain-English explanation（白话解释），必要时再补中文解释。

## 数学 helper 层次

- `kernel（核函数，通常指约化区间上的多项式近似）`：只处理已经 range reduction 后的小区间输入，不是完整 `sin/cos/exp/log`。
- `range reduction（范围约化，把原始输入变换到适合多项式拟合的小区间）`。
- `reconstruction（重构，把核函数结果还原到原函数输出）`。
- `finite-domain helper（有限输入域 helper，只承诺某个有限区间内的快速近似）`。
- `strict libm replacement（严格 libm 替换，需要覆盖公开 libm 语义和特殊值）`。
- `full helper（完整 helper，包含域检查、约化、核函数、重构和特殊值策略）`。
- `lane-level RVV helper（RVV lane 级 helper，只处理向量寄存器和 vl，不负责数组循环、load/store 或 strip-mining）`。
- `batch wrapper（批量包装层，负责数组 load/store 和 strip-mining，通常只用于测试或上层驱动）`。
- `experimental path（实验路径，只用于候选对比，不是 production 形态）`。

## 误差和证据口径

- `kernel error（核函数误差，只看约化区间上的多项式误差）`。
- `dense-chain simulation（密集全链路仿真，包含约化、float FMA、重构和特殊值策略）`。
- `scalar same-chain（标量同构链路，用和 RVV 相同的操作顺序做对拍）`。
- `RVV path（RVV intrinsic 链路）`。
- `reference path（参考链路，通常是 libm 或 double reference）`。
- `ULP（相邻浮点表示单位距离）`。
- `absolute error（绝对误差）`。
- `relative error（相对误差，过零函数附近不能作为主指标）`。

## 周期函数常见术语

- `bounded mask segmentation（有限域 mask 分段，用比较 mask 显式划分区间）`。
- `quadrant reconstruction（象限重构，根据区间决定 sin/cos 交换和符号）`。
- `hi/lo split（高低常量拆分，用 high part 加 low part 降低约化误差）`。
- `FRM/FCSR（RISC-V 浮点舍入模式/浮点控制状态寄存器）`。
- `FMA（融合乘加）`。

这些术语如果出现在代码注释或文档中，应同时说明证据边界。例如：`kernel（核函数）` 不是完整 helper，`caller smoke（调用方 smoke）` 不能替代数学专项测试，`QEMU correctness（QEMU 正确性）` 不能作为性能结论。
