# TEPTPL production-dispatch current vs fused board summary

EvidenceDecision:

```text
production-candidate/full-cloud-f32-aos-layout-gated-source-xyz-target-xyznormal-float-rvv-fused-formula-block-dispatch-representative-pointtypes
```

本文件是 summary-only evidence artifact（只提交摘要的证据文件）。原始板卡日志和 run 目录不提交到仓库；下方 host-local archive 只记录当轮取证位置。长期审计以本文件内的 values、命令、analyzer hash 和源码 diff 为准。

## Scope

- Topic: `transformation_estimation_point_to_plane_lls`
- Device: `Milkv-Jupiter`
- Case filter: `production-dispatch`
- Sizes: `65536,262144`
- Runs: `5`
- Bench contract: std/RVV 两侧都调用真实 public full-cloud overload。
- Board-covered point combinations:
  - `PointNormal -> PointNormal`
  - `PointXYZ -> PointNormal`
  - `PointXYZ -> PointXYZINormal`
- Gate-allowed but not individually board-covered: 其它满足 source `RVVXYZAoSFloatLayout` 和 target `RVVXYZNormalFloatLayout` 的 f32 AoS 点型组合。

## Commands

current block 复核使用同一 production-shaped public overload；本轮原始 current archive 来自 fused 默认切换前的 RVV build。reviewer cleanup 后 production 不再提供 current block 可配置入口；current block 只保留在 `test_support/` 作为 direct diagnostic baseline，历史 production-shaped A/B 以本文件记录的 archive values 为准。

fused-formula production-dispatch 使用默认 RVV build：

```text
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls run_board_bench_compare BENCH_ARGS="--size 65536,262144 --case-filter production-dispatch"
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls fetch_board_logs
python3 test-rvv/script/analyze_bench_repeated.py \
  /tmp/teptpl_production_dispatch_fused_5run_20260729_151217/analyze_bench_compare_1.log \
  /tmp/teptpl_production_dispatch_fused_5run_20260729_151217/analyze_bench_compare_2.log \
  /tmp/teptpl_production_dispatch_fused_5run_20260729_151217/analyze_bench_compare_3.log \
  /tmp/teptpl_production_dispatch_fused_5run_20260729_151217/analyze_bench_compare_4.log \
  /tmp/teptpl_production_dispatch_fused_5run_20260729_151217/analyze_bench_compare_5.log
```

current historical production-shaped baseline 重算命令：

```text
python3 test-rvv/script/analyze_bench_repeated.py \
  /tmp/teptpl_production_dispatch_current_5run_20260729_150811/analyze_bench_compare_1.log \
  /tmp/teptpl_production_dispatch_current_5run_20260729_150811/analyze_bench_compare_2.log \
  /tmp/teptpl_production_dispatch_current_5run_20260729_150811/analyze_bench_compare_3.log \
  /tmp/teptpl_production_dispatch_current_5run_20260729_150811/analyze_bench_compare_4.log \
  /tmp/teptpl_production_dispatch_current_5run_20260729_150811/analyze_bench_compare_5.log
```

Analyzer:

```text
2453697926ebf988d7854d3e69736c810507e7940628c3b29cc1df99b8995037  test-rvv/script/analyze_bench_repeated.py
```

Raw evidence archives:

```text
/tmp/teptpl_production_dispatch_current_5run_20260729_150811
/tmp/teptpl_production_dispatch_fused_5run_20260729_151217
```

这些路径是当轮本机临时归档，不是长期提交内容，也不要求永久存在。每个 archive 当前保留 `analyze_bench_compare_1.log` 到 `analyze_bench_compare_5.log`；repeated summary 从这些 analyze log 中的 std/RVV raw ms 重新计算 speedup。

## Current Block Summary

| Benchmark Item | runs | 64K speedup median/min/p10 | 256K speedup median/min/p10 | values |
| --- | ---: | --- | --- | --- |
| `lls production-dispatch full-cloud pointnormal` | 5 | `2.68x/2.67x/2.68x` | `2.70x/2.62x/2.64x` | 64K: `2.70x, 2.69x, 2.67x, 2.68x, 2.68x`; 256K: `2.70x, 2.70x, 2.62x, 2.67x, 2.71x` |
| `lls production-dispatch full-cloud pointxyz-to-pointnormal` | 5 | `2.92x/2.91x/2.91x` | `2.37x/1.33x/1.35x` | 64K: `2.92x, 2.91x, 2.91x, 2.95x, 2.96x`; 256K: `2.95x, 1.39x, 2.37x, 2.95x, 1.33x` |
| `lls production-dispatch full-cloud pointxyz-to-pointxyzinormal` | 5 | `2.92x/2.89x/2.89x` | `2.83x/1.23x/1.46x` | 64K: `2.92x, 2.89x, 2.89x, 2.93x, 2.94x`; 256K: `2.83x, 1.23x, 2.85x, 2.92x, 1.82x` |

current block 仍是历史 production-shaped path 的有效 baseline，但 256K generic source/target rows 出现明显低谷，且 reviewer cleanup 后不再是 production 可配置 selector。

## Fused-Formula Summary

| Benchmark Item | runs | 64K speedup median/min/p10 | 256K speedup median/min/p10 | values |
| --- | ---: | --- | --- | --- |
| `lls production-dispatch full-cloud pointnormal` | 5 | `2.80x/2.77x/2.78x` | `2.82x/2.81x/2.81x` | 64K: `2.81x, 2.77x, 2.80x, 2.80x, 2.78x`; 256K: `2.82x, 2.81x, 2.81x, 2.83x, 2.83x` |
| `lls production-dispatch full-cloud pointxyz-to-pointnormal` | 5 | `3.13x/3.11x/3.12x` | `3.15x/3.14x/3.14x` | 64K: `3.13x, 3.13x, 3.13x, 3.13x, 3.11x`; 256K: `3.14x, 3.15x, 3.16x, 3.16x, 3.14x` |
| `lls production-dispatch full-cloud pointxyz-to-pointxyzinormal` | 5 | `3.11x/3.11x/3.11x` | `3.14x/3.12x/3.12x` | 64K: `3.12x, 3.11x, 3.11x, 3.11x, 3.11x`; 256K: `3.12x, 3.14x, 3.14x, 3.14x, 3.12x` |

fused-formula production-dispatch 三类代表点型在 64K/256K、5-run repeated board A/B 中稳定正向；没有发现 current block 256K 那种低谷或回退。该 evidence 允许 fused-formula 成为唯一 production block；current baseline 留在 `test_support/` 和历史 summary 中。结论仍只覆盖 full-cloud / f32 AoS layout-gated / `Scalar=float` / representative point types 边界。

QEMU timing 不作为性能结论。本文件只索引板卡 repeated summary；QEMU correctness、反汇编和本地测试结果见 evaluation 文档。
