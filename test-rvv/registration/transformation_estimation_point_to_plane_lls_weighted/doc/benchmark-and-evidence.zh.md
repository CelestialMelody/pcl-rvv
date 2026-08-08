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

## Bench Label 语法

bench label 用空格分段。读者可以按下表解析。

```text
weighted lls <layer> <row-source> <variant> <point-type-label> [no-solve] <size>
```

常见字段：

| 字段 | 含义 |
| --- | --- |
| `weighted lls` | 带权 point-to-plane LLS。 |
| `production-dispatch` | std/RVV 都调用真实 public full-cloud overload。 |
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
| 最后一段数字 | 点数。当前 production evidence 使用 `262144`。 |

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

## Case-Filter 字典

空 case-filter 对应 `run_bench_default_diagnostic`。它是综合诊断入口，也就是默认大汇总。它适合做快速 smoke 和日志形状检查。数据源取舍复核使用 `row-sources` case-filter。

| case-filter | 测试类型 | 入口边界 | 主要 label | 证明范围 | 不能证明 |
| --- | --- | --- | --- | --- | --- |
| 空 | 综合诊断入口 | test_support helpers | full-cloud、block-reduction、fused formula、source/dual/correspondences | 默认大汇总是否能构建、运行和输出 checksum。 | 真实生产路径性能；单独的数据源取舍结论。 |
| `row-sources` | 行来源诊断 | test_support row source candidates | `row-sources full-cloud/source-indices/dual-indices/correspondences pointnormal` | 隔离四类 row source candidate 的耗时和 checksum 形状。 | production dispatch；fused-abcd-ilp 对 indexed/correspondences 的收益。 |
| `fused-formula` | 组件消融子集 | test_support fused helpers | `block-fused-* pointnormal` | fused formula PointNormal direct diagnostic。 | representative point types 和 production dispatch。 |
| `production-shaped-fused-formula` | 生产形态诊断 | layout-gated test_support full estimate | `production-shaped full-cloud block-* pointnormal` | 同边界 helper A/B。 | 真实 public overload。 |
| `generic-fused-abc` | 组件消融 + 生产形态诊断 | layout-gated generic test_support helpers | 三类 representative point label | `abc` 与 `abc-ilp` representative correctness/perf 形状。 | D 项和 abcd 全候选。 |
| `generic-fused-formula` | 组件消融 + 生产形态诊断 | layout-gated generic test_support helpers | `abc`、D 项、`abcd` 候选 | 三类代表点型 fused formula 消融。 | production dispatch。 |
| `generic-fused-abc-trace` | 逐轮追踪诊断 | layout-gated generic test_support helpers | pointxyz-to-pointxyzinormal trace | iteration 长尾排查。 | production default trace。 |
| `production-dispatch` | 真实生产路径性能测试 | 真实 public full-cloud overload | 三类 representative point label | std/RVV production direct speedup。 | indexed/correspondences 和所有点型逐类型性能。 |
| `production-default-fused-abcd-ilp` | 默认生产路径追踪 | 真实 public full-cloud overload，RVV-only trace | 三类 representative point label | 默认 RVV path timing stability、checksum 序列。 | block/fused B/A，当前默认二进制不保留旧 pair。 |

## 推荐 Target

下列表格中的日志路径表示运行 target 后的默认输出位置。它们不自动进入提交候选。当前提交候选以“当前 Production Evidence”和“提交边界”章节为准。

| target | 测试类型 | case-filter / 作用 | 主要日志 |
| --- | --- | --- | --- |
| `run_bench_default_diagnostic` | 综合诊断入口 | 空；默认大汇总。 | `log/qemu/run_bench_default_diagnostic_std.log`、`run_bench_default_diagnostic_rvv.log`、`analyze_bench_compare_default_diagnostic.log`。 |
| `run_bench_row_sources` | 行来源诊断 | `row-sources`；隔离数据源候选。 | `log/qemu/run_bench_row_sources_std.log`、`run_bench_row_sources_rvv.log`、`analyze_bench_compare_row_sources.log`。 |
| `run_bench_fused_formula` | 组件消融 | `fused-formula`。 | `log/qemu/run_bench_fused_formula_std.log`、`run_bench_fused_formula_rvv.log`、`analyze_bench_compare_fused_formula.log`。 |
| `run_bench_production_dispatch` | 真实生产路径性能测试 | `production-dispatch`。 | `log/qemu/run_bench_production_dispatch_std.log`、`run_bench_production_dispatch_rvv.log`、`analyze_bench_compare_production_dispatch.log`。 |
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
| `run_board_bench_fused_formula` | `run_bench_fused_formula` | 组件消融。 | `log/board/run_board_bench_fused_formula/`。 |
| `run_board_bench_production_dispatch` | `run_bench_production_dispatch` | 真实生产路径性能测试。 | `log/board/run_board_bench_production_dispatch/`。 |
| `run_board_bench_production_shaped_fused_formula` | `run_bench_production_shaped_fused_formula` | 生产形态诊断。 | `log/board/run_board_bench_production_shaped_fused_formula/`。 |
| `run_board_bench_generic_fused_abc` | `run_bench_generic_fused_abc` | 组件消融。 | `log/board/run_board_bench_generic_fused_abc/`。 |
| `run_board_bench_generic_fused_formula` | `run_bench_generic_fused_formula` | 组件消融。 | `log/board/run_board_bench_generic_fused_formula/`。 |
| `run_board_bench_generic_fused_abc_trace` | `run_bench_generic_fused_abc_trace` | 逐轮追踪。 | `log/board/run_board_bench_generic_fused_abc_trace/`。 |
| `run_board_bench_production_default_fused_abcd_ilp` | `run_bench_production_default_fused_abcd_ilp` | 默认生产路径追踪。 | `log/board/run_board_bench_production_default_fused_abcd_ilp/`。 |

## 计时边界

| case 类型 | 包含 | 不包含 |
| --- | --- | --- |
| production-dispatch full-cloud | `setCorrespondenceWeights` 后的 public estimate、normal-equation、Eigen solve、matrix checksum。 | 输入点云和权重构造。 |
| production-default trace | 同 production-dispatch；额外记录每次 iteration。 | 旧 block baseline。 |
| production-shaped full estimate | test_support estimate wrapper、normal-equation、Eigen solve、matrix checksum。 | 真实 production dispatch。 |
| component no-solve | normal-equation 构造和 component checksum。 | Eigen solve 和 matrix 构造。 |
| source-indexed | valid-index scan、index staging、source gather、target stride load、weight load、solve、matrix。 | index vector 构造。 |
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
| `log/qemu/run_test_std.log` | `run_test_compare` | std gtest 39 passed + 1 skipped。 | tracked |
| `log/qemu/run_test_rvv.log` | `run_test_compare` | RVV gtest 40 passed。 | tracked |
| `log/board/production_dispatch_fused_abcd_ilp/summary.md` | `collect_board_production_dispatch_repeated` | 三类代表点型 262144 点 repeated std/RVV speedup 均正向。 | tracked |
| `log/board/production_default_fused_abcd_ilp/trace_summary.md` | `collect_board_production_default_fused_abcd_ilp` 或 `summarize_board_production_default_fused_abcd_ilp` | 默认 RVV path trace 的 5-run timing 和 checksum 摘要。 | tracked |
| `log/board/production_default_fused_abcd_ilp/checksum_validation.md` | `collect_teptplw_board_rvv_ba.py` | 每轮 checksum 序列存在且一致。 | tracked |
| `log/board/production_default_fused_abcd_ilp/asm_production_symbol_attribution.md` | `asm_production_default_fused_abcd_ilp` | 默认 production helper 的 RVV 指令归因。 | tracked |

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

该 summary 证明当前 production default 在三类代表点型上相对 std 正向。它不证明 indexed/correspondences、`Scalar=double`、非连续权重或所有 gate-allowed 点型逐类型性能。

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

QEMU bench 形状检查：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted run_bench_compare
```

row source 诊断子集：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted run_bench_row_sources
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
- trace summary。
- checksum validation。
- asm attribution summary。

默认不提交：

- raw board run logs。
- board env logs。
- collection manifest。
- QEMU bench raw logs。
- single-run analyze logs。
- full asm dump。
- vec missed logs。

新增 bench 数据时，先更新本文的证据表，再决定是否放开对应文件。没有被文档解释的数据日志不进入提交候选。
