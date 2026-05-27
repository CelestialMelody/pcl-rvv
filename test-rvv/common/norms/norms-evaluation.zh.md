# `common/include/pcl/common/impl/norms.hpp`：函数级梳理、筛选评估与 RVV 优先级

本文档是 `impl/norms.hpp` 在 `test-rvv/common/norms` 下的唯一规范说明，对应工作流程中的「函数级筛选与评估」；算法语义与筛选结论为主，具体 intrinsic、编译门控与长度阈值见源码与构建日志。

---

## 1. 函数（族）梳理

文件在 `namespace pcl` 内实现 `norms.h` 中声明的模板，无额外重载分支：所有范数均为「两路下标访问 `a[i]`、`b[i]` + 维数 `dim`」的逐维规约或逐元更新。

| 函数 / 族 | 功能概要 | 复杂度与访存 | 备注 |
| --------- | -------- | ------------ | ---- |
| `selectNorm` | 按 `NormType` 分派到各 `*_Norm` | O(1) 分派 + 所选范数的代价 | 自身无热点循环 |
| `L1_Norm` | Σ \|a[i]−b[i]\| | O(dim)，双指针（或 `[]`）步进 | 本仓库 NARF 描述子距离 |
| `L2_Norm_SQR` | Σ (a[i]−b[i])² | 同上 | `L2_Norm` 依赖 |
| `L2_Norm` | sqrt(L2_Norm_SQR) | O(dim) + 一次 sqrt | |
| `Linf_Norm` | max \|a[i]−b[i]\| | 同上 | 归约为 max |
| `JM_Norm` | sqrt(Σ (√a[i]−√b[i])²) | O(dim)，每元 sqrt | 定义域依赖非负 |
| `B_Norm` | −log(Σ√(a[i]b[i])) 或 0 | O(dim) + log | `norm≤0` 特判 |
| `Sublinear_Norm` | Σ sqrt(\|a[i]−b[i]\|) | O(dim) | |
| `CS_Norm` | Σ (a−b)²/(a+b)（分母 0 跳过） | O(dim)，分支 | |
| `Div_Norm` | Σ (a−b)log(a/b)（条件分支） | O(dim)，log | RVV：`vfdiv` + `logf_RVV_f32m2` + 掩码累加 |
| `PF_Norm` / `K_Norm` | 带标量参数 P1、P2 的 L2² / L1 形 | O(dim) | 不经 `selectNorm` |
| `KL_Norm` | KL 散度形累加 | O(dim)，log 与分支 | 同 Div：`vfdiv` + `logf_RVV_f32m2`；条件为 `b≠0` 且 `a/b>0` |
| `HIK_Norm` | Σ min(a[i],b[i]) | O(dim) | |

仓库内调用关系：`Narf::getDescriptorDistance` 使用 `L1_Norm`（`float*`）；`MultiscaleFeaturePersistence::distanceBetweenFeatures` 使用 `selectNorm<std::vector<float>>`（默认 `L1`，可配置其它 `NormType`），二者均为连续 float 缓冲。

---

## 2. 函数级筛选：评估标准说明

| 维度 | 含义 | 与向量化 / RVV 的关系 |
| ---- | ---- | ---------------------- |
| 循环规模 | 主循环长度为 `dim` | `dim` 小（如 3～数十）时启动成本高；描述子或直方图可较大 |
| 算术密度 | 每元 abs/sub/mul、或 sqrt/log | L1 / L2² / L∞ 算术规整；含 log、除法、分支者密度与实现成本升高 |
| 访存规整性 | `[]` 是否对应连续 `float` | 模板不保证；**仅对已知连续类型**可做条带 `vle` |
| 分支复杂度 | 逐元 if、特判 | CS 等用 mask + `vmerge`；Div / KL 在向量块后用标量 `log` 保持 `libm` 语义 |
| 数值语义风险 | 累加 / max 顺序、非结合律 | 向量累加顺序与标量可能略有浮点差；需容差验收 |
| 可测试性 | 固定 `dim`、随机或确定性向量 | 易与双精度参考或标量参考对拍 |

---

## 3. 优先级与向量化候选总表

| 优先级 | 状态 | 函数 / 族 | 优化方向（语义层） | 主要风险 | 预期收益 | 回退条件 |
| ------ | ---- | --------- | ------------------- | -------- | -------- | -------- |
| 高 | 已完成 | `L1_Norm`、`L2_Norm_SQR`、`Linf_Norm`（及 `L2_Norm`） | 连续 float 条带 + 规约；`dim` 过小时标量 | 浮点结合顺序差异 | `dim` 大或 hot path 反复调用时中–高 | bench 无收益或 QEMU/板卡异常则保留标量 |
| 高 | 已完成 | `JM_Norm`、`B_Norm`、`Sublinear_Norm`、`CS_Norm`、`HIK_Norm` | 与 L 族相同模式：`*_Std` / `*_RVV` / `if constexpr` 分发 | JM/B 依赖 sqrt 定义域；CS 除零用 mask | 视 `dim` 与 ALU | 同左 |
| 中 | 已完成 | `PF_Norm`、`K_Norm` | 连续 float 上缩放差分 + 规约 | 同 L2² / L1 | 随 P1/P2 调用场景 | — |
| 中 | 已完成 | `Div_Norm`、`KL_Norm` | `vle` + `vfdiv` + `pcl::logf_RVV_f32m2` + 掩码累加（`vfadd` `_mu`）；无中间 buffer | `logf_RVV` 与 `std::log` 存在逼近误差；无效 lane 仍可能参与 `log` 计算 | 主路径全在向量寄存器内 | 若 `b_i` 大量为 0 可考虑掩码版 `log`（需 API） |
| 中 | 已完成 | `selectNorm` | 无独立循环；随被调范数自动受益 | — | 随上列 | — |

**实现约定（与源码一致）**

- `namespace pcl::detail`：仅 `kNormRvvContiguousFloatV`、`norm_contiguous_float_data`。
- 各范数族：`////` 分段，顺序为 **标量 `*_Norm_Std` → `*_Norm_RVV`（仅 `__RVV10__`）→ 对外模板**；`selectNorm` 置于文件后部、所有范数之后。
- `kNormRvvMinDim`：低于该长度走 `*_Norm_Std`，与 L 族一致。

---

## 4. 与后续流程的衔接

1. Baseline：在 `test-rvv/common/norms` 下 `make run_bench_std`，将输出记入 `output/qemu/run_bench_std.log` 等。`bench_norms` 覆盖 **L1 / L2² / L2 / L∞ / JM / B / Sublinear / CS / Div / KL / HIK / PF / K** 及 **`selectNorm`（各 `NormType`，不含 PF/K）**；计时行带 **`dim=` 唯一名称**，避免 loose 解析时 `dict` 覆盖同名项；单位为 **us/iter**。`analyze_bench_compare.py` 会按首条计时行识别 **us/ms**，表格列显示为 **Avg (us) / Total (us)**（或仍为 ms 的旧日志）。
2. 编译器自动向量化分析：`make compile_norms_probe` 仅编译 `norms_vec_probe.cpp` 为 `.o`，在 `log/vec_missed_log/` 下生成带 `norms.hpp` 行号的 `fopt-info-vec-missed` 摘录；`make generate_vec_report` 可汇总。
3. RVV 实现：在 `impl/norms.hpp` 对「连续 `float`」类型做窄分发；小 `dim` 标量回退；保持非连续/generic `FloatVectorT` 仍为原标量循环。
4. 验证：`make run_test_std` / `run_test_rvv`（QEMU）→ 板卡 `run_bench_rvv` 与 baseline 对比 speedup；倒退则查实现或关闭 RVV 路径。

### 4.1 静态向量化的样例结论

在仅编译 `norms_vec_probe.cpp` 的日志中，除 `norms.hpp` 内层循环外，常见条目还包括：`std::vector` 分配/析构与异常路径、控制流分割、not profitable 等；`selectNorm` 经 `std::vector` 传参时还与 throw 语义 相关。说明：不能指望编译器对模板 + 容器边界的范数循环自动完成可靠 SIMD 展开，与手工 RVV 条带互补。

### 4.2 数值验收说明

有序浮点求和在向量规约与从左到右标量累加之间可能存在 \(O(\mathrm{dim}\cdot\varepsilon)\) 量级差异。单测对 `__RVV10__` 构建在 **JM / B / Sublinear / CS / HIK / PF / K** 上使用随 `dim` 放宽的绝对容差（见 `test_norms.cpp` 中 `tolRvvVsNaive`）；**Div / KL** 在正数数据上使用较紧容差 `tolRvvLogNorms`；RVV 路径为 `logf_RVV_f32m2`，与朴素 `std::log` 参考在ulp 级可能略有差异。标量构建则与朴素 float 参考对齐。**JM / B** 等单测使用 **非负随机向量**，避免 `sqrt` 定义域问题；**Div / KL** 使用 **严格正** 随机向量。

---

## 5. 代码与测试索引

| 项目 | 路径 |
| ---- | ---- |
| 实现 | `common/include/pcl/common/impl/norms.hpp` |
| 声明 | `common/include/pcl/common/norms.h` |
| 评估本文档 | `test-rvv/common/norms/norms-evaluation.zh.md` |
| Bench | `test-rvv/common/norms/bench_norms.cpp` |
| 单测 | `test-rvv/common/norms/test_norms.cpp` |
| Std vs RVV 同进程对拍 | `test-rvv/common/norms/test_norms_std_vs_rvv_compare.cpp`（`make run_test_std_vs_rvv_compare`） |
| Vec 探针 TU | `test-rvv/common/norms/norms_vec_probe.cpp` |
| 板卡运行 | `test-rvv/common/norms/board.mk`（`run_bench_compare` / `run_test` / `run_test_std_vs_rvv_compare`）；开发机先 `make deploy_*` |
