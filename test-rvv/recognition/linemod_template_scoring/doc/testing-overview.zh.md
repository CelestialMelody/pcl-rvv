# LINEMOD Template Scoring 测试总览

## 当前测试边界

本 topic 的测试资产覆盖 `recognition/src/linemod.cpp` 中 LINEMOD 模板打分链路。QEMU（仿真器）只用于 correctness（正确性）、构建和日志形状；性能结论只引用 board repeated（板卡重复测试）summary。

| target 类别 | 当前入口 | 证明什么 | 不能证明什么 |
| --- | --- | --- | --- |
| correctness aggregate（正确性汇总入口） | `make run_test_compare TEST_ARGS="--gtest_filter=LINEMODTemplateScoring.*"` | 测试专用 candidate helper 的 score accumulation、scan、energy map 和 linearized copy 与标量参考一致 | 不证明 production dispatch 或目标硬件性能 |
| production-direct correctness | `make run_production_direct_test_compare TEST_ARGS="--gtest_filter=LINEMODTemplateScoringProductionDirect.*"` | 当前源码编译出的 `matchTemplates`、`detectTemplates` 和 semi-scale 公开入口可运行并生成稳定 detection | 不证明未启用的 separate-energy 宏 |
| bench diagnostic aliases（诊断性能入口） | `collect_score_scan_repeated_board`、`collect_accumulation_ablation_repeated_board`、`collect_energy_map_repeated_board`、`collect_linearized_map_copy_repeated_board`、`collect_full_chain_split_repeated_board` | 分阶段判断 candidate family 的同边界收益或退化 | 不能替代 Phase 070 / 080 production-public 证据 |
| production board repeated | `make collect_production_direct_repeated_board BENCH_ARGS="4096 96 200 5"`、`make collect_semiscale_production_direct_repeated_board BENCH_ARGS="4096 96 200 5"` | 接入后真实公开入口 Std/RVV 性能对比 | 不覆盖 NMS、averaged detection 和 separate-energy |
| asm attribution（反汇编归属） | `make dump_production_direct_bench_rvv`、`make dump_semiscale_production_direct_bench_rvv` | production-linked RVV binary 中存在 `linearizeEnergyMapRVV`、`vlse8.v` 和 `vse8.v`，Phase 080 要求出现第三个 helper 调用点 | 不单独证明端到端收益 |
| doctor / registry aliases | `make record_production_direct_evidence_state`、`make record_semiscale_production_direct_evidence_state`、对应 `check_*_evidence_freshness` | 生成并检查 summary、manifest、Evidence Doctor 和 registry | 不替代源码审查 |

## Target Granularity Audit

当前 target 粒度足以支撑 Phase 070 / 080 closeout：diagnostic 阶段有按 candidate family 分开的 board target，production 阶段有真实公开入口 test、bench、asm、doctor 和 freshness target。historical probe（历史探针）保留在 phase 文档中，不作为当前 production 采纳证据。

Raw board logs 默认留在 `test-rvv/recognition/linemod_template_scoring/log/board/`，不进入默认提交边界。可提交证据是 phase-local summary、manifest、Evidence Doctor 和 `log/evidence_registry.json`。
