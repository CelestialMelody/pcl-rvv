# Phase 112 Result: source-indexed-pointxyzi-adoption-closeout

## 当前结论

Phase 112 按用户确认把 Phase 110 source-indexed-cloud-pair（源索引点云对）exact
`PointXYZI -> PointXYZI` production-public probe（真实公开入口生产探针）转为
adopted / retained production behavior（已采纳并保留的生产行为）。

采纳范围只包括：

- row source：source-indexed-cloud-pair；
- point type：exact `pcl::PointXYZI -> pcl::PointXYZI`；
- `Scalar=float`；
- valid source indices、dense finite、`indices_src.size() == cloud_tgt.size()`、size >= 16；
- 失败时回退原 `ConstCloudIterator` 标量路径。

本阶段不采纳 source-indexed generic widening。Phase 103/104 仍是 guarded probe；Phase 106 full
representative public variance 仍为 negative；Phase 111 Normal 类负向归因仍有效。

## 证据依据

Phase 110/112 已完成同边界证据：

| 证据 | 结果 |
| --- | --- |
| correctness | `record_qemu_correctness_state` fresh rerun；Std/RVV `84/84` pass。 |
| QEMU / asm | `record_qemu_source_indexed_pointxyzi_public_state` fresh rerun；QEMU Doctor `0/0/0`；`production_public_source_indexed_generic_boundary` focused 85 RVV lines。 |
| board repeated | `record_board_source_indexed_pointxyzi_public_state TE2D_BOARD_REPEATED_RUNS=20` 复用独立 label `source_indexed_pointxyzi_public_phase110_repeated`；4K/64K/256K 为 `3.979x / 3.547x / 3.602x`，`B/A<1=0/20`。 |
| Board Doctor | `0/3/0`，Warnings 是 long-tail / variance；无 Error。 |
| registry / diff | `evidence_status` fresh；YAML parse pass；path-limited `git diff --check` pass。 |

## 执行范围回填

| 计划动作 | 状态 | 说明 |
| --- | --- | --- |
| Phase 112 plan/result | done | 新增本阶段 plan/result，作为用户采纳后的收尾入口。 |
| production state sync | done | `doc-rvv`、evaluation、testing overview、benchmark/evidence、optimization evidence、code map、phase README/history、matrix 和 roadmap 已把 exact `PointXYZI -> PointXYZI` 写为 adopted / retained。 |
| generic / Normal 边界 | done | Phase 103/104/106/111 继续写为 guarded / negative / not adopted；不从 exact `PointXYZI` 证据外推。 |
| Makefile registry docs | done | `evidence_status`、correctness record 和 PointXYZI record target 已加入 Phase 112 plan/result doc-ref。 |
| post-adoption verification | done | Fresh correctness、QEMU state、board Doctor、evidence_status、YAML parse 和 diff check 已通过。 |

## Continue / Stop Decision

`continue_stop_decision`：准备 topic-only commit。

`next_phase_default`：无默认性能探索。用户已决定不继续推进 Normal 类 source-indexed 优化；correspondence
或其它 point-type 扩展只在未来明确授权时另开独立 phase。
