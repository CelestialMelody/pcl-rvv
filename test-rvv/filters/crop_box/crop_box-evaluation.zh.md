# filters/crop_box 函数级 RVV 评估

## 范围

- 主题：`crop_box`
- 主要源码：
  - `filters/include/pcl/filters/crop_box.h`
  - `filters/include/pcl/filters/impl/crop_box.hpp`
  - `filters/src/crop_box.cpp`
- 专项目录：`test-rvv/filters/crop_box/`
- 当前结论：已实现 `CropBox<PointT>::applyFilter(Indices&)` 的 dense、identity indices、无 transform/translation/rotation、标准 float xyz 点类型 RVV 路径；其它路径保持标量。

## 函数族评估

| 函数族 | 当前状态 | RVV 适配度 | 覆盖 / 回退 |
| --- | --- | --- | --- |
| `CropBox<PointT>::applyFilter(Indices&)`，identity transform | 已实现 RVV | 中。线性扫描 `x/y/z` 六个区间比较，输出 indices / removed indices 可用 mask + `vcompress` 保序压缩 | 覆盖 `PointT` 标准布局、`float x/y/z`、`input_->is_dense=true`、`fake_indices_=true`、大规模输入、无 translation/rotation/transform；小规模、显式 subset、non-dense、任一变换配置回退 Std |
| `CropBox<PointT>::filter(PointCloud&)` | 间接受益 | 中。基类先调用 indices 过滤，再复制点到输出点云 | 当内部 `applyFilter(Indices&)` 命中 RVV 时受益；点拷贝保持原路径 |
| `CropBox<PointT>::applyFilter(Indices&)`，translation/rotation/transform | 暂缓 | 中。矩阵变换可向量化，但需要融合变换和区间筛选，且 rotation 会构造 inverse transform | 本轮回退 Std，避免改变几何变换顺序和验证范围 |
| `CropBox<PointT>::applyFilter(Indices&)`，显式 subset indices | 暂缓 | 中。需要 gather subset index 再 gather AoS xyz，输出仍需保序压缩 | 当前收益不确定且验证成本高，回退 Std |
| `CropBox<PointT>::applyFilter(Indices&)`，non-dense | 暂缓 | 中。可用 finite mask，但标量先跳过 invalid，不进入 removed indices；需额外证明 NaN/Inf 与区间 mask 的组合 | 本轮回退 Std，保持 invalid 点处理语义 |
| `CropBox<pcl::PCLPointCloud2>::applyFilter(Indices&)` | 暂缓 | 中。字节步进 xyz 可 RVV 化 | 位于 `filters/src/crop_box.cpp`，且字段 offset、PCLPointCloud2 cloud 输出、keep_organized 写坏点组合更复杂，本轮不扩大范围 |

## 输入输出边界

- 输入：`pcl::PointCloud<PointT>`，由 `input_` 和 `indices_` 管理。
- 输出：`Indices &indices`，保存满足 CropBox 条件的输入下标，顺序必须与标量扫描一致。
- 可选输出：`removed_indices_`。`negative_ == false` 时 outside 写入 removed；`negative_ == true` 时 inside 写入 removed。
- 区间语义：
  - inside：`min_pt_[0] <= x <= max_pt_[0]`、`min_pt_[1] <= y <= max_pt_[1]`、`min_pt_[2] <= z <= max_pt_[2]`。
  - `negative_ == false`：保留 inside。
  - `negative_ == true`：保留 outside。
- dense 标量路径不会额外做 finite 检查；因此 RVV dense 路径也只表达六个边界比较。non-dense 仍回退标量，保留 invalid 点直接跳过的行为。

## 数据布局

`PointCloud<PointT>` 是 AoS。RVV 主路径使用 `sizeof(PointT)` 作为 stride，分别从 `offsetof(PointT, x/y/z)` 做 `vlse32`：

```text
chunk base -> p[c]      p[c+1]    p[c+2] ...
             x y z ...  x y z ... x y z ...
RVV load:    vlse32(x), vlse32(y), vlse32(z)
mask:        x/y/z 六个区间比较
output:      vcompress(source_index, keep_mask)
```

## RVV 覆盖条件

`applyFilterIndicesRVV` 只在以下条件全部满足时返回 true：

- `__RVV10__` 编译；
- `PointT` 是标准布局，且存在 `float x/y/z`；
- `fake_indices_ == true`，输入 indices 是 PCLBase 生成的 identity 全量索引；
- `input_` 有效且 `input_->is_dense == true`；
- `indices_->size() >= 64`；
- 点数不超过 `int` 可表达范围；
- `rotation_ == 0`；
- `translation_ == 0`；
- `transform_` 是 identity。

## 回退条件

- 非 RVV 编译；
- 小规模输入；
- 显式 `setIndices()` subset；
- non-dense；
- 非标准布局或 xyz 不是 `float` 的点类型；
- translation、rotation 或 transform 非 identity；
- `PCLPointCloud2` 特化；
- 任一运行时条件不满足。

回退后调用常驻 `applyFilterIndicesStd`，保留原标量代码文本和语义。

## 风险点

- `negative_` 与 `extract_removed_indices_` 的组合必须保序；RVV 通过 `vcompress(source_index, mask)` 写出 identity source index。
- dense 路径对 NaN 的处理必须与标量比较一致：NaN 不满足 `<` / `>`，因此可能被视为 inside；本轮只覆盖 dense，non-dense 回退。
- AoS stride load 与 `vcompress` 对真实硬件收益依赖较强；QEMU 不能作为性能结论。
- 变换路径若强行 RVV，需要证明矩阵乘、translation、inverse rotation 顺序与标量完全一致，本轮暂缓。
- `PCLPointCloud2` keep_organized 输出会改写 xyz 字段，当前不纳入模板主题。

## 专项测试计划与结果

专项测试文件：`test-rvv/filters/crop_box/test_crop_box.cpp`。

覆盖：

- 小规模标量公式；
- 大规模 dense identity 正向区间 RVV 主路径；
- 大规模 dense identity `negative_`；
- `extract_removed_indices_` 顺序；
- cloud 输出经基类间接受益；
- 显式 subset fallback；
- non-dense invalid fallback；
- translation fallback。

已运行：

```text
make -C test-rvv/filters/crop_box run_test_compare
```

结果：std 与 RVV 二进制均通过 7 个专项用例。

## bench、QEMU 与反汇编

专项 bench 文件：`test-rvv/filters/crop_box/bench_crop_box.cpp`。

bench 输出包含 `Dataset:`、`Iterations:`、`Build:`，每个 case 输出 `<name> : <avg> ms/iter` 和 `Total Time`。

已运行：

```text
make -C test-rvv/filters/crop_box run_bench_compare dump_bench_rvv
```

`output/qemu/analyze_bench_compare.log` 已解析 Dataset / Iterations / Total Time，无 `未解析`、`n/a` 或 `Total Time 不计算`。QEMU 结果只作为构建、运行、日志格式和指令路径证据，不作为性能结论。

反汇编文件：

- `test-rvv/filters/crop_box/build/asm/riscv/bench_crop_box_rvv.full.asm`

已确认关键指令路径：

- `vlse32.v`
- `vcompress.vm`
- `vcpop.m`
- `vmflt.vf`
- `vmfgt.vf`
- `vmand.mm`
- `vmnot.m`
- `vsetvli ... e32,m2`

## 上游测试与板卡结果

- 上游 `test/filters/test_clipper.cpp` 已通过专项 Makefile 补齐 `pcl_filters`、`pcl_io`、`pcl_sample_consensus`、`pcl_search`、`pcl_kdtree`、`pcl_octree`、`flann`、`boost`、`hdf5`、`lz4`、`zlib`、`png` 等依赖。
- `test_clipper.cpp` 不需要额外 PCD 参数，`UPSTREAM_TEST_ARGS` 默认为空。
- `make -C test-rvv/filters/crop_box run_upstream_test_compare` 已通过 std/RVV 两套上游 clipper 测试。
- `make -C test-rvv/filters/crop_box run_board_bench_compare fetch_board_logs` 已完成，日志已拉回 `test-rvv/filters/crop_box/output/board/`。
- 真实板卡性能结论：
  - `crop_box indices identity 64K`：`3.08x`；
  - `crop_box indices identity 1M`：`3.01x`；
  - `crop_box indices negative 1M`：`2.28x`；
  - `crop_box indices removed 1M`：`2.09x`；
  - `crop_box cloud-out identity 1M`：`2.63x`，该 case 通过 RVV indices 阶段间接受益，点云拷贝仍保持原路径；
  - `crop_box explicit subset fallback 1M`：`1.00x`，用于证明显式 subset 未覆盖路径保持接近 Std；
  - `crop_box non-dense fallback 1M`：`1.00x`，用于证明 invalid 点处理保持 Std；
  - `crop_box translation fallback 1M`：`1.00x`，用于证明 transform 相关路径未被 RVV 主路径误覆盖。
