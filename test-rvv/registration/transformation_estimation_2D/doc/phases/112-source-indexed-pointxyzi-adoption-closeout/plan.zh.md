# Phase 112 Plan: source-indexed-pointxyzi-adoption-closeout

## 阶段意图和边界

本阶段把 Phase 110 source-indexed-cloud-pair（源索引点云对）exact
`PointXYZI -> PointXYZI` bounded production probe（有界生产探针）按用户确认转为
adopted / retained production behavior（已采纳并保留的生产行为）。

本阶段只覆盖：

- row source：source-indexed-cloud-pair；
- point type：exact `pcl::PointXYZI -> pcl::PointXYZI`；
- `Scalar=float`；
- dense finite、valid source indices、`indices_src.size() == cloud_tgt.size()`、size >= 16；
- 已在源码中的 `tryTransformationEstimation2DSourceIndexedCloudPairRVV` exact gate。

本阶段不扩大：

- Phase 103/104 source-indexed generic widening guarded probe；
- Phase 106 full representative generic public variance negative；
- Phase 111 Normal 类 source-indexed widening rejection；
- dual-indexed generic、correspondence、`Scalar=double`、RGB/RGBA 或自定义 traits 点型。

## 当前状态清单

| 项目 | 当前状态 |
| --- | --- |
| production source | `registration/include/pcl/registration/impl/transformation_estimation_2D.hpp` 已包含 `PointXYZI -> PointXYZI` exact gate。 |
| Phase 110 correctness | `run_test_compare` 历史结果 Std/RVV `84/84` pass。 |
| Phase 110 QEMU / asm | `record_qemu_source_indexed_pointxyzi_public_state` 历史结果 Doctor `0/0/0`，`production_public_source_indexed_generic_boundary` focused 85 RVV lines。 |
| Phase 110 board | `source_indexed_pointxyzi_public_phase110_repeated` 20-run board：4K/64K/256K `3.979x / 3.547x / 3.602x`，`B/A<1=0/20`，Doctor `0/3/0`。 |
| 用户决策 | 用户已同意有收益的实现接入，并授权整理后提交。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | test | board | asm | Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| source-indexed PointXYZI exact production dispatch | source-indexed-cloud-pair | exact `PointXYZI -> PointXYZI`, `Scalar=float`, 32B AoS xyz offset 0/4/8 | rerun aggregate correctness | reuse Phase 110 20-run board; no overwrite | rerun or verify QEMU asm state | QEMU/board Doctor expected no Error | adopted after user confirmation |

## 实现和测试动作

1. 把长期 `doc-rvv`、evaluation、phase README/history、optimization matrix、roadmap 和 Handoff
   中 Phase 110 的 `pending user adoption` 语言改为 exact `PointXYZI -> PointXYZI` adopted。
2. 保留 source-indexed generic / Normal 负向和 guarded 边界，不把 exact `PointXYZI` 证据外推。
3. 把 Phase 112 plan/result 加入 `evidence_status` doc inputs，确保 registry freshness 能覆盖提交入口。
4. 重新运行接入后验证：`run_test_compare`、`record_qemu_source_indexed_pointxyzi_public_state`、
   `run_board_evidence_doctor_source_indexed_pointxyzi_public`、`evidence_status`、YAML parse 和
   path-limited `git diff --check`。
5. 按 topic-only 策略 staged commit：提交 production/test/doc/phase 摘要，不提交 `log/**`、
   tmp handoff、无关 topic 或本机私有产物。

## Evidence Doctor 和 registry 规则

Phase 112 不覆盖 Phase 110 board raw run，也不新建 board label；它复用
`source_indexed_pointxyzi_public_phase110_repeated` 作为已完成采纳依据。若 Doctor 或 registry
freshness 变化，必须回写本文、result、matrix、roadmap、evaluation 和 Handoff。

## 完成条件

- exact `PointXYZI -> PointXYZI` 的 production 状态在所有正式文档中一致为 adopted / retained；
- Phase 103/104/106/111 generic / Normal 仍明确为 guarded / negative / not adopted；
- 接入后 correctness、QEMU smoke/asm、board Doctor、registry freshness 和 diff check 有新鲜结果；
- staged set 不含 logs、tmp handoff 和无关 dirty files；
- topic-only commit 成功创建，或若验证失败则停在失败证据处不提交。

## 继续 / 停止条件

本阶段完成后默认停止在已提交 topic closeout。用户已明确不继续推进 Normal 类后续优化；correspondence
或其它 point-type inventory 只作为未来另开 phase 的候选，不是本阶段默认继续项。
