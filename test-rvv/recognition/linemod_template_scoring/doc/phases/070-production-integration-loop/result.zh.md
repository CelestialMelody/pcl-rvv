# Phase 070 Result: production-integration-loop

## 当前结论

Phase 070 完成 `recognition/src/linemod.cpp` 的 production integration loop（生产接入闭环）。当前 production patch 只接管默认宏路径下 `matchTemplates` 和 `detectTemplates` 的 `EnergyMaps -> LinearizedMaps` 拷贝循环；公开 API 不变，`LINEMOD_USE_SEPARATE_ENERGY_MAPS`、`detectTemplatesSemiScaleInvariant`、NMS、averaged detection、score accumulation 和 threshold scan 保持既有标量路径。

接入后的 production-public（真实公开入口）板卡 repeated summary 为 positive：`matchTemplates` median `1.138x`，`detectTemplates` median `1.129x`，两项均 0/5 退化，Evidence Doctor（证据体检）为 `Errors=0 / Warnings=0 / Suggestions=0`。用户本轮明确规则是“板卡上的测试结果如果显示有收益即可采纳”，因此本阶段把该 production patch 记录为 `adopted production behavior`。

## 计划动作回填

| PI step | result | evidence |
| --- | --- | --- |
| PI2 RED | done | production-direct asm / helper 检查在接入前缺少 `linearizeEnergyMapRVV` 归属；接入后由 `dump_production_direct_bench_rvv` 变为通过。 |
| PI2 GREEN | done | `recognition/src/linemod.cpp` 新增 `linearizeEnergyMapStd`、`linearizeEnergyMapRVV` 和 `linearizeEnergyMap` dispatch（分流逻辑）。 |
| PI3 tests | done | `make run_production_direct_test_compare TEST_ARGS="--gtest_filter=LINEMODTemplateScoringProductionDirect.*"`，Std/RVV 两侧各 2 个 production-direct gtest 通过。 |
| PI4 asm | done | `make dump_production_direct_bench_rvv` 生成 `build/asm/riscv/bench_linemod_template_scoring_production_direct_rvv.full.asm`，能定位 `linearizeEnergyMapRVV`、`vlse8.v` 和 `vse8.v`。 |
| PI4 board repeated | done | `make collect_production_direct_repeated_board BENCH_ARGS="4096 96 200 5"` 采集 5-run，summary 路径见下方证据表。 |
| PI4 doctor / registry | done | `make record_production_direct_evidence_state BENCH_ARGS="4096 96 200 5"` 生成 summary、manifest、doctor 并登记 registry。 |
| PI5 decision | adopted | 接入后板卡两项公开入口均 positive，doctor clean；按用户规则采纳并创建正式 `doc-rvv` 文档。 |

## Production Direct Evidence

| evidence | result | path |
| --- | --- | --- |
| production-public board summary | `matchTemplates` median `1.138x`，`detectTemplates` median `1.129x`，均 0/5 退化 | `test-rvv/recognition/linemod_template_scoring/doc/phases/070-production-integration-loop/production-direct-repeated-summary.md` |
| Evidence Doctor | `Errors=0 / Warnings=0 / Suggestions=0` | `test-rvv/recognition/linemod_template_scoring/doc/phases/070-production-integration-loop/production-direct-repeated-evidence-doctor.md` |
| manifest | evidence role 为 `production-public`，binary hash 为 `sha256:cd16a4e705ba652a92718ebba7c25bfaac2543d93bca0fc3ded803552465b658` | `test-rvv/recognition/linemod_template_scoring/doc/phases/070-production-integration-loop/production-direct-repeated-evidence-manifest.json` |
| correctness | Std/RVV production-direct tests 通过；验证默认公开入口能稳定生成 detection checksum | `test-rvv/recognition/linemod_template_scoring/log/qemu/phase070-production-direct-test-std.log`、`test-rvv/recognition/linemod_template_scoring/log/qemu/phase070-production-direct-test-rvv.log` |
| asm attribution（反汇编归属） | production-linked RVV bench 中可定位 `linearizeEnergyMapRVV`，并看到 `vlse8.v` / `vse8.v` | `test-rvv/recognition/linemod_template_scoring/build/asm/riscv/bench_linemod_template_scoring_production_direct_rvv.full.asm` |
| evidence registry | Phase 070 summary / manifest / doctor 已登记 | `test-rvv/recognition/linemod_template_scoring/log/evidence_registry.json` |

QEMU（仿真器）只证明 correctness 和日志形状；本阶段性能结论只来自 Milkv-Jupiter 板卡 repeated summary。

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `production-public`；Phase 050 的 production-shaped diagnostic 只作为进入 PI2-PI5 的依据。 |
| A/B boundary | `public_overload_current_source`；Std/RVV 两侧都链接当前源码，真实调用 `LINEMOD::matchTemplates` 与 `LINEMOD::detectTemplates`。 |
| 当前决策问题 | 当前 public RVV path 是否快于当前 public scalar path，以及是否值得保留 production patch。 |
| diagnostic 是否可外推到 production | 不再依赖外推；最终采纳依据是 Phase 070 接入后的 production-public 板卡证据。 |
| comparison-boundary / baseline mismatch 风险 | 已通过 production-linked test / bench 降低；仍不覆盖 semi-scale、separate-energy、NMS 或 averaged detection。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 已完成 bounded probe；结果为 positive。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前只有一个 production RVV family，不是 family selection（实现族选择）；若以后新增 fused/multi-bin family，需要同边界 RVV-vs-RVV A/B。 |

## Doc Suite Role Inventory

| area | current shape scan | quality bar | decision | evidence | next action |
| --- | --- | --- | --- | --- | --- |
| topic_navigation | `README.zh.md` 存在并列出正式文档、evaluation、phase 和常用命令 | README 只负责导航和证据白名单 | adopted | 本阶段已刷新当前状态和 Phase 070 命令 | final verification |
| testing_overview | 新增 `doc/testing-overview.zh.md` | 需要 target 粒度、QEMU / board 边界和证据白名单 | adopted | 文档列出 Phase000-070 target 分类 | final verification |
| correctness_tests | 新增 `doc/correctness-tests.zh.md` | 每个 gtest 说明输入、断言和证明范围 | adopted | 文档覆盖 `LINEMODTemplateScoring.*` 与 production-direct tests | final verification |
| benchmark_and_evidence | 新增 `doc/benchmark-and-evidence.zh.md` | bench label、summary、doctor、registry 分工清楚 | adopted | 文档引用 Phase070 production-public summary 和 registry | final verification |
| optimization_evidence | 新增 `doc/optimization-evidence.zh.md` | candidate family 状态可审查 | adopted | 文档区分 adopted / attempted-negative / deferred | final verification |
| optimization_roadmap | `doc/optimization-roadmap.zh.md` 存在 | roadmap 保存候选搜索空间和恢复条件 | adopted | 已新增 Phase070 adopted 状态和默认恢复队列 | final verification |
| test_support_code_map | 新增 `doc/test-support-code-map.zh.md` | 能从 helper / test / bench / script 跳到证据 | adopted | 文档覆盖 `src/`、`include/impl/`、script、production helper | final verification |
| phase suite | `doc/phases/README.zh.md`、各 phase plan/result、matrix 存在 | phase result 和 matrix 记录每阶段事实 | adopted | 本 result 补齐 Phase070 closeout | final verification |
| evaluation | `doc/linemod_template_scoring-evaluation.zh.md` 存在 | S2 和 S11 production closeout 分工清楚 | adopted | 本阶段会同步 production evidence 和 Traceability Map | final verification |
| production_topic_doc | 新增 `doc-rvv/recognition/linemod_template_scoring-RVV.zh.md` | 只记录 adopted production behavior、证据链和未覆盖范围 | adopted | 数据采用 Phase070 接入后板卡结果 | final verification |
| artifact tracking | topic 文档和 summary evidence 均在当前 topic 路径或正式 `doc-rvv` 路径 | 引用文档必须存在并进入 to-be-staged artifact 集合 | adopted | `git status --short --untracked-files=all -- <topic paths>` 待最终验证列出 | final verification |

## Continue / Stop Decision

当前阶段完成矩阵已闭合。仍可继续探索的方向包括 semi-scale 和 `LINEMOD_USE_SEPARATE_ENERGY_MAPS`，但它们会扩大到未被 Phase 070 采纳的入口或非默认编译宏，不能把当前 production-public 证据外推过去。默认动作是完成 final verification 并停在 ready-for-review；若用户要求继续当前 topic，建议新建窄 phase，而不是在已采纳 patch 上直接扩大生产范围。
