# filters/shadowpoints RVV 优化说明

本文档记录 `pcl::ShadowPoints<PointT, NormalT>` 的 RVV 可行性实验、bench 诊断 helper 和验证结果。实验显示该 RVV 方案正确且命中指令，但 Milkv-Jupiter 上真实性能不成立，因此上游源码已回退到原始标量实现，RVV 只保留在专项 bench 中作为诊断证据。

## 1. 入口作用

`ShadowPoints` 是 filters 模块里的几何筛选器，用于根据点到视线 / 法线方向的关系移除边缘不连续处的 shadow points。调用者提供输入点云和同下标 normals；`filter(Indices&)` 输出保序下标，`filter(PointCloud&)` 输出点云。

本轮评估的是 `filter(Indices&)` 间接调用的 `ShadowPoints::applyFilter(Indices&)` 主路径。它在算法管线中承担线性判定和保序 indices 压缩职责。最终实现不改变公开 API，也不改变生产分流；`filter(PointCloud&)` 的整点复制、`keep_organized_` 坏点写和 `is_dense` 更新保持标量。

## 2. 覆盖范围

bench-diagnosis RVV helper 只覆盖：

- `PointT = pcl::PointXYZ`
- `NormalT = pcl::PointNormal`
- 全云 fake indices，即没有显式 subset indices
- 点数 `>=64`
- normals 数量不小于输入点数

生产和诊断回退条件：

- subset indices：双 AoS gather 成本和收益未证明；
- 非 `PointXYZ` / 非 `PointNormal`：避免字段布局误判；
- `filter(PointCloud&)`：输出点云语义包含整点复制和 organized 坏点写；
- 小规模输入：RVV 启动和压缩开销不稳定。

`ShadowPoints::applyFilter(Indices&)` 保持上游原始标量实现。当前主题没有生产 RVV 分流宏，也没有在 `filters/include/pcl/filters/impl/shadowpoints.hpp` 中保留 `*_RVV` helper。

## 3. 实现设计

`test-rvv/filters/shadowpoints/bench_shadowpoints.cpp` 保留诊断实现：

- `shadowPointsBenchOnlyRVV`：`__RVV10__ && PCL_SHADOWPOINTS_RVV_BENCH_ONLY` 下的 bench-diagnosis RVV helper；
- `benchIndices` 中的 `if constexpr`：只允许 `PointXYZ` + `PointNormal`、全云 fake indices 命中诊断 helper；
- 其它类型、subset indices 和 cloud-output case 继续调用生产 `ShadowPoints` 标量路径，用于验证上游源码回退后的 fallback 语义和成本。

核心 RVV 片段按一个 VL chunk 读取点和法线：

```cpp
vfloat32m2_t px, py, pz;
pcl::rvv_load::strided_load3_f32m2<sizeof(pcl::PointXYZ),
                                   offsetof(pcl::PointXYZ, x),
                                   offsetof(pcl::PointXYZ, y),
                                   offsetof(pcl::PointXYZ, z)>(point_chunk, vl, px, py, pz);

vfloat32m2_t nx, ny, nz;
pcl::rvv_load::strided_load3_fields_f32m2<sizeof(pcl::PointNormal),
                                          offsetof(pcl::PointNormal, normal_x),
                                          offsetof(pcl::PointNormal, normal_y),
                                          offsetof(pcl::PointNormal, normal_z)>(
    normal_chunk, vl, nx, ny, nz);

vfloat32m2_t dot = __riscv_vfmul_vv_f32m2 (nx, px, vl);
dot = __riscv_vfmacc_vv_f32m2 (dot, ny, py, vl);
dot = __riscv_vfmacc_vv_f32m2 (dot, nz, pz, vl);
const vbool16_t inlier =
    __riscv_vmfge_vf_f32m2_b16 (__riscv_vfabs_v_f32m2 (dot, vl), threshold, vl);
```

`PointXYZ` 和 `PointNormal` 都是 AoS，点与法线数组下标一一对应，因此使用公共 RVV load 封装的 stride-load primitive，而不是重排输入或在 bench 中重复裸 `vlse32` 细节。`vcompress` 将满足 mask 的 source index 保序写入输出；`extract_removed_indices_` 开启时，使用反向 mask 写 removed indices。`negative_` 只交换 keep / removed mask 的语义。

该 helper 不使用 `_rm` intrinsic，也不修改 FRM/FCSR。

## 4. VL Chunk 数值算例

设 `threshold = 0.1`，一个 VL chunk 有 4 个点：

| lane | `pt.x, pt.y, pt.z` | `normal_x, normal_y, normal_z` | `abs(dot)` | keep |
| ---: | --- | --- | ---: | --- |
| 0 | `(1, 0, 0)` | `(0.2, 0, 0)` | `0.2` | yes |
| 1 | `(0, 1, 0)` | `(0, 0.05, 0)` | `0.05` | no |
| 2 | `(0, 0, -2)` | `(0, 0, 0.2)` | `0.4` | yes |
| 3 | `(NaN, 0, 1)` | `(1, 0, 0)` | `NaN` | no |

图示：

```text
point lanes:   p0      p1      p2      p3
normal lanes:  n0      n1      n2      n3
abs(dot):      .20     .05     .40     NaN
mask >= .10:   1       0       1       0
vcompress id:  [0, 2]
```

NaN lane 的有序 `>=` 比较为 false，与标量 `std::abs(NaN) >= threshold` 的布尔结果一致。

## 5. 验证

QEMU：

- `make -C test-rvv/filters/shadowpoints run_test_compare` 通过；
- `make -C test-rvv/filters/shadowpoints run_upstream_test_compare` 通过，`ShadowPoints.Filters` std/RVV 均通过；
- `make -C test-rvv/filters/shadowpoints run_bench_compare dump_bench_rvv` 通过，checksum 对齐；
- `output/qemu/analyze_bench_compare.log` 无 `未解析`、`n/a`、`Total Time 不计算`；
- 反汇编摘录 `output/qemu/rvv_asm_check.log` 确认 `vlse32.v`、`vfmacc.vv`、`vfabs.v`、`vmfge.vf`、`vcompress.vm`、`vcpop.m`、`vsetvli ... e32,m2`。

上游测试链接复核中，直接编译 `test/filters/test_filters.cpp` 曾缺少 `pcl::internal::optimizeModelCoefficientsEllipse3D`。该符号来自 `sample_consensus/src/sac_model_ellipse3d.cpp`；专项 Makefile 补入该源文件后，std/RVV 上游测试均通过。这是专项链接覆盖不足，不是环境阻塞。

板卡：

```text
make -C test-rvv/filters/shadowpoints run_board_test run_board_bench_compare fetch_board_logs
```

设备：Milkv-Jupiter。Dataset：synthetic `PointXYZ` + `PointNormal` clouds; full-cloud dot/abs threshold bench-diagnosis RVV cases and subset/type/cloud-output fallback cases。Iterations：`30`。日志路径：

```text
test-rvv/filters/shadowpoints/output/board/run_test.log
test-rvv/filters/shadowpoints/output/board/run_bench_std.log
test-rvv/filters/shadowpoints/output/board/run_bench_rvv.log
test-rvv/filters/shadowpoints/output/board/analyze_bench_compare.log
```

## 6. 性能结果与结论

每个 speedup 按 `Std avg / RVV avg` 计算。前三个 case 直接调用 bench-diagnosis RVV helper，用于证明双 AoS dot/abs/mask/压缩方案的真实性能；后四个 case 是 fallback / 未覆盖路径语义和成本证据，不作为 RVV 主路径性能结论。

| case | 入口 / 参数 | 路径 | Std ms/iter | RVV ms/iter | speedup | 结论 |
| --- | --- | --- | ---: | ---: | ---: | --- |
| `shadowpoints pointxyz full-cloud 64K` | `filter(Indices&)`，64K `PointXYZ` + `PointNormal`，`negative=false` | bench-diagnosis RVV | `1.9789` | `3.1508` | `0.63x` | 小规模主路径不成立 |
| `shadowpoints pointxyz full-cloud 1M` | `filter(Indices&)`，1M `PointXYZ` + `PointNormal`，`negative=false` | bench-diagnosis RVV | `30.2056` | `61.3398` | `0.49x` | 大规模主路径显著慢于 Std |
| `shadowpoints negative removed 1M` | `filter(Indices&)`，1M，`negative=true`，提取 removed indices | bench-diagnosis RVV | `35.2654` | `80.6662` | `0.44x` | 双路压缩输出进一步放大成本 |
| `shadowpoints subset fallback 1M` | 显式 subset indices | Std fallback | `16.3322` | `16.2578` | `1.00x` | 未覆盖 subset 语义 / 成本保持 |
| `shadowpoints normal type fallback 1M` | `PointXYZ` + `Normal` | Std fallback | `29.2840` | `29.2587` | `1.00x` | 类型 fallback 保持 |
| `shadowpoints pointxyzi fallback 1M` | `PointXYZI` + `PointNormal` | Std fallback | `31.2208` | `31.1084` | `1.00x` | 非目标点类型 fallback 保持 |
| `shadowpoints cloud keep_organized fallback 64K` | `filter(PointCloud&)`，`keep_organized=true` | Std fallback | `1.8734` | `1.8772` | `1.00x` | cloud-out 路径保持标量语义 |

结论：`shadowPointsBenchOnlyRVV` 的正确性、checksum 和指令路径成立，但真实板卡性能不成立。双 AoS stride load、绝对值比较和 `vcompress` 输出在该入口上不能抵消额外成本；`negative_ + removed_indices_` 需要额外压缩 removed 输出，退化更明显。因此本主题收敛为 bench-diagnosis microbench / 诊断 case，不接入生产主路径；上游 `shadowpoints` 源码保持原始标量实现。
