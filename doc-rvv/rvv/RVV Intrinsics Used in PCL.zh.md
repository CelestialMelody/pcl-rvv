# PCL RVV Intrinsics 使用统计

## 统计范围

本文统计当前 PCL RVV 优化源码中直接调用的 RISC-V Vector intrinsic，函数名均以 `__riscv_` 开头。

统计命令口径：

```text
git ls-files '*.h' '*.hpp' '*.cpp'
```

再从上述文件中提取 `__riscv_[A-Za-z0-9_]+`。统计不包含 `doc-rvv` 文档里的示例代码，避免把说明片段重复计入。`#include <riscv_vector.h>` 中出现的 `__riscv_vector` 也不计入。

当前唯一 intrinsic 数量：`112`。

主要覆盖位置：

- `common/include/pcl/common/impl/centroid.hpp`
- `common/include/pcl/common/impl/transforms.hpp`
- `common/include/pcl/impl/rvv_point_load.hpp`
- `common/include/pcl/impl/rvv_point_store.hpp`
- `filters/include/pcl/filters/impl/*.hpp`
- `sample_consensus/include/pcl/sample_consensus/impl/*.hpp`
- `test-rvv/**/bench_*.cpp`、`test-rvv/**/test_*.cpp`

## 命名约定速查

RVV intrinsic 名称通常可以拆成：

```text
__riscv_<operation>_<operand-shape>_<type/lmul>[_mask/tail]
```

常见片段：

| 片段 | 含义 | PCL 中的常见作用 |
| --- | --- | --- |
| `e32` | 32-bit element | `float`、`int32_t`、`uint32_t` 点字段 / index |
| `m1` / `m2` / `m4` | LMUL 寄存器组大小 | PCL float 主路径多用 `f32m2`，double 输出转换用 `f64m4` |
| `f32` / `i32` / `u32` / `f64` | 元素类型 | float 点字段、int indices、uint byte offset、double distances |
| `vv` | vector-vector | 两个向量逐 lane 运算 |
| `vf` | vector-float scalar | 向量与 float 标量运算 |
| `vx` | vector-integer scalar | 向量与整数标量运算 |
| `_m` | masked 版本 | 只让 mask 为 true 的 lane 参与 |
| `_tu` | tail undisturbed | 尾部未激活 lane 保持旧值，常用于跨 chunk 累加器 |
| `_mu` | mask undisturbed | mask 为 false 的 lane 保持旧值 |
| `seg` / `sseg` / `uxseg` | segment / strided segment / indexed segment | AoS 点字段成组 load/store |
| `luxei32` / `suxei32` | unordered indexed load/store by 32-bit byte offset | indexed 点云 gather/scatter |

## VL 控制

| intrinsic | 功能 | 常见作用 |
| --- | --- | --- |
| `__riscv_vsetvl_e32m2` | 根据剩余元素数设置当前 VL，元素宽度 32bit，LMUL=m2 | strip-mining 主循环，每轮处理 `vl` 个点 / index |
| `__riscv_vsetvlmax_e32m2` | 获取当前配置下 e32m2 的最大 VL | 初始化跨 chunk 累加器，最后一次性归约 |

## Load

| intrinsic | 功能 | 常见作用 |
| --- | --- | --- |
| `__riscv_vle32_v_f32m2` | 连续加载 `float` | SoA 或 packed float buffer |
| `__riscv_vle32_v_i32m2` | 连续加载 `int32_t` | `indices`、voxel id、压缩输出源 index |
| `__riscv_vle32_v_u32m2` | 连续加载 `uint32_t` | 无符号 index / offset |
| `__riscv_vlse32_v_f32m2` | 按 byte stride 加载 `float` | AoS 点云中读取 `x/y/z/normal` 字段 |
| `__riscv_vluxei32_v_f32m2` | 用 32-bit byte offset indexed gather `float` | indexed 点云、PCLPointCloud2 字段 gather |
| `__riscv_vlseg3e32_v_f32m2x3` | 连续 segment load 3 个 float 通道 | packed `xyzxyz...` 数据 |
| `__riscv_vlsseg3e32_v_f32m2x3` | stride segment load 3 个 float 通道 | AoS 中一次取 `x/y/z` |
| `__riscv_vlsseg4e32_v_f32m2x4` | stride segment load 4 个 float 通道 | AoS 中一次取 `x/y/z/intensity` 或 4 field |
| `__riscv_vluxseg3ei32_v_f32m2x3` | indexed segment gather 3 个 float 通道 | indexed `x/y/z` gather，减少三次单字段 gather 组织成本 |

## Store

| intrinsic | 功能 | 常见作用 |
| --- | --- | --- |
| `__riscv_vse32_v_f32m2` | 连续存储 `float` | SoA / packed 输出 buffer |
| `__riscv_vse32_v_i32m2` | 连续存储 `int32_t` | 输出 indices、voxel id |
| `__riscv_vse32_v_u32m2` | 连续存储 `uint32_t` | 输出无符号 index |
| `__riscv_vse64_v_f64m4` | 连续存储 `double` | sample_consensus distance 输出 |
| `__riscv_vsse32_v_f32m2` | 按 byte stride 存储 `float` | AoS 点云写回 `x/y/z/normal` 字段 |
| `__riscv_vsse32_v_f32m2_m` | masked stride store `float` | 只替换 invalid `z` 等条件写回 |
| `__riscv_vsseg3e32_v_f32m2x3` | 连续 segment store 3 个 float 通道 | packed `xyz` 输出 |
| `__riscv_vsseg4e32_v_f32m2x4` | 连续 segment store 4 个 float 通道 | packed 4-field 输出 |
| `__riscv_vssseg3e32_v_f32m2x3` | stride segment store 3 个 float 通道 | AoS 点云写回 `x/y/z` |
| `__riscv_vssseg4e32_v_f32m2x4` | stride segment store 4 个 float 通道 | AoS 点云写回 4 个 float 字段 |
| `__riscv_vsuxei32_v_f32m2` | indexed scatter store `float` | 非连续输出位置写回单字段 |
| `__riscv_vsuxseg3ei32_v_f32m2x3` | indexed segment scatter 3 个 float 通道 | indexed 写回 `x/y/z` |
| `__riscv_vsuxseg4ei32_v_f32m2x4` | indexed segment scatter 4 个 float 通道 | indexed 写回 4-field 点类型 |

## Tuple / Segment 辅助

| intrinsic | 功能 | 常见作用 |
| --- | --- | --- |
| `__riscv_vundefined_f32m2x3` | 创建未定义 3-tuple 初值 | 构造 segment store tuple 前的占位 |
| `__riscv_vundefined_f32m2x4` | 创建未定义 4-tuple 初值 | 构造 4-field segment store tuple |
| `__riscv_vset_v_f32m2_f32m2x3` | 向 3-tuple 指定槽位写入一个 vector | 组装 `x/y/z` segment store 输入 |
| `__riscv_vset_v_f32m2_f32m2x4` | 向 4-tuple 指定槽位写入一个 vector | 组装 `x/y/z/intensity` 等输出 |
| `__riscv_vget_v_f32m2x3_f32m2` | 从 3-tuple 取一个 vector | segment load 后拆出 `x/y/z` |
| `__riscv_vget_v_f32m2x4_f32m2` | 从 4-tuple 取一个 vector | segment load 后拆出 4 个字段 |

## 标量广播、lane 提取和类型重解释

| intrinsic | 功能 | 常见作用 |
| --- | --- | --- |
| `__riscv_vfmv_v_f_f32m2` | float 标量广播到向量 | 初始化常量、矩阵系数、阈值、累加器 |
| `__riscv_vfmv_s_f_f32m1` | float 标量写入 m1 scalar vector | reduction 初值 |
| `__riscv_vfmv_f_s_f32m1_f32` | 从 m1 scalar vector 取出 float | reduction 结果回标量 |
| `__riscv_vmv_x_s_u32m2_u32` | 从 u32 vector lane 0 取标量 | 诊断 / 首元素提取 |
| `__riscv_vreinterpret_v_f32m2_u32m2` | `float` bit reinterpret 为 `uint32` | sign/bit mask 操作 |
| `__riscv_vreinterpret_v_u32m2_f32m2` | `uint32` bit reinterpret 为 `float` | bit 操作后回 float |
| `__riscv_vreinterpret_v_i32m2_u32m2` | `int32` bit reinterpret 为 `uint32` | index 转 byte offset |
| `__riscv_vreinterpret_v_u32m2_i32m2` | `uint32` bit reinterpret 为 `int32` | offset / index 类型转换 |

## Mask 和条件判断

| intrinsic | 功能 | 常见作用 |
| --- | --- | --- |
| `__riscv_vmfeq_vf_f32m2_b16` | float vector == scalar | 阈值或特殊值判断 |
| `__riscv_vmfeq_vv_f32m2_b16` | float vector == vector | `x == x` 检测非 NaN |
| `__riscv_vmfne_vf_f32m2_b16` | float vector != scalar | 排除特殊值 |
| `__riscv_vmfne_vv_f32m2_b16` | float vector != vector | NaN 检测 |
| `__riscv_vmflt_vf_f32m2_b16` | float vector < scalar | 上界、阈值、inlier 判断 |
| `__riscv_vmflt_vv_f32m2_b16` | float vector < vector | 每 lane 动态阈值比较 |
| `__riscv_vmfle_vf_f32m2_b16` | float vector <= scalar | 闭区间判断 |
| `__riscv_vmfgt_vf_f32m2_b16` | float vector > scalar | 下界判断 |
| `__riscv_vmfge_vf_f32m2_b16` | float vector >= scalar | 闭区间判断 |
| `__riscv_vmseq_vx_i32m2_b16` | int32 vector == scalar | voxel id / sentinel 判断 |
| `__riscv_vmseq_vx_u32m2_b16` | uint32 vector == scalar | bit-field 判断 |
| `__riscv_vmsne_vx_u32m2_b16` | uint32 vector != scalar | bit-field 非零判断 |
| `__riscv_vmand_mm_b16` | mask AND | 组合 finite、range、inside box 等条件 |
| `__riscv_vmnot_m_b16` | mask NOT | negative filter、invalid 替换 |
| `__riscv_vcpop_m_b16` | mask popcount | 统计保留 lane 数、inlier 数、压缩写入长度 |
| `__riscv_vfirst_m_b16` | 找第一个 true lane | 诊断、找到首个命中元素 |

## 压缩和选择

| intrinsic | 功能 | 常见作用 |
| --- | --- | --- |
| `__riscv_vcompress_vm_f32m2` | 按 mask 压缩 float lane | sample_consensus 输出命中 distance |
| `__riscv_vcompress_vm_i32m2` | 按 mask 压缩 int32 lane | 输出保留 indices |
| `__riscv_vcompress_vm_u32m2` | 按 mask 压缩 uint32 lane | 输出无符号 indices |
| `__riscv_vmerge_vvm_f32m2` | 按 mask 在两个 float vector 中选择 | keep/replace、条件输出 |
| `__riscv_vmerge_vvm_i32m2` | 按 mask 在两个 int32 vector 中选择 | 条件 index / voxel id |

常见组合模式：

- [RVV 多谓词掩码收敛模式](RVV%20Mask%20Predicate%20Convergence.zh.md)：`vfabs/vmfle` 或其他比较生成局部 mask，再用 `vmand` / `vmor` 收敛为最终 `keep`。
- [RVV 掩码压缩与保序索引输出模式](RVV%20Compress%20Index%20Output.zh.md)：`vid + vadd + vcompress + vcpop + vse32` 实现保序 indices 输出。
- [RVV 多字段压缩 staging 输出模式](RVV%20Multi-Field%20Compress%20Staging.zh.md)：多个字段共用同一个 `keep` mask，分别 `vcompress` 后经临时 SoA buffer 组装为 AoS staging。

## Float 算术

| intrinsic | 功能 | 常见作用 |
| --- | --- | --- |
| `__riscv_vfabs_v_f32m2` | float 绝对值 | distance、finite mask、阈值比较 |
| `__riscv_vfadd_vf_f32m2` | vector + scalar | 平移、偏置 |
| `__riscv_vfadd_vv_f32m2` | vector + vector | 坐标 / stencil 累加 |
| `__riscv_vfadd_vv_f32m2_tu` | tail-undisturbed vector + vector | 累加器保留尾部 lane |
| `__riscv_vfadd_vv_f32m2_mu` | mask-undisturbed vector + vector | masked 条件加法 |
| `__riscv_vfsub_vf_f32m2` | vector - scalar | 去中心化、坐标平移 |
| `__riscv_vfsub_vv_f32m2` | vector - vector | 点差、方向差 |
| `__riscv_vfrsub_vf_f32m2` | scalar - vector | `1 - curvature` 等反向减法 |
| `__riscv_vfmul_vf_f32m2` | vector * scalar | scale、矩阵系数、leaf inverse size |
| `__riscv_vfmul_vv_f32m2` | vector * vector | 点积、平方距离 |
| `__riscv_vfdiv_vv_f32m2` | vector / vector | 权重归一化、除以计数 |
| `__riscv_vfdiv_vf_f32m2` | vector / scalar | 统一标量分母归一化 |
| `__riscv_vfdiv_vv_f32m2_mu` | mask-undisturbed vector / vector | masked 除法 |
| `__riscv_vfsqrt_v_f32m2` | sqrt | 欧氏距离 |
| `__riscv_vfmin_vf_f32m2` | min(vector, scalar) | clamp 上界 |
| `__riscv_vfmin_vv_f32m2` | min(vector, vector) | per-lane clamp / min |
| `__riscv_vfmin_vv_f32m2_tu` | tail-undisturbed min | 跨 chunk min 累加器 |
| `__riscv_vfmax_vf_f32m2` | max(vector, scalar) | clamp 下界 |
| `__riscv_vfmax_vv_f32m2` | max(vector, vector) | per-lane clamp / max |
| `__riscv_vfmax_vv_f32m2_tu` | tail-undisturbed max | 跨 chunk max 累加器 |
| `__riscv_vfmerge_vfm_f32m2` | 按 mask 在 float vector 和 float scalar 中选择 | 条件替换为标量值 |
| `__riscv_vfsgnj_vv_f32m2` | 拷贝符号 | 符号修正 |
| `__riscv_vfsgnjx_vv_f32m2` | XOR 符号位；同一输入时等价 abs | distance 绝对值 |
| `__riscv_vfslide1down_vf_f32m2` | vector lane 下滑一位并在尾部插入标量 | 邻域 / 滑窗类计算 |

## FMA / 多项式类运算

| intrinsic | 功能 | 常见作用 |
| --- | --- | --- |
| `__riscv_vfmacc_vf_f32m2` | `acc += scalar * vector` | 矩阵变换、stencil、线性组合 |
| `__riscv_vfmacc_vf_f32m2_tu` | tail-undisturbed `acc += scalar * vector` | 跨 chunk 累加器 |
| `__riscv_vfmacc_vv_f32m2` | `acc += vector * vector` | 点积、平方距离、协方差 |
| `__riscv_vfmacc_vv_f32m2_tu` | tail-undisturbed `acc += vector * vector` | 协方差 / moment 跨 chunk 累加 |
| `__riscv_vfmsac_vv_f32m2` | `acc -= vector * vector` | 差分公式、几何表达式 |
| `__riscv_vfnmsub_vf_f32m2` | fused negative multiply-subtract | 特定线性公式重排，减少中间误差和指令数 |

相关语义规则见 [RVV Eigen 表达式语义对齐](RVV%20Eigen%20Expression%20Semantic%20Alignment.zh.md)。手工展开 Eigen 小矩阵、小向量、投影或 `Vector3f::norm()` 时，应先用反汇编确认标量 lowering，再选择 `vfmacc` / `vfmul` / `vfadd` 形态；若结果进入像素、voxel、阈值谓词或保序输出序列，应按 bit / predicate / output 等价处理。

## Reduction

| intrinsic | 功能 | 常见作用 |
| --- | --- | --- |
| `__riscv_vfredosum_vs_f32m2_f32m1` | ordered float sum reduction | 需要较稳定顺序的求和、centroid / covariance |
| `__riscv_vfredusum_vs_f32m2_f32m1` | unordered float sum reduction | 对顺序不敏感的快速求和 |
| `__riscv_vfredmin_vs_f32m2_f32m1` | float min reduction | 点云 min bound |
| `__riscv_vfredmin_vs_f32m2_f32m1_m` | masked float min reduction | finite / range mask 下的 min |
| `__riscv_vfredmax_vs_f32m2_f32m1` | float max reduction | 点云 max bound |
| `__riscv_vfredmax_vs_f32m2_f32m1_m` | masked float max reduction | finite / range mask 下的 max |

## Float / Int 转换

| intrinsic | 功能 | 常见作用 |
| --- | --- | --- |
| `__riscv_vfcvt_f_x_v_f32m2` | int32 -> float32 | index / bin 转 float |
| `__riscv_vfcvt_x_f_v_i32m2` | float32 -> int32，使用当前 FRM | 普通 float-to-int 转换；需关注当前舍入模式 |
| `__riscv_vfcvt_x_f_v_i32m2_rm` | float32 -> int32，显式舍入模式 | `floor` / voxel bin；必须保存恢复 FRM/FCSR 语义 |
| `__riscv_vfwcvt_f_f_v_f64m4` | float32 -> float64 widening convert | sample_consensus double distance 输出 |

## Integer / Bit 运算

| intrinsic | 功能 | 常见作用 |
| --- | --- | --- |
| `__riscv_vid_v_u32m2` | 生成 `[0, 1, 2, ...]` lane id | 构造连续 source index |
| `__riscv_vadd_vx_i32m2` | int32 vector + scalar | voxel / index 偏移 |
| `__riscv_vadd_vx_u32m2` | uint32 vector + scalar | byte offset 加字段偏移 |
| `__riscv_vsub_vx_i32m2` | int32 vector - scalar | voxel bin 减 `min_b` |
| `__riscv_vmul_vx_i32m2` | int32 vector * scalar | voxel linear id |
| `__riscv_vmul_vx_u32m2` | uint32 vector * scalar | index 转 byte offset |
| `__riscv_vmacc_vx_i32m2` | `acc += scalar * int32 vector` | voxel linear id 组合 |
| `__riscv_vmin_vx_i32m2` | min(int32 vector, scalar) | clamp index 上界 |
| `__riscv_vmax_vx_i32m2` | max(int32 vector, scalar) | clamp index 下界 |
| `__riscv_vand_vx_u32m2` | bitwise AND with scalar | mask bit-field |
| `__riscv_vor_vx_u32m2` | bitwise OR with scalar | 设置 bit-field |
| `__riscv_vsll_vx_u32m2` | left shift | packed bit / channel 组合 |
| `__riscv_vsrl_vx_u32m2` | logical right shift | packed bit / channel 拆分 |

## 常见使用模式

### 1. AoS 点字段线性扫描

```text
vsetvl -> vlse32(x/y/z, sizeof(PointT)) -> arithmetic / mask -> vsse32
```

常见于 `PointCloud<PointT>` 的 `x/y/z/normal` 字段扫描。优点是无需改存储布局；风险是 stride load/store 可能成为主成本。

### 2. Indexed 点云 gather

```text
vle32(indices) -> vmul_vx_u32m2(sizeof(PointT)) -> vluxei32(field)
```

常见于 `indices` 子集、`PCLPointCloud2` 字段访问。收益取决于 gather 局部性和后续算术量。

### 3. Mask + compress 输出 indices

```text
compare -> vmand/vmnot -> vcpop -> vcompress -> vse32
```

常见于 filters 的保留 index 生成。`vcpop` 给出本 chunk 写出数量，`vcompress` 把命中 lane 压到前面。

### 3.1 Mask + compress 输出多字段 staging

```text
compare -> vcompress(field0..fieldN) -> vse32(temp SoA buffers) -> scalar pack AoS staging
```

常见于输出 `Candidate{index, target, x, y, z}` 这类多字段候选结构。每个字段必须使用同一个 `keep` mask 和同一个 `vcpop` 结果，避免压缩后字段错位。该模式适合先把 predicate 和主要数学计算 RVV 化，再用短标量循环保持 AoS staging 与后续 scalar tail 的可维护性。

### 3.2 Eigen / FMA / norm 语义对齐

```text
反汇编确认标量 lowering -> 选择 vfmacc/vfmul/vfadd/vfsqrt/vfcvt 形态 -> adversarial bit/predicate 测试
```

常见于 `Matrix4f * getVector4fMap()`、`projection_matrix * Vector3f`、`Vector3f::norm()`、像素投影和距离阈值。该模式的重点不是新增 intrinsic，而是确认 RVV 的舍入点、FMA contraction、`sqrt` 计算域和 float/double 比较谓词与生产标量路径一致。详见 [RVV Eigen 表达式语义对齐](RVV%20Eigen%20Expression%20Semantic%20Alignment.zh.md)。

### 4. Reduction

```text
load -> optional mask -> vfredmin/vfredmax/vfredosum -> scalar extract
```

常见于 min/max bound、centroid、covariance。若使用 masked reduction，需要确保全 false chunk 或全非 finite 输入有显式 fallback / found flag。

### 5. Segment load/store

```text
vlsseg3e32 / vssseg3e32
```

用于把 AoS 中相邻字段作为 tuple 处理，减少手写三次 load/store 的样板代码。是否更快取决于硬件对 segment/stride 的实现。

### 6. Float-to-int / floor

```text
vfcvt_x_f_v_i32m2_rm(..., RDN)
```

用于 voxel bin、leaf id 等 `floor` 语义。显式舍入模式 intrinsic 或任何 FRM/FCSR 修改都必须保证调用后恢复调用者浮点环境，并用“先 RVV 主路径、后 fallback”的同进程顺序测试回归。

## 维护建议

- 新增 intrinsic 后同步更新本文，至少放入对应功能组。
- 新增 masked、tail-undisturbed、segment、gather/scatter 或显式舍入 intrinsic 时，应在主题文档说明语义边界和 fallback 条件。
- QEMU 反汇编可证明路径命中，但性能结论仍以板卡或目标硬件为准。
- 如果 bench-diagnosis microbench 引入实验 intrinsic，应在本文中保留，但在主题文档中明确它不属于生产主路径。
