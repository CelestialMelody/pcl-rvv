# logf_RVV_f32m2 实现说明

本文描述 `pcl::logf_RVV_f32m2` 的算法与 RVV 实现细节。该函数面向批量正浮点输入，返回自然对数 `log(x)` 的 float 逼近；`+0` 返回 `-inf`，`x<0` 为 qNaN，`+inf` 为 `+inf`，输入 NaN 原样返回，行为与 `std::logf` 的常见情形一致。

---

## 1. 接口与语义

接口声明位于 `common/include/pcl/common/common.h`，实现位于 `common/include/pcl/common/impl/common.hpp`，仅在 `__RVV10__` 下可用。`vl` 由调用方通过 `vsetvl_e32m2` 设定。

```cpp
inline vfloat32m2_t
logf_RVV_f32m2(const vfloat32m2_t& x, const std::size_t vl);
```

`Div_Norm` / `KL_Norm` 的 RVV 路径在 `common/include/pcl/common/impl/norms.hpp` 中调用本函数，以替代「向量除法 + 标量 `std::log`」。

---

## 2. 数学路径与实现映射（尾数 约化 → log(1+u) 多项式 → 指数项）

对有限正数 \(x\)，有 \(x = m \cdot 2^E\)，其中 \(m \in [1,2)\) 为规格化尾数。令 \(u = m-1 \in [0,1)\)，则：

\[
\log(x) = E \ln 2 + \log(m) = E \ln 2 + \log(1+u)
\]

### 2.1 次正规数与 `kLogfSubnormScale`（`0x1p+24f` = \(2^{24}\)）

对规格化正数，可直接用「指数域 + 带隐式前导 1 的尾数」得到 \(m \in [1,2)\)。次正规数的指数为偏置 0 且显式尾数非 0，没有隐式前导 1，不能沿用同一套位组合公式。

本实现用一次乘法把正次正规数抬成正规数：

\[
x' = x \cdot 2^{24}
\]

常量 `kLogfSubnormScale`（`16777216.f`，即 C 中 `0x1p+24f`，等于 \(2^{24}\)）的选取原因：

- 单精度最小正次正规数约为 \(2^{-149}\)。乘以 \(2^{24}\) 后约为 \(2^{-125}\)；而单精度最小正规化数约为 \(2^{-126}\)，故 \(2^{-125}\) 已处于正规数范围，一次乘法即可让任意正次正规数变成「可用指数域与隐式 1 解析」的正规数，无需循环移位。
- 在 \(\log x = \log x' - 24\ln 2\) 中，对 \(x'\) 读出的无偏指数比真实 \(E\) 多 24，因此在实现里对指数再减 24，与乘 \(2^{24}\) 相抵。

若用小于 \(2^{24}\) 的 2 的幂，某些极小的次正规数仍可能一次乘后仍为次正规；用 \(2^{24}\) 是常见、且一次完成的折中。随后对 \(x'\) 用与正规数相同的路径解出 \(m\)、\(u\)，并加上与 \(-24\) 对应的指数修正。

### 2.2 参数与脚本

- \(\ln(2)\) 分解：与 `expf` 中 `kExpfLog2Hi` / `kExpfLog2Lo` 相同，在实现里记为 `kLogfLog2Hi` / `kLogfLog2Lo`，用 `e * hi + e * lo` 形式减少 `E * ln(2)` 的舍入损失（`e` 为 32 位有符号指数）。

- \(\log(1+u)\) 多项式系数：在区间 \([0,1]\) 上逼近 \(\log(1+u)\)。脚本为 `test-rvv/rvv/math/logf/script/parms_log1p.py`（名称与标准库 `log1p(x)=log(1+x)` 一致；`1p` 即 one plus）。当前 `common.hpp` 中 `kLogfLog1pC0..C7` 是历史 LP-derived baseline；`parms_log1p.py` 当前 report 中的 `(1) baseline/current` 与 `(4) lp` 是不同候选，不应视为同一组系数。总述与 exp/atan/acos 脚本关系见 [`remez-coeffs.zh.md`](remez-coeffs.zh.md)。再经 `float` Horner 与 `e*ln(2)` 项相加，与 `std::logf` 比较时，典型正数抽样上的最大相对误差仍受 `expf` 造点与双重舍入影响（见下节测试）。

```bash
cd test-rvv/rvv/math
make parms_log1p
```

### 2.3 RVV 位操作

- 由 `vreinterpret` 得到 `uint32` 位型，对指数域、尾数域做 `vsrl` / `vand` / `vor`（隐式前导 1.0 对应 `0x3f800000`）。
- 指数 `E` 以 `vint32m2_t` 参与 `e * ln2`；`e` 经 `vfcvt` 为 `float` 后与常数 FMA（`vfmacc_vf`）。
- \(\log(1+u)\) 为 Horner 链（`vfmacc_vv`），与 `expf` 风格一致。

### 2.4 特判合并

在主体计算后，用 `vmerge` 对 `+0`、负、`+inf`、NaN 进行通道合并，不依赖对非法通道「主体结果」的可用性；主体入口对「有效正有限」通道使用真实 `x`，否则先写入 `1.0f` 以得到确定占位值，再被各特判覆盖。

---

## 3. 测试与结果

### 3.1 logf_test：与 `std::logf` 对比

程序：`test-rvv/rvv/math/logf/logf_test.cpp`，`make -C test-rvv/rvv/math run_logf_test`。在 `log` 域 \([-20, 20]\) 上取 `t`，令 `x = expf(t)`，以 `ref = std::logf(x)` 为参考。标量路径使用与 `common.hpp` 相同常量的 Remez+约化，RVV 路径直接调用 `pcl::logf_RVV_f32m2`。

### 3.2 板卡上运行

开发机构建并部署（与 `expf_test` 相同，需能 `rsync` 到板卡且板卡上 `pcl` 等库在 `REMOTE_LIB_DIR` 可查）：

```bash
cd test-rvv/rvv/math
make deploy_logf_test
```

将 `board.mk` 同步到板卡与 `logf_test` 同目录，在板卡上：

```bash
make run_logf_test
```

日志写到 `board.mk` 中的 `$(REMOTE_OUTPUT_DIR)/run_logf_test.log`（默认与远程测试目录下 `output/run_logf_test.log` 对齐）。

### 3.3 当前复核结论（QEMU / 板卡）

本轮只读复核中，QEMU 与板卡 `run_logf_test` 的误差表一致；板卡性能以实机为准。`n=10000` 专项网格上的关键结果如下：

```text
  (1) current/common.hpp baseline:
    max relative error:  6.739247e-05
    mean absolute error: 1.585479e-07
  (2) remez1:
    max relative error:  4.477345e-05
    mean absolute error: 2.058984e-07
  (4) lp:
    max relative error:  5.738253e-05
    mean absolute error: 1.610150e-07
  (6) pcl::logf_RVV_f32m2:
    max relative error:  6.739247e-05
    mean absolute error: 1.585479e-07
    board time:          9.031 ms, about 3.99x vs std
  RVV vs scalar max diff: 0.000000e+00
```

`remez1` 可降低该网格上的 max relative error，但 mean absolute error 变大；当前 `lp` 候选也不是当前 common.hpp baseline，且 mean absolute error 略高。`Div_Norm` / `KL_Norm` 通过 norms 下游 QEMU 与板卡 `run_test_rvv`、`run_test_std_vs_rvv_compare` 复核。综合收益与下游容差风险，本轮暂不替换 logf 系数，继续保留 current/common.hpp baseline。

---

## 4. 适用范围与限制

与 `expf`/`atan2` 文档相同：以吞吐与实现简洁为主，不保证与 `libm` 在所有 NaN/Inf/域错误上的位级行为完全一致；需严格复现时仍应使用 `std::logf`。

---

## 5. 参考

- 指数与尾数分解：常见 `libm` /fdlibm 类实现。
- Remez 脚本：仿照同目录下 `parms_expf.py` 的交换算法，目标函数为 `log1p(u)`。
