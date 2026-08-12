# transformation_estimation_point_to_plane_lls_weighted Benchmark 与证据说明

## 本文职责

本文解释 bench case、case-filter、checksum、trace、asm attribution 和日志提交边界。读者可以不先阅读 bench C++ 代码。

bench 入口：

```text
src/bench_teptplw.cpp
```

bench case registry：

```text
include/impl/teptplw_bench_cases.hpp
```

bench harness：

```text
include/impl/teptplw_bench_harness.hpp
```

## Bench 输出格式

bench 程序先打印上下文，再按 case 输出耗时和 checksum。

字段含义：

| 字段 | 含义 |
| --- | --- |
| `Dataset` | 数据集说明。当前是确定性 synthetic PointNormal weighted point-to-plane LLS diagnostic。 |
| `Iterations` | 计时循环次数。 |
| `Warmup Iterations` | 计时前 warm-up 次数，不进入 `Total Time`。 |
| `Case filter` | 当前 case-filter。为空时运行默认 diagnostic case 集合。 |
| `Build` | `std` 或 `rvv`。由是否定义 `__RVV10__` 决定。 |
| `<case>: <ms/iter> ms/iter` | 单个 case 的平均耗时。 |
| `Total Time` | 单个 case 的总耗时，或所有 case 的总耗时。 |
| `Checksum` | 单个 case 或全部 case 的校验和指纹。 |
| `Iteration Times` | trace case 的逐 iteration 耗时。 |
| `Iteration Min/Median/Max` | trace case 的最小、中位、最大 iteration 耗时。 |

## CLI 参数

| 参数 | 默认值 | 含义 |
| --- | --- | --- |
| `--size` | `65536,262144` | 一个或多个点数，用逗号分隔。 |
| `--case-filter` | 空 | 选择 case 组。 |
| `--iterations` | `20` | 每个 case 的计时 iteration 数。 |
| `--warmup-iterations` | `0` | 每个 case 计时前 warm-up 次数。 |
| `--generic-abc-order` | `baseline,abc,ilp` | `generic-fused-abc-trace` 的输出顺序。 |

直接运行 bench binary 时 `--warmup-iterations` 的 CLI 默认仍是 0；topic-local Makefile 的
`run_bench_*` 和 `run_board_bench_*` target 默认传 `5`。带 Std/RVV 数字的当前性能分析必须使用带
warm-up 的板卡或目标硬件 run；旧 no-warmup 日志只能写成 historical / smoke。

## Bench Label 语法

bench label 用空格分段。读者可以按下表解析。

```text
weighted lls <layer> <row-source> <variant> <point-type-label> [no-solve] <size>
```

常见字段：

| 字段 | 含义 |
| --- | --- |
| `weighted lls` | 带权 point-to-plane LLS。 |
| `production-dispatch` | std/RVV 都调用真实 public overload。row-source 字段决定 full-cloud 或 source-indexed。 |
| `production-default` | 当前默认 production RVV path，用 trace 输出逐 iteration。 |
| `production-shaped` | test_support layout-gated helper，形态接近 production。 |
| `component` | 只测 normal-equation 构造，不包含 Eigen solve 和 matrix 构造。 |
| `row-sources` | test_support row source candidate 诊断。 |
| `full-cloud` | source[k] 与 target[k] 按相同下标组成 row。 |
| `source-indices` | source 由 indices 指定，target 顺序扫描。 |
| `dual-indices` | source 和 target 都由独立 index stream 指定。 |
| `correspondences` | source/target 和 weight 来自 correspondence。 |
| `block-baseline` | 同边界 block-reduction baseline。 |
| `block-fused-*` | fused formula 候选。 |
| `no-solve` | component no-solve case，只返回 normal-equation checksum。 |
| `pointnormal` | `PointNormal -> PointNormal`。 |
| `pointxyz-to-pointnormal` | `PointXYZ -> PointNormal`。 |
| `pointxyz-to-pointxyzinormal` | `PointXYZ -> PointXYZINormal`。 |
| 最后一段数字 | 点数。当前 full-cloud production evidence 使用 `262144`；source-indexed production evidence 使用 `65536` 和 `262144`。 |

示例：

```text
weighted lls production-dispatch full-cloud pointxyz-to-pointnormal 262144
```

含义：

| 字段 | 解释 |
| --- | --- |
| `weighted lls` | 带权点到平面最小二乘。 |
| `production-dispatch` | 调用真实 public full-cloud overload。 |
| `full-cloud` | row 使用 `source[k] + target[k] + weights_[k]`。 |
| `pointxyz-to-pointnormal` | source 是 `PointXYZ`，target 是 `PointNormal`。 |
| `262144` | 输入点数。 |

第二个示例：

```text
weighted lls production-dispatch source-indices pointnormal 65536
```

含义：

| 字段 | 解释 |
| --- | --- |
| `production-dispatch` | 调用真实 public source-indexed overload。 |
| `source-indices` | row 使用 `source[indices_src[k]] + target[k] + weights_[k]`。 |
| `pointnormal` | source 和 target 都是 `PointNormal`。 |
| `65536` | 输入 index stream 长度、target 点数和权重数量。 |

## Case-Filter 字典

空 case-filter 对应 `run_bench_default_diagnostic`。它是综合诊断入口，也就是默认大汇总。它适合做快速 smoke 和日志形状检查。数据源取舍复核使用 `row-sources` case-filter。

| case-filter | 测试类型 | 入口边界 | 主要 label | 证明范围 | 不能证明 |
| --- | --- | --- | --- | --- | --- |
| 空 | 综合诊断入口 | test_support helpers | full-cloud、block-reduction、fused formula、source/dual/correspondences | 默认大汇总是否能构建、运行和输出 checksum。 | 真实生产路径性能；单独的数据源取舍结论。 |
| `row-sources` | 行来源诊断 | test_support row source candidates | `row-sources full-cloud/source-indices/dual-indices/correspondences pointnormal` | 隔离四类 row source candidate 的耗时和 checksum 形状；source-indices 行曾触发 production integration 复核。 | production dispatch；最终 production 证据应看 dedicated target。 |
| `source-indexed-family` | 实现族迁移诊断 | test_support source-indexed candidates | `source-indexed-family staged-gather/block-baseline/block-fused-abcd-ilp pointnormal` 和 `component ... no-solve` | 隔离 source-indexed staged-gather、block-baseline 和 fused-abcd-ilp 的 pre-production 诊断信号。 | production dispatch；source-indexed current family 的 repeated production 证据；永久拒绝某个 production 候选。 |
| `dual-correspondence-family` | 实现族迁移诊断 | test_support dual/correspondence candidates | `dual-correspondence-family dual-indices/correspondences staged-gather/block-baseline/block-fused-abcd-ilp pointnormal` | 隔离 dual-indices / correspondences 的实现族 carry-over 正负信号。 | production dispatch；只能作为 no-production closeout 边界。 |
| `fused-formula` | 组件消融子集 | test_support fused helpers | `block-fused-* pointnormal` | fused formula PointNormal direct diagnostic。 | representative point types 和 production dispatch。 |
| `production-shaped-fused-formula` | 生产形态诊断 | layout-gated test_support full estimate | `production-shaped full-cloud block-* pointnormal` | 同边界 helper A/B。 | 真实 public overload。 |
| `generic-fused-abc` | 组件消融 + 生产形态诊断 | layout-gated generic test_support helpers | 三类 representative point label | `abc` 与 `abc-ilp` representative correctness/perf 形状。 | D 项和 abcd 全候选。 |
| `generic-fused-formula` | 组件消融 + 生产形态诊断 | layout-gated generic test_support helpers | `abc`、D 项、`abcd` 候选 | 三类代表点型 fused formula 消融。 | production dispatch。 |
| `generic-fused-abc-trace` | 逐轮追踪诊断 | layout-gated generic test_support helpers | pointxyz-to-pointxyzinormal trace | iteration 长尾排查。 | production default trace。 |
| `production-dispatch` | 真实生产路径性能测试 | 真实 public full-cloud overload | 三类 representative point label | std/RVV production direct speedup。 | source-indexed、dual-indices、correspondences 和所有点型逐类型性能。 |
| `production-source-indices` | source-indexed 真实生产路径性能测试 | 真实 public source-indexed overload | `production-dispatch source-indices` 三类 representative point label | source-indexed std/RVV production direct speedup。 | dual-indices、correspondences、invalid index 行为。 |
| `production-default-fused-abcd-ilp` | 默认生产路径追踪 | 真实 public full-cloud overload，RVV-only trace | 三类 representative point label | 默认 RVV path timing stability、checksum 序列。 | block/fused B/A，当前默认二进制不保留旧 pair。 |

## 推荐 Target

下列表格中的日志路径表示运行 target 后的默认输出位置。它们不自动进入提交候选。当前提交候选以“当前 Production Evidence”和“提交边界”章节为准。

| target | 测试类型 | case-filter / 作用 | 主要日志 |
| --- | --- | --- | --- |
| `run_bench_default_diagnostic` | 综合诊断入口 | 空；默认大汇总。 | `log/qemu/run_bench_default_diagnostic_std.log`、`run_bench_default_diagnostic_rvv.log`、`analyze_bench_compare_default_diagnostic.log`。 |
| `run_bench_row_sources` | 行来源诊断 | `row-sources`；隔离数据源候选。 | `log/qemu/run_bench_row_sources_std.log`、`run_bench_row_sources_rvv.log`、`analyze_bench_compare_row_sources.log`。 |
| `run_bench_source_indexed_family` | 实现族迁移诊断 | `source-indexed-family`；隔离 source-indexed staged-gather、block-baseline 和 block-fused-abcd-ilp candidate。 | `log/qemu/run_bench_source_indexed_family_std.log`、`run_bench_source_indexed_family_rvv.log`、`analyze_bench_compare_source_indexed_family.log`。 |
| `run_bench_dual_correspondence_family` | 实现族迁移诊断 | `dual-correspondence-family`；隔离 dual-indices / correspondences 的 staged-gather、block-baseline 和 block-fused-abcd-ilp candidate。 | `log/qemu/run_bench_dual_correspondence_family_std.log`、`run_bench_dual_correspondence_family_rvv.log`、`analyze_bench_compare_dual_correspondence_family.log`。 |
| `run_bench_fused_formula` | 组件消融 | `fused-formula`。 | `log/qemu/run_bench_fused_formula_std.log`、`run_bench_fused_formula_rvv.log`、`analyze_bench_compare_fused_formula.log`。 |
| `run_bench_production_dispatch` | 真实生产路径性能测试 | `production-dispatch`。 | `log/qemu/run_bench_production_dispatch_std.log`、`run_bench_production_dispatch_rvv.log`、`analyze_bench_compare_production_dispatch.log`。 |
| `run_bench_production_source_indices` | source-indexed 真实生产路径性能测试 | `production-source-indices`。 | `log/qemu/run_bench_production_source_indices_std.log`、`run_bench_production_source_indices_rvv.log`、`analyze_bench_compare_production_source_indices.log`。 |
| `run_bench_production_shaped_fused_formula` | 生产形态诊断 | `production-shaped-fused-formula`。 | `log/qemu/run_bench_production_shaped_fused_formula_std.log`、`run_bench_production_shaped_fused_formula_rvv.log`、`analyze_bench_compare_production_shaped_fused_formula.log`。 |
| `run_bench_generic_fused_abc` | 组件消融 | `generic-fused-abc`。 | `log/qemu/run_bench_generic_fused_abc_std.log`、`run_bench_generic_fused_abc_rvv.log`、`analyze_bench_compare_generic_fused_abc.log`。 |
| `run_bench_generic_fused_formula` | 组件消融 | `generic-fused-formula`。 | `log/qemu/run_bench_generic_fused_formula_std.log`、`run_bench_generic_fused_formula_rvv.log`、`analyze_bench_compare_generic_fused_formula.log`。 |
| `run_bench_generic_fused_abc_trace` | 逐轮追踪 | `generic-fused-abc-trace`。 | `log/qemu/run_bench_generic_fused_abc_trace_std.log`、`run_bench_generic_fused_abc_trace_rvv.log`、`analyze_bench_compare_generic_fused_abc_trace.log`。 |
| `run_bench_production_default_fused_abcd_ilp` | 默认生产路径追踪 | `production-default-fused-abcd-ilp` 的 QEMU std/RVV 形状检查。 | `log/qemu/run_bench_production_default_fused_abcd_ilp_std.log`、`run_bench_production_default_fused_abcd_ilp_rvv.log`、`analyze_bench_compare_production_default_fused_abcd_ilp.log`。 |
| `run_bench_production_default_fused_abcd_ilp_rvv` | 默认生产路径追踪 | `production-default-fused-abcd-ilp` 的 RVV-only 形状检查。 | `log/qemu/run_bench_production_default_fused_abcd_ilp_rvv.log`。 |

## 板卡 Smoke Target

每个 bench case-filter 都有对应的单次板卡 target。它们调用 `run_board_bench_compare`，再执行 `fetch_board_logs`。这些 target 只证明板卡可运行和日志形状；性能结论仍以 repeated board summary 为准。

| target | 对应 QEMU target | 测试类型 | 默认输出 |
| --- | --- | --- | --- |
| `run_board_bench_default_diagnostic` | `run_bench_default_diagnostic` | 综合诊断入口。 | `log/board/run_board_bench_default_diagnostic/`。 |
| `run_board_bench_row_sources` | `run_bench_row_sources` | 行来源诊断。 | `log/board/run_board_bench_row_sources/`。 |
| `run_board_bench_source_indexed_family` | `run_bench_source_indexed_family` | source-indexed 实现族迁移诊断。 | `log/board/run_board_bench_source_indexed_family/`；默认带 5 次 warm-up。 |
| `run_board_bench_dual_correspondence_family` | `run_bench_dual_correspondence_family` | dual-indices / correspondences 实现族迁移诊断。 | `log/board/run_board_bench_dual_correspondence_family/`。 |
| `run_board_bench_fused_formula` | `run_bench_fused_formula` | 组件消融。 | `log/board/run_board_bench_fused_formula/`。 |
| `run_board_bench_production_dispatch` | `run_bench_production_dispatch` | 真实生产路径性能测试。 | `log/board/run_board_bench_production_dispatch/`。 |
| `run_board_bench_production_source_indices` | `run_bench_production_source_indices` | source-indexed 真实生产路径性能测试。 | `log/board/run_board_bench_production_source_indices/`。 |
| `run_board_bench_production_shaped_fused_formula` | `run_bench_production_shaped_fused_formula` | 生产形态诊断。 | `log/board/run_board_bench_production_shaped_fused_formula/`。 |
| `run_board_bench_generic_fused_abc` | `run_bench_generic_fused_abc` | 组件消融。 | `log/board/run_board_bench_generic_fused_abc/`。 |
| `run_board_bench_generic_fused_formula` | `run_bench_generic_fused_formula` | 组件消融。 | `log/board/run_board_bench_generic_fused_formula/`。 |
| `run_board_bench_generic_fused_abc_trace` | `run_bench_generic_fused_abc_trace` | 逐轮追踪。 | `log/board/run_board_bench_generic_fused_abc_trace/`。 |
| `run_board_bench_production_default_fused_abcd_ilp` | `run_bench_production_default_fused_abcd_ilp` | 默认生产路径追踪。 | `log/board/run_board_bench_production_default_fused_abcd_ilp/`。 |

## 计时边界

| case 类型 | 包含 | 不包含 |
| --- | --- | --- |
| production-dispatch full-cloud | `setCorrespondenceWeights` 后的 public estimate、normal-equation、Eigen solve、matrix checksum。 | 输入点云和权重构造。 |
| production-source-indices | `setCorrespondenceWeights` 后的 public source-indexed estimate、valid-index scan、index staging、source gather、target stride load、weight load、solve、matrix checksum。 | 输入点云、权重和 source index vector 构造。 |
| production-default trace | 同 production-dispatch；额外记录每次 iteration。 | 旧 block baseline。 |
| production-shaped full estimate | test_support estimate wrapper、normal-equation、Eigen solve、matrix checksum。 | 真实 production dispatch。 |
| source-indexed-family full estimate | source-indexed candidate 的 valid-index scan、source gather、target stride load、weight load、normal-equation sink、Eigen solve 和 matrix sink。 | 输入点云、权重和 source index vector 构造；真实 production dispatch。 |
| component no-solve | normal-equation 构造和 component checksum。 | Eigen solve 和 matrix 构造。 |
| row-sources source-indexed | test_support source-indexed candidate 的 valid-index scan、index staging、source gather、target stride load、weight load、solve、matrix。 | 输入点云、权重和 source index vector 构造；真实 production dispatch。 |
| dual-indices | 两条 index stream 的 staging/gather、weight load、solve、matrix。 | index vector 构造。 |
| correspondences | correspondence index/weight 展开、gather、solve、matrix。 | correspondence 输入构造。 |

## Checksum 来源

checksum 是 bench 日志指纹。它用于检查 case 是否稳定执行并产出同类结果。它不能替代 gtest 的 numerical correctness。

### 1. Matrix checksum

位置：

```text
include/impl/teptplw_common.hpp
```

函数：

```text
diag::matrix_checksum(matrix)
```

算法：

```text
checksum = sum(matrix(row, col) * (1 + row * 4 + col))
```

用途：

- full estimate case。
- production-dispatch case。
- production-shaped case。

很多 full estimate case 还会加：

```text
accepted_points * 1e-6
```

这样可以让 checksum 同时携带 matrix 和 accepted point 指纹。

### 2. Normal-equation checksum

位置：

```text
include/impl/teptplw_bench_harness.hpp
```

函数：

```text
normal_equation_checksum(eq)
```

算法：

```text
checksum = accepted_points * 1e-6
for each row/col in ATA:
  checksum += ATA(row, col) * (1 + row * 6 + col) * 1e-9
for each row in ATb:
  checksum += ATb(row) * (1 + row) * 1e-6
```

用途：

- `component ... no-solve` case。
- 防止编译器把只构造 normal-equation 的计算消掉。
- 区分 accepted point 和 normal-equation 变化。

### 3. Iteration checksum

位置：

```text
include/impl/teptplw_bench_harness.hpp
```

函数：

```text
run_case(...)
run_case_trace(...)
```

算法：

```text
for warmup iteration:
  warmup_checksum += fn()
for measured iteration:
  checksum += fn()
```

warm-up checksum 使用 `volatile`，避免 warm-up 调用被移除。warm-up checksum 不打印。measured checksum 会写入每个 case 的 `Checksum:` 行。

### 4. 多轮 checksum validation

位置：

```text
script/collect_teptplw_board_rvv_ba.py
script/summarize_teptplw_trace.py
```

脚本读取 raw log 中的：

```text
Checksum: <value>
```

然后检查：

- 每轮 raw log 是否都有 checksum 行。
- 每轮 checksum 序列是否完全一致。
- final checksum 是否一致。

当前提交的 summary：

```text
log/board/production_default_fused_abcd_ilp/checksum_validation.md
```

该文件说明 5 轮 raw log 都有 4 行 checksum，final checksum 为 `2184.271025`，序列一致。

## 当前 Production Evidence

| 证据文件 | 生成方式 | 主要结论 | 提交状态 |
| --- | --- | --- | --- |
| `log/qemu/run_test_std.log` | `run_test_compare` | std gtest 45 passed + 1 skipped。 | tracked |
| `log/qemu/run_test_rvv.log` | `run_test_compare` | RVV gtest 47 passed。 | tracked |
| `log/qemu/run_test_source_indices_std.log` | `run_test_source_indices_compare` | source-indexed production direct std 6 passed。 | tracked |
| `log/qemu/run_test_source_indices_rvv.log` | `run_test_source_indices_compare` | source-indexed production direct RVV 7 passed。 | tracked |
| `log/board/production_dispatch_fused_abcd_ilp/summary.md` | `collect_board_production_dispatch_repeated` | 三类代表点型 262144 点 repeated std/RVV speedup 均正向。 | tracked |
| `log/board/production_dispatch_fused_abcd_ilp/evidence_manifest.json` | `generate_teptplw_evidence_manifest.py` | production-dispatch summary、checksum 和 asm 归因的 Evidence Doctor manifest。 | tracked |
| `log/board/production_dispatch_fused_abcd_ilp/evidence_doctor.md` | `test-rvv/script/evidence_doctor.py --manifest ...` | 0 Errors / 0 Warnings / 3 Suggestions；建议补 binary identity，不阻塞当前结论。 | tracked |
| `log/board/production_source_indices_staged_gather/summary.md` | `collect_board_production_source_indices_repeated` | source-indexed 三类代表点型在 65536 和 262144 点 repeated std/RVV speedup 均正向。 | tracked |
| `log/board/production_source_indices_staged_gather/evidence_manifest.json` | `generate_teptplw_evidence_manifest.py --kind production-source-indices` | source-indexed repeated board manifest；source-indexed-specific asm boundary 仍缺。 | tracked |
| `log/board/production_source_indices_staged_gather/evidence_doctor.md` | `test-rvv/script/evidence_doctor.py --manifest ...` | 0 Errors / 7 Warnings / 6 Suggestions；repeated board 正向，但 asm / binary identity 边界仍未闭合。 | tracked |
| `log/board/run_board_bench_row_sources/analyze_bench_compare.log` | `run_board_bench_row_sources` | row-source diagnostic 原始 compare log；source-indexed 正向，dual-indices / correspondences 负向。 | tracked |
| `log/board/run_board_bench_row_sources/evidence_manifest.json` | `generate_teptplw_evidence_manifest.py --kind row-sources-diagnostic` | row-source diagnostic manifest；evidence_role=diagnostic，记录 observed speedup。 | tracked |
| `log/board/run_board_bench_row_sources/evidence_doctor.md` | `test-rvv/script/evidence_doctor.py --manifest ... --fail-on never` | 0 Errors / 0 Warnings / 0 Suggestions；只做诊断边界，不升级成 production direct。 | tracked |
| `log/qemu/run_bench_dual_correspondence_family_std.log` | `run_bench_dual_correspondence_family` | dual-indices / correspondences family QEMU bench compare；std 侧基线。 | tracked |
| `log/qemu/run_bench_dual_correspondence_family_rvv.log` | `run_bench_dual_correspondence_family` | dual-indices / correspondences family QEMU bench compare；RVV 侧候选。 | tracked |
| `log/board/run_board_bench_dual_correspondence_family/analyze_bench_compare.log` | `run_board_bench_dual_correspondence_family` | dual-indices / correspondences family 单次板卡诊断 compare。 | tracked |
| `log/board/run_board_bench_dual_correspondence_family/evidence_manifest.json` | `generate_teptplw_evidence_manifest.py --kind dual-correspondence-family` | dual-indices / correspondences family diagnostic manifest；20 comparisons。 | tracked |
| `log/board/run_board_bench_dual_correspondence_family/evidence_doctor.md` | `test-rvv/script/evidence_doctor.py --manifest ... --fail-on never` | 20 Errors / 21 Warnings / 0 Suggestions；所有 Errors 都是 `ba_degradation_frequency`。 | tracked |
| `log/board/production_default_fused_abcd_ilp/trace_summary.md` | `collect_board_production_default_fused_abcd_ilp` 或 `summarize_board_production_default_fused_abcd_ilp` | 默认 RVV path trace 的 5-run timing 和 checksum 摘要。 | tracked |
| `log/board/production_default_fused_abcd_ilp/checksum_validation.md` | `collect_teptplw_board_rvv_ba.py` | 每轮 checksum 序列存在且一致。 | tracked |
| `log/board/production_default_fused_abcd_ilp/asm_production_symbol_attribution.md` | `asm_production_default_fused_abcd_ilp` | 默认 production helper 的 RVV 指令归因。 | tracked |

## Row-Source Diagnostic Trigger

文件：

```text
log/board/run_board_bench_row_sources/analyze_bench_compare.log
```

生成方式：

```text
run_board_bench_row_sources
```

该日志是 source-indexed 接入的诊断触发原始日志，不是最终 production 性能结论。它记录了 row-sources case-filter 下 source-indexed 65536 / 262144 正向、dual-indices 和 correspondences 负向的单次板卡信号。对应的 diagnostic manifest / doctor 现在已经补成 `log/board/run_board_bench_row_sources/evidence_manifest.json` 和 `evidence_doctor.md`，用于保留 pre-production 边界；source-indexed 的最终性能结论仍以 `production_source_indices_staged_gather/summary.md` 为准。

`run_board_bench_dual_correspondence_family/analyze_bench_compare.log` 则是 dual-indices / correspondences 的实现族迁移诊断原始日志。它把 staged-gather、block-baseline、block-fused-abcd-ilp 和 component no-solve 逐项跑在同一个 board compare 里，QEMU 两侧都能通过，但 RVV 在所有列出的 compare case 上都慢于 std。对应的 `evidence_manifest.json` 与 `evidence_doctor.md` 记录了 `20 comparisons`、`Errors=20`、`Warnings=21`、`Suggestions=0` 的边界，其中所有 Errors 都来自 `ba_degradation_frequency`。这说明 dual-indices / correspondences 只能停在 diagnostic attempted，不应继续外推到 production evidence。

`run_board_bench_source_indexed_family/analyze_bench_compare.log` 是 source-indexed 实现族迁移诊断原始日志。Phase 030 之前的该目录日志没有 warm-up，且 full estimate 与 component no-solve sink 口径不够隔离；它只能作为 historical diagnostic。Phase 030 之后，`run_board_bench_source_indexed_family` 默认带 5 次 warm-up，source-indexed-family full estimate 也改为 normal-equation + matrix sink。当前 5-run repeated summary 已由 `collect_board_source_indexed_family_repeated` 生成，对应 wrapper target `doctor_board_source_indexed_family_repeated` 已生成 manifest / doctor；结论是不支持 `block-fused-abcd-ilp` 进入 production-candidate investigation。

## Source-Indexed Family Repeated Target

| target | case-filter / 作用 | 默认输出 | 证据边界 |
| --- | --- | --- | --- |
| `collect_board_source_indexed_family_repeated` | `source-indexed-family`；多轮采集 staged-gather、block-baseline、block-fused-abcd-ilp 和 component no-solve 的 repeated board speedup。 | `log/board/source_indexed_family_repeated/summary.md`。 | 只负责上板采集 summary；raw logs 默认临时清理，显式 `--raw-dir` 时才作为 local-only 诊断材料保留。 |
| `doctor_board_source_indexed_family_repeated` | `source-indexed-family-repeated` manifest / Evidence Doctor wrapper。 | `log/board/source_indexed_family_repeated/evidence_manifest.json`、`evidence_doctor.md`、`evidence_doctor.json`。 | 只在 repeated summary 已存在后运行；该 manifest 仍是 pre-production diagnostic，不是 production direct。 |

默认参数与命令：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted \
  collect_board_source_indexed_family_repeated
```

它默认使用 `size=262144`、`runs=5`、`iterations=20` 和 `warmup=5`。这是一条用来重新校准 source-indexed family 的重复板卡路径，不是 QEMU bench 形状检查，也不是 production-dispatch summary。

当前结果：

| case | median | min | max | 退化频率 |
| --- | ---: | ---: | ---: | --- |
| `source-indexed-family staged-gather pointnormal 262144` | `1.04x` | `0.95x` | `1.32x` | `2/5` 低于 `1.0x` |
| `source-indexed-family block-baseline pointnormal 262144` | `1.05x` | `0.92x` | `1.38x` | `1/5` 低于 `1.0x` |
| `source-indexed-family block-fused-abcd-ilp pointnormal 262144` | `0.90x` | `0.76x` | `1.20x` | `4/5` 低于 `1.0x` |
| `component source-indexed-family block-baseline no-solve 262144` | `1.16x` | `1.00x` | `1.29x` | `0/5` 低于 `1.0x` |
| `component source-indexed-family block-fused-abcd-ilp no-solve 262144` | `0.98x` | `0.85x` | `1.18x` | `3/5` 低于 `1.0x` |

## Evidence Doctor / Manifest 边界

Evidence Doctor（证据体检）不是性能结论本身。它把 summary、checksum、asm attribution 和环境字段翻译成可审查的 Errors / Warnings / Suggestions。当前证据分层如下：

| evidence | 当前 manifest / doctor 状态 | 结论边界 |
| --- | --- | --- |
| production-dispatch full-cloud repeated summary | 已有 `log/board/production_dispatch_fused_abcd_ilp/evidence_manifest.json` 和 `evidence_doctor.md`。 | 可支撑当前 full-cloud production direct performance；doctor Suggestions 只提示后续补 binary identity。 |
| source-indexed production repeated summary | 已有 `log/board/production_source_indices_staged_gather/evidence_manifest.json` 和 `evidence_doctor.md`。 | 可作为 source-indexed production repeated board summary 使用，但 doctor 仍给出 asm boundary missing、long-tail 和 binary identity 缺口；不能写成 clean pass。 |
| row-source diagnostic trigger | 已有 `log/board/run_board_bench_row_sources/evidence_manifest.json` 和 `evidence_doctor.md`。 | 只证明 pre-production diagnostic 边界；manifest 记录 observed speedup，doctor 维持 diagnostic-only，不会升级成 production direct。 |
| source-indexed implementation-family diagnostic | 已有 `log/board/run_board_bench_source_indexed_family/evidence_manifest.json` 和 `evidence_doctor.md`。 | Phase 030 起新增 zero-warmup、component/full sink 和 solve-delta 异常检查；旧 no-warmup 结果降级为 historical diagnostic。 |
| source-indexed implementation-family repeated summary | 已有 `log/board/source_indexed_family_repeated/summary.md`、`evidence_manifest.json`、`evidence_doctor.md` 和 `evidence_doctor.json`；doctor 为 `3E / 6W / 13S`。 | 当前结果不支持 `block-fused-abcd-ilp` production-candidate；仍不能替代 production direct、fallback、asm 和 production bench。 |

production source-indexed summary、source-indexed-family diagnostic summary 和 row-source diagnostic 必须分别使用 production direct、implementation-family diagnostic 和 row-source diagnostic evidence role。row-source 或 family diagnostic 即使通过 doctor，也只能说明触发或拒绝生产接入的诊断证据，不会自动升级成 production evidence。

## Production-Dispatch Summary 口径

文件：

```text
log/board/production_dispatch_fused_abcd_ilp/summary.md
```

采集参数：

| 参数 | 值 |
| --- | --- |
| case filter | `production-dispatch` |
| size | `262144` |
| runs | `5` |
| iterations | `20` |
| warm-up iterations | `5` |

当前结果：

| case | 中文含义 | median/min | 边界 |
| --- | --- | ---: | --- |
| `weighted lls production-dispatch full-cloud pointnormal 262144` | `PointNormal -> PointNormal` 真实 public overload。 | `2.76x / 2.73x` | 代表 `PointNormal` 布局。 |
| `weighted lls production-dispatch full-cloud pointxyz-to-pointnormal 262144` | source generic gate representative。 | `2.98x / 2.95x` | 代表 source 只提供 xyz。 |
| `weighted lls production-dispatch full-cloud pointxyz-to-pointxyzinormal 262144` | source + target generic gate representative。 | `3.00x / 2.96x` | 代表 target 有 xyz+normal 和额外字段。 |

speedup 公式：

```text
std/RVV speedup = std_ms / rvv_ms
```

该 summary 证明当前 full-cloud production default 在三类代表点型上相对 std 正向。它不证明 source-indexed、dual-indices、correspondences、`Scalar=double`、非连续权重或所有 gate-allowed 点型逐类型性能。source-indexed 结论使用下一节的 dedicated summary。

## Production-Source-Indices Summary 口径

文件：

```text
log/board/production_source_indices_staged_gather/summary.md
```

采集参数：

| 参数 | 值 |
| --- | --- |
| case filter | `production-source-indices` |
| size | `65536,262144` |
| runs | `5` |
| iterations | `20` |
| warm-up iterations | `5` |

该 case-filter 调用真实 public source-indexed overload。source index stream 由 `make_source_indices(n)` 在计时前构造。计时区域包含 `setCorrespondenceWeights` 后的 estimate、valid-index scan、`uint32_t` staging、source gather、target stride load、weight load、Eigen solve 和 matrix checksum。

当前结果：

| case | 中文含义 | median/min | 边界 |
| --- | --- | ---: | --- |
| `weighted lls production-dispatch source-indices pointnormal 262144` | `PointNormal -> PointNormal` 真实 source-indexed public overload。 | `2.33x / 1.99x` | 代表 `PointNormal` source gather。 |
| `weighted lls production-dispatch source-indices pointnormal 65536` | 同上，较小规模。 | `2.59x / 2.47x` | 代表 64K index stream。 |
| `weighted lls production-dispatch source-indices pointxyz-to-pointnormal 262144` | source 是 `PointXYZ`，target 是 `PointNormal`。 | `2.22x / 2.18x` | 代表 source generic xyz layout。 |
| `weighted lls production-dispatch source-indices pointxyz-to-pointnormal 65536` | 同上，较小规模。 | `2.57x / 2.49x` | 代表 64K generic source。 |
| `weighted lls production-dispatch source-indices pointxyz-to-pointxyzinormal 262144` | source 是 `PointXYZ`，target 是 `PointXYZINormal`。 | `2.37x / 2.28x` | 代表 source 和 target generic layout。 |
| `weighted lls production-dispatch source-indices pointxyz-to-pointxyzinormal 65536` | 同上，较小规模。 | `2.78x / 2.70x` | 代表 64K generic source + target。 |

该 summary 证明 valid source-indexed production path 在三类代表点型和两个规模上相对 std 正向。它不证明 dual-indices、correspondences、invalid index public API 行为、`Scalar=double` 或非连续权重。

## Production-Default Trace 口径

文件：

```text
log/board/production_default_fused_abcd_ilp/trace_summary.md
```

case-filter：

```text
production-default-fused-abcd-ilp
```

该 case-filter 调用真实 public full-cloud overload，并使用 `run_case_trace` 打印逐 iteration 时间。当前默认 production 已经采用 fused-abcd-ilp 公式形态。接入后默认二进制不再保留旧 block baseline 和 fused helper pair，因此该目录的 trace 不输出同一二进制内的 fused-vs-block B/A。

trace summary 证明：

- 每轮运行都有 checksum 行。
- checksum 序列一致。
- 三类代表点型的 iteration timing 可复核。
- 长尾 iteration 可定位。

## ASM Attribution 口径

文件：

```text
log/board/production_default_fused_abcd_ilp/asm_production_symbol_attribution.md
```

生成脚本：

```text
script/generate_teptplw_asm_attribution.py
```

profile：

```text
production-default-fused-abcd-ilp
```

脚本按优先级寻找符号边界：

1. `buildPointToPlaneLLSWeightedFullCloudBlockRVV` detail symbol。
2. `estimatePointToPlaneLLSWeightedFullCloudRVV` wrapper。
3. public full-cloud overload。
4. iterator clone。

输出中的 `boundary` 列记录实际边界。不同 `boundary` 的总指令数不能直接横向比较。

当前三类代表点型都确认：

- `vlse32`：source/target AoS stride load。
- `vle32`：连续 weights load。
- `vcpop` / `merge`：finite mask 和 invalid lane 归零。
- `vfmsac`：fused `a/b/c` 公式形态。
- `vfmacc`：D 项和 A/B/C/N partial sums。
- `vfredosum`：block group 横向规约。
- `spill/reload=0`：没有 vector spill/reload。

## 复现命令

QEMU correctness：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted run_test_compare
```

source-indexed production correctness：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted \
  run_test_source_indices_compare
```

QEMU bench 形状检查：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted run_bench_compare
```

row source 诊断子集：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted run_bench_row_sources
```

source-indexed production bench 形状检查：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted \
  run_bench_production_source_indices
```

row source repeated board：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted \
  collect_board_row_sources_repeated \
  TEPTPLW_ROW_SOURCE_CASE_FILTER=row-sources \
  TEPTPLW_ROW_SOURCE_SIZE=262144 \
  TEPTPLW_ROW_SOURCE_RUNS=5 \
  TEPTPLW_ROW_SOURCE_ITERATIONS=20 \
  TEPTPLW_ROW_SOURCE_WARMUP_ITERATIONS=5
```

production-dispatch repeated board：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted \
  collect_board_production_dispatch_repeated \
  TEPTPLW_COMPARE_CASE_FILTER=production-dispatch \
  TEPTPLW_COMPARE_SIZE=262144 \
  TEPTPLW_COMPARE_RUNS=5 \
  TEPTPLW_COMPARE_ITERATIONS=20 \
  TEPTPLW_COMPARE_WARMUP_ITERATIONS=5
```

source-indexed production repeated board：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted \
  collect_board_production_source_indices_repeated \
  TEPTPLW_SOURCE_INDICES_CASE_FILTER=production-source-indices \
  TEPTPLW_SOURCE_INDICES_SIZE=65536,262144 \
  TEPTPLW_SOURCE_INDICES_RUNS=5 \
  TEPTPLW_SOURCE_INDICES_ITERATIONS=20 \
  TEPTPLW_SOURCE_INDICES_WARMUP_ITERATIONS=5
```

production-default trace/checksum/asm：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted \
  collect_board_production_default_fused_abcd_ilp \
  TEPTPLW_BOARD_SIZE=262144 \
  TEPTPLW_BOARD_RUNS=5 \
  TEPTPLW_BOARD_ITERATIONS=20 \
  TEPTPLW_BOARD_WARMUP_ITERATIONS=5
```

只重建 asm attribution：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted \
  asm_production_default_fused_abcd_ilp
```

## 提交边界

当前 topic 使用 summary-only 策略。文档提到 raw log 时，应说明它是可再生成输入，默认不提交。

默认提交候选：

- QEMU correctness logs。
- repeated board summary。
- production-dispatch evidence manifest 和 evidence doctor summary。
- trace summary。
- checksum validation。
- asm attribution summary。
- Evidence Doctor manifest 和 doctor summary，前提是它们已被本文或 evaluation 明确引用。
- source-indexed production direct 细粒度 correctness logs。
- row-source 诊断触发 analyze log。
- source-indexed repeated board summary。

默认不提交：

- raw board run logs。
- board env logs。
- collection manifest。
- QEMU bench raw logs。
- 未被文档引用的 single-run analyze logs。
- full asm dump。
- vec missed logs。

新增 bench 数据时，先更新本文的证据表，再决定是否放开对应文件。没有被文档解释的数据日志不进入提交候选。
