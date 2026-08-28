# Phase 080 Result: semi-scale-production-direct-probe

## 执行范围

本阶段按计划只扩展默认宏路径下 `detectTemplatesSemiScaleInvariant()` 的 `EnergyMaps -> LinearizedMaps` copy loop。生产源码改动复用 Phase 070 已采纳的 `linearizeEnergyMap()` helper；`LINEMOD_USE_SEPARATE_ENERGY_MAPS` 四套 map 分支、score accumulation、threshold scan、NMS 和 averaged detection 保持原路径。

## 动作回填

| action | status | command / evidence | result |
| --- | --- | --- | --- |
| 新增 semi-scale production-direct correctness | done | `make run_semiscale_production_direct_test_compare TEST_ARGS="--gtest_filter=LINEMODTemplateScoringProductionDirect.SemiScale*"` | Std/RVV 各 1 个 gtest 通过；scale 字段只来自 `1.0f` / `2.0f`，checksum 非 0。 |
| 新增 semi-scale production-direct bench label | done | `LINEMOD production semi-scale detectTemplates total` | 计时边界是真实公开入口 `LINEMOD::detectTemplatesSemiScaleInvariant()`，包含默认 energy maps、linearized copy、每个 scale 的 scalar accumulation / scan 和 detection 构造。 |
| RED asm gate | done | `make dump_semiscale_production_direct_bench_rvv` before production edit | 初始失败，因为 RVV binary 只有 Phase 070 两个 `linearizeEnergyMapRVV` 调用点，semi-scale 未接入。 |
| GREEN production edit | done | `recognition/src/linemod.cpp` | semi-scale 默认单套 map copy loop 改为 `linearizeEnergyMap()`；separate-energy 编译分支不变。 |
| asm attribution | done | `make dump_semiscale_production_direct_bench_rvv` | `linearizeEnergyMapRVV` 调用点达到 3 个，反汇编中可见 `vlse8.v` / `vse8.v`。 |
| board repeated | done | `make collect_semiscale_production_direct_repeated_board` | 5-run Milkv-Jupiter：median `1.136x`，range `1.130x` - `1.141x`，0/5 退化。 |
| manifest / doctor / registry | done | `make record_semiscale_production_direct_evidence_state` | 生成 summary、manifest、Evidence Doctor 并登记 registry；Evidence Doctor 为 `Errors=0 / Warnings=0 / Suggestions=0`。 |

## 证据路径

- summary: `test-rvv/recognition/linemod_template_scoring/doc/phases/080-semi-scale-production-direct-probe/semi-scale-production-direct-repeated-summary.md`
- manifest: `test-rvv/recognition/linemod_template_scoring/doc/phases/080-semi-scale-production-direct-probe/semi-scale-production-direct-repeated-evidence-manifest.json`
- Evidence Doctor: `test-rvv/recognition/linemod_template_scoring/doc/phases/080-semi-scale-production-direct-probe/semi-scale-production-direct-repeated-evidence-doctor.md`
- Evidence Doctor JSON: `test-rvv/recognition/linemod_template_scoring/doc/phases/080-semi-scale-production-direct-probe/semi-scale-production-direct-repeated-evidence-doctor.json`
- registry: `test-rvv/recognition/linemod_template_scoring/log/evidence_registry.json`

## 优化矩阵更新

| candidate family | row source policy | scope and entry | correctness | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| semi-scale production-public linearized map copy RVV | production quantized modality public entry | `detectTemplatesSemiScaleInvariant()` default macro path | passed | median `1.136x`，0/5 退化 | 3 个 `linearizeEnergyMapRVV` call site，含 `vlse8.v` / `vse8.v` | 0/0/0 | adopted production behavior under current user rule |

## Diagnostic To Production Mismatch Audit

| question | result |
| --- | --- |
| evidence role | `production-public`，不是 diagnostic 外推。 |
| A/B boundary | Std/RVV 两侧都链接当前源码公开入口；case-filter 只采纳 `LINEMOD production semi-scale detectTemplates total`。 |
| 当前决策问题 | 当前 semi-scale public RVV path 是否快于当前 semi-scale public scalar path。 |
| comparison-boundary / baseline mismatch 风险 | manifest 记录相同 wrapper、row source、timer boundary、checksum policy 和 gate；checksum / index checksum 一致。 |
| clean adoption 是否需要 RVV-vs-RVV A/B | 本阶段只复用已采纳 helper，不做新 family selection；若后续新增 semi-scale 专用 helper，需要同边界 RVV-vs-RVV A/B。 |

## Doc Suite Role Inventory

| role | status | closeout evidence |
| --- | --- | --- |
| topic_navigation | `standalone:test-rvv/recognition/linemod_template_scoring/README.zh.md` | README 给出阅读顺序、常用 target、当前可提交 summary / manifest / doctor / registry 证据和 raw log 排除边界。 |
| testing_overview | `standalone:test-rvv/recognition/linemod_template_scoring/doc/testing-overview.zh.md` | 已区分 correctness aggregate、production-direct correctness、diagnostic bench、production board repeated、asm attribution、doctor / registry target。 |
| correctness_tests | `standalone:test-rvv/recognition/linemod_template_scoring/doc/correctness-tests.zh.md` | 已逐项说明 diagnostic gtest 与 production direct gtest 的输入、断言和证明范围。 |
| benchmark_and_evidence | `standalone:test-rvv/recognition/linemod_template_scoring/doc/benchmark-and-evidence.zh.md` | 已记录 Phase 070 / 080 production-public summary、manifest、Evidence Doctor、binary hash 和 registry freshness 命令。 |
| optimization_evidence | `standalone:test-rvv/recognition/linemod_template_scoring/doc/optimization-evidence.zh.md` | 已把 score accumulation、threshold scan、energy map generation、linearized copy、semi-scale 和 separate-energy 分别标成 attempted-negative、adopted 或 deferred。 |
| optimization_roadmap | `standalone:test-rvv/recognition/linemod_template_scoring/doc/optimization-roadmap.zh.md` | 已记录当前 adopted 边界、负向候选、Phase 070 / 080 反思和 separate-energy 的恢复条件。 |
| test_support_code_map | `standalone:test-rvv/recognition/linemod_template_scoring/doc/test-support-code-map.zh.md` | 已定位 `src/`、`include/`、`include/impl/`、topic-local script、production helper 和 registry；没有旧 `test_support/` 或 legacy alias。 |
| phase_index | `standalone:test-rvv/recognition/linemod_template_scoring/doc/phases/README.zh.md` | 阶段索引与 `doc/phases/optimization-matrix.zh.md` 保存 000-080 的 plan/result 和证据入口。 |
| evaluation_production | `standalone:test-rvv/recognition/linemod_template_scoring/doc/linemod_template_scoring-evaluation.zh.md` | S2/S11 分工已更新；Traceability Map 覆盖 production helper、test、bench、script、summary 和 Phase 070 / 080。 |
| production_topic_doc | `standalone:doc-rvv/recognition/linemod_template_scoring-RVV.zh.md` | 适用，因为已有 adopted production behavior；长期文档只保存当前生产行为、fallback、范围决策、Traceability Map 和正确性与高效性证据链。 |
| artifact_tracking | `to-be-staged topic artifacts` | 本 topic 新增文档、测试资产、summary / manifest / doctor 和 registry 均在 `test-rvv/recognition/linemod_template_scoring/**` 或正式 `doc-rvv` 路径内；`build/`、raw `log/board` / `log/qemu` 和 `work/` 临时输出不进入默认提交。 |

## Closeout Audit

| area | current shape scan | quality bar | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| production dispatch / fallback | `linearizeEnergyMap()` 在 RVV 构建调用 `linearizeEnergyMapRVV()`，非 RVV 构建调用 `linearizeEnergyMapStd()`；公开 API 不变。 | production helper 应分层清楚，fallback 保留原标量事实来源。 | adopted | production-direct correctness、asm 和 Phase 070 / 080 board summary 已闭合。 | reviewer 抽查源码即可；无下一 phase。 |
| doc suite | README、testing overview、correctness tests、benchmark/evidence、optimization evidence、roadmap、code map、evaluation、phase suite 和正式 `doc-rvv` 均存在。 | 复杂 topic closeout 前必须有 role inventory 和可追踪证据路径。 | adopted | 上方 `Doc Suite Role Inventory` 已列出主归属；文档引用均为仓库相对路径。 | 无未阻塞文档补齐动作。 |
| target granularity | Makefile/board.mk 提供 diagnostic、production direct、asm、board repeated、doctor 和 freshness target。 | target 应能支撑 correctness、board、doctor、registry 和历史 probe 边界。 | adopted | `testing-overview.zh.md` 与 Makefile target 对齐。 | 无下一 phase。 |
| evidence freshness | Phase 070 / 080 summary、manifest、doctor 和 registry 已登记。 | 提交前必须重新运行 freshness check。 | adopted after final verification | 本阶段依赖 `check_production_direct_evidence_freshness` 和 `check_semiscale_production_direct_evidence_freshness`。 | final verification 后进入 commit flow。 |
| remaining optimization directions | score accumulation、threshold scan 和 energy map generation 已有负向同边界证据；separate-energy 属非默认宏和四套 map 语义。 | roadmap 仍有授权未阻塞 high-priority action 时不能 closeout。 | turn_stop_deferred with stop_condition_hit | 当前默认宏 adopted 范围内无高优先级未阻塞方向；separate-energy 需要实际 build 需求或新授权。 | 结束当前 topic；如后续需要 separate-energy，另起 phase。 |

## Continue / Stop Decision

Phase 080 的完成矩阵已闭合，且按用户规则“接入后板卡有收益即可采纳”进入 adopted production behavior。当前 topic 内仍可考虑的方向只剩 `LINEMOD_USE_SEPARATE_ENERGY_MAPS` helper phase，但它依赖非默认编译宏和四套 map 语义，当前公开入口证据不能外推；没有证据表明默认 build 需要继续扩大到该宏。score accumulation、threshold scan 和 energy map generation 已有同边界负向证据，除非出现新的实现族或 profile 热点，不建议继续重试。

默认下一步是运行 final verification；若通过，当前 topic 可以结束并进入 topic-only commit flow。raw board logs、QEMU logs、build 输出、`work/` 临时输出、其它 recognition topic 和 `.agents` 改动不属于本 topic 提交范围。
