# Phase 070 Result: Doc suite parity closeout

## 阶段结论

本阶段完成 PPF topic-local doc suite（主题本地文档套件）补齐。Phase 060 的
traits-gated `alpha_m` RVV production path（生产路径）和接入后板卡收益没有变化；本阶段没有修改
production 源码、bench case 或测试代码。

`continue_stop_decision`: stop / ready for review。

`stop_condition_hit`: 当前 production boundary（生产边界）、测试支撑结构、文档套件、roadmap
和 optimization matrix（优化矩阵）已经闭合；当前 PPF topic 内没有高优先级未阻塞优化动作。

## 计划执行回填

| action | 状态 | 产物 / 证据 | 结论 |
| --- | --- | --- | --- |
| A1 写独立 role docs | done | `doc/testing-overview.zh.md`、`doc/correctness-tests.zh.md`、`doc/benchmark-and-evidence.zh.md`、`doc/optimization-evidence.zh.md`、`doc/test-support-code-map.zh.md` | 原本挤在 README / evaluation 的测试、bench、证据和代码地图职责已有稳定主归属。 |
| A2 更新导航和 phase 状态 | done | README、phase index、roadmap、optimization matrix | 阅读路径能直接跳到 role docs；Phase 070 已加入恢复入口。 |
| A3 写 `doc_suite_role_inventory` | done | 本文下方审计表 | 每个 doc-suite role 均为 standalone、merged 或 not_applicable，没有 `phase_deferred + unblocked`。 |
| A4 更新 Handoff | done | `tmp/rvv-work-logs/features/ppf/current-handoff/current-handoff.zh.md` | 当前 Handoff 的 phase loop 状态已从 Phase 060 刷新到 Phase 070。 |
| A5 验证 | done | `git diff --check`、文档路径扫描、Phase 060 evidence path 存在性检查、`make -C test-rvv/features/ppf run_test_compare` | 无 whitespace error；新增文档均可由 README / phase index / Handoff 找到；当前本地 Std 5/5、RVV 9/9 pass。 |

## Doc suite role inventory

| role | 状态 | 主路径 / 章节 | 证据和边界 |
| --- | --- | --- | --- |
| topic_navigation | standalone:`test-rvv/features/ppf/README.zh.md` | 当前状态、阅读路径、常用命令、提交边界 | README 已列出新增 role docs 和正式 `doc-rvv` 路径。 |
| testing_overview | standalone:`test-rvv/features/ppf/doc/testing-overview.zh.md` | target 粒度审计、运行入口、覆盖矩阵 | 从 Makefile / board.mk / gtest / bench case 抽取真实入口。 |
| correctness_tests | standalone:`test-rvv/features/ppf/doc/correctness-tests.zh.md` | 9 个 gtest 的输入、断言、证明范围 | 区分 reference、candidate、production direct 和 fallback。 |
| benchmark_and_evidence | standalone:`test-rvv/features/ppf/doc/benchmark-and-evidence.zh.md` | bench CLI、case label、summary evidence、registry 状态 | 明确 QEMU 不作为性能证据，raw logs 默认 local-only。 |
| optimization_evidence | standalone:`test-rvv/features/ppf/doc/optimization-evidence.zh.md` | adopted / rejected / deferred candidate 到证据的映射 | Phase 010 negative、Phase 030 diagnostic positive、Phase 040/060 production adopted 均有主归属。 |
| optimization_roadmap | standalone:`test-rvv/features/ppf/doc/optimization-roadmap.zh.md` | 候选搜索空间和默认恢复队列 | Phase 070 已补入，后续方向写成 separate phase / topic required。 |
| test_support_code_map | standalone:`test-rvv/features/ppf/doc/test-support-code-map.zh.md` | `src/`、`include/`、`include/impl/`、script、Makefile 职责 | 当前没有旧 `test_support/` 或 compatibility alias 需要迁移。 |
| phase_index / phase_plan / phase_result / matrix | standalone:`test-rvv/features/ppf/doc/phases/` | phase index、070 plan/result、optimization matrix | Phase 070 已加入 README 和 matrix。 |
| evaluation_production | standalone:`test-rvv/features/ppf/doc/ppf-evaluation.zh.md` | EvidenceDecision、Traceability Map、生产接入判断 | 继续作为函数级决策审计主文档；新增 role docs 承担测试细节。 |
| production_topic_doc | standalone:`doc-rvv/features/ppf-RVV.zh.md` | adopted production behavior 长期说明 | 只保存当前生产行为、fallback、证据链和长期边界，不承担 test support 全量说明。 |
| evidence registry | not_applicable with evidence | 当前无 `test-rvv/features/ppf/log/evidence_registry.json` | 本 topic 已有 manifest / Doctor summary，并在 `benchmark-and-evidence.zh.md` 记录 manual freshness check；严格归档时另开 evidence hardening phase。 |

## Test support shape scan

| area | current shape scan | quality bar | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| source layout | `src/test_ppf.cpp`、`src/bench_ppf.cpp` | 使用 `artifact_layout.source_subdir` | adopted | 真实源码已在 `src/` 下。 | none |
| aggregator | `include/ppf.h` | 聚合入口短小稳定 | adopted | 文件只引入 topic helper。 | none |
| internal helper layout | `include/impl/*.hpp` | reference / candidate 按职责拆分 | adopted | 三个 internal header 分别承载 reference、alpha candidate、pair-feature candidate。 | none |
| helper size | 最大 helper 272 行，test 317 行，bench 245 行 | 未超过 hard line limit，职责清楚 | adopted | `wc -l` 扫描确认。 | none |
| legacy alias | 无旧 `test_support/` 目录，无 compatibility alias | 默认删除旧入口 | adopted | `find test-rvv/features/ppf -maxdepth 4 -type f` 未发现旧目录。 | none |
| topic-local docs | 新增五个 role docs | 复杂 topic 优先拆出 role docs | adopted | README 已引用每个 role doc。 | none |

## Target granularity audit

| target 类别 | 当前状态 | decision | 证据 |
| --- | --- | --- | --- |
| correctness aggregate | `run_test_compare` | adopted | 顺序跑 Std/RVV tests。 |
| correctness aliases | `run_test_std`、`run_test_rvv` | adopted | Std 5/5、RVV 9/9 的历史结果记录在 Phase 060。 |
| bench diagnostic aliases | `--case-filter candidate_ppf_pair_feature_batch_rvv`、`--case-filter candidate_ppf_alpha_m_batch_rvv` | adopted | Phase 010 / 030 使用这些 case label。 |
| QEMU smoke aliases | `run_bench_rvv` 加单 case-filter | adopted | Phase 060 两个 public case checksum 均记录为 `213486`。 |
| board smoke aliases | `run_board_test fetch_board_logs` | adopted | Phase 060 result 记录 RVV 9/9 pass；raw `run_test.log` 默认 local-only。 |
| board repeated aliases | `board_repeated evidence_doctor_repeated` | adopted | 两个 Phase 060 repeated output dir 已生成 manifest / Doctor。 |
| doctor / registry aliases | `evidence_manifest_repeated`、`evidence_doctor_repeated`；无 registry target | adopted with manual registry | manifest / Doctor 已足够支撑当前 summary-only 结论；registry hardening 另开 phase。 |
| historical probe guard | case-filter 隔离历史 diagnostic / negative case | adopted | 非默认 case label 不会被写成当前 production evidence。 |

## Evidence freshness status

Phase 060 的 current truth（当前事实）仍来自接入后 production-public board summaries：

- `test-rvv/features/ppf/log/board/phase060-pointxyzi-normal/repeated/evidence_manifest.json`
- `test-rvv/features/ppf/log/board/phase060-pointxyzi-normal/repeated/evidence_doctor.md`
- `test-rvv/features/ppf/log/board/phase060-pointxyz-pointnormal/repeated/evidence_manifest.json`
- `test-rvv/features/ppf/log/board/phase060-pointxyz-pointnormal/repeated/evidence_doctor.md`

本阶段未覆盖这些证据文件。`doc-rvv/features/ppf-RVV.zh.md`、evaluation、Phase 060 result 和
README 中的 Phase 060 数值一致。`evidence_registry_status`: `not_available / manual freshness check used`。

## 当前轮验证

本阶段完成文档补齐后运行：

```bash
make -C test-rvv/features/ppf run_test_compare
```

结果：Std build 5/5 pass，RVV build 9/9 pass。该命令在 QEMU 中运行，证明 correctness 和
production-direct trace tests 仍通过；它不作为性能证据。

## Continue / Stop Decision

当前不建议继续在同一 PPF topic 内尝试新优化：

- `alpha_m` RVV path 已经有 production-public board positive 数据；继续压缩 staging buffer 需要新 profile
  和 RVV-vs-RVV A/B，目前没有证据说明它是高价值瓶颈。
- Phase 010 的 SoA-staged pair-feature batch RVV 已有板卡负向证据；direct-AoS revisit 需要新的 profile
  或实现假设，不能无证据自动继续。
- evidence hardening 只增强归档质量，不是新的优化方式。
- PPFRGB、CPPF 或其它 caller 有不同 color、region search、row source 和 output 语义，必须作为独立 topic。

`next_phase_default`: ready for review。若用户后续明确要求继续扩展，默认恢复到独立 phase / topic：
evidence hardening、profile-driven direct-AoS pair-feature revisit，或 PPFRGB 跟随评估。
