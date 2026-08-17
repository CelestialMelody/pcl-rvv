# correspondence_rejection_poly 优化矩阵

当前矩阵同时保留历史 Phase 030 负向证据和 Phase 050 的临时 production patch replay。Phase 050 已完成 production-direct 重跑，`production_edge_batch_rvv` 为 `rollback/no-production`：证据支持回滚 / 不采纳，用户已明确确认，生产补丁已还原。

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `edge_length_batch` | correspondences（对应关系） | `PointXYZ` / `float` / precomputed distances | test-only edge batch helper | pass: `run_test_compare` | QEMU smoke: `run_bench_edge_batch_smoke` | historical `weak_positive`: `log/board/edge_batch_repeated/summary.md` | partial: RVV 指令存在，helper 内联 | clean | attempted diagnostic | 已由 production-direct 探针复核；不接 production |
| `edge_gather_staging` | correspondences | `PointXYZ` / `float` / AoS point load + staging buffers | test-only production-shaped gather helper | pass: QEMU / board correctness | QEMU smoke: `run_bench_edge_gather_staging_smoke` | historical `weak_positive`: `log/board/edge_gather_staging_repeated/summary.md` | partial: bench binary 中有 RVV formula 指令 | clean | historical diagnostic | 不能替代 production direct |
| `production_edge_batch_rvv` | correspondences | `PointXYZ` / `float` / xyz AoS | Phase 050 temporary public entry probe; reverted after user confirmation | pass: QEMU / board 8-test correctness；QEMU production-direct checksum match | `production-direct` target 可运行；当前无 patch 时只测标量 public entry | `negative`: `log/board/production_direct_repeated/summary.md`，2048 median 0.904x、8192 median 0.977x，均 5/5 degradation | Phase 050 full asm 可见 `getRemainingCorrespondencesRVV` / `Standard` 和 RVV 指令 | Errors=2 | `rollback/no-production` | 无 production action；若要求继续，先开 profile / ablation phase |
| `accept_rate_filter` | contiguous counters（连续计数数组） | `int` counters -> `float` rates | test-only accept-rate helper + scalar append | pass: `run_test_compare` | QEMU smoke: `run_bench_acceptance_smoke` | `neutral`: `log/board/acceptance_filter_confirm/summary.md` | partial: RVV 指令存在，helper 内联 | warning in confirm | attempted diagnostic/no-production | profile 指向时再恢复 |
| `full_entry_scalar_smoke` | correspondences | `PointXYZ` / `float` | real `CorrespondenceRejectorPoly` public entry, fixed seed | pass: deterministic + seeded random public-entry tests | `full-entry` smoke | board correctness pass | not_applicable after rollback | manual | adopted scalar regression | none |
| `histogram_otsu_scalar` | not_applicable | accept-rate values | scalar semantic guard | pass: histogram / Otsu tests | not scheduled | not_applicable | not_applicable | manual | adopted scalar | none |
| `structure_parity_doc_suite` | not_applicable | topic-local documentation | README、testing overview、correctness、benchmark/evidence、optimization evidence、code map、evaluation、phase index、Handoff | not_applicable | not_applicable | not_applicable | not_applicable | manual doc-suite quality bar | completed Phase 040 | none |

## 当前停止条件

Phase 050 的真实公开入口 replay 在板卡上仍退化，并且 Evidence Doctor 报告 Error。Phase 040 文档套件结构对齐已完成。用户已确认不接入，生产 patch 已还原；当前停止在 `rollback/no-production` closeout。
