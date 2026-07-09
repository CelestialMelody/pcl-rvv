# 3D 向量锐角计算 (`getAcuteAngle3D` & `acos`)

## 1. 功能与数学原理

`getAcuteAngle3D`（计算两个向量的锐角）和 `acos`（反余弦函数的快速近似）用于快速计算两个 3D 向量之间的**锐角**夹角。

> `getAcuteAngle3D` & `acos`函数位于 common.hpp 中。

### 1.1 `getAcuteAngle3D` 原理

计算两个向量 $\vec{v_1} = (x_1, y_1, z_1)$ 和 $\vec{v_2} = (x_2, y_2, z_2)$ 夹角的公式为：

$$
\theta = \arccos \left( \frac{\vec{v_1} \cdot \vec{v_2}}{|\vec{v_1}| |\vec{v_2}|} \right)
$$

**PCL 的特殊优化假设**：

1. **输入已归一化**：输入的向量要求是单位向量（模长为 1），因此分母 $|\vec{v_1}| |\vec{v_2}| = 1$，省略除法。
2. **只求锐角**：向量的点积可能是负数（表示钝角）。为了获得锐角（0 到 90度），我们需要对点积取绝对值：$| \vec{v_1} \cdot \vec{v_2} |$。
3. **数值稳定性 (Clamping)**：由于浮点误差，点积结果可能略微超过 1.0（例如 1.000001），这会导致 `acos` 返回 NaN。因此必须将输入限制在 `min(1.0, |dot|)`。

   > 向量单位化可从代码编写角度，以及测试文件 test_sample_consensus_plane_models.cpp 的使用角度看出。
   >

### 1.2 `pcl::acos_RVV_f32m2` (快速近似) 原理

标准的 `std::acos` 计算非常耗时。RVV 当前实现保留上游 PCL 八常数 sqrt 结构作为测试 baseline，但 `common.hpp` 中的 `pcl::acos_RVV_f32m2` 已改用更直接的约化模型：

$$
\arccos(x) \approx \sqrt{u}\,Q(u),\quad u=1-x
$$

其中 \(Q(u)\) 为 5 次多项式，系数来自 `test-rvv/common/common/script/parms_acos.py --method remez2-deg5`（默认区间 `x in [0, 0.999]`）。采用 deg5 remez2 是精度与速度的折中：相对旧 PCL 八常数结构，误差从约 `7.75e-4 rad` 降到约 `1.31e-6 rad`，同时板卡历史记录显示速度接近旧 RVV PCL 路径。

---

## 2. 代码迁移分析：从 AVX 到 RVV

下面对比分析如何将 x86 AVX 代码转换为 RISC-V RVV 代码。

### 2.1 `pcl::acos_RVV_f32m2` 实现

RVV 逻辑仍使用 FMA 链做 Horner 求值，但输入变量从旧结构中的 `x` 改为约化变量 `u = max(1 - x, 0)`。实现步骤为：

1. 计算 `u = 1.0f - x`，并用 `vfmax` 避免输入略大于 1 时出现负数开方。
2. 用 Horner 计算 deg5 remez2 多项式 `Q(u)`。
3. 返回 `sqrt(u) * Q(u)`。

核心代码形态如下：

```cpp
vfloat32m2_t u = __riscv_vfsub_vv_f32m2(one, x, vl);
u = __riscv_vfmax_vf_f32m2(u, 0.0f, vl);

vfloat32m2_t q = q5;
q = q4 + u * q;
q = q3 + u * q;
q = q2 + u * q;
q = q1 + u * q;
q = q0 + u * q;

return sqrt(u) * q;
```

| **操作** | **AVX (__m256)** | **RVV (vfloat32m2_t)** | **说明** |
| ------ | ------ | ------ | ------ |
| **广播常数** | `_mm256_set1_ps(1.5f)` | `__riscv_vfmv_v_f_f32m2(1.5f, vl)` | 将标量复制到整个向量寄存器 |
| **多项式计算** | Horner 链 | `vfmacc(acc, u, q)` | RVV 使用 FMA 计算 `Q(u)` |
| **平方根** | `_mm256_sqrt_ps` | `__riscv_vfsqrt_v_f32m2` | 对 `u=1-x` 开方 |

---

### 2.2 `getAcuteAngle3D` 实现对比

#### A. 点积 (Dot Product)

- **AVX**: 需要分别计算 `x1*x2`, `y1*y2`, `z1*z2` 然后两次 `add`。

  ```cpp
  const __m256 dot_product = _mm256_add_ps (_mm256_add_ps (_mm256_mul_ps (x1, x2), _mm256_mul_ps (y1, y2)), _mm256_mul_ps (z1, z2));
  ```

- **RVV**: 使用 `vfmacc` 级联，无需中间临时变量。

  ```cpp
  // dot = x1*x2 + y1*y2 + z1*z2
  const vfloat32m2_t dot = __riscv_vfmacc_vv_f32m2(
    __riscv_vfmacc_vv_f32m2(
        __riscv_vfmul_vv_f32m2(x1, x2, vl),
        y1, y2, vl),
    z1, z2, vl);
  ```

#### B. 绝对值 (Absolute Value)

- **AVX (Bit Hack)**:

  ```cpp
  // 清除符号位：与 "-0.0f" (0x80000000) 进行 AND NOT 操作
  _mm256_andnot_ps(_mm256_set1_ps(-0.0f), dot_product)
  ```

- RVV (专用指令):

  RISC-V 提供了浮点符号注入指令 vfsgnj (Sign Injection)。

  - `vfsgnjx`: 结果符号 = `src1` 符号 XOR `src2` 符号。
  - **技巧**：如果 src1 和 src2 是同一个数，`Sign XOR Sign` 永远是 0 (正)。

  ```cpp
  // 绝对值：自己异或自己的符号位 -> 正数
  vfloat32m2_t dot_abs = __riscv_vfsgnjx_vv_f32m2(dot, dot, vl);
  ```

#### C. 数值截断 (Clamp to 1.0)

- **AVX**: `_mm256_min_ps(val, 1.0f)`
- **RVV**: `__riscv_vfmin_vf_f32m2(val, 1.0f, vl)` (直接使用向量-标量版本，更简洁)

---

## 3. 参数与验证记录

参数来源：

- 脚本：`test-rvv/common/common/script/parms_acos.py`
- 模型：`acos(x) ~= sqrt(1-x) * Q(1-x)`
- 当前默认：`deg5 remez2`
- 系数：
  - `q0 = 1.414212408248559`
  - `q1 = 0.117926522053977`
  - `q2 = 0.02571508511147162`
  - `q3 = 0.01095480727067022`
  - `q4 = -0.002360310714948563`
  - `q5 = 0.004346735271181379`

本轮替换前复核：

```bash
make -C test-rvv/common/common parms_acos
make -C test-rvv/common/common run_acos_test
```

QEMU 专项结果：当前 `pcl::acos_RVV_f32m2` 最大误差 `1.311302e-06 rad`，与标量 deg5 remez2 的 `max |diff|` 为 `0`；历史 PCL 八常数 baseline 最大误差为 `7.749423e-04 rad`。

板卡专项结果：`make deploy_acos_test` 后在板卡侧执行 `make run_acos_test` 通过。当前 `pcl::acos_RVV_f32m2` 最大误差 `1.311302e-06 rad`，与标量 deg5 remez2 的 `max |diff|` 为 `0`；计时为 `29.203 ms`，`17.01x vs std`。同次测试中 RVV deg7 remez2 最大误差为 `2.384186e-07 rad`，速度约为当前 common.hpp 的 `0.89x`。
