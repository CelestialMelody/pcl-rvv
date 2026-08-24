# Phase 041 结果：generic point type public board / ASM

## EvidenceDecision

`positive`。

本阶段把 Phase 040 的代表点型 correctness scout 继续推进到 generic public board evidence 和 ASM attribution。当前 `PointXYZI` / `PointXYZRGB` 代表点型在 public path 上有 QEMU smoke、board repeated summary 和 Evidence Doctor，可作为已采纳 production behavior 的泛型覆盖扩展证据，但仍不能外推到全部 xyz AoS 点型或其它 row source。

## 实际修改

| 文件 | 修改 | 证据角色 |
| --- | --- | --- |
| `src/bench_tesvd_scale.cpp` | 新增 `generic-xyz-point-types-public` case-filter 和代表点型 bench cases。 | 让 generic public smoke / board repeated 可隔离运行。 |
| `script/generate_tesvd_scale_qemu_evidence_manifest.py` | 让 generic public smoke 具备单独 manifest 语义。 | 生成 generic public QEMU smoke manifest / doctor。 |
| `script/generate_tesvd_scale_board_repeated_summary.py` | 支持 generic public repeated board summary。 | 生成 generic public board summary / manifest / doctor。 |

## 证据结果

| target | result |
| --- | --- |
| `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_generic_public_state` | QEMU generic public smoke Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_generic_xyz_point_types_public_repeated` | 5-run board repeated completed；summary / manifest / doctor / registry 已登记。 |

## board repeated 摘要

| case | median B/A | bucket |
| --- | ---: | --- |
| `PointXYZI -> PointXYZI 4K` | `23.679x` | `positive` |
| `PointXYZI -> PointXYZI 64K` | `27.556x` | `positive` |
| `PointXYZI -> PointXYZI 256K` | `27.237x` | `positive` |
| `PointXYZI -> PointXYZRGB 64K` | `25.532x` | `positive` |
| `PointXYZRGB -> PointXYZ 64K` | `28.268x` | `positive` |
| `PointXYZRGB -> PointXYZRGB 4K` | `24.602x` | `positive` |
| `PointXYZRGB -> PointXYZRGB 64K` | `25.340x` | `positive` |
| `PointXYZRGB -> PointXYZRGB 256K` | `27.483x` | `positive` |

Evidence Doctor 结果为 `Errors=0`、`Warnings=0`、`Suggestions=0`。

## 证据边界

| 维度 | 结论 |
| --- | --- |
| production source | 未修改；只扩展 topic-local bench / manifest / summary / docs。 |
| correctness | 已有 Phase 040 correctness scout 作为前提。 |
| QEMU smoke | 正向；只证明日志形状与 manifest。 |
| ASM attribution | 已可从 RVV asm 中看到 generic public bench 实例和 RVV 指令归属。 |
| board repeated | 正向；只覆盖代表点型和当前 public generic 入口。 |
| all generic point types | 未关闭；当前仍是代表点型。 |

## optimization matrix 更新

`generic-point-type-expansion` 从 `correctness_scout_positive_board_pending` 更新为 `positive_generic_public_board_complete`。下一步若继续推进，应独立扩大更多 xyz AoS 点型或新的 row source，而不是重写当前 generic public board 结论。

## 下一步

默认恢复入口回到 row-source / 更广泛 generic 扩展队列。当前 generic point type public board / ASM 已闭合，后续只需要按新的 phase 继续扩边界。
