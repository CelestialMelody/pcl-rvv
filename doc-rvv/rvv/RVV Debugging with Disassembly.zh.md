# RVV 问题诊断：用反汇编建立线索

## 适用场景

RVV 优化后如果出现以下现象，不能只看 C++ 源码推测原因：

- Std / RVV checksum 不一致；
- 单测通过，但 bench 中某个 fallback case 不一致；
- 单独运行某 case 正常，放进完整 bench 顺序后异常；
- QEMU 与板卡行为不同；
- 编译器 intrinsic 看起来语义正确，但结果仍有 1 ulp 级漂移；
- 反汇编中出现了预期之外的 CSR、舍入模式、mask、tail policy 或 libm 调用。

这类问题通常不是“某一行公式错了”这么简单，可能来自：

- 编译器为 intrinsic 生成的状态修改指令；
- helper 退出后遗留的浮点环境 / 向量状态；
- bench case 顺序导致的进程内状态污染；
- fallback 路径实际经过了其他已有 RVV helper；
- checksum 口径把非确定字段、顺序差异或微小浮点漂移放大。

## 基本原则

诊断时按“现象、隔离、证据、因果、修复、回归”推进。

不要直接把异常归因到最容易想到的外部模块。先回答几个具体问题：

- 差异的原始值是什么，例如 checksum、输出 size、min/max、首尾点；
- 差异是否只在完整 bench 顺序中出现；
- RVV helper 是否真的被当前 case 命中；
- fallback case 是否经过其他 RVV 路径；
- 反汇编是否出现了能解释该现象的指令；
- 修复后是否能复现原触发顺序并消除差异。

## 推荐流程

### 1. 固定原始差异

先从日志中记录具体值，不要只写“不一致”。

示例：

```text
Std checksum: 5717660375792973828
RVV checksum: 16415332133840058372
case: vgcov non-dense fallback 256K
```

如果 checksum 不一致，进一步打印：

- output size；
- min/max；
- 排序后前几个点和后几个点；
- 是否存在 NaN / Inf；
- 每轮迭代 checksum 是否稳定。

这样可以区分：

- 输出集合不同；
- 输出顺序不同；
- 浮点末位漂移；
- checksum 函数本身不适合该 case。

### 2. 做最小复现

不要马上修改正式 bench。先写临时诊断程序或临时 case，把数据构造、参数和 checksum 口径保持一致。

至少跑两种顺序：

```text
只运行可疑 fallback case
先运行 RVV 主路径，再运行可疑 fallback case
```

如果“只运行 fallback”一致，而“先 RVV 后 fallback”不一致，说明问题很可能是进程内状态污染，而不是 fallback 公式本身。

### 3. 确认路径命中

在源码层面检查分流条件：

- 当前 case 是否满足 RVV 覆盖条件；
- 是否因为 `is_dense`、字段名、点类型、小规模、indices 等条件回退；
- fallback 是否调用了 common / search / transform 等其他模块的 RVV helper。

如果源码显示当前 case 没有进入本主题 RVV helper，但完整 bench 顺序仍受影响，就应优先怀疑“之前 case 留下的状态”。

### 4. 用反汇编找状态线索

反汇编不是只用来证明存在 `vle` / `vse`。遇到异常时，要查找能改变执行环境的指令。

常用查询：

```bash
rg -n "vfcvt|fsrmi|fsrm|frrm|fscsr|frcsr" build/asm/...full.asm
rg -n "vsetvli|vcompress|vfirst|vfred|vlse32|vluxei32" build/asm/...full.asm
```

重点关注：

- `fsrmi` / `fsrm` / `frrm`：浮点舍入模式 FRM/FCSR 是否被读写；
- `vfcvt.x.f.v`：float-to-int 转换是否存在，是否可能依赖或修改舍入模式；
- `vsetvli`：SEW/LMUL/tail policy 是否与预期一致；
- `vcompress` / mask 指令：压缩路径是否可能改变输出顺序或计数；
- `vfred*`：归约顺序是否与标量不同；
- `vlse` / `vluxei`：AoS stride/gather 是否符合字段布局。

反汇编只提供线索，不直接等于结论。看到 `fsrmi zero,2` 只能说明代码路径可能设置 RDN；还需要用顺序实验验证它是否污染后续 fallback。

### 5. 构造因果实验

根据反汇编线索设计实验，而不是继续猜。

如果看到 `fsrmi zero,2` / `fsrm`：

- 单独跑 fallback；
- 先跑包含 `_rm vfcvt` 的 RVV 主路径，再跑 fallback；
- 在 helper 中保存 / 恢复 FRM 后复跑同一顺序。

如果看到 `vcompress`：

- 打印保留元素数量；
- 比较输出顺序；
- 使用 order-independent checksum 和逐点排序对比。

如果看到 `vfred*`：

- 增加容差检查；
- 打印相对误差 / 绝对误差分布；
- 用小规模手算 case 确认公式正确。

目标是建立下面这种因果链：

```text
完整 bench 复现差异
单独 fallback 不复现
反汇编显示前序 RVV case 修改 FRM
先 RVV 后 fallback 可复现 1 ulp 漂移
保存 / 恢复 FRM 后同一顺序不再复现
```

这样的结论比“可能是外部路径”可靠得多。

## voxel_grid_covariance 案例

`filters/voxel_grid_covariance` 初版 bench 中，`non-dense fallback 256K` 曾出现 Std / RVV checksum 不一致。

最初容易误判为：

```text
non-dense fallback 不进入本主题 RVV helper，
所以差异可能来自 common::getMinMax3D 或其它外部路径。
```

后续排查推翻了这个判断。

### 现象

正式 bench 中：

```text
Std checksum: 5717660375792973828
RVV checksum: 16415332133840058372
```

但单独运行 non-dense fallback 诊断程序时：

```text
Std checksum: 5717660375792973828
RVV checksum: 5717660375792973828
```

### 反汇编线索

RVV bench 反汇编中能看到：

```text
fsrmi zero,2
vfcvt.x.f.v
fsrm ...
```

这说明 leaf-id 的 explicit round-down 转换确实涉及 FRM。`2` 对应 RDN，用于匹配 `floor`。

### 因果实验

临时诊断程序先跑 dense RVV 主路径若干次，再跑 non-dense fallback。此时 non-dense 的 min/max 和输出点坐标出现 1 ulp 级向下漂移，checksum 改变。

这证明问题不是 non-dense fallback 自己的公式，也不是 `common::getMinMax3D` 的 RVV 分流，而是前序 RVV 主路径遗留的浮点舍入模式影响了后续标量路径。

### 修复

`computeVoxelGridCovarianceLeafIndicesRVV` 在进入 RVV 转换前保存 FRM，完成所有 VL chunk 后恢复：

```cpp
const unsigned int saved_rounding_mode = pcl::getVoxelGridCovarianceRoundingMode ();

while (i < n)
{
  // __riscv_vfcvt_x_f_v_i32m2_rm(..., kRoundDownMode, vl)
}

pcl::setVoxelGridCovarianceRoundingMode (saved_rounding_mode);
```

修复后，同一顺序复跑：

```text
先 dense RVV 主路径，再 non-dense fallback
checksum: 5717660375792973828
```

正式 bench 中 Std / RVV 也恢复一致。

## 写入文档时应记录什么

函数级评估文档中至少记录：

- 原始异常值；
- 临时诊断如何复现或证伪；
- 反汇编中看到的关键线索；
- 最终确认的因果链；
- 修复方式；
- 复跑哪些 test / bench / asm；
- 如果某个早期推测被证伪，要明确写出来。

不要只写：

```text
checksum 不一致，已修复。
```

更好的记录方式：

```text
checksum 不一致只在“先 RVV 主路径、后 fallback”的同进程顺序中复现；
反汇编显示 explicit-RM vfcvt 会操作 FRM；
保存/恢复 FRM 后同一顺序不再复现。
```

## 常用命令

生成反汇编：

```bash
make -C test-rvv/<module>/<topic> clean_bench_rvv dump_bench_rvv
```

查关键 RVV 指令：

```bash
rg -n "vlse32\\.v|vle32\\.v|vluxei32|vse32\\.v|vfcvt\\.x\\.f\\.v|vsetvli" \
  test-rvv/<module>/<topic>/build/asm/riscv/*full.asm
```

查浮点环境指令：

```bash
rg -n "fsrmi|fsrm|frrm|fscsr|frcsr" \
  test-rvv/<module>/<topic>/build/asm/riscv/*full.asm
```

查 bench 解析问题：

```bash
rg -n "未解析|n/a|Total Time 不计算" \
  test-rvv/<module>/<topic>/output/qemu/analyze_bench_compare.log
```

## 检查清单

遇到异常时，按下面顺序过一遍：

- 是否记录了原始 Std / RVV 差异值；
- 是否单独运行了可疑 case；
- 是否验证了完整 bench 顺序；
- 是否确认当前 case 命中 RVV、fallback，还是间接受益；
- 是否检查过反汇编中的状态指令；
- 是否设计了能验证反汇编线索的实验；
- 是否修复后复跑了同一触发顺序；
- 是否把问题、证伪的推测和最终结论写入评估文档 / 主题文档 / 工作日志。
