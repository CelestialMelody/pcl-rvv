# Phase 110 Result: source-indexed-pointxyzi-exact-public-probe

## 当前结论

Phase 110 已完成 source-indexed-cloud-pair（源索引点云对）exact
`PointXYZI -> PointXYZI` production-public probe（真实公开入口生产探针）。当前证据为
positive：板卡 20-run repeated 在 4K / 64K / 256K 三个规模的 median B/A 分别为
`3.979x / 3.547x / 3.602x`，每个规模 `B/A<1=0/20`。Board Evidence Doctor（证据体检）
为 `0/3/0`，三个 Warning 都是 long-tail / variance（长尾或方差）提示。

本阶段结论只支持一个有界生产候选：source-indexed exact `PointXYZI -> PointXYZI`、
`Scalar=float`、valid source indices、dense finite、size >= 16。它不证明
source-indexed generic widening（泛型扩大），也不改变 Phase 103/104 guarded probe
（受保护探针）和 Phase 106 full representative generic negative 的结论。

当前 production patch 在 Phase 110 结束时保留在源码中等待用户决策；随后 Phase 112 已按用户确认把
该 exact `PointXYZI -> PointXYZI` probe 转为 adopted / retained production behavior。

## 执行范围回填

| 计划动作 | 状态 | 证据 / 说明 |
| --- | --- | --- |
| 新建 Phase 110 plan/result | done | `doc/phases/110-source-indexed-pointxyzi-exact-public-probe/plan.zh.md` 和本文。 |
| 有界 production probe patch | done / pending decision | `tryTransformationEstimation2DSourceIndexedCloudPairRVV` exact gate 当前允许 `PointXYZ -> PointXYZ` 或 `PointXYZI -> PointXYZI`；其它点型仍 fallback（回退）到标量。 |
| PointXYZI-only bench filter 和独立 label | done | case-filter：`source-indexed-pointxyzi-public`；board label：`source_indexed_pointxyzi_public_phase110_repeated`。 |
| correctness | done | `make -C test-rvv/registration/transformation_estimation_2D run_test_compare`；Std `84/84` pass，RVV `84/84` pass。 |
| QEMU smoke / asm / Doctor | done | `record_qemu_source_indexed_pointxyzi_public_state`；QEMU Doctor `0/0/0`；asm summary 中 `production_public_source_indexed_generic_boundary` 覆盖 `PointXYZI -> PointXYZI` source-indexed public overload，focused category 85 RVV lines，包含 `vlsseg3e32.v`、`vluxseg3ei32.v`、`vfmacc`、`vfredosum`。QEMU timing 不作为性能证据。 |
| board repeated / Doctor / registry | done | 20-run Milkv-Jupiter board repeated 已完成并登记；Board Doctor `0/3/0`。 |
| 文档 / Handoff 同步 | done | README、history、matrix、roadmap、evaluation、长期 `doc-rvv`、Handoff 和 `evidence_status` doc inputs 已纳入 Phase 110。 |

## Board 20-run 证据

路径：

```text
test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_pointxyzi_public_phase110_repeated/summary.md
test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_pointxyzi_public_phase110_repeated/evidence_manifest.json
test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_pointxyzi_public_phase110_repeated/evidence_doctor.md
```

| case | runs | median | min | max | p10 | p90 | B/A<1 | bucket |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `PointXYZI->PointXYZI 4K` | 20 | `3.979x` | `3.437x` | `4.070x` | `3.867x` | `4.040x` | 0 | positive |
| `PointXYZI->PointXYZI 64K` | 20 | `3.547x` | `1.883x` | `4.015x` | `2.566x` | `3.805x` | 0 | positive |
| `PointXYZI->PointXYZI 256K` | 20 | `3.602x` | `2.090x` | `3.738x` | `2.169x` | `3.694x` | 0 | positive |

Board Doctor 的三个 Warning 都提示长尾 / 方差：64K 和 256K 的 min 明显低于 median，但仍没有
below-1 run，decision bucket 保持 positive。结论中保留 min / median / max 和 Warning 数量，
不剔除异常值，也不把本阶段写成 clean generic adoption。

## QEMU / asm 证据

路径：

```text
test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_pointxyzi_public/run_bench_source_indexed_pointxyzi_public_rvv.log
test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_pointxyzi_public/evidence_manifest.json
test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_pointxyzi_public/evidence_doctor.md
test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_pointxyzi_public/asm_attribution.md
```

QEMU manifest 只记录 smoke（小型验证）和路径命中；脚本刻意不把 QEMU 的 observed_ms 写成
Std/RVV performance comparison。asm attribution（反汇编归属）显示 source-indexed
`PointXYZI -> PointXYZI` public overload 中存在 production RVV 边界，说明当前 probe patch
确实能命中 RVV helper。

## diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | `production-public`。board 证据回答真实 public source-indexed overload 中当前 RVV path 是否快于 public scalar path。 |
| A/B boundary | Std build public source-indexed `PointXYZI -> PointXYZI` overload vs RVV build 同一 overload；timer boundary 为 public estimate + 2D solve。 |
| 当前决策问题 | 是否将 exact `PointXYZI -> PointXYZI` source-indexed production probe 保留为 adopted production behavior。 |
| 是否可外推到 production | yes for this exact public boundary；no for full source-indexed generic widening。 |
| comparison-boundary / baseline mismatch 风险 | 与 Phase 106 full generic representative 的风险已通过独立 label 和 PointXYZI-only filter 隔离；但本阶段没有做 full generic family selection。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段为 positive，进入用户决策点；若用户不采纳或要求退回，才回滚 probe patch。 |
| clean adoption 是否需要 RVV-vs-RVV detail A/B | 若只采纳 exact public scalar-vs-RVV path，可由用户在 PI5 检查点确认；若要声明实现族最优或扩大泛型，需要另开同边界 family A/B 和 representative variance。 |

## Optimization Matrix 更新

新增矩阵条目：

- candidate family：source-indexed PointXYZI exact public probe。
- row source：source-indexed-cloud-pair。
- scope：exact `PointXYZI -> PointXYZI`、`Scalar=float`、valid source indices、dense finite、size >= 16。
- decision：positive bounded production probe；Phase 112 superseded user-decision state and adopted the exact gate。
- unblocked next action：用户决策；不能自动扩大到 Phase 103/104 generic widening。

## Continue / Stop Decision

`continue_stop_decision`：turn-stop at user decision checkpoint。

`stop_condition_hit`：继续会把 Phase 110 production probe 从 evidence-positive candidate 变成 adopted
production behavior，属于需要用户确认的 PI5 决策边界。

`next_phase_default`：

1. 若用户确认采纳：进入 Phase 111 `pointxyzi-source-indexed-adoption-closeout`，把 probe 写成 adopted，刷新 `doc-rvv`、evaluation、matrix、Handoff，并准备 topic-only commit 候选。
2. 若用户要求退回：进入 Phase 111 rollback closeout，回退 exact `PointXYZI -> PointXYZI` gate 并重跑接入后测试 / evidence_status。
3. 若用户要求继续探索但不采纳：优先按 roadmap 进入 source-indexed negative-case investigation 或新的 bounded candidate；不得把 Phase 110 外推为 generic widening。
