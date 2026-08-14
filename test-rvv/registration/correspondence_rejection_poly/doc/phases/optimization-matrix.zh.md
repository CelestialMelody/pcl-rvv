# correspondence_rejection_poly 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `edge_length_batch` | correspondences（对应关系） | `PointXYZ` / `float` / precomputed distances | test-only edge batch helper | pass: `run_test_compare` | QEMU smoke: `run_bench_edge_batch_smoke` | historical `weak_positive`: `log/board/edge_batch_repeated/summary.md` | partial: RVV 指令存在，helper 内联 | clean | attempted diagnostic | 已由 production-direct 探针复核；不接 production |
| `edge_gather_staging` | correspondences | `PointXYZ` / `float` / AoS point load + staging buffers | test-only production-shaped gather helper | pass: QEMU / board correctness | QEMU smoke: `run_bench_edge_gather_staging_smoke` | historical `weak_positive`: `log/board/edge_gather_staging_repeated/summary.md` | partial: bench binary 中有 RVV formula 指令 | clean | historical diagnostic | 不能替代 production direct |
| `production_edge_batch_rvv` | correspondences | `PointXYZ` / `float` / xyz AoS | real public entry probe before rollback | pass: QEMU / board 8-test correctness | `production-direct` probe，默认 target 已加显式开关 | `negative`: `log/board/production_direct_repeated/summary.md` | historical probe: `getRemainingCorrespondencesRVV` 符号在回滚前 asm 中可见 | Errors=2: degradation frequency | `rollback/no-production` | none；若继续需另开 profile / 消融候选 |
| `accept_rate_filter` | contiguous counters（连续计数数组） | `int` counters -> `float` rates | test-only accept-rate helper + scalar append | pass: `run_test_compare` | QEMU smoke: `run_bench_acceptance_smoke` | `neutral`: `log/board/acceptance_filter_confirm/summary.md` | partial: RVV 指令存在，helper 内联 | warning in confirm | attempted diagnostic/no-production | profile 指向时再恢复 |
| `full_entry_scalar_smoke` | correspondences | `PointXYZ` / `float` | real `CorrespondenceRejectorPoly` public entry, fixed seed | pass: deterministic + seeded random public-entry tests | `full-entry` smoke | board correctness pass | not_applicable after rollback | manual | adopted scalar regression | none |
| `histogram_otsu_scalar` | not_applicable | accept-rate values | scalar semantic guard | pass: histogram / Otsu tests | not scheduled | not_applicable | not_applicable | manual | adopted scalar | none |
| `structure_parity_doc_suite` | not_applicable | topic-local documentation | README、testing overview、correctness、benchmark/evidence、optimization evidence、code map、evaluation、phase index、Handoff | not_applicable | not_applicable | not_applicable | not_applicable | manual doc-suite quality bar | completed Phase 040 | none |

## 当前停止条件

`production_edge_batch_rvv` 的真实公开入口探针在板卡上退化，并且 Evidence Doctor 报告 Error。生产补丁已回滚。Phase 040 文档套件结构对齐已完成。当前矩阵没有仍在授权范围内、能改变 production 决策的未阻塞动作；后续性能探索需要新的 profile / ablation 假设。
