# filters/conditional_removal 函数级 RVV 评估

## 1. 目标与源码入口

本主题来自 `doc-rvv/library-screening/filters/filters-retained-candidate-rescreen.zh.md` 的后续执行清单，目标为 `filters/include/pcl/filters/impl/conditional_removal.hpp`。

`ConditionalRemoval<PointT>::applyFilter(PointCloud&)` 是 PCL 中按条件对象筛选点云的公开过滤器入口。用户通过 `ConditionAnd` / `ConditionOr` 组合 `FieldComparison`、`PackedRGBComparison`、`TfQuadraticXYZComparison` 等比较对象；`filter(output)` 调用 `applyFilter` 后输出满足条件的点云，必要时记录 `removed_indices_`。`keep_organized_` 会保留 organized 形状并把未保留点写成 `user_filter_value_`；显式 `setIndices()` 只筛选 subset。

## 2. 函数级候选表

| 候选 | 优先级 | 评估结论 | RVV 覆盖条件 | 回退条件 / 原因 |
| --- | --- | --- | --- | --- |
| `ConditionalRemoval<PointT>::applyFilter` 中 `!keep_organized_`、全云、单个 `FieldComparison<float>` | 中 | 已实现生产 RVV | `PointT` 有标准 layout 的 `float x/y/z`，`input_->is_dense == true`，未显式 `setIndices()`，点数 `>=64`，条件为 `ConditionAnd` 且仅包含一个 `FieldComparison`，字段 datatype 为 `FLOAT32`，op 为 `GT/GE/LT/LE` | 小规模、non-dense、显式 subset、`keep_organized_`、复合条件、`ConditionOr`、非 float 字段、`EQ`、非标准点布局回退 `applyFilterStd` |
| `PointXYZI` / 带附加 float 字段的 XYZ-compatible 点类型 | 中 | 已实现生产 RVV | 同上；字段可为 `intensity` 等 `FLOAT32`，整点输出仍用 `copyPoint` 保留所有字段 | 非 XYZ-compatible 点类型回退，避免假设泛型布局 |
| `keep_organized_` 路径 | 中 | 暂缓 | 无生产 RVV | 需要按全云位置写坏点并处理未选 subset 的 organized 语义，主成本是整点复制和 `getVector4fMap().setConstant`，板卡 fallback case `0.96x`，不作为 RVV 主路径 |
| 显式 `setIndices()` subset | 中 | 暂缓 | 无生产 RVV | 需要 gather 字段和保持 subset 输出 index 语义；当前直接主路径已有收益，subset 成本在板卡约 `1.00x`，收益不可证明 |
| 多比较 / 嵌套条件树 | 中 | 暂缓 | 无生产 RVV | 需要展开多态条件树、短路顺序和不同 comparison 类型；容易改变语义和维护边界 |
| `EQ` on float 字段 | 中 | 回退标量 | 无生产 RVV | `PointDataAtOffset::compare` 对 float NaN 的 `EQ` 语义是“两次比较均 false 即 compare_result=0”，直接 `vmfeq` 对 NaN 不等价；专项 test 覆盖该 fallback |

## 3. 实现计划与结果

实现采用既有 filters RVV 结构：

- 保留 `applyFilterStd`，基本迁移原 `applyFilter` 标量主体；
- `__RVV10__` 下新增 `applyFilterRVV`；
- `applyFilter` 中用短路条件识别可覆盖形态，命中后调用 RVV，否则回到 `applyFilterStd`；
- 主路径 helper 位于 `pcl` 命名空间，不额外放入 `pcl::detail`；
- 公开 API 不变，仅增加 `ConditionalRemoval` 对内部条件对象的 `friend` 访问，用于读取现有 comparison metadata，不新增用户可见 getter。

`applyFilter` 在进入 `applyFilterRVV` 前会执行一次条件树识别和 metadata 展开。具体包括：确认 `condition_` 为 `ConditionAnd<PointT>`，读取其已有 `conditions_` / `comparisons_`，确认无嵌套 condition 且只有一个 `FieldComparison<PointT>`；再确认字段数据存在、datatype 为 `FLOAT32`、op 为 `GT/GE/LT/LE`、比较值可转为 `float`，最后把原 comparison 的 `field_offset`、`op`、`compare_val` 传入 RVV helper。这些检查用于证明当前对象谓词等价于 `finite(x,y,z) && field op compare_val`，属于每次 filter 调用一次的覆盖条件判断，不在逐点循环或 VL chunk 内重复执行。识别失败时回退 `applyFilterStd`，因此不会扩大 `ConditionalRemoval` 的用户可见语义。

RVV helper 使用 `pcl/rvv_point_load.h` 的 `strided_load3_f32m2` 读取 `x/y/z`，用 `strided_load_f32m2` 读取目标 float 字段。每个 VL chunk 先生成 finite xyz mask，再生成字段比较 mask；`vcompress` 压缩 kept index 到临时 buffer 后按该 buffer 逐点 `copyPoint`，保证输出点云顺序和完整字段复制与标量一致。`extract_removed_indices_` 打开时，drop mask 也用 `vcompress` 写入 removed indices。

## 4. 风险与处理

| 风险 | 处理 |
| --- | --- |
| `FieldComparison` / `ConditionBase` 内部状态原本受保护，公开 getter 会扩大 API | 使用 `friend class ConditionalRemoval` 做内部识别，不新增公开 getter |
| 分流前 `dynamic_cast` / metadata 检查比普通入口短路更重 | 该成本每次 `filter()` 调用一次，用于避免逐点 `condition_->evaluate` / `PointDataAtOffset::compare` 调用链；bench 使用相同条件对象对比 std/RVV，板卡结果已包含这部分成本 |
| float `EQ` NaN 语义与 RVV `vmfeq` 不同 | `EQ` 一律回退标量；专项 test `EQFallbackKeepsScalarNaNFieldSemantics` 覆盖 |
| 输出为点云而非 indices，不能只压缩输出 index | RVV 只批量生成 kept/drop index；整点输出仍用 `copyPoint` |
| QEMU fallback case 出现看似加速 | QEMU 不写性能结论；板卡结果显示 `keep_organized` fallback `0.96x`、subset fallback `1.00x`，仅证明回退语义/成本，不作为 RVV 主路径 |

## 5. 测试与验证

专项测试：

- 命令：`make -C test-rvv/filters/conditional_removal run_test_compare`
- 结果：std/RVV 二进制均通过 7 个用例。
- 覆盖：主路径 `PointXYZ z > 0.12`、non-finite xyz、`keep_organized` fallback、subset fallback、复合条件 fallback、float `EQ` fallback、`PointXYZI intensity > 4`。

QEMU bench / 指令：

- 命令：`make -C test-rvv/filters/conditional_removal run_bench_compare dump_bench_rvv`
- `output/qemu/analyze_bench_compare.log` 可解析，无 `未解析`、`n/a`、`Total Time 不计算`。
- 反汇编摘录：`test-rvv/filters/conditional_removal/output/qemu/rvv_asm_check.log`。
- 关键指令：`vlse32.v`、`vmfeq.vv`、`vmflt.vf`、`vmfgt.vf`、`vmand.mm`、`vcompress.vm`、`vcpop.m`、`vsetvli ... e32,m2`。

上游测试：

- 命令：`make -C test-rvv/filters/conditional_removal run_upstream_test_compare`
- `UPSTREAM_TEST_ARGS` 默认指向仓库已有 `test/bun0.pcd` 与 `test/milk_cartoon_all_small_clorox.pcd`。
- 结果：std/RVV 两套 `test/filters/test_filters.cpp` 均通过 21 项。
- 早期未传参数时曾输出缺少 `bun0.pcd` 和 `milk_cartoon_all_small_clorox.pcd`；定位为运行参数问题，Makefile 已补默认参数，不作为环境阻塞。

板卡验证：

- 设备：Milkv-Jupiter。
- 命令：`make -C test-rvv/filters/conditional_removal run_board_test run_board_bench_compare fetch_board_logs`
- 日志：`test-rvv/filters/conditional_removal/output/board/run_test.log`、`run_bench_std.log`、`run_bench_rvv.log`、`analyze_bench_compare.log`。
- 结果：专项板卡 test 7 项通过；bench compare 可解析。

## 6. 板卡性能结论

| case | 入口 / 参数 | Std ms/iter | RVV ms/iter | speedup | 结论 |
| --- | --- | ---: | ---: | ---: | --- |
| `conditional_removal single z>0.05 64K` | `PointXYZ`、全云、`z GT 0.05`、不记录 removed | 6.9191 | 2.1095 | 3.28x | 主路径收益成立 |
| `conditional_removal single z>0.05 removed 1M` | `PointXYZ`、全云、`z GT 0.05`、记录 removed | 113.4504 | 39.5442 | 2.87x | 主路径含 removed 压缩收益成立 |
| `conditional_removal single y<=0.10 1M` | `PointXYZ`、全云、`y LE 0.10` | 112.4027 | 37.3565 | 3.01x | 不同字段 / op 主路径收益成立 |
| `conditional_removal pointxyzi intensity>4 1M` | `PointXYZI`、全云、`intensity GT 4` | 95.8147 | 20.6196 | 4.65x | 附加 FLOAT32 字段主路径收益成立 |
| `conditional_removal keep-organized fallback 1M` | `keep_organized=true` | 195.4857 | 204.3193 | 0.96x | fallback 语义验证，不作为 RVV 性能 |
| `conditional_removal subset fallback 1M` | 显式 subset indices | 56.1692 | 55.8958 | 1.00x | fallback 语义验证，不作为 RVV 性能 |

板卡结论：生产 RVV 覆盖的单个 `FieldComparison<float>` 全云主路径在 Milkv-Jupiter 上为 `2.87x` 到 `4.65x`。fallback case 保持语义，`keep_organized` 略慢但未命中 RVV 主路径。
