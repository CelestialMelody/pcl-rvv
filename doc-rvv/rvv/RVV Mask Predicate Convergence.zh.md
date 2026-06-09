# RVV 多谓词掩码收敛模式

本文记录 PCL RVV 优化中常见的“多条件筛选”写法：先为每个标量谓词生成一个 `vbool*_t` 掩码，再用 mask 逻辑逐步收敛为最终 `keep` 掩码。它适用于盒裁剪、条件过滤、模型内点判断、有限值过滤等“每个输入元素独立判定是否保留”的主路径。

## 1. 标量语义

标量代码经常把真实成本藏在单点 helper 里。例如 `BoxClipper3D::clipPoint3D(point)` 的语义不是外层 `if`，而是：

```cpp
q = transformation_.matrix() * point.getVector4fMap();
keep = (q.array().abs() <= 1).all();
```

RVV 化前应先把这种 helper 展开成 lane 级公式：

```text
qx = m00*x + m01*y + m02*z + m03*w
qy = m10*x + m11*y + m12*z + m13*w
qz = m20*x + m21*y + m22*z + m23*w
qw = m30*x + m31*y + m32*z + m33*w
keep = abs(qx)<=1 && abs(qy)<=1 && abs(qz)<=1 && abs(qw)<=1
```

PCL 标准点类型通常有 `w = 1`，因此 RVV 主路径可把矩阵最后一列作为常量项加入。若点类型、字段布局、齐次分量或输入索引模式不满足该假设，应回退到标量 helper。

## 2. 比较生成局部掩码

RVV 比较 intrinsic 直接返回掩码类型。以 `vfloat32m2_t` 为主数据类型时，比较结果通常是 `vbool16_t`：

```cpp
vbool16_t keep = __riscv_vmfle_vf_f32m2_b16 (
    __riscv_vfabs_v_f32m2 (tx, vl), 1.0f, vl);
```

这条语句表示当前 VL chunk 中所有 lane 的 `abs(tx) <= 1`。后续维度继续生成掩码并合并：

```cpp
keep = __riscv_vmand_mm_b16 (
    keep,
    __riscv_vmfle_vf_f32m2_b16 (__riscv_vfabs_v_f32m2 (ty, vl), 1.0f, vl),
    vl);
keep = __riscv_vmand_mm_b16 (
    keep,
    __riscv_vmfle_vf_f32m2_b16 (__riscv_vfabs_v_f32m2 (tz, vl), 1.0f, vl),
    vl);
keep = __riscv_vmand_mm_b16 (
    keep,
    __riscv_vmfle_vf_f32m2_b16 (__riscv_vfabs_v_f32m2 (tw, vl), 1.0f, vl),
    vl);
```

这种写法对应标量里的 `&&` 链。所有 lane 执行相同指令流，是否保留由 `keep` 的每一位决定。

## 3. NaN / Inf 语义必须按标量公式选择比较形式

区间判断有两种常见写法：

```text
inside = !(x < min) && !(x > max)
inside = (min <= x) && (x <= max)
```

二者对 NaN 的语义不同。若标量公式是 `abs(x) <= bound`，RVV 应使用 `vfabs + vmfle`，因为 NaN lane 在 `<=` 比较中为 false；Inf 则按大小比较自然被排除。若改写成“不是小于下界且不是大于上界”，NaN 会被误保留。

因此，多谓词收敛的第一步不是选 intrinsic，而是确认标量里的 NaN、Inf、边界闭开区间、符号零等语义。

## 4. VL chunk 算例

假设当前 VL chunk 有 4 个点，矩阵变换后得到：

```text
lane        0      1      2      3
qx        0.2    1.3   -0.7    NaN
qy       -0.4    0.1    1.2    0.0
qz        0.8    0.9   -0.2    0.0
qw        1.0    1.0    1.0    1.0
```

逐维比较：

```text
abs(qx)<=1  1      0      1      0
abs(qy)<=1  1      1      0      1
abs(qz)<=1  1      1      1      1
abs(qw)<=1  1      1      1      1
keep        1      0      0      0
```

最终只保留 lane 0。这里 lane 3 的 `qx = NaN` 被排除，原因正是 `vmfle(abs(NaN), 1)` 为 false。

简图：

```text
x/y/z load -> affine rows -> per-row compare -> AND convergence -> keep mask
```

## 5. 适用条件和回退边界

适合使用该模式的条件：

- 每个元素的判定彼此独立；
- 标量 helper 可展开为固定数量的比较、线性形式或有限值检查；
- 输出可以由最终 `keep` 掩码驱动，例如压缩 indices、压缩距离、条件写回或计数；
- 对 NaN、Inf、边界值的标量语义可以明确复现。

应回退或暂缓的情况：

- 判定依赖前后元素、动态短路副作用或异常处理；
- 单点 helper 内部调用外部函数，无法可靠证明 lane 级公式等价；
- 点类型字段布局不满足 RVV load helper 的覆盖条件；
- 输出需要复杂对象构造，压缩写回会破坏原有生命周期或异常安全。

## 6. 与 Eigen 表达式的关系

Eigen RVV backend 可以优化 Eigen 表达式内部的 packet math，但通常不会把 PCL 的 AoS 点云循环、单点 helper 调用、分支判定和 `push_back` 输出整体重组为跨点 VL chunk。多谓词掩码收敛模式是算法级 RVV 化：保留 Eigen/PCL 的标量语义，把“每点小表达式”提升为“多点同构公式”。

因此，实现文档应明确说明：

- 原标量 helper 的数学公式；
- RVV helper 中每个掩码对应哪条标量谓词；
- 掩码合并顺序与 `&&` / `||` 语义；
- NaN / Inf / 闭开区间的处理；
- 不满足覆盖条件时回退到哪个标量路径。
