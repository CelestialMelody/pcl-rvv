# `norms`（`impl/norms.hpp`）：RVV 优化实现说明

本文说明 `pcl/common/include/pcl/common/impl/norms.hpp` 中各类向量范数与 `selectNorm` 在本仓库相对上游 PCL 的 `__RVV10__` 扩展：公开模板 API 不变，在「连续 `float` 缓冲 + 维数不低于阈值」时走 `*Norm_RVV` 助手，否则保留 `*_Norm_Std` 标量循环。含对数的 `Div_Norm` / `KL_Norm` 在 RVV 路径使用 `pcl::logf_RVV_f32m2`（实现见 `common/include/pcl/common/impl/common.hpp`）。

本仓库实现文件：[common/include/pcl/common/impl/norms.hpp](../../common/include/pcl/common/impl/norms.hpp)、[common/include/pcl/common/norms.h](../../common/include/pcl/common/norms.h)；`logf` 逼近与次正规处理见 [common/include/pcl/common/impl/common.hpp](../../common/include/pcl/common/impl/common.hpp)。

上游对照文件：[impl/norms.hpp](https://github.com/PointCloudLibrary/pcl/blob/master/common/include/pcl/common/impl/norms.hpp)、[norms.h](https://github.com/PointCloudLibrary/pcl/blob/master/common/include/pcl/common/norms.h)。

当前工作概述：在 `norms.hpp` 内为 L1、L2²、L∞、L2、JM、B、Sublinear、CS、Div、PF、K、KL、HIK 等增加 `*Norm_RVV`（`#if defined(__RVV10__)`），通过 `detail::kNormRvvContiguousFloatV` 与 `norm_contiguous_float_data` 将 `float*` / `std::vector<float>` 导向 RVV；`dim < kNormRvvMinDim`（当前为 16）时回退标量。`Div_Norm_RVV` / `KL_Norm_RVV` 使用 `logf_RVV_f32m2` 与掩码累加，替代「向量除法 + 标量 `std::log`」。测试侧增加同进程对拍 `test_norms_std_vs_rvv_compare.cpp` 与 `test-rvv/common/norms` 下 bench / 部署 / 板卡流程；板卡与 QEMU 的 `bench_norms_std` / `bench_norms_rvv` 对比见 `analyze_bench_compare.py` 输出。

条带尾段 `_tu` / `_mu` 语义、`vfloat32m2_t` 与 LMUL 选型见各模块 RVV 说明中的统一约定；`logf` / `expf` 专题见 [logf-RVV.zh.md](./logf-RVV.zh.md)、[expf-RVV.zh.md](./expf-RVV.zh.md)。

---

## 1. 背景与需求

上游 `impl/norms.hpp` 中各范数以模板 `*_Norm_Std` 实现，按 `dim` 做标量循环；`norms.h` 声明 `selectNorm` 等。上游该路径不提供 x86 SSE/AVX intrinsic，也不涉及 Eigen 向量化封装。

本仓库在 `__RVV10__` 下的约束如下：

- API 与模板签名不变：对外仍为 `L1_Norm (FloatVectorT a, FloatVectorT b, int dim)` 等形式。
- 语义对齐：数学定义与 `*_Norm_Std` 一致；RVV 路径与标量路径因浮点结合律与 `log` 实现不同，允许在工程容差内偏差（见 §2 数值说明与对拍容差）。
- 数据布局：仅当 `FloatVectorT` 为 `float*` / `const float*` 或 `std::vector<float>`（连续存储）时，`if constexpr` 取裸指针并调用 `*Norm_RVV`；其它容器或逐元访问类型仍走 `*_Norm_Std`，不引入 gather。
- 小规模与负维：`dim <= 0` 时直接返回与标量一致；`n < kNormRvvMinDim` 时回退 `*_Norm_Std`，避免条带与归约固定开销压过算力收益。
- 含分支的标量循环（如 `Div_Norm_Std` 中 `if ((a/b) > 0)`）：RVV 用掩码在向量上合入可贡献项，与标量「跳过无效 lane」等价。

相关横切与专题文档：上文 `logf-RVV.zh.md` / `expf-RVV.zh.md`；`norms` 目录说明见 [test-rvv/common/norms/README.zh.md](../../test-rvv/common/norms/README.zh.md)。

---

## 2. 与上游实现的差异

| 条目 | 上游实现要点 | 本仓库在 **RVV10** 下的变化 |
| --- | --- | --- |
| `L1_Norm` / `L2_Norm_SQR` / `Linf_Norm` / `L2_Norm` | 标量循环 | 连续 `float` 且 `n >= 16`：`L1_Norm_RVV` 等；`Linf` 用 `vfredmax` 等；否则 `*_Norm_Std` |
| `JM_Norm` / `B_Norm` / `Sublinear_Norm` / `CS_Norm` 等 | 标量 + `std::sqrt` / `min` 等 | 同上分流的 `*Norm_RVV`；`B_Norm_RVV` 条带和后用标量 `std::log` 对总和取负对数（与上游公式一致，总和为标量） |
| `Div_Norm` / `KL_Norm` | 标量 + `std::log` | `*Norm_RVV`：`vfdiv` + `logf_RVV_f32m2` + 掩码 + `vfredosum`；`KL` 另用 `b!=0` 与 `ratio>0` 的掩码合入 |
| `PF_Norm` / `K_Norm` / `HIK_Norm` | 标量 | 有对应 `*Norm_RVV`（`HIK` 为 `min` 与 FMA/归约，依实现而定） |
| `selectNorm` | 分类型调用各 `*_Norm` | 不单独实现 RVV；经各范数模板的 `if constexpr` 分发，行为与直调 `L1_Norm` 等一致 |
| 非 `float` 连续存储的 `FloatVectorT` | 原模板标量 | 未 RVV 化，仍 `*_Norm_Std` |

数值一致性说明：

- 与 **有序标量累加** 比较时，条带内 `vfadd` / `vfmacc` 与全向量 `vfredosum` 的**结合顺序**与从左到右的 `float` 累加不同，可产生约 $O(\mathrm{dim} \cdot \epsilon)$ 量级差异；`test_norms_std_vs_rvv_compare.cpp` 中 `tolOrdered(dim) = max(1\mathrm{e}{-5}, 2\mathrm{e}{-6} \cdot \max(\mathrm{dim},1))` 与仓库内 `test_norms` 的 `tolRvvVsNaive` 对齐思想一致。
- `Div` / `KL` 路径：标量用 `std::log`，RVV 用 `logf_RVV_f32m2`（Remez + 约化），与 `libm` 存在 ulp 级差；对拍用 `tolLog(dim) = max(1.2\mathrm{e}{-4}, 5\mathrm{e}{-7} \cdot \max(\mathrm{dim},1))`。
- 逐元比较类范数在**多个下标同时取等**时，标量 if 与掩码**端点**在极少边界输入上是否完全一致：当前材料不足，未下结论；对拍采用 `EXPECT_NEAR` 而非位级相等。

---

## 3. 总体设计

### 3.1 分流条件

- 编译期：`#if defined(__RVV10__)` 下编译出 `*Norm_RVV`；未定义时仅存在 `*_Norm_Std` 与向模板直连。
- 运行期：各 `*Norm_RVV` 在 `dim <= 0` 早退与标量同；`n = (std::size_t)dim` 且 `n < kNormRvvMinDim`（16）时调用 `*_Norm_Std`。
- 模板级：`kNormRvvContiguousFloatV` 为真时以 `norm_contiguous_float_data` 得到 `const float*`，否则标量。

### 3.2 组织方式

- `*Norm_RVV` 与 `*Norm_Std` 同文件、同名空间 `pcl`；`#if __RVV10__` 包住 RVV 助手。
- `Div_Norm` / `KL_Norm` 的 RVV 实现 `#include` `impl/common.hpp` 以使用 `logf_RVV_f32m2`；未再使用早期的 `norm_rvv_ratio_scratch` 线程局部缓冲做逐元 `log`。
- 测试与 bench 见 `test-rvv/common/norms`：两套可执行 `bench_norms_std` / `bench_norms_rvv` 分别链未定义 / 定义 `__RVV10__` 的 `libpcl_common`，与 Gaussian 等模块的对比方式一致。

---

## 4. 详细实现

### 4.1 入口与函数分发

公开 API（如 `L1_Norm`）在 `__RVV10__` 下对 `if constexpr (detail::kNormRvvContiguousFloatV<FloatVectorT>)` 为真时调用 `L1_Norm_RVV (norm_contiguous_float_data (a), norm_contiguous_float_data (b), dim)`，否则 `L1_Norm_Std`。

| 符号 | 回退条件 |
| --- | --- |
| `*Norm_RVV` | 未以 `__RVV10__` 编译时符号不存在，链接的库内仅为标量 |
| 各 `*Norm_RVV` 内部 | `dim <= 0`；`n < kNormRvvMinDim`；否则进入条带循环 |
| `*Norm_Std` | 非连续 `float` 布局；或上述回退；或非 RVV 构建 |

对应实现中，`L1_Norm` 模板分发骨架如下：

```cpp
// common/include/pcl/common/impl/norms.hpp: L1_Norm(...)
#if defined(__RVV10__)
  if constexpr (detail::kNormRvvContiguousFloatV<FloatVectorT>)
    return L1_Norm_RVV (detail::norm_contiguous_float_data (a),
                        detail::norm_contiguous_float_data (b), dim);
  else
    return L1_Norm_Std (a, b, dim);
#else
  return L1_Norm_Std (a, b, dim);
#endif
```

### 4.2 `L1_Norm_RVV`

与标量 `L1_Norm_Std` 相同：求 $\sum_i |a_i - b_i|$。条带以 `__riscv_vsetvl_e32m2 (n - i)` 推进；`a`、`b` 上 `vle32` 连续加载，差、绝对值后用 `__riscv_vfadd_vv_f32m2_tu` **合并到** 累加向量 `v_acc`（同类型条带和在同一向量寄存器上累加，避免每段 `vfred` 一次）；全段结束后再用 `__riscv_vfredosum_vs_f32m2_f32m1` 将 m2 规约到标量。选用 **有序 undisturbed** 合并是为了在固定 `max_vl` 下对整段 $n$ 使用同一归约向量的 `vfredosum` 定义（与实现中 `max_vl = vsetvl_e32m2 (n)` 的写法一致）。

```cpp
// common/include/pcl/common/impl/norms.hpp: L1_Norm_RVV(...)
  const std::size_t max_vl = __riscv_vsetvl_e32m2 (n);
  vfloat32m2_t v_acc = __riscv_vfmv_v_f_f32m2 (0.f, max_vl);
  while (i < n) {
    const std::size_t vl = __riscv_vsetvl_e32m2 (n - i);
    const vfloat32m2_t va = __riscv_vle32_v_f32m2 (a + i, vl);
    const vfloat32m2_t vb = __riscv_vle32_v_f32m2 (b + i, vl);
    const vfloat32m2_t vd = __riscv_vfsub_vv_f32m2 (va, vb, vl);
    const vfloat32m2_t vabs = __riscv_vfabs_v_f32m2 (vd, vl);
    v_acc = __riscv_vfadd_vv_f32m2_tu (v_acc, v_acc, vabs, vl);
    i += vl;
  }
  const vfloat32m1_t v_sum = __riscv_vfredosum_vs_f32m2_f32m1 (v_acc, v_zero, max_vl);
```

### 4.3 `Div_Norm_RVV`

标量定义仅当 $(a_i/b_i) > 0$ 时累加 $(a_i - b_i)\log(a_i/b_i)$。向量化中 `vratio = vfdiv` 后对全部 lane 计算 `logf_RVV_f32m2`；用 `mpos` 对「比值为正」的 lane 将 `vterm` **掩码合入** `v_acc`（`__riscv_vfadd_vv_f32m2_mu`），无效 lane 不污染累加，对应标量 `if` 跳过。全段后再 `vfredosum`。与 `std::log` 的差由 `logf` 逼近与 FMA 顺序共同决定，影响随 `dim` 累积；对拍用 `tolLog(dim)` 约束。

```cpp
// common/include/pcl/common/impl/norms.hpp: Div_Norm_RVV(...)
    const vfloat32m2_t vratio = __riscv_vfdiv_vv_f32m2 (va, vb, vl);
    const vfloat32m2_t vlog = pcl::logf_RVV_f32m2 (vratio, vl);
    const vfloat32m2_t vdiff = __riscv_vfsub_vv_f32m2 (va, vb, vl);
    const vfloat32m2_t vterm = __riscv_vfmul_vv_f32m2 (vdiff, vlog, vl);
    const vbool16_t mpos = __riscv_vmfgt_vf_f32m2_b16 (vratio, 0.0f, vl);
    v_acc = __riscv_vfadd_vv_f32m2_mu (mpos, v_acc, v_acc, vterm, vl);
```

### 4.4 `JM_Norm_RVV` 及其它条带+归约型

`JM_Norm_Std` 在循环内用 `std::sqrt (a[i]) - std::sqrt (b[i])` 后平方。`JM_Norm_RVV` 在条带内用 `__riscv_vfsqrt_v_f32m2` 对两路向量化开方，再 `vfsub`、`vfmacc` 累加平方，条带间仍 `vfredosum`，最外层 `std::sqrt` 对标量路径一致。`B_Norm` 在向量段求和后仍对标量结果做一次 `std::log`，因公式依赖**总和**的标量对数，与上游一致。

若某维数下 RVV 构建错误或未链入含 `__RVV10__` 的 `libpcl_common`，应用侧会始终走标量：是否执行向量指令**仅**由所链共享库的编译选项决定，与可执行自身宏无关（与 [gaussian.zh.md](./gaussian.zh.md) 中说明一致）。

---

## 5. 测试与验证

### 5.1 测试入口与运行方式

- 功能单测（链安装版 `libpcl_common`）：`test-rvv/common/norms/test_norms.cpp`，`make run_test` 等，见该目录 [Makefile](../../test-rvv/common/norms/Makefile)。
- 同进程对拍（`float*` 上 `*_Norm_Std` 与 `*_Norm`（RVV 分发））：`test_norms_std_vs_rvv_compare.cpp`，**固定** `-D__RVV10__` 于独立 `CXXFLAGS_TEST_COMPARE`，见 Makefile 中 `build_test_std_vs_rvv_compare`；运行 `make run_test_std_vs_rvv_compare` 或板卡 [board.mk](../../test-rvv/common/norms/board.mk) 中 `run_test_std_vs_rvv_compare`。
- 性能：`bench_norms.cpp` 分别编译为 `bench_norms_std`（不定义 `__RVV10__`）与 `bench_norms_rvv`（定义 `__RVV10__`），**两套 binary 对比**；`make run_bench_compare` 调用 [analyze_bench_compare.py](../../test-rvv/script/analyze_bench_compare.py) 生成 Std/RVV 表。
- 运行环境：开发机默认可为 QEMU（如 `vlen=256`）；板卡侧见部署目录与 `board.mk` 中 `LD_LIBRARY_PATH` 指向的 `libpcl_common`。

### 5.2 日志与数据来源

- 表 §5.3 的数值取自板卡一次完整 `run_bench_compare` 的汇总，原始与汇总见 [test-rvv/common/norms/output/board/bench_compare.log](../../test-rvv/common/norms/output/board/bench_compare.log)（内嵌 `run_bench_std` / `run_bench_rvv` 与 `analyze_bench_compare` 输出；设备名 Milkv-Jupiter）。
- QEMU 下同类结果在 [test-rvv/common/norms/output/qemu/](../../test-rvv/common/norms/output/qemu/) 目录；因模拟器上 RVV 与标量时序与真机不同，**性能对比以板卡为优先**；QEMU 用于 CI 与逻辑回归时需在文中注明。

### 5.3 测试结果与说明

下列数据来板卡 [bench_compare.log](../../test-rvv/common/norms/output/board/bench_compare.log) 中 `Avg (us/iter)`，表中时间与 bench 一致，单位为 us；**Speedup** 为汇总脚本中 `Std Avg / RVV Avg`（同 log）。选取 $dim \in \{8, 4096\}$ 与多类范数，避免重复罗列全部 benchmark 行。

| Benchmark 项 | Std Avg (us) | RVV Avg (us) | Speedup |
| --- | --- | --- | --- |
| `L1_Norm(float*), dim=8` | 0.0542 | 0.0528 | 1.03 |
| `L1_Norm(float*), dim=4096` | 12.70 | 3.20 | 3.97 |
| `Linf_Norm(float*), dim=4096` | 51.52 | 6.07 | 8.49 |
| `JM_Norm(float*), dim=4096` | 378.7 | 23.58 | 16.06 |
| `Div_Norm(float*), dim=4096` | 161.4 | 129.5 | 1.25 |
| `selectNorm(vec, JM), dim=4096` | 386.63 | 44.27 | 8.73 |

`L2_Norm_SQR(float*), dim=512` 在板卡日志中出现 RVV 慢于 Std（约 0.95×），与单段条带和调度/指令混合有关，不宜外推为所有维数；若需全表对比应以同一次 `run_bench_*.log` 为准。

`Div_Norm` / `KL_Norm` 加速比受 `logf_RVV` 与 `vfdiv` 主导，$dim=4096$ 上约 1.2–1.3×，低于纯代数条带和范数；与实现选择一致，非单独结论性口号。

---

## 6. 总结

`norms` 各范数对外模板签名与 `selectNorm` 行为保持与上游一致；连续 `float` 且 $dim \ge 16$ 时在以 `__RVV10__` 编译的 `libpcl_common` 中走 `vle32` 条带、`vfmacc` / `vfadd` 与 `vfredosum` / `vfredmax` 等，对 `Div` / `KL` 增加 `logf_RVV` 与掩码合入。条带间用**同一** `max_vl` 做最终归约、对条件项使用 `vfadd_mu` 等，属于实现上的访存与舍入组织方式；`B_Norm` 末段仍为标量 `log`。**已知限制**：非连续存储无 RVV；小维回退；`log` 与标量 `libm` 存在有界差；`test_norms_std_vs_rvv_compare` 的容差若随工具链变化需重审。`L2_SQR` 在个别维数上 RVV 不升速的成因当前材料不足，未下结论。
