# filters/shadowpoints 函数级 RVV 评估

## 1. 主题与入口

- 主题：`shadowpoints`
- 主文件：`filters/include/pcl/filters/impl/shadowpoints.hpp`
- 公开类：`pcl::ShadowPoints<PointT, NormalT>`
- 专项目录：`test-rvv/filters/shadowpoints/`
- 模块依据：`doc-rvv/library-screening/filters/filters-retained-candidate-rescreen.zh.md` 的后续执行清单第三项。

`ShadowPoints` 用输入点 `pt` 和同下标法线 `normal` 的点积筛除边缘不连续处的 shadow / ghost points。`filter(Indices&)` 输出通过判定的原始点下标；`filter(PointCloud&)` 输出点云，并受 `keep_organized_`、`user_filter_value_`、`extract_removed_indices_` 和 `negative_` 影响。

标量公式：

```text
val = abs(normal.normal_x * pt.x +
          normal.normal_y * pt.y +
          normal.normal_z * pt.z)
keep = (val >= threshold_) xor negative_
```

## 2. 函数级评估

| 函数 / 路径 | 优先级 | RVV 决策 | 覆盖 / 回退 |
| --- | --- | --- | --- |
| `applyFilter(Indices&)` 全云 fake indices | 高 | 已尝试 RVV，收敛为 bench 诊断，不接生产 | 诊断 helper 覆盖 `PointXYZ` + `PointNormal`、点数 `>=64`、normals 数量不小于 cloud size；生产入口保持 Std |
| `applyFilter(Indices&)` 显式 subset indices | 中 | 暂缓，回退 Std | 需要点云和法线双 AoS gather，收益和实现复杂度需另行证明 |
| `applyFilter(PointCloud&)` | 中 | 暂缓，回退原标量 | 除判定外还有整点复制、`keep_organized_` 坏点写、`is_dense` 更新；本轮先优化 indices 主路径 |
| `PointXYZI` / `Normal` / 其它模板组合 | 中 | 回退 Std | traits 限定 `PointXYZ` + `PointNormal`，避免非目标布局误入 RVV |
| 小规模输入 | 低 | 回退 Std | `n < 64` 时避免 RVV 启动和压缩开销 |

## 3. RVV 设计

新增结构：

- 上游 `filters/include/pcl/filters/impl/shadowpoints.hpp` 已恢复为原始标量实现，不再保留 `*_Std` / `*_RVV` helper 或生产分流宏；
- `test-rvv/filters/shadowpoints/bench_shadowpoints.cpp` 在 `__RVV10__ && PCL_SHADOWPOINTS_RVV_BENCH_ONLY` 下保留 `shadowPointsBenchOnlyRVV`，承载诊断 RVV 指令路径；
- 诊断 helper 只在 bench 中以 `if constexpr` 限制 `PointXYZ` + `PointNormal`、全云 fake indices；专项测试仍调用生产 `ShadowPoints`，用于证明源码回退后公开 API 语义不变。

bench-diagnosis helper 按 VL chunk 同时 stride-load 点云 `x/y/z` 和 normals `normal_x/normal_y/normal_z`，计算点积、绝对值和有序 `>= threshold` mask。点云 xyz load 复用 `pcl/rvv_point_load.h` 的 `strided_load3_f32m2`，normal 字段 load 复用同一公共封装的 `strided_load3_fields_f32m2` primitive，避免在诊断代码中复制裸 stride-load 细节。`vcompress` 写 kept indices；当 `extract_removed_indices_` 开启时，另用反向 mask 写 removed indices。`negative_` 只反转 keep / removed mask，不改变点积公式。

`input.is_dense` 不作为 RVV 分流条件。原标量路径没有检查 finite；若点或法线包含 NaN，`abs(dot) >= threshold` 为 false，RVV 的有序比较也会得到 false，随后按 `negative_` 反转，语义一致。

## 4. 测试与 bench 计划

专项测试：`test-rvv/filters/shadowpoints/test_shadowpoints.cpp`

- 全云 `PointXYZ` + `PointNormal` indices 主路径；
- `negative_` + `extract_removed_indices_`；
- 显式 subset fallback；
- `PointXYZ` + `Normal` 类型 fallback；
- `filter(PointCloud&)` + `keep_organized_` fallback；
- 同一进程先运行 `__RVV10__` 构建，再运行 fallback，确认生产分流关闭后 fallback 语义不变。

专项 bench：`test-rvv/filters/shadowpoints/bench_shadowpoints.cpp`

- `shadowpoints pointxyz full-cloud 64K`
- `shadowpoints pointxyz full-cloud 1M`
- `shadowpoints negative removed 1M`
- `shadowpoints subset fallback 1M`
- `shadowpoints normal type fallback 1M`
- `shadowpoints pointxyzi fallback 1M`
- `shadowpoints cloud keep_organized fallback 64K`

bench 输出包含 `Dataset:`、`Iterations:`、`Build:`、case 行、`Total Time` 和 checksum。

上游测试：`test/filters/test_filters.cpp` 已有 `ShadowPoints.Filters`。专项 Makefile 通过：

```text
UPSTREAM_TEST_ARGS = test/bun0.pcd test/milk_cartoon_all_small_clorox.pcd --gtest_filter=ShadowPoints.Filters
```

复用仓库原始测试参数和数据，不复制测试数据。

## 5. 验证结果

- QEMU 专项测试：`make -C test-rvv/filters/shadowpoints run_test_compare` 通过，std/RVV 构建均通过 6 个专项测试。
- QEMU 上游测试：`make -C test-rvv/filters/shadowpoints run_upstream_test_compare` 通过，std/RVV 两套 `test/filters/test_filters.cpp --gtest_filter=ShadowPoints.Filters` 均通过。
- 上游测试链接诊断：初次直接链接 `test_filters.cpp` 时缺少 `pcl::internal::optimizeModelCoefficientsEllipse3D`，定位到 `sample_consensus/src/sac_model_ellipse3d.cpp`，专项 Makefile 已把该源文件加入 `SRCS_UPSTREAM_TEST`。补齐后 std/RVV 上游测试均通过；该问题是专项链接覆盖不足，不是环境阻塞。
- QEMU bench：`make -C test-rvv/filters/shadowpoints run_bench_compare` 通过，std 与 bench-diagnosis RVV checksum 对齐；`output/qemu/analyze_bench_compare.log` 无 `未解析`、`n/a`、`Total Time 不计算`。QEMU 仅作为构建、格式、正确性和指令路径证据，不作为性能结论。
- 反汇编：`make -C test-rvv/filters/shadowpoints dump_bench_rvv` 生成 `build/asm/riscv/bench_shadowpoints_rvv.full.asm`，摘录 `output/qemu/rvv_asm_check.log` 确认 `vlse32.v`、`vfmacc.vv`、`vfabs.v`、`vmfge.vf`、`vcompress.vm`、`vcpop.m`、`vsetvli ... e32,m2`。
- 板卡验证：`make -C test-rvv/filters/shadowpoints run_board_test run_board_bench_compare fetch_board_logs` 通过，日志已拉回 `output/board/`。

Milkv-Jupiter bench-diagnosis 结果：

- `shadowpoints pointxyz full-cloud 64K`：Std `1.9789` ms/iter，RVV `3.1508` ms/iter，`0.63x`；
- `shadowpoints pointxyz full-cloud 1M`：Std `30.2056` ms/iter，RVV `61.3398` ms/iter，`0.49x`；
- `shadowpoints negative removed 1M`：Std `35.2654` ms/iter，RVV `80.6662` ms/iter，`0.44x`；
- `shadowpoints subset fallback 1M`：Std `16.3322` ms/iter，RVV `16.2578` ms/iter，`1.00x`；
- `shadowpoints normal type fallback 1M`：Std `29.2840` ms/iter，RVV `29.2587` ms/iter，`1.00x`；
- `shadowpoints pointxyzi fallback 1M`：Std `31.2208` ms/iter，RVV `31.1084` ms/iter，`1.00x`；
- `shadowpoints cloud keep_organized fallback 64K`：Std `1.8734` ms/iter，RVV `1.8772` ms/iter，`1.00x`。

## 6. 结论

`shadowPointsBenchOnlyRVV` 的正确性、checksum 和指令路径均成立，但板卡真实性能不成立。双 AoS stride load 加上 mask 压缩的成本高于当前标量循环；`negative_ + removed_indices_` 还需要额外压缩 removed 输出，退化更明显。因此本主题不接入生产主路径，RVV 实验只保留在 `test-rvv/filters/shadowpoints/bench_shadowpoints.cpp` 的 bench-diagnosis microbench / 诊断 case 中。上游 `ShadowPoints::applyFilter(Indices&)` 已恢复原始标量实现，未改变公开 API 和运行语义。
