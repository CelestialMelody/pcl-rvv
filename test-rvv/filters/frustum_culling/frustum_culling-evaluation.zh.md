# filters/frustum_culling 函数级 RVV 评估

## 范围

- 主文件：`filters/include/pcl/filters/impl/frustum_culling.hpp`
- 公开入口：`pcl::FrustumCulling<PointT>::applyFilter(Indices&)`
- 专项目录：`test-rvv/filters/frustum_culling/`
- 模块依据：`doc-rvv/library-screening/filters/filters-second-pass-retained-candidate-rescreen.zh.md` 的后续执行清单第二项。

## 函数级结论

`FrustumCulling` 根据相机 pose、水平 / 垂直 FOV、near/far plane 和 ROI 构造 6 个平面，随后扫描输入点云，把在视锥内的点下标输出到 `indices`。它在 filters 管线中承担“视锥几何筛选 + 保序 indices 输出”的职责；`negative_` 反转 inlier/outlier 语义，`extract_removed_indices_` 额外保存未输出的下标。

标量判定是：

```text
inside(p) =
  dot([x,y,z,1], left)   <= 0 &&
  dot([x,y,z,1], right)  <= 0 &&
  dot([x,y,z,1], top)    <= 0 &&
  dot([x,y,z,1], bottom) <= 0 &&
  dot([x,y,z,1], far)    <= 0 &&
  dot([x,y,z,1], near)   <= 0

keep = inside(p) XOR negative
```

本轮实现覆盖：

- XYZ-compatible 点类型：`pcl::rvv::kRVVXYZPointCompatible<PointT>` 为 true，即
  standard-layout 且有直接 `float x/y/z` 成员；
- `fake_indices_ == true`，即未显式 `setIndices()` 的全云扫描；
- `input_->is_dense == true`；
- 点数 `>= 64` 且不超过 `int` 范围；
- `negative_` 和 `extract_removed_indices_` 两种输出分流。

以下路径保持标量：

- 显式 subset indices：需要先读用户 indices 再 gather 点字段，当前不作为第一主路径；
- non-dense：原实现没有 finite 过滤，但为避免与无效点语义混淆，本轮保守回退；
- 非 XYZ-compatible 点类型：不能证明直接 `float x/y/z` member AoS load；
- 小规模输入：`vsetvl`、mask 合并和 `vcompress` 开销不稳定；
- `keep_organized_` 只影响 `FilterIndices` 的 cloud 输出后处理，当前 RVV 入口仍只优化 `applyFilter(Indices&)`。

## RVV 实现计划

- 常驻 `frustumCullingApplyFilterStd`，保留原标量扫描、`negative_` 和 `removed_indices_` 语义；
- `__RVV10__` 下新增 `frustumCullingApplyFilterRVV`；
- 公开 `applyFilter` 仍负责构造 6 个平面，之后用 `if constexpr (pcl::rvv::kRVVXYZPointCompatible<PointT>)` 短路调用 RVV；
- RVV helper 位于 `pcl` 命名空间，不额外放入 `pcl::detail`；
- RVV 路径按 AoS stride 读取 x/y/z，对 6 个平面分别计算 dot，合并 mask 后用 `vcompress` 保序写出 kept 和 removed indices。

## 风险和处理

| 风险 | 处理 |
| --- | --- |
| far plane infinity 会临时改写 `fp_dist_` | 保持原平面构造逻辑不动，RVV 只接收已构造平面；专项 test 覆盖 `std::numeric_limits<float>::max()` |
| `PointXYZI` 等类型字段兼容但未评估 | 当前实现已切到公共 `kRVVXYZPointCompatible`；`PointXYZI` 会进入 RVV，QEMU 对拍已覆盖 |
| subset gather 收益不确定 | `fake_indices_ == false` 回退 Std，专项 test/bench 覆盖 subset fallback |
| non-dense 与 NaN/Inf | 本轮回退 Std；主路径只覆盖 dense |
| removed_indices 保序 | RVV 对 keep mask 和 removed mask 分别 `vcompress`，顺序与标量扫描一致 |
| 浮点舍入环境 | 不使用 `_rm` intrinsic，不修改 FRM/FCSR |

## 测试计划

专项测试：`test-rvv/filters/frustum_culling/test_frustum_culling.cpp`

- 小规模输入 fallback 与显式 all-indices 标量路径对拍；
- 大规模 dense `PointXYZ` 全云主路径与显式 all-indices 标量路径对拍；
- `negative_` 对拍；
- `extract_removed_indices_` 对拍；
- far plane infinity 对拍；
- non-dense fallback；
- `PointXYZI` XYZ-compatible 路径与显式 all-indices 标量路径一致。

上游测试：`test/filters/test_filters.cpp` 已有 `FrustumCulling.Filters`。专项 Makefile 默认：

```text
UPSTREAM_TEST_ARGS = test/bun0.pcd test/milk_cartoon_all_small_clorox.pcd --gtest_filter=FrustumCulling.Filters
```

不复制 PCD 数据，直接复用仓库 `test/` 下已有文件。

## Bench 计划

专项 bench：`test-rvv/filters/frustum_culling/bench_frustum_culling.cpp`

输出保持可解析：

- `Dataset: synthetic PointXYZ/PointXYZI clouds; full-cloud six-plane frustum RVV cases and subset fallback cases`
- `Iterations: 30`
- 每个 case 一行 `<name> : <avg> ms/iter`，详情行包含 `Total Time` 和 checksum。

case：

| case | 入口 | 规模 / 参数 | 路径含义 |
| --- | --- | --- | --- |
| `frustum_culling pointxyz full-cloud 64K` | `FrustumCulling<PointXYZ>::filter(Indices&)` | 64K dense 全云 | RVV 主路径，中等规模 |
| `frustum_culling pointxyz full-cloud 1M` | 同入口 | 1M dense 全云 | RVV 主路径主性能 case |
| `frustum_culling pointxyz negative removed 1M` | 同入口 | 1M dense，`negative=true`，`extract_removed_indices=true` | RVV 主路径，双输出压缩 |
| `frustum_culling nondense fallback 1M` | 同入口 | 1M `PointXYZ`，`is_dense=false` | fallback 语义 / 成本 |
| `frustum_culling subset fallback 1M` | 同入口 + `setIndices(subset)` | 1M 的一半 subset | fallback 语义 / 成本 |
| `frustum_culling pointxyzi xyz-compatible 1M` | `FrustumCulling<PointXYZI>` | 1M `PointXYZI` | XYZ-compatible RVV 主路径；证明额外字段点类型 checksum 对齐 |

## 当前状态

- 函数级评估：完成。
- RVV 实现：已接入 XYZ-compatible dense 全云主路径；`PointXYZ` 是已量化板卡性能主 case。
- 专项 test / bench / Makefile / board.mk：已建立。
- QEMU 对拍：`make -C test-rvv/filters/frustum_culling run_test_compare` 通过，std/RVV 均通过 7 个专项测试。
- 上游测试：`make -C test-rvv/filters/frustum_culling run_upstream_test_compare` 通过，std/RVV 两套 `test/filters/test_filters.cpp --gtest_filter=FrustumCulling.Filters` 均通过。
- QEMU bench：`make -C test-rvv/filters/frustum_culling run_bench_compare` 通过，std/RVV checksum 对齐；`output/qemu/analyze_bench_compare.log` 无 `未解析`、`n/a`、`Total Time 不计算`。QEMU 主路径 RVV 慢于 Std，只记录为模拟器执行现象，不作为性能结论。
- 反汇编：`make -C test-rvv/filters/frustum_culling dump_bench_rvv` 生成 `build/asm/riscv/bench_frustum_culling_rvv.full.asm`，在 `frustumCullingApplyFilterRVV<PointXYZ>` 中确认 `vlse32.v`、`vfmacc.vf`、`vmfle.vf`、`vcompress.vm`、`vcpop.m`、`vsetvli ... e32,m2`。
- 板卡验证：`make -C test-rvv/filters/frustum_culling run_board_test run_board_bench_compare fetch_board_logs` 通过，日志已拉回 `test-rvv/filters/frustum_culling/output/board/`。

## 板卡结果

设备：`Milkv-Jupiter`。数据集：synthetic `PointXYZ` clouds，full-cloud six-plane frustum RVV cases and subset/type fallback cases。Iterations: `30`。该板卡结果来自原始 `PointXYZ` closeout；本轮类型放宽的正确性和日志格式证据以 QEMU `pointxyzi xyz-compatible` case 为准。

| case | Std ms/iter | RVV ms/iter | speedup | 结论 |
| --- | ---: | ---: | ---: | --- |
| `frustum_culling pointxyz full-cloud 64K` | 3.7764 | 0.6484 | 5.82x | `PointXYZ` dense 全云主路径，中等规模收益成立 |
| `frustum_culling pointxyz full-cloud 1M` | 60.6842 | 10.2248 | 5.94x | 1M 主路径，6 平面 dot + mask 合并 + 保序压缩收益成立 |
| `frustum_culling pointxyz negative removed 1M` | 66.1487 | 15.2663 | 4.33x | RVV 同时压缩 kept 和 removed indices，双输出仍有收益 |
| `frustum_culling nondense fallback 1M` | 60.6530 | 61.0681 | 0.99x | non-dense fallback，证明未覆盖路径成本接近 |
| `frustum_culling subset fallback 1M` | 30.4931 | 30.6678 | 0.99x | subset fallback，不作为 RVV 性能结论 |
| `frustum_culling pointxyzi fallback 1M` | 76.4250 | 76.3607 | 1.00x | 历史类型 fallback case；不作为本轮 `PointXYZI` RVV 主路径性能结论 |

结论：`FrustumCulling<PointXYZ>::applyFilter(Indices&)` dense 全云主路径在板卡上约
`4.33x` 到 `5.94x`。当前源码 gate 已扩大为 XYZ-compatible；`PointXYZI` 正确性由 QEMU
compare 覆盖，类型真实性能如需纳入结论，可后续单独重跑板卡。
