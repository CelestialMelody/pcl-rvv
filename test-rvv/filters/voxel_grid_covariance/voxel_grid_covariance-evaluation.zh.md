# filters/voxel_grid_covariance 函数级 RVV 评估

## 范围

- 主题：`voxel_grid_covariance`
- 主要源码：
  - `filters/include/pcl/filters/voxel_grid_covariance.h`
  - `filters/include/pcl/filters/impl/voxel_grid_covariance.hpp`
- 专项目录：`test-rvv/filters/voxel_grid_covariance/`
- 当前结论：已实现 `VoxelGridCovariance<PointT>::applyFilter(PointCloud&)` 中 dense、无 distance field、标准 float xyz 点类型的 voxel leaf index 预计算 RVV 路径；per-leaf 累加、centroid、covariance、eigen、inverse covariance、searchable kd-tree 保持原标量逻辑。

## 函数族评估

| 函数族 | 当前状态 | RVV 适配度 | 覆盖 / 回退 |
| --- | --- | --- | --- |
| `VoxelGridCovariance<PointT>::applyFilter(PointCloud&)`，无 `filter_field_name_` 的 dense first pass | 已实现 RVV | 中。`floor(x * inverse_leaf_size)` 与线性 voxel id 可用 VL chunk 批量计算，但后续 `std::map` leaf 聚合仍是标量热点 | 覆盖 `PointT` 标准布局、`float x/y/z`、`input_->is_dense=true`、`downsample_all_data_=false`、无 rgb/rgba 字段、无 distance field、大规模输入；小规模、non-dense、distance field、RGB / all fields 回退 Std leaf id |
| per-leaf mean / covariance 累加 | 保持标量 | 低到中。每点写 `std::map<int, Leaf>`，状态依赖和分配成本主导 | 不 RVV 化，避免改变 leaf 插入顺序、浮点累加顺序和 covariance 语义 |
| second pass centroid / covariance / eigen / inverse covariance | 保持标量 | 低。每个 leaf 规模小且含 Eigen solver、矩阵逆和状态过滤 | 不 RVV 化 |
| `filter(output, searchable=true)` | 间接受益 | 中。前置 leaf id 可受益，kd-tree 构建和搜索仍保持原路径 | 当 first pass 命中 RVV 时前置扫描受益；searchable 状态、`voxel_centroids_leaf_indices_` 和 kd-tree 语义不变 |
| distance field 路径 | 暂缓 | 中。字段读取和区间 mask 可向量化，但仍要跳过点并进入 map 聚合 | 本轮回退 Std，保持 `filter_limit_negative_` 和字段 offset 语义 |
| `downsample_all_data_` / RGB 特化 | 暂缓 | 低到中。leaf centroid 需要复制任意字段和 RGB 拆包 | 本轮回退 Std，避免扩大字段语义和验证范围 |
| non-dense 输入 | 暂缓 | 中。finite mask 可做，但 invalid 点直接跳过且影响 leaf 聚合 | 本轮回退 Std |

## 输入输出边界

- 输入：`pcl::PointCloud<PointT>`，由 `input_` 管理。
- 输出：`PointCloud &output`，保存满足 `min_points_per_voxel_` 的 voxel centroid。
- 内部状态：
  - `leaves_` 保存每个 voxel 的 `mean_`、`centroid`、`cov_`、`icov_`、`evecs_`、`evals_` 和点数；
  - `save_leaf_layout_` 时更新 `leaf_layout_`；
  - `searchable_` 时更新 `voxel_centroids_leaf_indices_` 并由 `filter(output, true)` 构建 kd-tree。
- 本轮 RVV 只预计算每个输入点的线性 voxel id，输出点、leaf 状态和矩阵计算仍由原标量代码产生。

## 数据布局

`PointCloud<PointT>` 是 AoS。RVV helper 以 `sizeof(PointT)` 为 stride，从 `offsetof(PointT, x/y/z)` 读取坐标：

```text
chunk base -> p[c]      p[c+1]    p[c+2] ...
             x y z ...  x y z ... x y z ...
RVV load:    vlse32(x), vlse32(y), vlse32(z)
scale:       x * inverse_leaf_size[0] 等
floor:       vfcvt.x.f.v with round-down mode
linear id:   (ix - min_b[0]) + (iy - min_b[1]) * divb_mul[1] + (iz - min_b[2]) * divb_mul[2]
```

负坐标是风险点：标量用 `Eigen::floor`，RVV 使用带 round-down 模式的 `vfcvt_x_f_v_i32m2_rm`，不能用默认 float-to-int 截断替代。

## RVV 覆盖条件

`computeVoxelGridCovarianceLeafIndicesRVV` 只在以下条件全部满足时返回 true：

- `__RVV10__` 编译；
- `PointT` 是标准布局，且存在 `float x/y/z`；
- `input_->is_dense == true`；
- 点数不少于 `64`；
- 点数不超过 `int` 可表达范围；
- `filter_field_name_` 为空；
- `downsample_all_data_ == false`；
- 点类型没有 `rgb` / `rgba` 字段。

## 回退条件

- 非 RVV 编译；
- 小规模输入；
- non-dense；
- 非标准布局或 xyz 不是 `float` 的点类型；
- distance field 过滤；
- `downsample_all_data_`；
- RGB / RGBA 特化；
- covariance / eigen / inverse covariance / kd-tree 阶段。

回退后调用常驻 `computeVoxelGridCovarianceLeafIndexStd`，保留原 `Eigen::floor(point.getArray4fMap() * inverse_leaf_size_)` 语义。

## 风险点

- leaf id 必须与标量 `Eigen::floor` 完全一致，尤其是负坐标；专项测试包含负坐标单 voxel 手算 centroid。
- RVV helper 使用显式 round-down 的 `vfcvt_x_f_v_i32m2_rm` 对齐 `floor`。实现必须保存并恢复 FRM，否则同一进程后续标量 fallback 可能继承 RDN 舍入模式，出现 1 ulp 级浮点结果漂移。
- RVV 预计算 `std::vector<int> leaf_indices` 会增加一次临时写入和后续读取；由于 `VoxelGridCovariance` 后续 `std::map`、covariance 和 eigen 成本很高，真实硬件收益必须由板卡日志判断。
- QEMU bench 显示 RVV 版本略慢，不能作为性能结论；该日志只用于构建、运行、格式和指令路径证据。
- 完整上游 `test_filters.cpp` 在 RVV 编译下包含其它 filters 主题失败项；本主题用 `--gtest_filter=VoxelGridCovariance.Filters` 做定向 std/RVV 对拍。

## 优化过程问题记录

实现过程中曾观察到 `non-dense fallback 256K` 在 RVV 编译下 checksum 与 Std 不一致：

- 差异现象：bench 原始日志中 Std 为 `5717660375792973828`，RVV 曾出现 `16415332133840058372`；单独运行 non-dense case 时 Std/RVV 均为 `5717660375792973828`；
- `computeVoxelGridCovarianceLeafIndicesRVV` 只在 `input_->is_dense == true` 时返回 true，non-dense 本身会回退到标量 leaf id；
- 诊断先运行 dense RVV 主路径，再运行 non-dense fallback 后，可稳定复现 min/max 与输出点坐标整体 1 ulp 级向下偏移；
- `common::getMinMax3D` 在 non-dense 输入下仍走 `getMinMax3DStandard`，因此“已有 common RVV getMinMax3D 分流导致差异”的推测不成立；
- 根因是 RVV helper 的显式 round-down 转换会操作 FRM；旧实现没有在 helper 退出前恢复调用者原舍入模式，导致同一进程后续标量 fallback 继承 RDN，进而影响 Eigen/标量浮点计算。

最终处理：

- `computeVoxelGridCovarianceLeafIndicesRVV` 入口保存 FRM，返回前恢复 FRM；
- 复跑诊断：先 dense RVV 后 non-dense fallback，min/max、输出 size 和 checksum 均与 Std 一致；
- `non-dense fallback 256K` 已保留在 bench 中，作为 fallback 语义 / 成本证据，不写成 RVV 主路径性能结论；
- 专项单测 `NonDenseFallbackSkipsInvalidPoints` 继续覆盖 non-dense 语义。

## 专项测试

专项测试文件：`test-rvv/filters/voxel_grid_covariance/test_voxel_grid_covariance.cpp`。

覆盖：

- dense 负坐标大输入；
- 80 点单 voxel 手算 centroid，覆盖 RVV 阈值以上的 round-down leaf id；
- `save_leaf_layout_` 与 `searchable=true`；
- distance field fallback；
- non-dense fallback；
- 小输入 fallback。

已运行：

```text
make -C test-rvv/filters/voxel_grid_covariance run_test_compare
```

结果：std 与 RVV 二进制均通过 6 个专项用例。

## bench、QEMU 与反汇编

专项 bench 文件：`test-rvv/filters/voxel_grid_covariance/bench_voxel_grid_covariance.cpp`。

bench 输出包含 `Dataset:`、`Iterations:`、`Build:`，每个 case 输出 `<name> : <avg> ms/iter` 和 `Total Time`。

已运行：

```text
make -C test-rvv/filters/voxel_grid_covariance run_bench_compare
make -C test-rvv/filters/voxel_grid_covariance clean_bench_rvv dump_bench_rvv
```

`output/qemu/analyze_bench_compare.log` 已解析 Dataset / Iterations / Total Time，无 `未解析`、`n/a` 或 `Total Time 不计算`。

反汇编文件：

- `test-rvv/filters/voxel_grid_covariance/build/asm/riscv/bench_voxel_grid_covariance_rvv.full.asm`

已确认关键指令路径：

- `vlse32.v`
- `vfcvt.x.f.v`
- `vmul.vx`
- `vmacc.vx`
- `vse32.v`
- `vsetvli`

## 上游测试与板卡结果

上游定向测试：

```text
make -C test-rvv/filters/voxel_grid_covariance run_upstream_test_compare
```

`UPSTREAM_TEST_ARGS` 使用仓库现有数据：

```text
test/bun0.pcd test/milk_cartoon_all_small_clorox.pcd --gtest_filter=VoxelGridCovariance.Filters
```

结果：std 与 RVV 两套 `VoxelGridCovariance.Filters` 均通过。

板卡入口已在 Makefile / `board.mk` 中提供：

- `deploy_board`
- `run_board_test`
- `run_board_bench_compare`
- `fetch_board_logs`

板卡 bench 已运行并拉回：

```text
make -C test-rvv/filters/voxel_grid_covariance run_board_bench_compare fetch_board_logs
```

本地日志：

- `test-rvv/filters/voxel_grid_covariance/output/board/run_bench_std.log`
- `test-rvv/filters/voxel_grid_covariance/output/board/run_bench_rvv.log`
- `test-rvv/filters/voxel_grid_covariance/output/board/analyze_bench_compare.log`

结果环境：

- Device: `Milkv-Jupiter`
- Dataset: synthetic PointXYZ clouds; dense leaf-index RVV cases plus distance/non-dense fallback cases
- Iterations: 8

真实性能结论：

| case | Std ms/iter | RVV ms/iter | speedup | 结论 |
| --- | ---: | ---: | ---: | --- |
| `vgcov dense leaf-index 64K` | 163.3928 | 108.6677 | 1.50x | 命中 dense leaf-id RVV 主路径 |
| `vgcov dense leaf-index 256K` | 677.8810 | 464.8946 | 1.46x | 大规模 dense 主路径稳定受益 |
| `vgcov dense save-layout 256K` | 677.6697 | 450.2459 | 1.51x | `save_leaf_layout_` 后续标量，但前置 leaf-id RVV 有效 |
| `vgcov dense searchable 64K` | 165.8238 | 109.1650 | 1.52x | `searchable=true` 后续 kd-tree 标量，前置 leaf-id RVV 有效 |
| `vgcov distance-field fallback 256K` | 344.1512 | 434.3232 | 0.79x | fallback 语义证据，不作为 RVV 主路径性能结论 |
| `vgcov non-dense fallback 256K` | 582.4420 | 610.2230 | 0.95x | fallback 语义证据；Std/RVV checksum 均为 `5717660375792973828` |

主路径 speedup 计算方式为 `Std Avg / RVV Avg`。fallback case 用于证明未覆盖路径保持语义和可比较成本，不作为 RVV 主路径收益结论。
