# ICP transformCloud Benchmark 与证据说明

## 本文职责

本文说明 `bench_icp` 的 label、计时边界、checksum 口径、QEMU/board 证据边界、Evidence Doctor、
manifest、registry 和提交边界。优化方式如何映射到这些证据见 `doc/optimization-evidence.zh.md`。

## Bench 输出格式

`src/bench_icp.cpp` 是薄入口，实际 bench harness 在 `include/bench_icp.h`。运行时输出固定包含：

```text
PCL registration/icp transformCloud diagnostic benchmark
Build: Std (__RVV10__ disabled) 或 Build: RVV (__RVV10__ enabled)
Dataset: synthetic PointXYZ and PointNormal clouds; production transformCloud full-cloud
Iterations: 20
Warmup Iterations: 3
<case label>: <ms/iter> ms/iter
  Total Time: <ms>, checksum: <coarse checksum>
```

`analyze_bench_compare.py` 依赖 `<case label>: <ms/iter> ms/iter` 和 `checksum:` 形状解析 Std/RVV 对比。

## Bench Label 语法

| label | point type | size | timer boundary |
| --- | --- | ---: | --- |
| `icp transform-cloud xyz 64K` | `PointXYZ` | 65536 | production `transformCloud` only |
| `icp transform-cloud xyz 256K` | `PointXYZ` | 262144 | production `transformCloud` only |
| `icp transform-cloud xyz-normal 64K` | `PointNormal` | 65536 | production `transformCloud` only |
| `icp transform-cloud xyz-normal 256K` | `PointNormal` | 262144 | production `transformCloud` only |

Label 中 `transform-cloud` 是 case-filter token。它只表示 ICP `transformCloud` full-cloud microbench，
不包括 nearest-neighbor search、correspondence rejector 或 transformation estimation。

## Case-Filter 字典

| filter | 包含 case | 推荐用途 |
| --- | --- | --- |
| `transform-cloud` | 上表 4 个 case | board repeated production direct evidence。 |
| none | 上表 4 个 case | 本 topic 当前没有其它 bench family；默认等同 `transform-cloud`。 |

## 推荐 Target

| 目标 | 命令 | 作用 | 是否可作为性能结论 |
| --- | --- | --- | --- |
| QEMU correctness | `make -C test-rvv/registration/icp run_test_compare record_qemu_correctness_state` | Std/RVV gtest 对拍。 | no |
| board correctness | `make -C test-rvv/registration/icp run_board_test fetch_board_logs` | 目标硬件 production direct correctness。 | no |
| board repeated | `make -C test-rvv/registration/icp collect_board_transform_cloud_repeated run_board_evidence_doctor record_board_evidence_state` | 5-run board Std/RVV compare、Doctor、registry。 | yes |
| asm attribution | `make -C test-rvv/registration/icp dump_bench_rvv` | 生成 RVV binary 反汇编并人工归因。 | no |
| evidence status | `make -C test-rvv/registration/icp evidence_status` | 检查已登记 evidence 与文档引用。 | no |

QEMU `run_bench_compare` 默认被公共 Makefile guard 阻止。不要在 QEMU 上运行完整 bench compare；若为了历史
log-shape smoke 显式设置 `ALLOW_QEMU_BENCH_COMPARE=1`，结果必须标成 `qemu_smoke_only`。

## 计时边界

Bench 使用 `ExposedICP` 调用 protected `IterativeClosestPoint::transformCloud`。每次 iteration 内只执行：

1. 对固定 synthetic input 调用 production `transformCloud`。
2. 对 output 计算粗粒度 checksum。
3. 用 `doNotOptimize(output)` 防止编译器删掉输出依赖。

计时不包括 ICP 的 source/target 设置、correspondence search、rejector、SVD 求解、收敛判断或可视化回调。

## Checksum 来源

`support::checksumXYZ` 每 17 个点抽样 XYZ；`support::checksumXYZNormal` 复用 XYZ checksum，并每 19 个点
抽样 normal。checksum 经过两位小数 bucket 化，目的是给 bench log 一个路径指纹，避免 RVV FMA 合法舍入差异
被误报为 checksum 不一致。正式数值等价以 `doc/correctness-tests.zh.md` 中的 gtest 为准。

## 当前 Production Evidence

板卡 repeated summary：
`test-rvv/registration/icp/log/board/transform_cloud_repeated/summary.md`

| case | runs | median | min | max | p10 | p90 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `icp transform-cloud xyz 64K` | 5 | 5.68x | 5.29x | 6.50x | 5.34x | 6.37x |
| `icp transform-cloud xyz 256K` | 5 | 5.30x | 5.17x | 5.32x | 5.21x | 5.32x |
| `icp transform-cloud xyz-normal 64K` | 5 | 3.76x | 3.62x | 3.97x | 3.62x | 3.89x |
| `icp transform-cloud xyz-normal 256K` | 5 | 3.93x | 3.85x | 4.07x | 3.87x | 4.02x |

运行上下文：

| key | value |
| --- | --- |
| device | `Milkv-Jupiter` |
| evidence role | `production_direct` |
| runs / iterations / warmup | `5 / 20 / 3` |
| governor / freq / temperature | `performance / 1600000 / 41000` |
| taskset | `not_pinned` |

## Evidence Doctor / Manifest 边界

| 文件 | 作用 |
| --- | --- |
| `log/board/transform_cloud_repeated/evidence_manifest.json` | 记录 4 个 `production_direct` comparison、计时边界、checksum policy、asm boundary 和 board metadata。 |
| `log/board/transform_cloud_repeated/evidence_doctor.md` | 对 board repeated summary 做异常模式检查。 |
| `log/evidence_registry.json` | 本机 evidence freshness registry；默认不在本提交中刷新。 |

当前 board Doctor 结果为 Errors=0，Warnings=2，Suggestions=0。两个 warning 都来自
`icp transform-cloud xyz 64K`：`long_tail_or_variance` 和 `group_outlier`。该 case 的全部 run 均明显正向，
因此 EvidenceDecision 不降级；文档保留 min/median/max，不把该收益外推到其它 case。

## ASM Attribution 口径

反汇编归因写在 `doc/asm-attribution.zh.md`。该文档说明 RVV 指令簇位于 production
`IterativeClosestPoint::transformCloud` 符号，Std fallback 有独立
`pcl::registration::detail::transformCloudStandard` 符号。ASM 只证明 hot path 和 fallback 可归因；
性能结论仍以 board repeated summary 为准。

## 提交边界

默认可提交：

- topic-local 文档、phase 文档和长期 `doc-rvv`。
- board repeated `summary.md` 和 `evidence_doctor.md` 这类摘要证据。
- 历史 QEMU smoke Doctor，仅用于说明该证据不参与性能结论。

默认不提交：

- `build/` 二进制、反汇编 raw output 和中间对象。
- `log/qemu/run_bench_*.log`、`log/qemu/analyze_bench_compare.log`。
- `log/board/run_bench_*.log`、`log/board/analyze_bench_compare.log`。
- collector 默认临时目录里的 per-run raw compare logs。
