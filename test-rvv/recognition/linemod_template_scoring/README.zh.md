# LINEMOD Template Scoring RVV Topic

## 本目录做什么

本 topic 针对 `recognition/src/linemod.cpp` 中 LINEMOD 模板打分主循环建立 RVV（RISC-V Vector，可变长度向量扩展）函数级评估。当前已完成默认宏路径的 production integration loop（生产接入闭环）：`matchTemplates`、`detectTemplates` 与 `detectTemplatesSemiScaleInvariant` 的 `EnergyMaps -> LinearizedMaps` 拷贝循环已接入 RVV helper，公开 API 不变。

## 阅读顺序

1. `../../../doc-rvv/recognition/linemod_template_scoring-RVV.zh.md`：正式长期主题文档，记录当前已采纳的 production 行为、fallback 和 Phase 070 / 080 板卡证据。
2. `doc/linemod_template_scoring-evaluation.zh.md`：函数级评估、候选取舍、生产接入审计和 Traceability Map（可追踪性地图）。
3. `doc/testing-overview.zh.md`：测试入口分类、target 粒度和覆盖边界。
4. `doc/correctness-tests.zh.md`：correctness（正确性）测试字典。
5. `doc/benchmark-and-evidence.zh.md`：bench（性能测试）、board repeated（板卡重复测试）、Evidence Doctor（证据体检）和 registry（证据登记表）说明。
6. `doc/optimization-evidence.zh.md`：各 candidate family 的 adopted / attempted / rejected 状态索引。
7. `doc/test-support-code-map.zh.md`：测试支撑代码、production helper、script 和 output 的定位图。
8. `doc/phases/070-production-integration-loop/result.zh.md` 和 `doc/phases/080-semi-scale-production-direct-probe/result.zh.md`：Phase 070 / 080 production direct closeout。
9. `doc/phases/README.zh.md` 和 `doc/phases/optimization-matrix.zh.md`：阶段恢复入口和优化矩阵。

## 常用命令

- `make run_test_compare TEST_ARGS="--gtest_filter=LINEMODTemplateScoring.*"`：在 QEMU 或当前 `rvv-topic.mk` 后端运行 Std/RVV correctness。
- `make collect_score_scan_repeated_board BENCH_ARGS="4096 96 200 5"`：采集 Phase 010 三项 score bench 的 5-run board repeated evidence。
- `make run_score_scan_evidence_doctor BENCH_ARGS="4096 96 200 5"`：生成 Phase 010 summary、manifest 和 Evidence Doctor 报告。
- `make collect_accumulation_ablation_repeated_board BENCH_ARGS="4096 96 200 5"`：采集 Phase 020 accumulation-only 消融的 5-run board repeated evidence。
- `make run_accumulation_ablation_evidence_doctor BENCH_ARGS="4096 96 200 5"`：生成 Phase 020 summary、manifest 和 Evidence Doctor 报告。
- `make collect_energy_map_repeated_board BENCH_ARGS="4096 96 200 5"`：采集 Phase 030 energy map generation 的 5-run board repeated evidence。
- `make run_energy_map_evidence_doctor BENCH_ARGS="4096 96 200 5"`：生成 Phase 030 summary、manifest 和 Evidence Doctor 报告。
- `make collect_linearized_map_copy_repeated_board BENCH_ARGS="4096 96 200 5"`：采集 Phase 040 linearized map copy 的 5-run board repeated evidence。
- `make run_linearized_map_copy_evidence_doctor BENCH_ARGS="4096 96 200 5"`：生成 Phase 040 summary、manifest 和 Evidence Doctor 报告。
- `make collect_full_chain_split_repeated_board BENCH_ARGS="4096 96 200 5"`：采集 Phase 050 full-chain split（完整链路分段）的 5-run board repeated evidence。
- `make run_full_chain_split_evidence_doctor BENCH_ARGS="4096 96 200 5"`：生成 Phase 050 summary、manifest 和 Evidence Doctor 报告。
- `make check_full_chain_split_evidence_freshness`：检查 Phase 050 summary / manifest / doctor / registry 是否仍为 fresh（新鲜可用）。
- `make run_production_direct_test_compare TEST_ARGS="--gtest_filter=LINEMODTemplateScoringProductionDirect.*"`：在 QEMU 或当前后端运行 Phase 070 生产直连 Std/RVV correctness。
- `make dump_production_direct_bench_rvv`：生成 Phase 070 production-linked RVV bench 反汇编，并检查 `linearizeEnergyMapRVV`、`vlse8.v` 和 `vse8.v`。
- `make collect_production_direct_repeated_board BENCH_ARGS="4096 96 200 5"`：采集 Phase 070 接入后的真实公开入口 5-run board repeated evidence。
- `make run_production_direct_evidence_doctor BENCH_ARGS="4096 96 200 5"`：生成 Phase 070 production direct summary、manifest 和 Evidence Doctor。
- `make record_production_direct_evidence_state BENCH_ARGS="4096 96 200 5"`：记录 Phase 070 summary / manifest / doctor 到 evidence registry。
- `make check_production_direct_evidence_freshness`：检查 Phase 070 evidence registry 与引用文档是否一致。
- `make run_semiscale_production_direct_test_compare TEST_ARGS="--gtest_filter=LINEMODTemplateScoringProductionDirect.SemiScale*"`：运行 Phase 080 semi-scale 生产直连 Std/RVV correctness。
- `make dump_semiscale_production_direct_bench_rvv`：生成 Phase 080 semi-scale RVV bench 反汇编，并检查第三个 `linearizeEnergyMapRVV` 调用点、`vlse8.v` 和 `vse8.v`。
- `make collect_semiscale_production_direct_repeated_board BENCH_ARGS="4096 96 200 5"`：采集 Phase 080 semi-scale 真实公开入口 5-run board repeated evidence。
- `make record_semiscale_production_direct_evidence_state BENCH_ARGS="4096 96 200 5"`：记录 Phase 080 summary / manifest / doctor 到 evidence registry。
- `make check_semiscale_production_direct_evidence_freshness`：检查 Phase 080 evidence registry 与引用文档是否一致。
- `make dump_linemod_score_bench_rvv`：生成 bench 二进制反汇编，用于后续 asm attribution（反汇编归属）。

## 当前证据

当前 score accumulation、conservative score scan 和 energy map generation 结论均为 `attempted-negative`，不进入 production patch。Phase 040 linearized map copy 为 `attempted-positive / partial-production-candidate`，median `2.160x`，0/5 退化；Phase 050 把该候选放回 full-chain split，得到 full-chain total median `1.565x`、linearized copy median `1.790x`，二者均 0/5 退化且 Evidence Doctor `Errors=0 / Warnings=0 / Suggestions=0`。

Phase 070 已把 linearized copy helper 接入 `recognition/src/linemod.cpp` 的默认宏 `matchTemplates` / `detectTemplates` 路径。接入后的 production-public（真实公开入口）板卡 5-run 结果为 positive：`matchTemplates` median `1.138x`，`detectTemplates` median `1.129x`，两项均 0/5 退化，Evidence Doctor `Errors=0 / Warnings=0 / Suggestions=0`。

Phase 080 继续把同一 helper 接入默认宏 `detectTemplatesSemiScaleInvariant` 路径。接入后的 semi-scale production-public 板卡 5-run 结果为 positive：median `1.136x`，range `1.130x` - `1.141x`，0/5 退化，Evidence Doctor `Errors=0 / Warnings=0 / Suggestions=0`。根据用户本轮规则，接入后板卡有收益即可采纳；当前状态为 `adopted production behavior`。Phase 000 的 `1.404x` positive 只保留为 historical evidence（历史证据）。主证据路径：

- `test-rvv/recognition/linemod_template_scoring/doc/phases/010-threshold-scan-and-detection-order/score-scan-repeated-summary.md`
- `test-rvv/recognition/linemod_template_scoring/doc/phases/010-threshold-scan-and-detection-order/score-scan-repeated-evidence-doctor.md`
- `test-rvv/recognition/linemod_template_scoring/doc/phases/020-accumulation-bench-boundary-ablation/accumulation-ablation-repeated-summary.md`
- `test-rvv/recognition/linemod_template_scoring/doc/phases/020-accumulation-bench-boundary-ablation/accumulation-ablation-repeated-evidence-doctor.md`
- `test-rvv/recognition/linemod_template_scoring/doc/phases/030-energy-map-generation/energy-map-repeated-summary.md`
- `test-rvv/recognition/linemod_template_scoring/doc/phases/030-energy-map-generation/energy-map-repeated-evidence-doctor.md`
- `test-rvv/recognition/linemod_template_scoring/doc/phases/040-linearized-map-copy-ablation/linearized-map-copy-repeated-summary.md`
- `test-rvv/recognition/linemod_template_scoring/doc/phases/040-linearized-map-copy-ablation/linearized-map-copy-repeated-evidence-doctor.md`
- `test-rvv/recognition/linemod_template_scoring/doc/phases/050-full-chain-timing-and-production-eligibility/full-chain-split-repeated-summary.md`
- `test-rvv/recognition/linemod_template_scoring/doc/phases/050-full-chain-timing-and-production-eligibility/full-chain-split-repeated-evidence-doctor.md`
- `test-rvv/recognition/linemod_template_scoring/doc/phases/070-production-integration-loop/production-direct-repeated-summary.md`
- `test-rvv/recognition/linemod_template_scoring/doc/phases/070-production-integration-loop/production-direct-repeated-evidence-doctor.md`
- `test-rvv/recognition/linemod_template_scoring/doc/phases/080-semi-scale-production-direct-probe/semi-scale-production-direct-repeated-summary.md`
- `test-rvv/recognition/linemod_template_scoring/doc/phases/080-semi-scale-production-direct-probe/semi-scale-production-direct-repeated-evidence-manifest.json`
- `test-rvv/recognition/linemod_template_scoring/doc/phases/080-semi-scale-production-direct-probe/semi-scale-production-direct-repeated-evidence-doctor.md`
- `test-rvv/recognition/linemod_template_scoring/doc/phases/080-semi-scale-production-direct-probe/semi-scale-production-direct-repeated-evidence-doctor.json`
- `test-rvv/recognition/linemod_template_scoring/log/evidence_registry.json`

这些路径是 summary / manifest / registry 级证据；raw board logs 默认留本机，不默认提交。

## Production Topic Doc 适用性

当前已有 adopted production behavior（已采用生产行为），正式长期主题文档位于 `doc-rvv/recognition/linemod_template_scoring-RVV.zh.md`。该文档只记录当前已采用的 production 行为、证据链和未覆盖范围；phase 探索、失败候选和测试支撑细节仍归属本 topic-local doc suite。
