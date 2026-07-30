# `transforms`（`impl/transforms.hpp`）：RVV 优化实现说明

本文说明 `common/include/pcl/common/impl/transforms.hpp` 中点云变换路径的 `__RVV10__` 扩展：公开 API 不变，在 dense、非 indexed、`Scalar=float` 且点字段布局兼容时走 RVV helper；其它路径保持上游标量或既有平台实现。

本仓库实现文件：[common/include/pcl/common/impl/transforms.hpp](../../common/include/pcl/common/impl/transforms.hpp)、[common/include/pcl/common/transforms.h](../../common/include/pcl/common/transforms.h)。

测试与性能材料：函数级筛选见 [test-rvv/common/transforms/transforms-evaluation.zh.md](../../test-rvv/common/transforms/transforms-evaluation.zh.md)，单测与 bench 见 [test-rvv/common/transforms](../../test-rvv/common/transforms)，板卡汇总见 [test-rvv/common/transforms/output/board/bench_compare.log](../../test-rvv/common/transforms/output/board/bench_compare.log)。

---

## 1. 背景与需求

`transforms.hpp` 的主要热点是按点遍历点云，对每个点执行 4x4 矩阵变换；`transformPointCloudWithNormals` 额外对 normal 执行 3x3 旋转。上游在 x86 上已有 `Transformer<float>` / `Transformer<double>` 的 SSE/AVX 单点特化，但 RISC-V 下原先没有对应的 RVV 批量路径。

本轮 RVV 覆盖范围：

- `transformPointCloud(cloud, cloud_out, Matrix4f, copy_all_fields)`；
- `transformPointCloudWithNormals(cloud, cloud_out, Matrix4f, copy_all_fields)`；
- `cloud_in.is_dense == true`；
- 非 indices 入口；
- `PointT` 为标准布局，且 `x/y/z` 为 `float`；
- normal 路径还要求 `normal_x/normal_y/normal_z` 为 `float`；
- `n >= kTransformRvvMinPoints` 时进入 RVV helper，小点云回退同文件标量路径。

保持原路径的范围：

- `Scalar=double`；
- non-dense 点云；
- indices / `PointIndices` 入口；
- `PointXY` 2D affine 入口；
- 单点 `transformPoint` / `transformPointWithNormal`；
- `getPrincipalTransformation`。

本轮目标不是替换所有 transform 入口，而是在最规整的整云 dense 路径上建立可验证的 RVV 实现，并通过 QEMU、板卡和 x86 SIMD 对照明确性能判断边界。

---

## 2. 与上游实现的差异

| 条目 | 上游 / 原路径 | 本仓库在 `__RVV10__` 下的变化 |
| --- | --- | --- |
| dense 整云 xyz | 每点调用 `Transformer<Scalar>::se3`；x86 上可由 `Transformer<float>` 使用 SSE | `Scalar=float` 且点布局兼容时，按 AoS stride 加载 x/y/z，RVV FMA 后写回 x/y/z |
| dense 整云 xyz + normal | 每点调用 `se3` 与 `so3`；x86 上仍偏单点寄存器处理 | 同一个 RVV 条带内加载 x/y/z 与 normal_x/y/z，分别完成 SE3 与 SO3 后写回，避免两次完整 AoS 扫描 |
| in-place | 单点 helper 先复制输入坐标 | RVV helper 每个条带先完成所有输入 load，再 store，保持 in-place 安全 |
| non-dense | 逐点 `isfinite` 检查 | 保持标量，避免掩码路径扩大实现范围 |
| indices | `indices[i]` 间接读取 | 保持标量，暂不引入 gather |
| double transform | SSE/AVX 或通用标量 | 保持原路径，避免 float RVV 与 double 语义混用 |

x86 SSE/AVX 实现可参考其“减少重复访存、把同一语义操作合并处理”的组织思路，但不能照搬实现粒度。x86 当前更偏单点寄存器级处理；RVV 更适合按 VL chunk 处理多点，并且需要额外关注 AoS stride、字段间距和 in-place 写回顺序。

---

## 3. 总体设计

### 3.1 分流条件

RVV 分发位于 dense、非 indexed 的整云入口内，分流条件分为三层：

- 编译期：仅在 `#if defined(__RVV10__)` 下编译 RVV helper；未定义时完全走原路径。
- 类型期：`Scalar` 必须为 `float`；`PointT` 必须满足标准布局和字段类型兼容。
- 运行期：`cloud_in.is_dense == true` 且点数不低于 `kTransformRvvMinPoints`；小点云直接回退标量，避免条带设置成本压过收益。

xyz 字段兼容性复用已有 common RVV 点字段封装中的判断；normal 路径增加 `HasNormalFields` / `kTransformRvvNormalCompatible`，确认 `normal_x/normal_y/normal_z` 均存在且为 `float`。

### 3.2 数据访问组织

PCL 点类型通常是 AoS 布局，例如每个点对象内包含 `x/y/z`、颜色、normal 等多个字段。RVV helper 不改变点云存储格式，而是使用字段 offset 与 `sizeof(PointT)` 做 strided load/store：

- `pcl::rvv_load::strided_load3_f32m2`：按点 stride 加载三路 `float` 字段；
- `pcl::rvv_store::strided_store3_f32m2`：按点 stride 写回三路 `float` 字段。

该封装让 `PointXYZ`、`PointXYZRGBNormal` 等点类型可以复用同一访问方式；当字段布局满足封装内部条件时，也可由封装选择更合适的 segment 指令。

### 3.3 xyz 变换条带

`transformPointCloudXYZRVV` 按 `vsetvl` 做 strip-mining：每个 VL chunk 加载 x/y/z，分别计算输出 x/y/z。计算形式对应 4x4 变换矩阵前三行：

- `x' = m00*x + m01*y + m02*z + m03`
- `y' = m10*x + m11*y + m12*z + m13`
- `z' = m20*x + m21*y + m22*z + m23`

每个输出分量用 RVV FMA 组织。输入字段先全部加载到向量寄存器，再写回输出字段；因此 `cloud_in` 与 `cloud_out` 相同时不会因为先写 x/y/z 而污染本 chunk 尚未读取的输入。

### 3.4 xyz + normal fused 条带

`transformPointCloudWithNormals` 的第一版 RVV 实现曾拆成两次完整扫描：先处理 xyz，再处理 normal。板卡结果显示该组织对 `PointXYZRGBNormal` 这类 AoS 数据不合适，重复 strided load/store 与缓存流量抵消了 normal 部分收益。

当前实现改为 fused 条带：

1. 当前 VL chunk 内加载 x/y/z；
2. 计算 SE3 输出 x/y/z；
3. 同一 chunk 内加载 normal_x/normal_y/normal_z；
4. 计算 SO3 输出 normal_x/normal_y/normal_z；
5. 所有输入字段加载完成后，再写回 xyz 与 normal。

normal 只应用旋转，不应用平移：

- `nx' = m00*nx + m01*ny + m02*nz`
- `ny' = m10*nx + m11*ny + m12*nz`
- `nz' = m20*nx + m21*ny + m22*nz`

该组织保留了 in-place 安全，同时把同一语义操作压缩到一次 AoS 条带扫描中，是本轮 normals 路径从倒退转为正收益的关键。

---

## 4. 测试与验证

### 4.1 功能单测

命令：

```bash
make -C test-rvv/common/transforms run_test_std
make -C test-rvv/common/transforms run_test_rvv
```

结果：

| 构建 | RVV 专项单测 | 上游原始测试 | 日志 |
| --- | --- | --- | --- |
| Std | 6/6 passed | 23/23 passed | `test-rvv/common/transforms/output/qemu/run_test_std.log`、`test-rvv/common/transforms/output/qemu/run_upstream_test_std.log` |
| RVV | 6/6 passed | 23/23 passed | `test-rvv/common/transforms/output/qemu/run_test_rvv.log`、`test-rvv/common/transforms/output/qemu/run_upstream_test_rvv.log` |

覆盖内容：

- RVV 专项测试覆盖新增的大点云 RVV 分发、in-place、non-dense 回退、normal 和 indices 标量路径。
- 上游原始 `test/common/test_transforms.cpp` 覆盖 typed transform、float/double、indices、organized cloud、`PointXY` 等原有行为，作为公共模板头未破坏上游语义的回归门槛。

可一次运行两组测试：

```bash
make -C test-rvv/common/transforms run_test_all
```

### 4.2 QEMU bench 口径

命令：

```bash
make -C test-rvv/common/transforms run_bench_compare
```

QEMU bench 只用于确认构建、日志格式和 RVV 指令路径，不用于判断 RVV 是否比标量更快。QEMU 对 RVV intrinsic、strided/segment load-store 的模拟开销与真实板卡差异很大；后续写入 Prompt/SKILL 时应固定这一口径：性能收益必须以真实板卡或目标硬件为准。

已有 QEMU 日志位于 `test-rvv/common/transforms/output/qemu/`。

### 4.3 板卡 bench

板卡日志来自 [test-rvv/common/transforms/output/board/bench_compare.log](../../test-rvv/common/transforms/output/board/bench_compare.log)，设备为 Milkv-Jupiter，数据规模为 1,000,000 点、20 次迭代。日志中包含两次连续 `run_bench_compare`，两次趋势一致；下表采用最后一次汇总。

| Benchmark Item | Std Avg | RVV Avg | Board Speedup |
| --- | ---: | ---: | ---: |
| `transformPointCloud PointXYZ dense copy=true` | 40.5023 ms | 16.2019 ms | 2.50x |
| `transformPointCloud PointXYZ dense copy=false` | 34.6585 ms | 10.3335 ms | 3.35x |
| `transformPointCloud PointXYZ dense in-place` | 40.3866 ms | 11.4018 ms | 3.54x |
| `transformPointCloudWithNormals dense copy=true` | 75.4194 ms | 47.6673 ms | 1.58x |
| `transformPointCloudWithNormals dense copy=false` | 59.5605 ms | 32.0457 ms | 1.86x |

板卡结论：

- `transformPointCloud` 的 RVV 路径有稳定收益，板卡证据为 2.50x–3.54x。
- `transformPointCloudWithNormals` 在 fused 条带循环后转为正收益，板卡证据为 1.58x–1.86x。
- normals 的改进说明瓶颈主要来自第一版的两次完整 AoS 扫描；合并到单个 VL chunk 后，重复访存和写回压力明显下降。

### 4.4 x86 bench

`transforms.hpp` 在 x86 上已有 SSE/AVX 单点 SIMD 特化，因此 `test-rvv/common/transforms/Makefile` 增加 x86 对比入口，用来观察同一 workload 在宿主机上的 baseline 与 x86 SIMD 表现。

命令：

```bash
make -C test-rvv/common/transforms run_bench_x86_compare
```

对比口径：

- `run_bench_x86_std`：关闭显式 SSE/AVX 宏并禁用编译器自动向量化，作为 x86 baseline；
- `run_bench_x86_simd`：使用 `-march=native -mtune=native`，保留当前 x86 SSE/AVX 路径；
- 输出目录：`test-rvv/common/transforms/output/x86/`。

当前 x86 结果：

| Benchmark Item | Std Avg | SIMD Avg | x86 SIMD Speedup |
| --- | ---: | ---: | ---: |
| `transformPointCloud PointXYZ dense copy=true` | 1.9727 ms | 1.6016 ms | 1.23x |
| `transformPointCloud PointXYZ dense copy=false` | 1.5922 ms | 1.2131 ms | 1.31x |
| `transformPointCloud PointXYZ dense in-place` | 2.2109 ms | 1.7957 ms | 1.23x |
| `transformPointCloudWithNormals dense copy=true` | 6.2752 ms | 5.6853 ms | 1.10x |
| `transformPointCloudWithNormals dense copy=false` | 4.0655 ms | 3.6820 ms | 1.10x |

x86 结论：当前 x86 SIMD 路径对 xyz 有稳定收益，对 normals 的收益较小。x86 与 RISC-V 板卡只能做各自平台内的优化判断，不应跨平台直接比较绝对耗时。

### 4.5 自动向量化诊断与反汇编

命令：

```bash
make -C test-rvv/common/transforms generate_vec_report
make -C test-rvv/common/transforms dump_bench_rvv
```

诊断摘要：

- `Memory_Side-Effects`：主要来自显式 RVV intrinsic；
- `Generic_Failure_Could_Not_Vectorize`、`Loop_Structure_Unknown_Iterations`、gather/scatter alias 相关项仍存在；
- `bench_transforms_rvv.asm` 中可匹配 RVV 指令 / 寄存器，说明 RVV 路径已进入 benchmark 二进制。

这些结果说明 `transforms.hpp` 的模板、AoS stride 和别名边界较复杂，不能只依赖编译器稳定自动向量化；手写 RVV 的收益判断仍以板卡 bench 为准。

---

## 5. 总结

本轮在 `transforms.hpp` 中为 dense、非 indexed、`Scalar=float` 的整云变换增加了 RVV 路径。`transformPointCloud` 通过 AoS strided load/store 批量处理 x/y/z；`transformPointCloudWithNormals` 采用 fused 条带循环，在同一 VL chunk 内处理 xyz 与 normal，避免第一版“两次完整扫描”的访存放大。

功能上，RVV 专项 Std/RVV 单测均通过，上游原始 `test/common/test_transforms.cpp` 的 Std/RVV 构建也均通过；构建和指令路径可由 QEMU、自动向量化诊断和反汇编辅助确认。性能上，QEMU 不用于判断加速，真实结论以板卡为准。Milkv-Jupiter bench 证据显示：xyz 路径为 2.50x–3.54x，xyz+normal 路径为 1.58x–1.86x。

后续若沉淀到 Prompt/SKILL，需要保留三条规则：文档路径使用仓库相对路径，不写个人机器绝对路径；QEMU 结果只说明正确性和路径可达，不说明相对标量是否提速；修改公共模板头时，除专项 RVV 单测外，应尽量补跑对应上游原始测试作为回归门槛。
