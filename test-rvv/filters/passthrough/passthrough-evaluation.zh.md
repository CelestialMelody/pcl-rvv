# filters/passthrough 函数级 RVV 评估

## 范围

- 主题：`passthrough`
- 主要源码：
  - `filters/include/pcl/filters/passthrough.h`
  - `filters/include/pcl/filters/impl/passthrough.hpp`
  - `filters/src/passthrough.cpp`
- 专项目录：`test-rvv/filters/passthrough/`
- 当前结论：已实现 `PassThrough<PointT>::applyFilterIndices` 的 `PointXYZI` 等标准 float xyz 点类型、identity indices、FLOAT32 字段区间过滤 RVV 路径；其它路径保持标量。

## 函数族评估

| 函数族 | 当前状态 | RVV 适配度 | 覆盖 / 回退 |
| --- | --- | --- | --- |
| `PassThrough<PointT>::applyFilterIndices`，`filter_field_name_` 非空 | 已实现 RVV | 中高。线性扫描、字段区间判断和输出 indices 顺序压缩适合 mask + `vcompress` | 覆盖标准布局、`x/y/z` 为 `float`、过滤字段为 `FLOAT32`、`fake_indices_` identity、大规模输入；小规模、显式 subset indices、非标准点类型、字段缺失 / 非 FLOAT32 回退原标量 |
| `PassThrough<PointT>::applyFilterIndices`，`filter_field_name_` 为空 | 保持标量 | 中。仅 xyz finite 过滤，与 `filter_indices` 主题已覆盖的 removeNaN 类似 | 本主题不重复覆盖；保持原标量 |
| `PassThrough<PointT>::filter(PointCloud&)` | 间接受益 | 中。基类先调用 indices 过滤，再 `copyPointCloud` | 当内部 `applyFilterIndices` 命中 RVV 时受益；点拷贝仍保持原路径 |
| `PassThrough<pcl::PCLPointCloud2>::applyFilter(Indices&)` | 暂缓 | 中。PCLPointCloud2 字节步进可 RVV 化，但公开 `src/passthrough.cpp` 非模板路径和 indices 初始化语义需单独处理 | 暂缓，避免把本主题一次性扩大到 PCLPointCloud2 专项 |
| `PassThrough<pcl::PCLPointCloud2>::applyFilter(PCLPointCloud2&)` | 暂缓 | 中低。区间判断可向量化，但输出整点 `memcpy`、`keep_organized_`、xyz 写坏点和 removed indices 组合复杂 | 暂缓，当前验证成本和结构侵入高于收益确定性 |

## 输入输出边界

- 输入：`pcl::PointCloud<PointT>`，由 `PassThrough<PointT>` 的 `input_` 和 `indices_` 管理。
- 输出：`Indices &indices`，顺序保持为标量扫描顺序；可选 `removed_indices_` 同样保持扫描顺序。
- 区间语义：
  - `negative_ == false`：保留 `field_value >= min && field_value <= max`。
  - `negative_ == true`：保留区间外点。
  - `x/y/z` 或过滤字段非有限时总是 removed，不会因为 `negative_` 变为 inlier。
- 字段：只覆盖 `pcl::getFieldIndex<PointT>` 返回 `FLOAT32` 的字段，例如 `PointXYZI::intensity`。

## 数据布局

`PointCloud<PointT>` 是 AoS。RVV 路径用 `sizeof(PointT)` 作为 stride，分别从 `offsetof(PointT, x/y/z)` 和字段 offset 做 `vlse32`：

```text
chunk base -> p[i]     p[i+1]   p[i+2] ...
             x y z f   x y z f  x y z f
RVV load:    vlse32(x), vlse32(y), vlse32(z), vlse32(f)
mask:        finite(x,y,z,f) & range(f)
output:      vcompress(source_index, keep_mask)
```

## RVV 覆盖条件

当前 RVV helper `applyFilterIndicesRVV` 只在以下条件全部满足时返回 true：

- `__RVV10__` 编译；
- `PointT` 为标准布局，且存在 `float x/y/z`；
- `filter_field_name_` 非空，字段存在且 datatype 为 `FLOAT32`；
- `fake_indices_ == true`，也就是输入 indices 是 PCLBase 生成的 identity 全量索引；
- `indices_->size() >= 64`；
- 点数不超过 `int` 可表达范围；
- 字段 offset 至少满足 float 对齐；
- `input_` 有效。

## 回退条件

- 非 RVV 编译；
- 小规模输入；
- 显式 `setIndices()` 的 subset / gather 路径；
- 非标准布局或 xyz 不是 `float` 的点类型；
- 字段名为空；
- 字段不存在、字段非 `FLOAT32`；
- `PCLPointCloud2` 特化；
- 任一运行时条件不满足。

回退后调用常驻 `applyFilterIndicesStd`，保持原标量代码语义和警告 / 错误行为。

## 风险点

- `negative_` 语义必须和标量一致：非有限字段仍 removed，不能被“区间外”逻辑保留。
- `removed_indices_` 的顺序必须和标量一致；RVV 用 drop mask 的 `vcompress` 写出 identity source index。
- AoS stride load 对 QEMU 和真实硬件的收益不确定，真实性能必须以板卡为准。
- 显式 subset indices 若强行 RVV 需要 gather，且 removed/inlier 顺序验证成本更高，当前回退。
- `PCLPointCloud2` 输出 cloud 路径涉及整点复制和 `keep_organized_` 写坏 xyz，暂缓。

## 专项测试计划与结果

专项测试文件：`test-rvv/filters/passthrough/test_passthrough.cpp`。

覆盖：

- 小规模标量 fallback；
- 大规模 identity indices 正向区间；
- 大规模 identity indices `negative_`；
- `extract_removed_indices_` 顺序；
- 显式 subset indices fallback；
- 字段为空时只过滤 finite xyz；
- cloud 输出经基类 `FilterIndices` 压缩。

已运行：

```text
make -C test-rvv/filters/passthrough run_test_compare
```

结果：std 与 RVV 二进制均通过 6 个专项用例。

## bench 与反汇编

专项 bench 文件：`test-rvv/filters/passthrough/bench_passthrough.cpp`。

bench 输出包含 `Dataset:`、`Iterations:`、`Build:`，每个 case 输出 `<name> : <avg> ms/iter` 和 `Total Time`。

已运行：

```text
make -C test-rvv/filters/passthrough run_bench_compare dump_bench_rvv
```

`output/qemu/analyze_bench_compare.log` 已解析 Dataset / Iterations / Total Time，无 `未解析`、`n/a` 或 `Total Time 不计算`。QEMU 结果只作为构建、运行、日志格式和指令路径证据，不作为性能结论。

反汇编文件：

- `test-rvv/filters/passthrough/build/asm/riscv/bench_passthrough_rvv.full.asm`

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

- 上游 `test/filters/test_filters.cpp` 已通过专项 Makefile 补齐 `pcl_filters`、`pcl_io`、`pcl_sample_consensus`、`pcl_search`、`pcl_kdtree`、`pcl_octree`、`flann`、`boost`、`hdf5`、`lz4`、`zlib`、`png` 等依赖。
- 上游 `test_filters.cpp` 需要 `bun0.pcd` 和 `milk_cartoon_all_small_clorox.pcd` 两个运行参数；专项 Makefile 通过 `UPSTREAM_TEST_ARGS` 指向仓库 `test/` 中已有数据文件，不复制 PCD 到专项目录。
- `make -C test-rvv/filters/passthrough run_upstream_test` 已通过上游 filters 全量 21 个测试；日志写入 `test-rvv/filters/passthrough/output/qemu/run_upstream_test.log`。
- 板卡入口已提供 `deploy_board`、`run_board_test`、`run_board_bench_compare`、`fetch_board_logs`、`board_smoke`。
- `Makefile` 已使用 `SSH_OPTS ?= -F $(HOME)/.ssh/config`，绕过 sandbox 中 owner 异常的系统 ssh config，同时保留用户配置中的 `IdentityFile`。
- `make -C test-rvv/filters/passthrough run_board_test` 通过，板卡 6 个专项用例通过。
- `make -C test-rvv/filters/passthrough run_board_bench_compare fetch_board_logs` 完成，结果已拉回 `test-rvv/filters/passthrough/output/board/`。
- 真实板卡性能结论：
  - `passthrough indices range 64K`：`3.21x`；
  - `passthrough indices range 1M`：`2.68x`；
  - `passthrough indices negative 1M`：`2.93x`；
  - `passthrough indices removed 1M invalid`：`2.60x`；
  - `passthrough cloud-out range 1M`：`1.63x`；
  - `passthrough explicit subset fallback 1M`：`0.99x`，该 case 是显式 subset indices fallback，用于证明未覆盖路径保持接近 Std。
