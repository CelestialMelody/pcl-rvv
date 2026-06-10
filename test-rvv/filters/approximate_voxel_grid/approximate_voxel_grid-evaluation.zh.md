# filters/approximate_voxel_grid 函数级 RVV 评估

## 1. 主题与入口

- 主题：`approximate_voxel_grid`
- 主文件：`filters/include/pcl/filters/impl/approximate_voxel_grid.hpp`
- 公开类：`pcl::ApproximateVoxelGrid<PointT>`
- 专项目录：`test-rvv/filters/approximate_voxel_grid/`
- 模块依据：`doc-rvv/library-screening/filters/filters-module-followup-rescreen.zh.md` 的 `6.2 bench-only / 诊断主题` 第一项。

`ApproximateVoxelGrid::applyFilter(PointCloud&)` 用固定大小的 history hash 表近似聚合相邻点。每个输入点先检查 `isXYZFinite`，再计算：

```text
ix = floor(point.x * inverse_leaf_size[0])
iy = floor(point.y * inverse_leaf_size[1])
iz = floor(point.z * inverse_leaf_size[2])
hash = (ix * 7171 + iy * 3079 + iz * 4231) & (histsize - 1)
```

随后处理 hash bucket 冲突、冲突时 `flush`、末尾 flush 和 centroid 累加。生产泛型路径还包含 `Eigen::VectorXf scratch`、`FieldList` 字段拷贝、`downsample_all_data_` 和 RGB/RGBA 特例。

本轮先做 leaf-id/hash 与 full `PointXYZ` 诊断，确认 bucket/flush/centroid 没有完全抵消 RVV 收益后，将 `PointT = pcl::PointXYZ`、全云线性扫描、`histsize_` 为 2 的幂、点数 `>=64` 的生产 `filter(output)` 主路径接入 RVV。公开 API 不变，非 RVV 编译、小规模、非 `PointXYZ` 和非法 `histsize_` 回退原标量实现。

## 2. 函数级评估

| 函数 / 片段 | 优先级 | RVV 决策 | 覆盖 / 回退 |
| --- | --- | --- | --- |
| `applyFilter` 的 `PointXYZ` finite + leaf-id/hash | 生产主路径 | 已接入 RVV | 覆盖 `PointT = pcl::PointXYZ`、`n>=64`、`histsize_` 为 2 的幂；否则回退 Std |
| `PointXYZ` history bucket / 冲突 `flush` / centroid 累加 | 生产主路径 | 保持标量状态机，纳入 RVV 主路径 | RVV 只预计算同序 leaf/hash；bucket 冲突、flush 顺序和 `x/y/z/count` 累加与标量语义一致 |
| `scratch` / 泛型字段累加 / RGB 特例 | 暂缓 | 保持生产标量 | 非 `PointXYZ` 仍走原 `applyFilterStd`；泛型 `FieldList`、动态 `Eigen::VectorXf`、`downsample_all_data_` 和 RGB/RGBA 不套用本 helper |
| 小规模 / 非 `PointXYZ` / 非 RVV 编译 | fallback | 调用 `applyFilterStd` | 避免小输入和泛型字段路径进入生产 RVV；诊断 helper 的 xyz-compatible 测试只用于片段上界，不代表生产覆盖 |
| leaf/hash 与 full `PointXYZ` microbench | 诊断证据 | 保留在 `test-rvv` | 证明前置 RVV、bucket/flush/centroid 稀释和生产接入收益来源 |

`fast_bilateral` 只有 `1.04x` 到 `1.10x` 仍接入，是因为其生产主路径正确且无退化；`shadowpoints` 主诊断为 `0.38x` 到 `0.79x`，因此回退。本主题 full `PointXYZ` 诊断为 `1.67x` 到 `1.68x`，生产 `PointXYZ` case 为 `1.92x`，符合接入标准。

## 3. RVV 设计

上游生产源码新增：

- `applyFilterStd`：原 `applyFilter` 标量实现常驻保留；
- `applyFilterPointXYZRVV`：`__RVV10__` 下的 `PointXYZ` 主路径 helper；
- `pcl::approximate_voxel_grid_rvv::computePointXYZLeafHashes`：复用 `pcl/common/rvv_point_load.h` 的 xyz stride-load wrapper，按 VL chunk 计算 finite mask、floor、hash 和 source index；
- `PointXYZHistoryEntry` / `flushPointXYZHistoryEntry`：只服务 `PointXYZ` centroid-only 主路径，不处理泛型字段和 RGB。

RVV helper 在一个 VL chunk 中加载 AoS `PointXYZ::x/y/z`，生成三路 finite mask，计算 `floor(xyz * inverse_leaf_size_)` 和 hash，再用 `vcompress` 保序写出有效 lane 的 `ix/iy/iz/hash/source_index`。随后 `applyFilterPointXYZRVV` 按同一顺序执行标量 history 状态机：同一 bucket 遇到不同 `(ix,iy,iz)` 时先 flush 旧 centroid，再累加当前点，末尾按 history 顺序 flush 剩余 entry。

`floor` 语义没有使用 `_rm` intrinsic，也不修改 FRM/FCSR。实现采用 `vfcvt.rtz.x.f.v` 先向 0 截断，再对负数非整数 lane 减 1：

```text
trunc = trunc_toward_zero(scaled)
floor = trunc - (scaled < float(trunc) ? 1 : 0)
```

该等价式覆盖负坐标，例如 `-0.02 * 4 = -0.08`，截断为 `0`，校正后为 `-1`，与 `std::floor` 一致。

## 4. 测试与 bench

专项测试：

- `ScalarFormulaCoversNegativeFloorAndHash`：手算负数 floor 与 hash；
- `RVVLeafHashMatchesScalar`：1024 点 leaf-hash 对拍；
- `RVVFiniteMaskSkipsInvalidXYZLikeScalar`：NaN / Inf 跳过；
- `FullPointXYZDiagnosticMatchesScalar`：RVV leaf/hash + 标量 history/flush/centroid 与纯标量 full `PointXYZ` 诊断对拍；
- `FallbackCasesRemainScalar`：小规模 fallback 与 xyz-compatible 诊断限定；生产分流另由 `PointXYZ` 入口测试覆盖；
- `ProductionApproximateVoxelGridStillRuns`：生产 `ApproximateVoxelGrid<PointXYZ>` 输出与标量诊断逐点对拍。

专项 bench：

- `approx voxel leaf-hash diag 64K`：只测前置 leaf-id/hash；
- `approx voxel leaf-hash diag 1M`：同上，放大规模；
- `approx voxel finite leaf-hash diag 1M`：含 NaN / Inf，验证 finite mask；
- `approx voxel full diag 64K`：RVV leaf/hash 后执行同一 scalar history bucket、冲突 flush 和 centroid 累加；
- `approx voxel full diag 1M`：同上，放大规模；
- `approx voxel finite full diag 1M`：含 NaN / Inf 的 full `PointXYZ` 诊断；
- `approx voxel production pointxyz 64K`：调用生产 `ApproximateVoxelGrid<PointXYZ>::filter(output)`，验证生产分流收益。

bench 输出包含 `Dataset:`、`Iterations:`、`Build:`、每个 case 一行 avg ms/iter，以及 `Total Time` 和 checksum。

上游原始测试：仓库没有直接针对 `ApproximateVoxelGrid` 的 filters 单测；`benchmarks/filters/voxel_grid.cpp` 包含 benchmark 入口但不是 gtest。当前以专项测试覆盖生产 `PointXYZ` 主路径与 fallback 边界。

## 5. 验证结果

- QEMU 专项测试：`make -C test-rvv/filters/approximate_voxel_grid run_test_compare` 通过，std/RVV 两套构建均通过 6 个专项测试。
- QEMU bench：`make -C test-rvv/filters/approximate_voxel_grid run_bench_compare` 通过，`output/qemu/analyze_bench_compare.log` 无 `未解析`、`n/a`、`Total Time 不计算`；QEMU 只作为构建、格式、checksum 和指令路径证据。
- 反汇编：`make -C test-rvv/filters/approximate_voxel_grid dump_bench_rvv` 生成 `build/asm/riscv/bench_approximate_voxel_grid_rvv.full.asm`；`output/qemu/rvv_asm_check.log` 确认生产与诊断路径中出现 `vlsseg3e32.v`、`vfcvt.rtz.x.f.v`、`vcompress.vm`、`vcpop.m`、`vsetvli ... e32,m2`。完整二进制中的 `frrm/fsrm` 来自 bench checksum 的 `std::lround` 路径，不属于 leaf-hash RVV helper。
- 板卡验证：`make -C test-rvv/filters/approximate_voxel_grid run_board_test run_board_bench_compare fetch_board_logs` 通过，日志在 `output/board/`。

Milkv-Jupiter 结果：

| case | Std ms/iter | RVV ms/iter | speedup | 结论 |
| --- | ---: | ---: | ---: | --- |
| `approx voxel leaf-hash diag 64K` | 7.8790 | 3.9333 | 2.00x | 前置 leaf-id/hash 片段在板卡上有收益 |
| `approx voxel leaf-hash diag 1M` | 125.9917 | 63.1650 | 1.99x | 大规模前置片段收益稳定 |
| `approx voxel finite leaf-hash diag 1M` | 125.7265 | 63.5819 | 1.98x | finite mask 与 hash 一起仍有收益 |
| `approx voxel full diag 64K` | 9.0551 | 5.3965 | 1.68x | `PointXYZ` bucket、flush 和 centroid 累加未抵消 RVV leaf/hash 收益 |
| `approx voxel full diag 1M` | 142.7057 | 85.1374 | 1.68x | 大规模 full `PointXYZ` 诊断仍有整体收益 |
| `approx voxel finite full diag 1M` | 142.5058 | 85.5158 | 1.67x | invalid 跳过、bucket 和 flush 一起纳入后仍成立 |
| `approx voxel production pointxyz 64K` | 15.3305 | 7.9986 | 1.92x | 生产 `ApproximateVoxelGrid<PointXYZ>::filter(output)` 主路径收益成立 |

## 6. 结论

`ApproximateVoxelGrid<PointXYZ>::filter(output)` 生产主路径已接入 RVV。板卡结果显示 leaf/hash 片段约 `1.98x` 到 `2.00x`，full `PointXYZ` 诊断约 `1.67x` 到 `1.68x`，生产 `PointXYZ` case 为 `1.92x`。这说明 bucket/flush/centroid 没有抵消前置 RVV 收益，且真实生产入口收益强于此前已接入的弱收益 `fast_bilateral`。

生产覆盖仍限定为 `PointT = pcl::PointXYZ`、点数 `>=64`、`histsize_` 为 2 的幂。非 `PointXYZ`、小规模、非 RVV 编译、泛型字段、`downsample_all_data_` 和 RGB/RGBA 特例保持原 `applyFilterStd` 标量语义。
