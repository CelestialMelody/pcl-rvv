# 语义对齐诊断

源码公式相同不等于机器级求值结构相同。RVV helper 手工展开 Eigen 小表达式、投影、距离、阈值或 float-to-int 逻辑时，应默认列入语义对齐诊断。

## 诊断顺序

1. 固定最小 adversarial case，优先构造整数边界、阈值边界、小 `z`、大/小量混合或 tie 附近数据。
2. 查看标量和 RVV 构建的反汇编，确认标量是否出现 `fmadd.s`、`fmsub.s`、自动向量化 reduction，以及 RVV 是否出现预期 `vfmacc.*`、`vfadd.*`、`vfmul.*`、`vfcvt.*`。
3. 判断差异来自转换/舍入、FRM/FCSR、FMA contraction、运算重排、临时精度，还是后续 gather / append 状态变化。
4. 差异可控时，优先用 fused intrinsic、调整累加顺序、显式舍入、保存恢复 FRM/FCSR、局部标量求值或边界 lane 回退恢复一致。
5. 如果不同编译器、优化选项或目标 ISA 下无法稳定对齐，保留为诊断或继续放在标量 tail。

## Production 决策树

语义异常定位后，按下面口径记录生产判断：

- 可修复：差异能归因到有限 instruction ordering、舍入模式、FMA contraction、初值或操作数顺序变化，并且可用 fused intrinsic、调整累加顺序、显式舍入、保存恢复 FRM/FCSR、局部标量求值或边界 lane 回退恢复 full output 一致。
- 可带条件修复：差异只在可检测边界 lane 出现，可以通过标量回退保持输出顺序和状态一致，同时 full diagnostic 或 production case 在目标硬件仍有收益。文档写清边界检测条件、漏判风险、回退比例和性能成本。
- 暂缓 production：当前数据集可修复，但依赖特定编译器 contraction、优化选项或目标 ISA；需要更多 adversarial 数据、点类型、构建选项或目标硬件验证。
- 不能进入 production：RVV 算法结构必然改变标量可见顺序，例如规约树无法复刻线性累加、跨 lane 压缩/重排影响状态机、gather 后异常/NaN/边界处理无法与逐点路径同步，或所有可行回退都会消除收益 / 引入不可维护复杂度。

不能在未尝试 FMA、显式舍入、FRM/FCSR、边界 lane 回退或标量 tail 诊断前，直接把问题写成不可 RVV 化。

## 断言口径

容差只适用于业务允许近似的路径，例如统计量或明确误差预算的近似数学函数。

以下路径默认要求 bit / predicate / output 等价：

- correspondences。
- indices。
- sampled indices。
- pixel / voxel id。
- 阈值筛选。
- 保序 staging。

若 stored field bit pattern 可由公开输出观察到，RVV 可以前移 predicate，但写出值可能仍需标量重算以绑定生产标量表达式。

## 文档记录

文档中记录：

- 失败现象。
- 反汇编线索。
- 导致差异的具体表达式。
- 修复后的 intrinsic 或 fallback 形态。
- 回归测试。
- 生产接入判断。

不要只写“浮点误差导致失败”。
