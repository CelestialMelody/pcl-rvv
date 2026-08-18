# filters/approximate_voxel_grid 函数级 RVV 评估

## 1. 主题与入口

- 主题：`approximate_voxel_grid`
- 主文件：`filters/include/pcl/filters/impl/approximate_voxel_grid.hpp`
- 公开类：`pcl::ApproximateVoxelGrid<PointT>`
- 专项目录：`test-rvv/filters/approximate_voxel_grid/`
- 模块依据：`doc-rvv/library-screening/filters/filters-second-pass-retained-candidate-rescreen.zh.md` 的 `6.2 bench-only / 诊断主题` 第一项。

`ApproximateVoxelGrid::applyFilter(PointCloud&)` 用固定大小的 history hash 表近似聚合相邻点。每个输入点先检查 `isXYZFinite`，再计算：

```text
ix = floor(point.x * inverse_leaf_size[0])
iy = floor(point.y * inverse_leaf_size[1])
iz = floor(point.z * inverse_leaf_size[2])
hash = (ix * 7171 + iy * 3079 + iz * 4231) & (histsize - 1)
```

随后处理 hash bucket 冲突、冲突时 `flush`、末尾 flush 和 centroid 累加。生产泛型路径还包含 `Eigen::VectorXf scratch`、`FieldList` 字段拷贝、`downsample_all_data_` 和 RGB/RGBA 特例。

本轮先做 leaf-id/hash 与 full `PointXYZ` 诊断，确认 bucket/flush/centroid 没有完全抵消 RVV 收益；随后把 leaf/hash helper 泛化为 `PointT` 模板，并将生产 `filter(output)` 接入两类路径：

- `PointT = pcl::PointXYZ`：RVV leaf/hash + `PointXYZHistoryEntry` 轻量快路径；
- 其它满足 `pcl::rvv::kRVVXYZPointCompatible<PointT>` 的点类型：RVV leaf/hash + 原泛型 `history_` / `FieldList` / RGB/RGBA 标量聚合路径。

公开 API 不变。非 RVV 编译、小规模、非法 `histsize_` 和 XYZ layout gate 失败回退原标量实现。

需要注意：`PointXYZ` 是当前 RVV 生产 helper 的窄输出语义边界，不是
`ApproximateVoxelGrid` 的算法使用边界。源码中 `filters/src/approximate_voxel_grid.cpp`
用 `PCL_XYZ_POINT_TYPES` 预编译该类，仓库调用点还能看到
`ApproximateVoxelGrid<PointXYZI>`、`ApproximateVoxelGrid<PointXYZRGBA>` 和
`ApproximateVoxelGrid<PointXYZRGBNormal>`。这些点类型的 `FieldList`、intensity、
normal、curvature 或 RGB/RGBA packing 由标量 `FieldList` / `flush` 语义处理；
`PointXYZHistoryEntry` 只保存 `sx/sy/sz/count`，不能直接作为泛型生产路径。

## 2. 函数级评估

| 函数 / 片段 | 优先级 | RVV 决策 | 覆盖 / 回退 |
| --- | --- | --- | --- |
| `applyFilter` 的 `PointXYZ` finite + leaf-id/hash | 生产主路径 | 已接入 RVV | 覆盖 `PointT = pcl::PointXYZ`、`n>=64`、`histsize_` 为 2 的幂；否则回退 Std |
| `PointXYZ` history bucket / 冲突 `flush` / centroid 累加 | 生产主路径 | 保持标量状态机，纳入 RVV 快路径 | RVV 只预计算同序 leaf/hash；bucket 冲突、flush 顺序和 `x/y/z/count` 累加与标量语义一致 |
| XYZ-compatible generic leaf/hash | 生产主路径 | 已接入 RVV | 覆盖 `pcl::rvv::kRVVXYZPointCompatible<PointT>`、`n>=64`、`histsize_` 为 2 的幂 |
| `scratch` / 泛型字段累加 / RGB 特例 | 生产主路径 | 复用标量语义 | generic staged 路径只替换 leaf/hash；泛型 `FieldList`、动态 `Eigen::VectorXf`、`downsample_all_data_` 和 RGB/RGBA packing 不 RVV 化 |
| 小规模 / gate 失败 / 非 RVV 编译 | fallback | 调用 `applyFilterStd` | 避免小输入和不满足 XYZ member layout 的类型进入生产 RVV |
| leaf/hash 与 full `PointXYZ` microbench | 诊断证据 | 保留在 `test-rvv` | 证明前置 RVV、bucket/flush/centroid 稀释和生产接入收益来源 |

本轮已经采用“RVV leaf/hash staging + 标量泛型字段聚合”的混合路径。RVV 先为
XYZ-compatible `PointT` 产出保序的 `ix/iy/iz/hash/source_index`，后续 bucket 冲突、
`FieldList` 累加、RGB/RGBA packing 和 flush 继续复用标量语义。

`fast_bilateral` 只有 `1.04x` 到 `1.10x` 仍接入，是因为其生产主路径正确且无退化；`shadowpoints` 主诊断为 `0.38x` 到 `0.79x`，因此回退。本主题板卡 production case 中 `PointXYZ` 为 `1.91x`，`PointXYZI` 为 `1.21x`，`PointXYZRGB` 为 `1.14x`，`PointXYZRGBA` 为 `1.12x`，符合接入标准。

## 3. RVV 设计

上游生产源码新增：

- `applyFilterStd`：原 `applyFilter` 标量实现常驻保留；
- `applyFilterPointXYZRVV`：`__RVV10__` 下的 `PointXYZ` 主路径 helper；
- `applyFilterXYZStagedRVV`：`__RVV10__` 下的 generic staged 生产 helper；
- `pcl::approximate_voxel_grid_rvv::computeXYZLeafHashes<PointT>`：复用 `pcl/rvv_point_load.h` 的 xyz stride-load wrapper，按 VL chunk 计算 finite mask、floor、hash 和 source index；
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
- `LeafHashGateCoversGenericXYZPointTypes`：小规模 fallback 与 `PointXYZI` leaf/hash gate 覆盖；
- `ProductionApproximateVoxelGridStillRuns`：生产 `ApproximateVoxelGrid<PointXYZ>` 输出与标量诊断逐点对拍；
- `ProductionGenericPointXYZIMatchesScalar`：生产 `PointXYZI` generic staged 路径与强制 `applyFilterStd` 对拍；
- `ProductionGenericPointXYZIWithoutAllDataMatchesScalar`：`PointXYZI` 在 `downsample_all_data=false` 下与强制标量对拍；
- `ProductionGenericPointXYZRGBMatchesScalar`：生产 `PointXYZRGB` generic staged 路径与强制标量 packed RGB 输出对拍；
- `ProductionGenericPointXYZRGBAMatchesScalar`：生产 `PointXYZRGBA` generic staged 路径与强制标量 packed RGBA/RGB 输出对拍。

专项 bench：

- `approx voxel leaf-hash diag 64K`：只测前置 leaf-id/hash；
- `approx voxel leaf-hash diag 1M`：同上，放大规模；
- `approx voxel finite leaf-hash diag 1M`：含 NaN / Inf，验证 finite mask；
- `approx voxel full diag 64K`：RVV leaf/hash 后执行同一 scalar history bucket、冲突 flush 和 centroid 累加；
- `approx voxel full diag 1M`：同上，放大规模；
- `approx voxel finite full diag 1M`：含 NaN / Inf 的 full `PointXYZ` 诊断；
- `approx voxel production pointxyz 64K`：调用生产 `ApproximateVoxelGrid<PointXYZ>::filter(output)`，验证生产分流收益。
- `approx voxel production pointxyzi 64K`：调用生产 `ApproximateVoxelGrid<PointXYZI>::filter(output)`，验证 generic staged + intensity 标量聚合；
- `approx voxel production pointxyzrgb 64K`：调用生产 `ApproximateVoxelGrid<PointXYZRGB>::filter(output)`，验证 generic staged + RGB 标量聚合；
- `approx voxel production pointxyzrgba 64K`：调用生产 `ApproximateVoxelGrid<PointXYZRGBA>::filter(output)`，验证 generic staged + RGBA/RGB 标量聚合。

bench 输出包含 `Dataset:`、`Iterations:`、`Build:`、每个 case 一行 avg ms/iter，以及 `Total Time` 和 checksum。

上游原始测试：仓库没有直接针对 `ApproximateVoxelGrid` 的 filters 单测；`benchmarks/filters/voxel_grid.cpp` 包含 benchmark 入口但不是 gtest。当前以专项测试覆盖生产 `PointXYZ` 快路径、generic staged 路径与 fallback 边界。

## 5. 验证结果

- QEMU 专项测试：`make -C test-rvv/filters/approximate_voxel_grid run_test_compare` 通过，std/RVV 两套构建均通过 10 个专项测试。
- QEMU bench：`make -C test-rvv/filters/approximate_voxel_grid run_bench_compare` 通过，`output/qemu/analyze_bench_compare.log` 无 `未解析`、`n/a`、`Total Time 不计算`；QEMU 只作为构建、格式、checksum 和指令路径证据。
- 反汇编：`make -C test-rvv/filters/approximate_voxel_grid dump_bench_rvv` 生成 `build/asm/riscv/bench_approximate_voxel_grid_rvv.full.asm`；`output/qemu/rvv_asm_check.log` 确认生产与诊断路径中出现 `vlsseg3e32.v`、`vfcvt.rtz.x.f.v`、`vcompress.vm`、`vcpop.m`、`vsetvli ... e32,m2`。完整二进制中的 `frrm/fsrm` 来自 bench checksum 的 `std::lround` 路径，不属于 leaf-hash RVV helper。
- 板卡验证：`make -C test-rvv/filters/approximate_voxel_grid run_board_test run_board_bench_compare fetch_board_logs` 通过，日志在 `output/board/`。

Milkv-Jupiter 结果：

| case | Std ms/iter | RVV ms/iter | speedup | 结论 |
| --- | ---: | ---: | ---: | --- |
| `approx voxel leaf-hash diag 64K` | 7.7217 | 3.9523 | 1.95x | 前置 leaf-id/hash 片段在板卡上有收益 |
| `approx voxel leaf-hash diag 1M` | 122.6544 | 63.7183 | 1.92x | 大规模前置片段收益稳定 |
| `approx voxel finite leaf-hash diag 1M` | 122.0924 | 64.0667 | 1.91x | finite mask 与 hash 一起仍有收益 |
| `approx voxel full diag 64K` | 8.6851 | 5.3418 | 1.63x | `PointXYZ` bucket、flush 和 centroid 累加未抵消 RVV leaf/hash 收益 |
| `approx voxel full diag 1M` | 137.1481 | 83.7587 | 1.64x | 大规模 full `PointXYZ` 诊断仍有整体收益 |
| `approx voxel finite full diag 1M` | 136.6427 | 84.1993 | 1.62x | invalid 跳过、bucket 和 flush 一起纳入后仍成立 |
| `approx voxel production pointxyz 64K` | 15.2353 | 7.9834 | 1.91x | 生产 `PointXYZ` 快路径收益成立 |
| `approx voxel production pointxyzi 64K` | 17.2405 | 14.2621 | 1.21x | generic staged + intensity 标量聚合有收益 |
| `approx voxel production pointxyzrgb 64K` | 20.3031 | 17.8527 | 1.14x | generic staged + RGB 标量聚合有收益 |
| `approx voxel production pointxyzrgba 64K` | 20.4090 | 18.2458 | 1.12x | generic staged + RGBA/RGB 标量聚合有收益 |

## 6. 结论

`ApproximateVoxelGrid<PointXYZ>::filter(output)` 生产快路径已接入 RVV；其它满足
`pcl::rvv::kRVVXYZPointCompatible<PointT>` 的点类型已接入 generic staged 生产路径。
板卡结果显示 leaf/hash 片段约 `1.91x` 到 `1.95x`，full `PointXYZ` 诊断约
`1.62x` 到 `1.64x`，生产 `PointXYZ` case 为 `1.91x`，generic production case 为
`1.12x` 到 `1.21x`。这说明 bucket/flush/centroid 和泛型字段标量聚合没有抵消前置
RVV 收益。

生产覆盖边界：点数 `>=64`、`histsize_` 为 2 的幂、RVV 编译且
`PointT` 满足 `pcl::rvv::kRVVXYZPointCompatible<PointT>`。不满足时回退
`applyFilterStd`。本轮没有把 intensity、normal、curvature 或 RGB/RGBA packing 本身
RVV 化。
