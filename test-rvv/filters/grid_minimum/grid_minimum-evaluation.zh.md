# filters/grid_minimum 函数级 RVV 评估

## 1. 主题与入口

- 主题：`grid_minimum`
- 主文件：`filters/include/pcl/filters/impl/grid_minimum.hpp`
- 公开类：`pcl::GridMinimum<PointT>`
- 专项目录：`test-rvv/filters/grid_minimum/`
- 模块依据：`doc-rvv/library-screening/filters/filters-retained-candidate-rescreen.zh.md` 的 `6.2 暂缓 / 不单独实施（诊断路径记录）` 第二项。

`GridMinimum<PointT>` 在输入点云上建立二维 `x/y` 网格，并在每个 cell 中选择 `z` 最小的原始点索引。公开调用 `filter(output)` 时，`applyFilter(PointCloud&)` 先调用 `applyFilterIndices(indices)` 得到被保留点索引，再通过 `copyPointCloud` 生成输出点云。

`applyFilterIndices` 的标量流程是：

```text
getMinMax3D(input, indices) -> min/max xy
-> 对每个 index 计算 ix=floor(x/res)-min_b0, iy=floor(y/res)-min_b1, idx=ix+iy*div_x
-> sort(idx, source_index)
-> 每个相同 idx 的连续段中选择最小 z 的 source_index
```

保留候选复筛已把本主题列为 diagnostic / bench-only 路径记录。当前实现只在 `test-rvv` 专项目录中做 RVV 原型和板卡诊断，不改变公开 API，不修改上游生产分流。

## 2. 函数级评估

| 函数 / 片段 | 优先级 | RVV 决策 | 覆盖 / 回退 |
| --- | --- | --- | --- |
| `applyFilterIndices` 的 cell-id 预计算 | bench 诊断 | 已实现专项 RVV 原型 | 覆盖 `PointXYZ`、显式 `indices`、点数 `>=64`；dense 使用所有 lane，non-dense 用 finite mask 跳过 invalid xyz |
| `sort(index_vector)` | 主成本风险 | 保持标量 | 排序主导 full 入口的一部分，RVV cell-id staging 后仍调用同一排序 |
| 每 cell 最小 `z` 选择 | 主成本风险 | 保持标量 | 按排序后连续段扫描，保证与标量 tie/order 语义一致 |
| `applyFilter(PointCloud&)` 生产入口 | 生产暂不接入 | 不修改上游源码 | 当前 bench 中 `production unchanged` 只反映未改源码时的整体成本；其中可能包含已有 `getMinMax3D` RVV 间接受益 |
| 小规模 / 非 `PointXYZ` / 非 RVV 编译 | fallback | Std | 诊断 helper 返回 false 或根本不编译 RVV 分支 |

结论：cell-id 片段在 Milkv-Jupiter 上有 `1.33x` 到 `1.52x` 收益，但 full diagnostic 只有 `1.09x` 到 `1.14x`。排序、分组和最小 z 选择明显稀释了前置片段收益；当前不接入生产分流，保留为 bench 诊断证据。

生产接入判断按当前 workflow 的 bench-diagnosis 升级标准执行：不能只依据局部 microbench speedup，必须确认 full diagnostic 或生产入口 case 稳定明显收益、覆盖入口主成本、fallback 边界清晰、语义风险和维护复杂度可控。`grid_minimum` 当前只满足“局部片段正确且加速”，不满足“full diagnostic 明显收益”和“生产复杂度与收益匹配”；因此不修改上游生产路径。

## 3. RVV 设计

专项 header `grid_minimum_diag.hpp` 新增：

- `GridCell`：保存 `idx/source_index/ix/iy/z`，对应上游 `point_index_idx` 加诊断字段；
- `computeGridCellsStd`：标量 cell-id 预计算；
- `computeGridCellsRVV`：`__RVV10__` 下的 bench-diagnosis RVV helper；
- `selectMinimumZIndices`：复用标量排序和每 cell 最小 z 选择；
- `gridMinimumPointXYZRVV`：RVV cell-id staging + 标量 sort / min-z full diagnostic。

本主题的详细设计按“类型边界 -> 数值 helper -> mask helper -> cell-id RVV 片段 -> full diagnostic 标量尾段”拆分，而不是只摘 grid 公式。这样可以区分三类成本：

- `computeGridCellsRVV` 是 cell-id 片段，对应 bench 的 `grid minimum cell-id diag ...`；
- `gridMinimumPointXYZRVV` 是 full diagnostic，对应 bench 的 `grid minimum full diag ...`，它在 RVV staging 后继续调用同一标量排序和每 cell 最小 z 扫描；
- `GridMinimum<PointXYZ>::filter(output)` 是未修改的生产入口，当前没有接入本主题 helper。

RVV cell-id helper 使用公共 `pcl/rvv_point_load.h`：

- `indexed_load3_f32m2<PointXYZ>` 对 `indices` 执行 `x/y/z` gather；
- `vfcvt.rtz.x.f.v` 加负数小数校正实现 `floor`，不使用 `_rm` intrinsic，不修改 FRM/FCSR；
- dense 输入用全 true mask，non-dense 输入用 `finiteMask(x)&finiteMask(y)&finiteMask(z)`；
- `vcompress` 保序压缩 `idx/source_index/ix/iy/z`，后续排序和 min-z 仍按标量逻辑执行。

`computeGridCellsRVV` 的核心不是单独的 `idx=ix+iy*div_x` 公式，而是一个完整 VL chunk：读取 indices、生成 AoS byte offset、gather `x/y/z`、按 dense 状态构造 keep mask、压缩有效 lane、把 `idx/source_index/ix/iy/z` 写入 `GridCell`。`vcompress` 只移除 invalid lane，不改变保留 lane 的相对顺序；`source_index` 和 `z` 保证后续标量 min-z 扫描仍使用同一原始点。

该设计只验证“2D grid id + floor 预计算”这一个局部问题。排序和每 cell 最小 z 扫描属于 full diagnostic 的标量尾段，不在 cell-id RVV 片段内；它们也是板卡 full 收益被稀释的主要来源。本主题不试图向量化排序或最小 z 分组扫描，也不改变 `GridMinimum` 生产入口。

## 4. 测试与 bench

专项测试：

- `ScalarFormulaCoversNegativeFloorAndGridId`：手算负坐标 floor 与 grid id；
- `RVVGridCellsMatchScalarFullIndices`：全量 indices cell-id 对拍；
- `RVVGridCellsMatchScalarSubsetAndInvalidSkip`：subset gather 与 non-dense invalid 跳过；
- `FullDiagnosticMatchesScalar`：RVV cell-id staging + 标量 sort/min-z 与纯标量 full diagnostic 对拍；
- `RVVMainThenFallbackOrderDoesNotPolluteRounding`：同一进程先命中 RVV，再跑 fallback/Std，验证没有 FRM 污染；
- `ProductionGridMinimumStillRuns`：未改生产入口仍可运行。

专项 bench：

- `grid minimum cell-id diag 64K` / `1M`：只测 cell-id 预计算；
- `grid minimum subset cell-id diag 1M`：indices gather 片段；
- `grid minimum finite cell-id diag 1M`：non-dense finite mask；
- `grid minimum full diag 64K` / `1M` / `finite 1M`：RVV cell-id 后接同一排序和 min-z；
- `grid minimum production unchanged 64K`：调用未修改的生产 `GridMinimum<PointXYZ>::filter(output)`，用于观察整体入口，不作为本主题新增 RVV 路径收益。

上游原始测试：`test/filters/test_grid_minimum.cpp` 已通过专项 Makefile 的 `run_upstream_test_compare` 完成 std/RVV 对拍。

## 5. 验证结果

- QEMU 专项测试：`make -C test-rvv/filters/grid_minimum run_test_compare` 通过，std/RVV 两套构建均通过 6 个专项测试。
- QEMU bench：`make -C test-rvv/filters/grid_minimum run_bench_compare` 通过，`output/qemu/analyze_bench_compare.log` 无 `未解析`、`n/a`、`Total Time 不计算`；QEMU 只作为构建、checksum 和日志格式证据。
- 上游测试：`make -C test-rvv/filters/grid_minimum run_upstream_test_compare` 通过，std/RVV 均通过 `Grid.Minimum`。
- 反汇编：`make -C test-rvv/filters/grid_minimum dump_bench_rvv` 生成 `build/asm/riscv/bench_grid_minimum_rvv.full.asm`；`output/qemu/rvv_asm_check.log` 确认 `vluxei32.v`、`vfcvt.rtz.x.f.v`、`vcompress.vm`、`vcpop.m`、`vsetvli ... e32,m2`。未使用 `_rm` intrinsic，未在摘录中出现本 helper 需要的 FRM 保存 / 恢复路径。
- 板卡验证：`make -C test-rvv/filters/grid_minimum run_board_test run_board_bench_compare fetch_board_logs` 通过，日志在 `output/board/`。

Milkv-Jupiter 结果：

| case | Std ms/iter | RVV ms/iter | speedup | 结论 |
| --- | ---: | ---: | ---: | --- |
| `grid minimum cell-id diag 64K` | 6.0054 | 4.4141 | 1.36x | cell-id 片段有收益 |
| `grid minimum cell-id diag 1M` | 95.9951 | 70.7949 | 1.36x | 大规模 cell-id 片段收益稳定 |
| `grid minimum subset cell-id diag 1M` | 48.3718 | 36.2863 | 1.33x | indices gather 片段仍有收益 |
| `grid minimum finite cell-id diag 1M` | 111.6683 | 73.3842 | 1.52x | finite mask + cell-id 片段有收益 |
| `grid minimum full diag 64K` | 13.1410 | 11.8129 | 1.11x | 排序和 min-z 稀释收益 |
| `grid minimum full diag 1M` | 243.8576 | 222.9570 | 1.09x | full 诊断收益偏弱 |
| `grid minimum finite full diag 1M` | 279.7051 | 246.0664 | 1.14x | non-dense full 诊断收益仍偏弱 |
| `grid minimum production unchanged 64K` | 14.9277 | 12.3948 | 1.20x | 未改生产源码；结果主要观察已有路径和整体成本，不代表本主题新增生产分流 |

## 6. 结论

`GridMinimum` 的 2D cell-id 预计算可以 RVV 化并保持标量语义，板卡片段收益为 `1.33x` 到 `1.52x`。但 full diagnostic 只有 `1.09x` 到 `1.14x`，说明排序和每 cell 最小 z 选择稀释了局部收益。当前主题收敛为 bench 诊断，不接入上游生产分流。

不接生产的直接原因是：生产接入需要新增 staging、RVV/Std 分流、indices gather、non-dense mask、floor 语义保护和文档 / 测试维护边界，但完整入口收益仅为弱收益区间。该收益不足以覆盖生产复杂度，也不足以排除不同数据分布下被排序和 min-z 扫描进一步稀释的风险。

后续若要重新纳入生产候选，需要先提出能覆盖排序后 min-z 分组或替代数据流的方案，并证明 full `GridMinimum::filter` 入口在板卡上有稳定收益，且不会改变 indices 顺序、tie 选择、non-dense invalid 跳过和公开 API。
