# expf_RVV_f32m2 实现说明

本文描述 `pcl::expf_RVV_f32m2` 的算法与 RVV 实现细节，并给出板卡侧测试数据。该函数面向批量 `float` 输入，返回与 `std::expf` 同量级的数值结果；实现以单精度吞吐为目标，输入在向量内夹取到 \([-88,88]\)。

---

## 1. 接口与语义

接口声明位于 `common/include/pcl/common/common.h`，实现位于 `common/include/pcl/common/impl/common.hpp`，仅在 `__RVV10__` 下可用。`vl` 由调用方通过 `vsetvl_e32m2` 设定，支持 strip-mining 与尾部处理。

接口签名（摘自 `common/include/pcl/common/common.h`）：

```cpp
expf_RVV_f32m2(const vfloat32m2_t& x, const std::size_t vl);
```

---

## 2. 数学路径与实现映射（约化 → 多项式 → 重构）

指数函数在远离 0 的区域直接做多项式展开会出现明显的误差与数值问题。这里采用常见的三段式结构，把计算约化到小区间内完成，再恢复到原尺度：

\[
e^x = e^{n\ln 2 + r} = 2^n\cdot e^r
\]

### 2.1 参数来源

本实现依赖三类常量：\(\ln 2\) 的拆分常量（`kExpfLog2Inv/kExpfLog2Hi/kExpfLog2Lo`）、输入夹取区间（`kExpfXMin/kExpfXMax`）以及 Remez 多项式系数（`kExpfRemezC0..C7`）。这些常量在仓库内有对应脚本与 Makefile 入口，便于复现与校验。

- \(\ln 2\) 拆分与基础常量：`test-rvv/common/common/script/parms.py` 使用 `decimal` 高精度计算 \(\ln 2\)，再将 `ln2` 的高位部分量化为 float32（脚本输出为 `kLog2Hi`），低位部分为 `kLog2Lo = ln2 - kLog2Hi`，并输出 `kLog2Inv = 1/ln2` 与 `kTwoToMinus127 = 2^-127` 等常量。对应 Makefile 目标为 `parms`。

- Remez 系数：`test-rvv/common/common/script/parms_expf.py` 默认在区间 \([-\ln 2/2,\ln 2/2]\) 上输出 `exp(r)` 的 degree=7 多项式（与 `round` 约化一致；可用 `--r-lo/--r-hi` 覆盖）。默认 `report` 包含 `remez1`、`remez1-rel`、`remez2-rel`、`lp-rel` 与 Sollya 脚本。其中 `remez1` 为第一算法风格交换实现（绝对误差），`remez1-rel/remez2-rel/lp-rel` 以相对误差为目标。当前 `common.hpp` 采用 `remez1-rel`，最终选择以真实链路相对误差与板卡测试为准；说明见 `doc-rvv/common/remez-coeffs.zh.md`。对应 Makefile 目标为 `parms_expf`。

参数获取方式如下：

```bash
cd test-rvv/common/common
python3 -m venv .venv
source .venv/bin/activate
pip install numpy
make parms_expf
```

### 2.2 范围约化

实现先将输入夹取到 \([-88,88]\)，避免上溢/下溢；随后计算：

- \(n \approx x/\ln 2\)（实现用 `kExpfLog2Inv = 1/ln2` 乘法得到 `flt_n`）
- `n` 由 `vfcvt_x_f` 转为整数，并再转回 `float` 得到 `flt_n`（用于后续 FMA）
- \(r = x - n\cdot \ln 2\)，其中 `ln2` 被拆成 `kExpfLog2Hi` 与 `kExpfLog2Lo` 两段，减少相减时的舍入损失

对应代码如下：

```cpp
  const float kExpfLog2Inv  = 1.4426950408889634f;     // 1 / ln(2)
  const float kExpfLog2Hi   = 0.6931471824645996f;     // ln(2) 高精度部分
  const float kExpfLog2Lo   = -1.904654290582768e-09f; // ln(2) 低精度部分（补偿）
  const float kExpfXMax     = 88.0f;
  const float kExpfXMin     = -88.0f;
  // ...
  vfloat32m2_t vx = __riscv_vfmin_vf_f32m2 (__riscv_vfmax_vf_f32m2 (x, kExpfXMin, vl), kExpfXMax, vl);
  vfloat32m2_t flt_n = __riscv_vfmul_vf_f32m2 (vx, kExpfLog2Inv, vl);
  vint32m2_t n = __riscv_vfcvt_x_f_v_i32m2 (flt_n, vl);
  flt_n = __riscv_vfcvt_f_x_v_f32m2 (n, vl);
  vfloat32m2_t r = __riscv_vfnmsub_vf_f32m2 (flt_n, kExpfLog2Hi, vx, vl);
  r = __riscv_vfnmsub_vf_f32m2 (flt_n, kExpfLog2Lo, r, vl);
```

### 2.3 多项式逼近

在约化后的 \(r\) 上，使用 7 次 `remez1-rel` 多项式逼近 \(e^r\)。实现采用 Horner 形式，依次执行 `poly = c_k + r*poly`，最后得到 `exp_r = c0 + r*poly`。系数在 `common.hpp` 里以常量形式给出。

```cpp
  const float kExpfRemezC0  = 0.9999999999876557f;
  const float kExpfRemezC1  = 1.000000000027863f;
  const float kExpfRemezC2  = 0.5000000053614374f;
  const float kExpfRemezC3  = 0.16666666439294f;
  const float kExpfRemezC4  = 0.04166635362288752f;
  const float kExpfRemezC5  = 0.008333359419394903f;
  const float kExpfRemezC6  = 0.001394106053653905f;
  const float kExpfRemezC7  = 0.0001986611354469939f;
  // Horner
  vfloat32m2_t poly = __riscv_vfmv_v_f_f32m2 (kExpfRemezC7, vl);
  poly = __riscv_vfmacc_vv_f32m2 (__riscv_vfmv_v_f_f32m2 (kExpfRemezC6, vl), r, poly, vl);
  poly = __riscv_vfmacc_vv_f32m2 (__riscv_vfmv_v_f_f32m2 (kExpfRemezC5, vl), r, poly, vl);
  poly = __riscv_vfmacc_vv_f32m2 (__riscv_vfmv_v_f_f32m2 (kExpfRemezC4, vl), r, poly, vl);
  poly = __riscv_vfmacc_vv_f32m2 (__riscv_vfmv_v_f_f32m2 (kExpfRemezC3, vl), r, poly, vl);
  poly = __riscv_vfmacc_vv_f32m2 (__riscv_vfmv_v_f_f32m2 (kExpfRemezC2, vl), r, poly, vl);
  poly = __riscv_vfmacc_vv_f32m2 (__riscv_vfmv_v_f_f32m2 (kExpfRemezC1, vl), r, poly, vl);
  vfloat32m2_t exp_r = __riscv_vfmacc_vv_f32m2 (__riscv_vfmv_v_f_f32m2 (kExpfRemezC0, vl), r, poly, vl);
```

### 2.4 重构：构造 \(2^n\) 并相乘

本实现按 IEEE754 `float` 的指数位构造 \(2^n\)：`exp_offset = n + 127`，夹取到 \([0,255]\)，再左移 23 位并 `vreinterpret` 为 `float`。对 `n == -127` 单独合并一个非规格化常量 `kExpfTwoToMinus127`，保持 `2^-127` 的数值形态。

最后返回 `exp_r * two_n`。

```cpp
  vint32m2_t exp_offset = __riscv_vadd_vx_i32m2 (n, 127, vl);
  exp_offset = __riscv_vmax_vx_i32m2 (exp_offset, 0, vl);
  exp_offset = __riscv_vmin_vx_i32m2 (exp_offset, 255, vl);
  vuint32m2_t res_bits =
      __riscv_vsll_vx_u32m2 (__riscv_vreinterpret_v_i32m2_u32m2 (exp_offset), 23, vl);
  vfloat32m2_t two_n_normal = __riscv_vreinterpret_v_u32m2_f32m2 (res_bits);
  const vbool16_t is_n_neg127 = __riscv_vmseq_vx_i32m2_b16 (n, -127, vl);
  vfloat32m2_t two_n_sub = __riscv_vfmv_v_f_f32m2 (kExpfTwoToMinus127, vl);
  vfloat32m2_t two_n = __riscv_vmerge_vvm_f32m2 (two_n_normal, two_n_sub, is_n_neg127, vl);
  return __riscv_vfmul_vv_f32m2 (exp_r, two_n, vl);
```

---

## 3. 测试与结果

### 3.1 expf_test：与 std::expf 对比

板卡侧 `run_expf_test` 对 `n=10000` 的专项输入网格比较最大相对误差，并进行 100 次迭代计时。当前 `common.hpp` 的 `remez1-rel` 系数与测试中的 `remez1-rel` 候选重合，RVV 与标量对照在该网格上逐项一致：

```text
=== expf approximation vs std::expf (n = 10000) ===
  (1) 标量 remez1-rel（同 common.hpp）:
    max relative error:  2.234380e-07
    mean absolute error: 6.639316e+27
  (7) pcl::expf_RVV_f32m2（remez1-rel，同(1)）:
    max relative error:  2.234380e-07
    mean absolute error: 6.639316e+27
  [pcl::expf_RVV vs 标量 (1)] max |diff| : 0.000000e+00

=== Performance (n = 10000, 100 iters) ===
  pcl::expf_RVV_f32m2（remez1-rel）: 4.5 ms 量级（约 9.2x-9.3x vs std）
  RVV remez1-rel 候选复核:          4.342 ms 量级（约 9.7x vs std）
```

在该 `run_expf_test` 网格口径下，相对旧 baseline（`max rel = 1.517213e-06`、板卡耗时 `4.500 ms`、约 `9.39x vs std`），`remez1-rel` 将最大相对误差降低到 `2.234380e-07`，板卡耗时保持在同一量级；选择阶段的同系数候选路径实测为 `4.342 ms`、约 `9.73x vs std`。`parms_expf.py` 另有 dense chain f32 仿真指标（200k x in [-88,88]），用于脚本侧复核整条约化/重构链路，口径不同于该专项网格。`Scalar Remez/LP` 在该测试口径下慢于 `std::expf`，收益主要来自 RVV 向量化、FMA 与 \(2^n\) 位构造，而不是标量多项式链路。

### 3.2 expf_remez_vs_taylor：Remez 与 Taylor 对比

`expf_remez_vs_taylor.cpp` 是旧 Remez 与 Taylor 的路径对比专项，用于说明 Remez 多项式与 RVV \(2^n\) 重构方式的差异；它不代表当前 `common.hpp` 采用的 `remez1-rel` 系数。历史板卡结果如下：

```text
=== expf approximation vs std::expf (n = 10000) ===
  Scalar Remez (degree 7):
    max relative error:  1.517213e-06
  Scalar Taylor (degree 7):
    max relative error:  2.949001e-01
  RVV Remez (table lookup 2^n):
    max relative error:  1.517213e-06
  RVV Remez (construct 2^n):
    max relative error:  1.517213e-06
  RVV Taylor:
    max relative error:  2.949001e-01
  // ...
=== Performance (n = 10000, 100 iters) ===
  std::expf:          42.735 ms
  Scalar Remez:     130.796 ms  (speedup: 0.33x vs std)
  RVV Remez [table 2^n]:         4.720 ms  (speedup: 9.05x vs std, 27.71x vs scalar)
  RVV Remez [construct 2^n]:     4.061 ms  (speedup: 10.52x vs std, 32.21x vs scalar)
```

---

## 4. 适用范围与限制

该实现以单精度为目标，偏向吞吐优先的数值内核：它对输入做 \([-88,88]\) 夹取，且不覆盖 `std::expf` 对 NaN/Inf 的完整逐项语义（测试程序的标量路径会显式处理 NaN/Inf，但 `common.hpp` 的 RVV 版本以夹取后的有限值为主要目标）。在需要严格复现标准库特殊值传播规则的场景，应继续使用 `std::expf`。
