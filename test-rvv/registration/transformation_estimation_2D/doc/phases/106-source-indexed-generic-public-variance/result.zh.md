# Phase 106 Result: source-indexed-generic-public-variance

## 当前阶段状态

本阶段已完成 correctness 复核、QEMU smoke、asm attribution、QEMU Evidence Doctor、
独立 board 20-run repeated、board Evidence Doctor 和 registry 记录。board 证据使用独立
run label / evidence dir：

```text
test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_generic_xyz_point_types_public_variance_repeated/
```

该路径没有覆盖 Phase 103/104 历史证据。

最终阶段结论：

```text
completed / board negative for full source-indexed generic public representative variance /
no clean adoption support
```

这不是 adopted 结论，也不是回滚授权。Phase 103/104 source-indexed generic widening 仍只能写成
guarded probe / pending user decision；Phase 091 source-indexed `PointXYZ -> PointXYZ`
narrow patch 继续保留。

## 已完成内容

| 项 | 结果 | 证据 |
| --- | --- | --- |
| correctness | Std/RVV 84/84 pass | `make -C test-rvv/registration/transformation_estimation_2D record_qemu_correctness_state` |
| QEMU smoke | 16-case representative public variance smoke pass | `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_generic_xyz_point_types_public_variance/run_bench_source_indexed_generic_xyz_point_types_public_variance_rvv.log` |
| asm attribution | `production_public_source_indexed_generic_boundary` 可归属 | `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_generic_xyz_point_types_public_variance/asm_attribution.md` |
| QEMU manifest | 16 comparisons, `qemu_smoke_only` | `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_generic_xyz_point_types_public_variance/evidence_manifest.json` |
| QEMU asm json | independent Phase 106 asm summary data | `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_generic_xyz_point_types_public_variance/asm_attribution.json` |
| QEMU Doctor | Errors=0, Warnings=0, Suggestions=0 | `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_generic_xyz_point_types_public_variance/evidence_doctor.md` |
| board repeated | 20-run Milkv-Jupiter representative variance completed | `test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_generic_xyz_point_types_public_variance_repeated/summary.md` |
| board manifest | 16 comparisons, row_source corrected to `source_indexed_cloud_pair` | `test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_generic_xyz_point_types_public_variance_repeated/evidence_manifest.json` |
| board Doctor | Errors=1, Warnings=27, Suggestions=0 | `test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_generic_xyz_point_types_public_variance_repeated/evidence_doctor.md` |
| registry | QEMU 和 board evidence 均已登记 | `test-rvv/registration/transformation_estimation_2D/log/evidence_registry.json` |

## QEMU smoke 细节

运行命令：

```bash
make -C test-rvv/registration/transformation_estimation_2D record_qemu_source_indexed_generic_public_variance_state
```

结果覆盖的代表性 case 与 Phase 103 同一 public source-indexed generic 边界：

- `PointXYZ -> PointXYZ`
- `PointXYZI -> PointXYZI`
- `PointNormal -> PointNormal`
- `PointXYZINormal -> PointXYZINormal`
- `PointXYZI -> PointXYZ`
- `PointXYZ -> PointXYZI`
- `PointNormal -> PointXYZINormal`
- `PointXYZINormal -> PointNormal`

QEMU doctor 无 Error / Warning / Suggestion，说明
`test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_generic_xyz_point_types_public_variance/evidence_manifest.json`
和 asm 归属都闭合，但 QEMU timing 不作为性能结论。

## Board 20-run 结果

运行命令：

```bash
make -C test-rvv/registration/transformation_estimation_2D run_board_bench_source_indexed_generic_xyz_point_types_public_variance_repeated TE2D_BOARD_REPEATED_RUNS=20
```

结果摘要：

| bucket | case 数 | 说明 |
| --- | ---: | --- |
| `positive` | 12 | 多数 `PointXYZ` / `PointXYZI` / mixed 64K 和部分 `PointNormal` / `PointXYZINormal` case 中位数正向且无 `B/A<1`。 |
| `weak_positive` | 1 | `PointNormal->PointXYZINormal 64K` median `2.441x`，min `1.123x`，无 `B/A<1`，但长尾明显。 |
| `negative` | 3 | `PointNormal->PointNormal 256K`、`PointXYZINormal->PointNormal 64K`、`PointXYZINormal->PointXYZINormal 256K` 出现 `B/A<1` 或 negative bucket。 |

重点 negative case：

| case | median | min | max | p10 | p90 | `B/A<1` | bucket |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `PointNormal->PointNormal 256K` | `1.574x` | `0.694x` | `2.652x` | `0.761x` | `2.596x` | `7/20` | `negative` |
| `PointXYZINormal->PointNormal 64K` | `2.507x` | `0.812x` | `2.780x` | `1.719x` | `2.614x` | `1/20` | `negative` |
| `PointXYZINormal->PointXYZINormal 256K` | `2.648x` | `0.753x` | `2.807x` | `1.542x` | `2.728x` | `1/20` | `negative` |

`PointXYZ->PointXYZ` 仍稳定正向：4K / 64K / 256K median 分别为
`4.118x / 4.797x / 4.607x`，`B/A<1=0/20`。这支持 Phase 091 窄范围已采纳 patch
继续保留，但不能覆盖 source-indexed generic widening 的负向代表性 case。

## Evidence Doctor 解释

board Doctor：

```text
Errors=1, Warnings=27, Suggestions=0
```

唯一 Error 是 `PointNormal->PointNormal 256K` 的退化频率：20-run 中 `7/20`
低于 1。Warnings 主要来自 long-tail / variance 和 group outlier。按 Evidence Doctor
策略，不能只用 median 正向掩盖退化频率；完整 representative generic public variance
不能写成 stable positive 或 clean-adopt。

本阶段还修正了 board summary 生成脚本中的 metadata 漏项：`source-indexed-generic-xyz-point-types-public-variance`
现在在 manifest summary 和 comparison 中都登记为 `source_indexed_cloud_pair`，而不是误标为
`ordered_cloud_pair`。该修正不改变 board 原始 run 数值，只纠正证据元数据。

## 决策边界

- Phase 106 独立 20-run 证据不支持 full source-indexed generic public widening clean adoption。
- Phase 103/104 仍保持 guarded probe / pending user decision，不能写成 adopted。
- Phase 091 narrow production patch 继续保留。
- 不自动回滚 Phase 103/104 guarded probe，也不自动提交。
- 若用户仍希望保留 source-indexed generic widening，需要显式选择收窄范围、继续追查负向 case 或另开 bounded candidate phase。
