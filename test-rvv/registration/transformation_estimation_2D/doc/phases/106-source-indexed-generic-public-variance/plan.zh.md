# Phase 106 Plan: source-indexed-generic-public-variance

## 阶段意图和边界

本阶段只做 source-indexed generic public representative 20-run variance evidence。
必须使用独立 board run label / evidence dir：

```text
source_indexed_generic_xyz_point_types_public_variance_repeated
```

不得覆盖 Phase 103/104 的历史证据，不自动采纳 generic widening，不回滚
Phase 091 已采纳的 narrow patch，不创建 commit。

要证明：

- source-indexed public overload 的 representative PointXYZ-like same/mixed 组合在
  20-run board 上的方差是否稳定；
- correctness、QEMU smoke、asm、board repeated、Evidence Doctor、registry 和文档链路
  均能闭合；
- Phase 103/104 仍应保持 guarded probe，不能在本阶段自动写成 adopted。

不证明：

- source-indexed generic widening 已经 adopted；
- dual-indexed / correspondence 也可直接继承本阶段结论；
- QEMU timing 代表真实性能。

## 代表性范围

| 维度 | validated_scope |
| --- | --- |
| row source | source-indexed-cloud-pair：`source[indices_src[i]] -> target[i]`。 |
| public overload | `estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, matrix)`。 |
| point types | `PointXYZ`、`PointXYZI`、`PointNormal`、`PointXYZINormal` same-type。 |
| mixed pairs | `PointXYZI -> PointXYZ`、`PointXYZ -> PointXYZI`、`PointNormal -> PointXYZINormal`、`PointXYZINormal -> PointNormal`。 |
| `Scalar` | `float`。 |
| layout | source/target 各自满足 `RVVXYZAoSFloatLayout<PointT>`。 |
| runtime gate | 32-bit `pcl::index_t`、valid source indices、`indices_src.size()==cloud_tgt.size()`、size >= 16、source/target dense、selected source rows 和 target prefix rows finite。 |
| board budget | 20 runs、20 iterations、5 warmup iterations。 |
| evidence role | production-public variance probe；只回答同一 public boundary 下的方差和 bucket，不自动升级结论。 |

## 当前状态清单

| item | current state |
| --- | --- |
| Phase 091 | source-indexed `PointXYZ -> PointXYZ` 已被用户采纳，保留。 |
| Phase 103 | source-indexed generic public probe 已完成，但仍是 guarded probe。 |
| Phase 104 | 单 case long-tail diagnostic 已完成，复现到 positive 但仍有 variance Warning。 |
| Phase 105 | evidence freshness and recovery sync 已完成。 |
| 本阶段 | 运行独立 public variance 20-run board，并同步 QEMU/asm/Doctor/registry/docs。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| source-indexed generic public variance | source-indexed | representative PointXYZ-like same/mixed pairs、`float`、source/target independent AoS xyz | same public overload | reuse current 84/84 correctness and refresh registry | `source-indexed-generic-xyz-point-types-public-variance` | 20-run Milkv-Jupiter repeated | production public source-indexed generic boundary | QEMU + board Doctor | planned / guarded variance evidence |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| P106-1 plan freeze | 本文件 | 写清独立 board run label / evidence dir、20-run 预算和停止条件。 |
| P106-2 correctness refresh | `run_test_compare`、`record_qemu_correctness_state` | 当前 correctness 与 registry 同步。 |
| P106-3 QEMU smoke / asm | `record_qemu_source_indexed_generic_public_variance_state` | QEMU smoke、asm attribution、manifest、Doctor 生成。 |
| P106-4 board repeated | `run_board_bench_source_indexed_generic_xyz_point_types_public_variance_repeated` | 20-run board summary、manifest、Doctor 生成，路径独立。 |
| P106-5 docs / handoff / registry | phase result、README、roadmap、matrix、current Handoff | 文档和 registry 同步，但不把 generic widening 自动写成 adopted。 |

## 继续 / 停止条件

完成 Phase 106 后，如果 evidence positive，也只停在用户决策点：

- 不能自动把 Phase 103/104 写成 adopted；
- 不能自动回滚 Phase 091 narrow patch；
- 不能自动扩展到 dual-indexed / correspondence。

默认下一步是先跑 correctness，再跑 QEMU smoke / asm / Doctor，然后跑 20-run board repeated。
