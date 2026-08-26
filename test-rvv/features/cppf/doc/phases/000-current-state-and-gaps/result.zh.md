# Phase 000 Result: Current State And Gaps

## EvidenceDecision

Phase 000 结论为 `bench-only diagnostic / no-production`。本阶段完成了 CPPF test scaffold、标量 reference、两个 test-only RVV candidate、QEMU/board correctness、反汇编抽查、board repeated benchmark 和 Evidence Doctor。两个 RVV candidate 都通过 correctness，但板卡 repeated 结果稳定负向，因此不进入 production integration loop（生产接入闭环）。

## 动作回填

| action | status | 产物 / 命令 | 结果 |
| --- | --- | --- | --- |
| 建立 topic scaffold | done | `Makefile`、`board.mk`、`include/cppf.h`、`include/impl/*`、`src/test_cppf.cpp`、`src/bench_cppf.cpp`、`script/generate_cppf_evidence_manifest.py` | 构建、QEMU、board 和 Doctor 入口可用。 |
| 标量 reference | done | `include/impl/cppf_reference.hpp` | 与 public `CPPFEstimation::compute` 对拍通过。 |
| RVV pair/HSV candidate | attempted / rejected | `include/impl/cppf_pair_hsv_candidate.hpp` | correctness 通过；board median `0.49x`，不支持 production probe。 |
| RVV alpha candidate | attempted / rejected | `include/impl/cppf_alpha_candidate.hpp` | closed-form 与 Eigen reference 对拍通过；board median `0.82x`，不支持 production probe。 |
| QEMU correctness | done | `make -C test-rvv/features/cppf run_test_compare` | Std/RVV 均 5/5 gtest 通过。 |
| QEMU bench smoke | done / smoke only | `ALLOW_QEMU_BENCH_COMPARE=1 ... run_bench_compare` | 用于日志形状和 checksum smoke，不作为性能结论。 |
| 反汇编抽查 | done | `make -C test-rvv/features/cppf dump_bench_rvv` | RVV bench binary 有 `computeCPPFPairHSVBatchRVV` 和 `computeCPPFAlphaMBatchRVV` 符号；full asm 含 RVV load/store、mask 和 FP 指令。 |
| 板卡 correctness | done | `make -C test-rvv/features/cppf run_board_test fetch_board_logs ...` | 板卡 gtest 5/5 通过。 |
| 板卡 repeated + Doctor | done | `make -C test-rvv/features/cppf BENCH_ARGS='--side 24 --index-count 64 --repeat 6 --iterations 8 --warmup 2 --case-filter all' board_repeated evidence_doctor_repeated` | 5-run repeated 完成；Doctor `3E/0W/9S`。 |
| 文档 closeout | done | README、evaluation、roadmap、matrix、本 result、筛选清单 | no-production 证据链和恢复边界已同步。 |

## Board Summary

| case | B/A values | mean | median | decision |
| --- | --- | --- | --- | --- |
| `component_cppf_reference` | `1.00, 1.01, 1.02, 1.01, 1.00` | `1.008x` | `1.01x` | diagnostic scalar baseline adopted；收益接近阈值，不写成加速结论。 |
| `candidate_cppf_pair_hsv_batch_rvv` | `0.48, 0.49, 0.49, 0.49, 0.48` | `0.486x` | `0.49x` | rejected。 |
| `candidate_cppf_alpha_m_batch_rvv` | `0.82, 0.83, 0.82, 0.82, 0.82` | `0.822x` | `0.82x` | rejected。 |
| `public_cppf_compute` | `0.99, 0.98, 0.99, 1.00, 0.99` | `0.99x` | `0.99x` | public smoke baseline only；production 当前没有 RVV dispatch。 |

证据摘要路径：`test-rvv/features/cppf/log/board/repeated/evidence_manifest.json` 和 `test-rvv/features/cppf/log/board/repeated/evidence_doctor.md`。这两个路径在 `test-rvv/.gitignore` 的 `**/log/**` 下，默认 local-only；若用户要求提交 evidence logs，应精确 `git add -f` 摘要文件，不提交 raw run logs。

## Evidence Doctor 处理

Doctor 报 `Errors=3, Warnings=0, Suggestions=9`。3 个 Error 都是 `ba_degradation_frequency`，说明候选或 smoke case 在 5-run 中高频低于 `1.0x`。处理动作：

| finding | 处理 |
| --- | --- |
| `candidate_cppf_pair_hsv_batch_rvv` 高频退化 | 降级为 rejected；不进入 production probe。 |
| `candidate_cppf_alpha_m_batch_rvv` 高频退化 | 降级为 rejected；不进入 production probe。 |
| `public_cppf_compute` 高频退化 | 只作为当前 production baseline smoke，不作为生产 RVV 证据。 |
| metadata / binary hash suggestions | 记录为证据边界。因为方向稳定负向，当前不追加复跑；若未来重开 production probe，需补 taskset/governor/freq/temperature 和 binary hash。 |

## Diagnostic 到 Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `diagnostic`；不是 production direct。 |
| A/B boundary | test helper 的 Std/RVV build 对比，以及 public baseline smoke。 |
| 当前决策问题 | 是否进入 bounded production probe。 |
| diagnostic 是否可外推到 production | 当前不能。两个候选都是测试专用 staging / formula 形态，且板卡结果为 negative。 |
| comparison-boundary / baseline mismatch 风险 | 有；component reference 和 production `push_back` public path 不是同一 helper 边界。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前 negative，按 phase plan 不建议继续。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 需要；但本阶段未进入 production integration loop。 |

## Doc Suite Role Inventory

| role | status | path / section | evidence |
| --- | --- | --- | --- |
| topic_navigation | standalone | `test-rvv/features/cppf/README.zh.md` | 提供当前结论、阅读入口、命令和证据白名单。 |
| testing_overview | merged | `README.zh.md#常用命令`、`cppf-evaluation.zh.md#正确性证据` | 当前只有 aggregate correctness 和 all-case bench；无更多细分 target。 |
| correctness_tests | merged | `cppf-evaluation.zh.md#正确性证据`、`src/test_cppf.cpp` | 5 个 gtest 的输入和证明范围已在源码注释与 evaluation 中说明。 |
| benchmark_and_evidence | merged | `cppf-evaluation.zh.md#诊断证据链`、本文件 `Board Summary` | case-filter、run budget、Doctor 和提交边界已说明。 |
| optimization_evidence | merged | `doc/phases/optimization-matrix.zh.md`、`cppf-evaluation.zh.md#诊断证据链` | 每个 candidate 的 decision 有板卡和 Doctor 证据。 |
| optimization_roadmap | standalone | `test-rvv/features/cppf/doc/optimization-roadmap.zh.md` | 记录 rejected / deferred candidate 和恢复条件。 |
| test_support_code_map | merged | `cppf-evaluation.zh.md#Traceability Map（可追踪性地图）` | production、test-support、bench、script、evidence output 可定位。 |
| phase_index | standalone | `test-rvv/features/cppf/doc/phases/README.zh.md` | 默认恢复入口明确。 |
| evaluation_diagnostic | standalone | `test-rvv/features/cppf/doc/cppf-evaluation.zh.md` | S2/S11、Traceability Map、no-production 决策已写入。 |
| evaluation_production | not_applicable with evidence | none | 没有 production patch 或 PI5 production evidence。 |
| production_topic_doc | not_applicable with evidence | none | 没有 adopted production behavior；不创建 `doc-rvv/features/cppf-RVV.zh.md`。 |

## Doc Suite Parity Audit

| area | current shape scan | quality bar / supplemental calibration | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| README navigation | 新增 README，列阅读入口、命令和证据白名单。 | README 只做导航，不承担长解释。 | adopted | 文件存在且在 topic artifact 边界。 | none |
| testing-overview | 合并进 README / evaluation。 | 当前 target 粒度较小，合并可审查。 | adopted | `Makefile` 只有 aggregate correctness、bench、board repeated、Doctor targets。 | none |
| target granularity audit | `run_test_compare`、`dump_bench_rvv`、`board_repeated`、`evidence_doctor_repeated` 已覆盖本阶段。 | 缺失 correctness aliases 可在职责单一时裁剪。 | rejected with evidence | gtest 数量少且全部服务同一 component boundary；无 production direct target。 | none |
| correctness-tests | 合并进 evaluation 和 test 源码注释。 | 每个 TEST 的输入、断言和证明范围需可定位。 | adopted | `src/test_cppf.cpp` 顶部注释和 TEST 名已说明。 | none |
| benchmark-and-evidence | 合并进 evaluation 与 phase result。 | 必须区分 QEMU smoke 和 board performance。 | adopted | QEMU smoke 已降级；board repeated / Doctor 路径已列出。 | none |
| optimization-evidence | matrix 独立保存。 | candidate decision 需映射到代码、target、board、asm、Doctor。 | adopted | `optimization-matrix.zh.md` 已覆盖。 | none |
| test-support-code-map | Traceability Map 合并进 evaluation。 | 复杂 topic 需能定位 production/test/script/output。 | adopted | map 已覆盖关键对象。 | none |
| evaluation | 新增 diagnostic evaluation。 | no-production 主归属在 evaluation / phase result。 | adopted | `cppf-evaluation.zh.md` 已写当前结论和 mismatch audit。 | none |
| long-term `doc-rvv` | 不创建。 | 只有 adopted production behavior 才适用。 | not_applicable with evidence | production 源码无改动，PI loop 未进入。 | none |
| phase index / result | 新增 phase README、matrix 和 result。 | phase loop 需可恢复。 | adopted | 默认恢复入口已写。 | none |
| artifact tracking | `git status --short --untracked-files=all -- test-rvv/features/cppf ...` 可发现新增 topic files；log/build 路径被 ignore。 | 新增文档需在 topic artifact 边界；ignored evidence 要说明 local-only。 | adopted | 本 result 和 README 已说明 evidence summary 默认 local-only。 | none |

## Handoff Packet

- `preferences_loaded`：defaults loaded；local override absent；prompt override 为用户授权持续推进且板卡可用。
- `loaded_instruction_sources`：`AGENTS.md`、`.agents/config/defaults.yaml`、`rvv-workflow`、`rvv-test`、`rvv-documentation` 及其 phase loop / closeout / traceability / doc-suite references。
- `production_status`：no production change；`features/include/pcl/features/impl/cppf.hpp` 未修改。
- `evidence_status`：QEMU Std/RVV correctness 5/5；board gtest 5/5；board repeated 5-run 完成；Doctor `3E/0W/9S`，Errors 已解释为性能退化信号。
- `document_ownership_check`：evaluation 承载函数评估和 no-production 证据链；phase result 承载阶段回填和 Handoff；roadmap 承载恢复条件；筛选清单只承载队列状态。
- `traceability_map_status`：required / updated；位置为 `test-rvv/features/cppf/doc/cppf-evaluation.zh.md#Traceability Map（可追踪性地图）`。
- `optimization_roadmap_status`：required / updated；当前无必须继续的未阻塞 RVV production action。
- `evidence_registry_status`：`not_available`；以 manifest / Doctor 路径和 git ignored-boundary 说明替代。若用户要求提交 evidence logs，先精确选择 summary artifacts。
- `instruction_feedback`：report-only；本轮未发现必须修改 `.agents/` 的规则缺口。
