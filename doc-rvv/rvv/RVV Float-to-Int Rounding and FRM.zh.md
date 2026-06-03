# RVV float-to-int rounding 与 FRM 注意事项

## 背景

PCL 中不少几何 / filter 路径会把浮点坐标映射到整数格子，例如 voxel id、grid cell、image bin 或 bucket index。常见标量公式是：

```cpp
int ix = static_cast<int> (std::floor (x * inverse_leaf_size));
```

RVV 化时很容易把它写成“float 向量乘法 + float-to-int 转换”。这里的关键不是转换本身，而是转换的舍入语义必须和 `floor` 一致，并且不能污染后续标量代码的浮点环境。

## `floor` 不是截断

`floor(x)` 是向负无穷方向取整：

| `x` | `floor(x)` | 向 0 截断 |
| ---: | ---: | ---: |
| `1.7` | `1` | `1` |
| `1.0` | `1` | `1` |
| `-0.2` | `-1` | `0` |
| `-1.2` | `-2` | `-1` |

正数上 `floor` 和截断经常相同，因此只用正坐标测试很容易漏掉错误。PCL 点云常包含负坐标，voxel/grid 公式必须覆盖负数样例。

## RVV 中应使用 round-down 转换

若标量语义是 `floor(scaled)`，RVV float-to-int 应使用 round-down，也就是 RDN：

```cpp
inline constexpr unsigned int kRoundDownMode = 2;

vint32m2_t ix = __riscv_vfcvt_x_f_v_i32m2_rm (sx, kRoundDownMode, vl);
```

这里 `_rm` 表示 explicit rounding mode。`kRoundDownMode = 2` 对应 RDN，语义是向负无穷舍入。这样 `-0.52f` 会得到 `-1`，而不是截断得到 `0`。

不要用默认 `vfcvt_x_f` 代替 `floor`，除非已经确认当前 FRM 就是 RDN，且 helper 会恢复原浮点环境。依赖外部 FRM 会让结果受调用上下文影响，不适合库代码。

## FRM / FCSR 副作用

RISC-V 浮点环境中有一个舍入模式状态，通常称为 FRM，属于 FCSR 的一部分。它不是 C++ 局部变量，而是当前线程 / hart 的浮点状态。某段代码如果把 FRM 改成 RDN，后续标量浮点代码可能继续在 RDN 下运行，直到有人改回去。

一些工具链会在 explicit-RM intrinsic 周围生成 `fsrmi` / `fsrm` / `frrm` 指令。即使 intrinsic 看起来只是在某条向量转换上使用指定舍入模式，也必须把它当成“可能影响浮点环境”的代码审查点。

典型污染现象：

- 单独运行 fallback case，Std / RVV 输出一致；
- 同一进程先运行 RVV 主路径，再运行 fallback case，fallback 出现 1 ulp 级坐标漂移或 checksum 不一致；
- fallback 本身没有进入 RVV helper，但继承了前面 helper 留下的 RDN 舍入模式。

## 保存 / 恢复模板

使用 explicit-RM 转换的 RVV helper 应保存调用者原 FRM，并在退出前恢复：

```cpp
inline unsigned int
getCurrentRoundingMode ()
{
  unsigned int mode = 0;
  asm volatile ("frrm %0" : "=r" (mode));
  return mode;
}

inline void
setCurrentRoundingMode (unsigned int mode)
{
  asm volatile ("fsrm %0" : : "r" (mode));
}

bool
someRVVHelper (...)
{
  const unsigned int saved_rounding_mode = getCurrentRoundingMode ();

  while (i < n)
  {
    // scale
    // __riscv_vfcvt_x_f_v_i32m2_rm(..., kRoundDownMode, vl)
    // store integer ids
  }

  setCurrentRoundingMode (saved_rounding_mode);
  return true;
}
```

若 helper 中存在多处 `return true`、异常路径或早退路径，应保证所有可能修改 FRM 之后的出口都会恢复。最简单的结构是：在覆盖条件检查之后保存 FRM，保存之后不再早退，最后统一恢复。若不得不在中途返回，需要使用局部 guard 或显式恢复。

不需要在未命中覆盖条件时保存 / 恢复 FRM。因为此时没有执行会修改 FRM 的 RVV 转换，直接返回 fallback 即可。

## 验证方法

单测和 bench 应至少覆盖以下顺序：

1. 只运行 RVV 主路径，验证 leaf/grid id 与标量公式一致。
2. 只运行 fallback case，验证 Std / RVV 输出一致。
3. 同一进程中先运行 RVV 主路径，再运行 fallback case，验证 fallback 仍与 Std 一致。

第 3 点很关键。FRM 污染是进程内状态问题，单独启动一个只跑 fallback 的二进制通常复现不了。

建议测试数据包含：

- 负坐标，例如 `-0.26 * 2 = -0.52`；
- 位于 voxel/grid 边界附近的值；
- dense 主路径和 non-dense / distance field / 小规模 fallback；
- checksum 或逐点比较，能发现 1 ulp 级漂移。

反汇编证据中可检查：

- `vfcvt.x.f.v`：float-to-int 转换路径存在；
- `fsrmi` / `fsrm` / `frrm`：舍入模式保存、设置或恢复路径存在。

## voxel_grid_covariance 复盘

`filters/voxel_grid_covariance` 的 RVV 优化需要把：

```text
ijk = floor(point.xyz * inverse_leaf_size)
idx = (ijk - min_b).dot(divb_mul)
```

批量预计算成 `leaf_indices`。实现使用：

```cpp
__riscv_vfcvt_x_f_v_i32m2_rm (scaled, kRoundDownMode, vl)
```

来匹配 `Eigen::floor`。

初版 bench 中 `non-dense fallback 256K` 曾出现 Std / RVV checksum 不一致。定位过程显示：

- 单独运行 non-dense fallback 时 Std / RVV 一致；
- 先运行 dense RVV 主路径，再运行 non-dense fallback 时，non-dense 输出出现 1 ulp 级向下漂移；
- non-dense 输入下 `common::getMinMax3D` 仍走 Standard 分支，不是 common RVV min/max 分流导致；
- 根因是 RVV leaf-id helper 没有恢复 FRM，后续标量 fallback 继承了 RDN。

修复后，helper 入口保存 FRM、退出前恢复 FRM。复跑后 `non-dense fallback 256K` 的 Std / RVV checksum 一致。该经验已写入 `doc-rvv/filters/voxel_grid_covariance-RVV.zh.md` 和专项评估文档。

## 检查清单

遇到 `floor` / `ceil` / `round` / float-to-int RVV 化时，先检查：

- 标量语义到底是 `floor`、向 0 截断、round-to-nearest，还是 C/C++ cast；
- 负数测试是否覆盖；
- 是否使用 `_rm` intrinsic 或修改 FCSR/FRM；
- helper 是否在所有修改 FRM 后的出口恢复原 FRM；
- 是否验证“先 RVV 主路径，后 fallback”的同进程顺序；
- 文档是否说明舍入模式、FRM 恢复和 fallback checksum 证据。
