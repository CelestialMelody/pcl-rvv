# filters `filter.hpp` / `filter_indices.hpp`：函数级梳理、筛选评估与 RVV closeout

本文档记录 filters 通用 NaN / normal 清理主题的函数级筛选、实现范围和验证结论。该主题来自 `doc-rvv/library-screening/filters/filters-function-evaluation-queue.zh.md` 的第 3 个执行项，没有重新做模块级候选选择。

## 1. 函数 / 函数组梳理

| 函数 / 函数组 | 所在文件 | 功能概要 | 当前状态 |
| --- | --- | --- | --- |
| `removeNaNFromPointCloud(cloud_in, index)` | `filters/include/pcl/filters/impl/filter_indices.hpp` | 只输出 xyz finite 点的原始下标 | 已实现 RVV：non-dense、标准 `float x/y/z`、大规模输入 |
| `removeNaNFromPointCloud(cloud_in, cloud_out, index)` | `filters/include/pcl/filters/impl/filter.hpp` | 移除 xyz 非 finite 点，同时输出压缩点云和原始下标 | 已实现 RVV：non-dense、标准 `float x/y/z`、大规模输入；点对象 copy 保持标量 |
| `removeNaNNormalsFromPointCloud(cloud_in, cloud_out, index)` | `filters/include/pcl/filters/impl/filter.hpp` | 移除 normal 非 finite 点，同时维护 `cloud_out.is_dense` | 已尝试 RVV mask；板卡退化，公开入口回退 Std |
| `FilterIndices<PointT>::applyFilter(output)` | `filters/include/pcl/filters/impl/filter_indices.hpp` | organized / copyPointCloud wrapper | 不直接 RVV，保持包装语义 |

## 2. 覆盖条件与 fallback

| 路径 | RVV 覆盖条件 | fallback 条件 | 风险控制 |
| --- | --- | --- | --- |
| indices-only xyz | `__RVV10__`、`PointT` 标准布局、`x/y/z` 为 `float`、`cloud_in.is_dense == false`、`size >= 64`、点数可放入 `int` indices | dense、小规模、非标准 xyz、非 float 字段、超大点数 | dense 路径仍用标量写 `0..N-1`；`vcompress` 保持顺序 |
| cloud-out xyz | 同上 | dense、小规模、非标准 xyz、非 float 字段、超大点数 | RVV 只生成 finite mask 和压缩 indices，`PointT` 复制保持标量，in-place 由先写 indices 再按输出位置顺序 copy 保证 |
| normals | 标准 `normal_x/y/z` 和 `x/y/z` float 曾进入 RVV 尝试 | 当前公开入口始终 Std | 板卡 1M normal case RVV prototype 为 `0.62x`，主要成本在 `PointT` copy 和额外 xyz dense 检查，故回退维护性更好 |

finite 判定等价于标量 `std::isfinite`：

- NaN：`v == v` 为 false；
- `+Inf/-Inf`：`abs(v) < inf` 为 false；
- 三个字段 mask 逐项 `vmand`。

## 3. 数据布局与 RVV 组织

`PointCloud<PointT>` 是 AoS。RVV helper 使用 `vlse32` 按 `sizeof(PointT)` stride 读取 `x/y/z` 或 normal 字段，每个 VL chunk 生成 finite mask：

```text
输入点:      p[i+0] p[i+1] ... p[i+vl-1]
字段读取:    x[]    y[]        z[]       (AoS stride load)
finite mask: m[] = finite(x) & finite(y) & finite(z)
indices:     source[] = i + [0, 1, ..., vl-1]
压缩输出:    vcompress(source, m) -> index[j ... j+popcount(m)-1]
```

cloud-out 路径不对整个 `PointT` 做 RVV scatter。PCL 点类型可能包含 rgb、label、normal、curvature 等非 float 或 padding 字段；为了保持字段拷贝、顺序和 in-place 语义，RVV 只负责 mask 和 ordered indices，随后按压缩后的 indices 标量复制点对象。

## 4. 测试与 bench

专项目录：`test-rvv/filters/filter_indices/`

| 项目 | 状态 |
| --- | --- |
| 专项测试 | `make -C test-rvv/filters/filter_indices run_test_compare` 通过；std/RVV 各 7 个用例 |
| QEMU bench | `make -C test-rvv/filters/filter_indices run_bench_compare` 通过；输出包含 `Dataset:` / `Iterations:`，分析日志无 `未解析`、`n/a`、`Total Time 不计算` |
| 反汇编 | `make -C test-rvv/filters/filter_indices dump_bench_rvv` 生成 `build/asm/riscv/bench_filter_indices_rvv.full.asm`，确认 `vlse32.v`、`vmfeq.vv`、`vmflt.vf`、`vmand.mm`、`vcompress.vm`、`vcpop.m`、`vsetvli` |
| 上游测试 | `run_upstream_test_compare` 已补齐 `pcl_io/search/kdtree/flann/lz4/hdf5/zlib/png` 等依赖；std/RVV 均构建并运行到外部 PCD 数据提示：需要 `bun0.pcd` 和 `milk_cartoon_all_small_clorox.pcd` 参数 |
| 板卡测试 | `make -C test-rvv/filters/filter_indices run_board_test` 通过，板卡 7 个专项用例通过 |
| 板卡 bench | `make -C test-rvv/filters/filter_indices run_board_bench_compare fetch_board_logs` 完成，日志在 `test-rvv/filters/filter_indices/output/board/` |

## 5. 板卡性能结论

板卡日志：`test-rvv/filters/filter_indices/output/board/analyze_bench_compare.log`，设备标签 `Milkv-Jupiter`，每 case `Iterations: 30`。

| Case | Std ms/iter | RVV ms/iter | Speedup | 结论 |
| --- | ---: | ---: | ---: | --- |
| `removeNaN indices-only sparse xyz 64K` | 1.9330 | 0.8212 | 2.35x | 保留 RVV |
| `removeNaN indices-only sparse xyz 1M` | 30.6799 | 13.9112 | 2.21x | 保留 RVV |
| `removeNaN indices-only dense xyz 1M` | 11.5556 | 11.4960 | 1.01x | dense fallback 等价，非目标收益 |
| `removeNaN cloud-out sparse xyz 1M` | 35.8324 | 22.0811 | 1.62x | 保留 RVV mask + 标量点拷贝 |
| `removeNaNNormals cloud-out sparse normal 1M` | 71.5442 | 71.7044 | 1.00x | 公开入口已回退 Std |

QEMU bench 仅作为构建、正确性、日志格式和指令路径证据，不作为性能结论。

## 6. 后续限制

- 未覆盖 dense indices 填充的 RVV；该路径主要顺序写整数，收益不明确。
- 未覆盖非标准或非 float xyz/normal 字段。
- 未直接 RVV 化 `FilterIndices::applyFilter(output)`，该 wrapper 主要负责 organized 输出和 `copyPointCloud`。
- normals RVV mask prototype 已评估但不启用；若未来能批量复制标准 normal 点类型或减少 dense 检查成本，可重新评估。
