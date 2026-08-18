# `filters/include/pcl/filters/impl/voxel_grid.hpp`：函数级梳理、筛选评估与 RVV 优先级

本文档记录 `voxel_grid` 主题的函数级筛选与 RVV 实施结论。该主题来自 `doc-rvv/library-screening/filters/filters-function-evaluation-queue.zh.md` 的 filters 模块第二轮筛选；当前已覆盖 `PCLPointCloud2 getMinMax3D` 的 dense、float、非 indexed 基础路径，并继续扩展了 dense、float 的 indices 与 distance field 中优先级路径。

## 0. 首轮实现与验证状态

当前首轮 RVV 分流已完成：

- 实现位置：`filters/include/pcl/filters/impl/voxel_grid.hpp`；
- 专项测试：`test-rvv/filters/voxel_grid/test_voxel_grid.cpp`；
- 专项 bench：`test-rvv/filters/voxel_grid/bench_voxel_grid.cpp`；
- 构建入口：`test-rvv/filters/voxel_grid/Makefile`。

已验证范围：

| 验证项 | 命令 / 产物 | 结果 | 说明 |
| --- | --- | --- | --- |
| QEMU 可用性 | `command -v qemu-riscv64 && qemu-riscv64 --version` | 通过 | `/usr/bin/qemu-riscv64`，版本 `11.0.0` |
| std/RVV 单测对拍 | `make -C test-rvv/filters/voxel_grid run_test_compare` | 通过 | std 与 RVV 两套二进制均通过 7 个专项用例，覆盖基础、indices、distance field、fallback、non-dense 回退语义 |
| QEMU bench 运行 | `make -C test-rvv/filters/voxel_grid run_bench_compare dump_bench_rvv` | 通过 | 只作为运行和正确性补充证据，不作为性能结论；`analyze_bench_compare.log` 已能解析 Dataset / Iterations，`Total Time` 不再为 `n/a` |
| RVV 指令路径 | `build/asm/riscv/bench_voxel_grid_rvv.full.asm` | 通过 | 已确认 `vlse32`、`vluxei32`、`vfredmin`、`vfredmax`、`vmflt` / `vmfgt` / `vmand` / `vmnot`、`vsetvli ... e32` |
| 板卡 bench | `test-rvv/filters/voxel_grid/output/board/bench_compare.log` | 通过 | `Milkv-Jupiter` 上已有真实板卡结果：基础路径约 `3.65x`~`4.58x`，indices 路径约 `3.60x`，distance field 路径约 `2.53x`~`2.70x` |
| 上游原始测试 | `make -C test-rvv/filters/voxel_grid run_upstream_test_compare` | 受阻 | 当前依赖链缺少 `flann/util/params.h`，不是首轮 RVV 实现引入的新错误 |

QEMU bench 当前只证明 RVV 二进制可运行、目标 helper 可被编译进二进制且存在预期指令路径；性能判断以 `test-rvv/filters/voxel_grid/output/board/bench_compare.log` 中的真实板卡日志为准。

## 1. 函数 / 函数组梳理

| 函数 / 函数组 | 功能概要 | 复杂度与访存 | 当前判断 |
| --- | --- | --- | --- |
| `getHalfNeighborCellIndices` / `getAllNeighborCellIndices` | 构造固定邻域偏移矩阵 | 固定小规模 | 不做 RVV |
| `getMinMax3D(PCLPointCloud2, x/y/z)` | 从字节布局点云中扫描 xyz min/max | O(N)，按 `point_step` strided load，6 路 min/max 规约 | 高优先级，适合先评估 dense、float、非 indexed 路径 |
| `getMinMax3D(PCLPointCloud2, indices, x/y/z)` | 带 indices 的 xyz min/max | O(M)，gather 访问 | 中优先级，已尝试并实现 dense、float、indices RVV gather 路径 |
| `getMinMax3D(PCLPointCloud2, distance field)` | 带距离字段过滤的 xyz min/max | O(N)，字段读取 + 分支过滤 + min/max | 中优先级，已尝试并实现 dense、float、非 indices distance field RVV mask 路径 |
| `getMinMax3D(PointCloud<PointT>, distance field)` | 模板点云 min/max，可能带距离过滤 | O(N)，AoS，Eigen map | 中优先级，已评估后暂缓；泛型点类型布局和 `getArray4fMap()` 语义不适合在当前主题内安全 RVV 化 |
| `VoxelGrid<PointT>::applyFilter` 前置 index 生成 | 为每个点计算 voxel cell index 并写入 `index_vector` | O(indices)，floor + int index + push_back；后接 sort | 中优先级，已评估后暂缓；收益被 `std::floor`、`emplace_back` 和后续 sort 主导，结构侵入与验证成本偏高 |
| `VoxelGrid<PointT>::applyFilter` sort / cell grouping | 按 voxel index 排序、统计 cell 范围 | 排序 + while grouping | 不适合 RVV 首轮 |
| `VoxelGrid<PointT>::applyFilter` centroid 聚合 | 每个 voxel 内求平均或 `CentroidPoint` 累加 | 不规则分组扫描 | 暂缓，收益依赖 voxel 分布 |

## 2. 筛选标准

| 维度 | 判断 |
| --- | --- |
| 循环规模 | `getMinMax3D` 与 `applyFilter` 前置扫描随点数线性增长，典型点云规模足够大 |
| 算术密度 | min/max 规约较轻；voxel index 生成包含 floor、乘加和整数转换 |
| 访存规整性 | `PCLPointCloud2` 是字节步进字段访问；标准 `PointXYZ` / `PointXYZI` 可用固定 offset；indices 路径为 gather |
| 分支复杂度 | dense、非 filtered 路径简单；non-dense、distance field、limit_negative 分支更多 |
| 数值语义风险 | min/max 顺序不敏感；floor 和边界值需要保持一致；NaN/Inf 路径不能误改 |
| 可测试性 | 上游 `test/filters/test_filters.cpp` 有 VoxelGrid 用例，`benchmarks/filters/voxel_grid.cpp` 有 benchmark 参考 |

## 3. 优先级与候选总表

| 优先级 | 状态 | 函数 / 族 | 第一轮优化方向 | 主要风险 | 预期收益 | fallback 条件 |
| --- | --- | --- | --- | --- | --- | --- |
| 高 | 已实现 / QEMU 已验证 | `getMinMax3D(PCLPointCloud2, x/y/z)` dense、float、非 indexed | strided load xyz + vector min/max reduction | 字段 offset / point_step 兼容、空点云边界 | 点数大时中等，减少扫描成本 | 非 FLOAT32、non-dense、小规模、非标准 offset |
| 中 | 已实现 / QEMU 已验证 | `getMinMax3D(PCLPointCloud2, indices, x/y/z)` dense、float | gather load + min/max reduction | gather 局部性不稳定，indices 越界语义必须保持 | indices 规模足够大时减少标量循环成本 | 非 FLOAT32、non-dense、小规模、空 indices |
| 中 | 已实现 / QEMU 已验证 | distance field 过滤版 `getMinMax3D(PCLPointCloud2)` dense、float、非 indices | distance mask + xyz min/max reduction | `limit_negative`、边界比较、无命中点语义必须保持 | 过滤比例适中时减少分支与规约成本 | 非 FLOAT32、non-dense、小规模、复杂字段类型 |
| 中 | 已评估后暂缓 | `getMinMax3D(PointCloud<PointT>, distance field)` | 暂不实现 | 泛型点类型布局与 `getArray4fMap()` 绑定，不能安全假设 xyz 连续 float 布局 | 不明确 | 保持标量 |
| 中 | 已评估后暂缓 | `applyFilter` 前置 voxel index 生成 | 暂不实现 | `std::floor`、`emplace_back`、后续 sort 主导；直接 RVV 化会明显侵入现有结构 | 不明确 | 保持标量 |
| 低 | 暂缓 | sort、cell grouping、centroid 聚合 | 不做首轮 RVV | 不规则分组、容器操作 | 不明确 | 保持标量 |

## 4. 已实现范围与暂缓范围

当前已覆盖：

- `PCLPointCloud2` 的 `getMinMax3D` dense、float、非 indices、无 distance field 基础路径；
- `PCLPointCloud2` 的 `getMinMax3D` dense、float、indices 路径；
- `PCLPointCloud2` 的 `getMinMax3D` dense、float、非 indices、distance field 路径；
- `x/y/z` 字段类型均为 `FLOAT32`；
- 点数达到阈值后进入 RVV，小点云保持标量。

暂不覆盖：

- `double`；
- non-dense；
- `PointCloud<PointT>` distance field 路径；
- `VoxelGrid::applyFilter` 前置 index 生成；
- `VoxelGrid::applyFilter` 后半段 sort、分组和 centroid 聚合。

暂缓原因：

- `PointCloud<PointT>` distance field 路径依赖泛型点类型布局和 `getArray4fMap()`，当前无法像 `PCLPointCloud2` 一样稳定确认字段 offset 与连续 float 访存边界；
- `applyFilter` 前置 index 生成的主要成本与语义集中在 `std::floor`、容器写入顺序和后续 sort，直接 RVV 化会侵入当前数据流，且难以把收益稳定归因到该阶段。

## 5. 测试与 bench 计划

| 项目 | 计划 |
| --- | --- |
| 专项单测 | 构造 `PCLPointCloud2`，覆盖 dense float xyz、不同 `point_step`、小规模 fallback、大规模 RVV 路径 |
| 上游测试 | 迁移或调用 `test/filters/test_filters.cpp` 中 VoxelGrid 相关用例 |
| Bench | 建立 `test-rvv/filters/voxel_grid/bench_voxel_grid.cpp`，单独测 `getMinMax3D` 与整体 VoxelGrid filter |
| QEMU | 只证明 std/RVV 构建、正确性和 RVV 指令路径 |
| 板卡 | 性能结论以 `test-rvv/filters/voxel_grid/output/board/bench_compare.log` 为准，当前已完成真实板卡 bench |

## 6. 代码与测试索引

| 项目 | 路径 |
| --- | --- |
| 实现 | `filters/include/pcl/filters/impl/voxel_grid.hpp` |
| 声明 | `filters/include/pcl/filters/voxel_grid.h` |
| 上游测试 | `test/filters/test_filters.cpp` |
| 上游 benchmark | `benchmarks/filters/voxel_grid.cpp` |
| 本评估文档 | `test-rvv/filters/voxel_grid/voxel_grid-evaluation.zh.md` |
| RVV 实现说明 | `doc-rvv/filters/voxel_grid-RVV.zh.md` |

## 7. 当前 closeout 状态

`voxel_grid` 的 closeout 状态不是“未开始”，也不是“完全结束”，而是“首个实现点已完成，主题 closeout 仍在进行中”。

已完成：

- `PCLPointCloud2 getMinMax3D` dense、float、非 indexed RVV 分流；
- `PCLPointCloud2 getMinMax3D` dense、float、indices RVV gather 分流；
- `PCLPointCloud2 getMinMax3D` dense、float、非 indices、distance field RVV mask 分流；
- `getMinMax3DStd` / `getMinMax3DIndicesStd` / `getMinMax3DDistanceStd` 与对应 `*_RVV` 主路径 helper 已按 PCL 文件风格放在 `pcl` 命名空间下，未额外套 `pcl::detail`；
- 基础、indices、distance field 三组 Std/RVV helper 已分别放在对应 `getMinMax3D` 分发入口附近，入口只保留类型检查、`__RVV10__` 短路尝试和 Std fallback；
- `test-rvv/filters/voxel_grid/` 专项测试与 bench；
- QEMU std/RVV 对拍；
- QEMU bench 运行与 RVV 指令路径确认；
- `test-rvv/filters/voxel_grid/Makefile` 与 `board.mk` 的板卡部署入口；
- 板卡 bench 日志：`test-rvv/filters/voxel_grid/output/board/bench_compare.log`，`Milkv-Jupiter` 上基础路径约 `3.65x`~`4.58x`，indices 路径约 `3.60x`，distance field 路径约 `2.53x`~`2.70x`；
- `doc-rvv/filters/voxel_grid-RVV.zh.md`。

已评估后暂缓：

- `PointCloud<PointT> + distance field getMinMax3D`：泛型点类型布局与 `getArray4fMap()` 绑定，无法在当前主题内安全假设 xyz 连续 float 布局；
- `VoxelGrid<PointT>::applyFilter` 前置 voxel index 生成：成本与语义主要受 `std::floor`、`emplace_back` 和后续 sort 影响，直接 RVV 化结构侵入与验证成本偏高。

仍需确认或补齐：

- 上游 `test/filters/test_filters.cpp` 的 std/RVV 对拍是否已补成正式记录。
