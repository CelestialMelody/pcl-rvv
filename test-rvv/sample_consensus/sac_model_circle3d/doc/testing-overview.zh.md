# sac_model_circle3d 测试总览

| target | 主测试类型 | 主要日志 / summary | 证据边界 |
| --- | --- | --- | --- |
| `run_circle3d_phase000_tests` | Phase 000 correctness（正确性） | `log/qemu/run_test_rvv.log` | 验证 test-only projection candidate 与 public count/select 一致；不证明性能。 |
| `run_test_compare` | Std/RVV correctness aggregate（汇总正确性入口） | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` | QEMU 只证明正确性和路径。 |
| `run_bench_rvv BENCH_ARGS='1024 2 1'` | QEMU log-shape smoke（日志形状冒烟） | `log/qemu/run_bench_rvv.log` | 验证 bench 字段和 `warmup_iterations` 可解析；不证明性能。 |
| `dump_bench_rvv` | asm attribution（反汇编归属）输入 | `build/asm/riscv/bench_sac_model_circle3d_rvv.full.asm` | 只证明指令存在和归属，不证明性能。 |
| `check_projection_asm` | asm gate（反汇编门禁） | `build/asm/riscv/bench_sac_model_circle3d_rvv.full.asm` | 确认 count/select candidate 符号内有目标 RVV 指令。 |
| `collect_projection_repeated_board_evidence` | board repeated component ablation | `log/board/repeated-20260828-phase000-circle3d-projection/run-*/` | 5-run raw logs；默认不提交。 |
| `collect_select_production_repeated_board_evidence` | historical board repeated production-public（已回滚） | `log/board/repeated-20260828-phase010-circle3d-select-production/run-*/` | `PointXYZ` 10-run raw logs；保留为历史证据，不再是当前可运行 closeout target。 |
| `collect_select_xyzi_repeated_board_evidence` / `collect_select_xyzrgb_repeated_board_evidence` / `collect_select_xyzrgba_repeated_board_evidence` | board repeated point-type expansion | `log/board/repeated-20260828-phase020-select-*/run-*/` | 三个扩展点型的 5-run raw logs；用于证明不扩大 traits-gated 泛型范围。 |
| `record_projection_board_evidence_state` | doctor / registry alias | `doc/phases/000-circle3d-projection-component-ablation/projection-repeated-evidence-manifest.json`、`projection-repeated-evidence-doctor.md`、`projection-repeated-evidence-doctor.json`、`log/evidence_registry.json` | 生成并登记 current summary artifacts。 |
| `record_select_production_board_evidence_state` | historical doctor / registry alias（已回滚） | `doc/phases/010-selectwithin-production-probe/select-production-repeated-evidence-manifest.json`、`select-production-repeated-evidence-doctor.md`、`select-production-repeated-evidence-doctor.json`、`log/evidence_registry.json` | 生成并登记 production-public summary；仅保留历史记录。 |
| `record_select_*_board_evidence_state` | doctor / registry alias | `doc/phases/020-select-point-type-expansion/select-*-repeated-evidence-*`、`log/evidence_registry.json` | 生成并登记点类型扩展 summary；当前三个点型均 rejected with evidence。 |
| `projection_evidence_status` | evidence freshness check（证据新鲜度检查） | 同上 | 要求文档引用已登记 summary artifacts。 |
| `select_production_evidence_status` / `select_*_evidence_status` | historical evidence freshness check | 同上 | 只用于回看历史摘要；当前 no-production closeout 不再把它当作活跃门禁。 |
