# Phase 010 Public-With-Candidate Diagnostic Result

## 结果摘要

Phase 010 已完成。它在 topic-local（主题本地）测试资产中新增了 public-shaped wrapper（公开入口形态包装器）和 `public_pfhrgb_k_with_candidate` bench case，用同一 KSearch（K 近邻搜索）外层循环调用 Phase 000 的 pair-batch RVV（批量点对 RVV）候选 helper。

当前 EvidenceDecision（证据决策）保持 `partial-production-candidate`，并升级为 `PI1-plan-ready`。
本阶段原始 run 显示 `public_pfhrgb_k_with_candidate` 正向；当前 `log/board/repeated/` 已由 Phase 030/040
rerun 覆盖，当前 truth（当前事实）是 `public_pfhrgb_k_with_candidate` 中位数 `1.21x`、最低 `1.20x`、
0/5 低于 1，`public_pfhrgb_k_with_candidate_reuse` 中位数 `1.25x`。但这些 wrapper 的 scalar side
（标量侧）比真实 `PFHRGBEstimation::compute` 更快，说明它们不是 production-public（真实生产公开入口）
证据，只能证明候选在 KSearch-shaped diagnostic（KSearch 形态诊断）中仍有价值。

## 计划执行回填

| action | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| RED-010 | done | `make -B -C test-rvv/features/pfhrgb run_test_rvv` 先因 `computePublicPFHRGBWithPairBatchCandidate` 缺失失败。 | 测试能捕获 public-shaped candidate wrapper 缺失。 |
| GREEN-010 | done | `make -B -C test-rvv/features/pfhrgb run_test_compare` | Std/RVV 两侧各 3 个 gtest 通过；新增 wrapper 与 public estimator descriptor 对拍一致。 |
| BENCH-010 | done | `src/bench_pfhrgb.cpp` 新增 `public_pfhrgb_k_with_candidate` case。 | Std build 走 scalar fallback，RVV build 走 RVV candidate；checksum 与 `public_pfhrgb_k` 一致。 |
| ASM-010 | done | `make -C test-rvv/features/pfhrgb dump_bench_rvv`；`build/asm/riscv/bench_pfhrgb_rvv.asm` | 反汇编仍包含 `vle32.v`、`vfmacc.vv`、`vfdiv.vv` 等 RVV 指令。 |
| BOARD-010 | done / historical baseline | `make -C test-rvv/features/pfhrgb REPEATED_BOARD_RUNS=5 board_repeated evidence_doctor_repeated` | 原始五轮 repeated 已被 Phase 030/040 rerun 覆盖；当前 summary 含五个 case。 |
| BOARD-SMOKE-POST | superseded | `make -C test-rvv/features/pfhrgb check_board_ssh` | Phase 030/040 rerun 已确认板卡可达；旧 `No route to host` 只作为历史阻塞信号。 |

## Repeated Board 统计

| case | role | Std mean | RVV mean | B/A values | bucket | 解释 |
| --- | --- | --- | --- | --- | --- | --- |
| `candidate_pfhrgb_pair_batch_rvv` | diagnostic | `67.9985 ms` | `70.4262 ms` | `0.98, 0.94, 0.97, 0.97, 0.98` | negative-current-rerun | 当前 rerun 退化；不再作为 helper-only 收益证据。 |
| `public_pfhrgb_k_with_candidate` | production-shaped diagnostic | `568.2984 ms` | `470.2156 ms` | `1.21, 1.20, 1.21, 1.21, 1.22` | positive | candidate 在 KSearch-shaped wrapper 中仍稳定正向，支撑 PI1 计划。 |
| `public_pfhrgb_k_with_candidate_reuse` | production-shaped diagnostic | `565.0126 ms` | `455.4444 ms` | `1.25, 1.25, 1.22, 1.24, 1.25` | positive | reusable workspace 是当前优先 PI1 shape。 |
| `component_pfhrgb_signature` | production-shaped diagnostic | `68.0928 ms` | `68.5011 ms` | `1.00, 0.99, 0.99, 0.99, 0.99` | neutral-negative | 只是标量背景，不作为收益证据。 |
| `public_pfhrgb_k` | production-shaped diagnostic | `615.4144 ms` | `609.2912 ms` | `1.00, 1.02, 1.01, 1.01, 1.00` | neutral | 生产源码未接入 RVV，所以 Std/RVV 公开入口基本没有变化。 |

## Evidence Doctor 回填

输入：`test-rvv/features/pfhrgb/log/board/repeated/evidence_manifest.json`。

当前 rerun 结果：Errors=2，Warnings=0，Suggestions=6。

| severity | item | 处理动作 | 对结论影响 |
| --- | --- | --- | --- |
| Error | `ba_degradation_frequency — candidate_pfhrgb_pair_batch_rvv` | helper-only case 降级为 negative-current-rerun。 | 阻止把 helper-only 收益外推到 production value。 |
| Error | `ba_degradation_frequency — component_pfhrgb_signature` | component case 降级为 neutral-negative。 | 不影响 public-shaped candidate / reuse 正向判断。 |
| Suggestion | `environment_metadata_missing` for five cases | 保留为 metadata 边界；没有编造 taskset、governor、freq、temperature。 | 不阻塞 diagnostic 结论；进入 production evidence 前应尽量补齐或明确边界。 |
| Suggestion | `near_threshold_ba — public_pfhrgb_k` | public case 降级为 neutral。 | 不影响 Phase 010；它证明当前未接 production 的公开入口没有收益。 |

Binary identity（binary 身份）已由 manifest 记录：

- `bench_std=sha256:4a2c0b670595e4c422d86ddf091589c4c749b29ca5b40754ae68742c4831c14c`
- `bench_rvv=sha256:a66366ccb8592ea90fb53b83db124dd400b8bd07a8ac27dfddf184a57aa358bc`

## Diagnostic-To-Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `public_pfhrgb_k_with_candidate` 是 production-shaped diagnostic，不是 production direct。 |
| A/B boundary | topic-local public-shaped wrapper；Std build scalar fallback vs RVV build candidate helper。 |
| 当前决策问题 | RVV-vs-scalar feasibility 和 public-path dilution（公开路径稀释）判断。 |
| diagnostic 是否可外推到 production | 不能直接外推，但比 Phase 000 fixed-neighborhood helper 更接近 public path；足以支持创建 PI1 production integration plan。 |
| comparison-boundary / baseline mismatch 风险 | 有。wrapper 通过 `nearestKSearch` 和 candidate helper 复刻核心流程，但不是 `PFHRGBEstimation::computeFeature` 的真实对象状态、protected helper 和 dispatch。 |
| scalar wrapper 与真实 public 的差异 | 当前 rerun 中 `public_pfhrgb_k_with_candidate` Std mean `568.2984 ms`，reuse wrapper Std mean `565.0126 ms`，真实 `public_pfhrgb_k` Std mean `615.4144 ms`；wrapper 更快，不能用 `1.21x` 或 `1.25x` 直接预测 production-public speedup。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前不是弱 / 负 / 中性 / 不稳定；允许进入 PI1 plan，但开始 production patch 前需要用户检查点。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要。PI5 后还必须由用户确认采纳或回滚，不能由 worker 自动收口。 |

## Phase Scope 与扩展队列

| area | validated_scope | unvalidated_scope | next action |
| --- | --- | --- | --- |
| point type | exact `pcl::PointXYZRGBNormal` | 泛型 RGB 点型、分离 `PointInT`/`PointNT`、RGBA、自定义布局 | PI1 先用 exact gate 或读取泛型策略后显式排队 point-type expansion。 |
| row source | fixed neighborhood 和 KSearch-shaped public wrapper | 真实 `computeFeature` dispatch、radius search、OMP、真实数据集 | PI1 production direct test / bench。 |
| implementation shape | test-only SoA staging + RVV tuple math + scalar scatter | production helper signature、fallback、allocation reuse、direct AoS load | PI1 plan 中冻结生产 patch 最小边界。 |
| evidence | QEMU correctness、ASM、5-run board、Doctor | production-public repeated board、production fallback、sanitized evidence logs | PI1/PI5 处理。 |

## Doc Suite Role Inventory

| role | status | evidence / path | next action |
| --- | --- | --- | --- |
| topic_navigation | standalone:`README.zh.md` | 当前状态、阅读路径、常用命令和 PI1 checkpoint 已记录。 | PI1 后刷新 production 状态。 |
| testing_overview | standalone:`doc/testing-overview.zh.md` | Phase 040 已拆出 target / case 角色。 | PI1 后刷新 production target。 |
| correctness_tests | standalone:`doc/correctness-tests.zh.md` | 四个 gtest 的作用已记录；Phase result 保存 RED/GREEN。 | PI1 增加 production direct / fallback 后刷新。 |
| benchmark_and_evidence | standalone:`doc/benchmark-and-evidence.zh.md` | bench case、board repeated、Doctor 和 registry 已记录。 | production evidence 产生后刷新。 |
| optimization_evidence | standalone:`doc/optimization-evidence.zh.md` | candidate family 到证据和 decision 的映射已拆出。 | PI1 后新增 production-direct row。 |
| optimization_roadmap | standalone:`doc/optimization-roadmap.zh.md` | next phase default 指向 PI1 checkpoint。 | 用户确认后推进或改走 staging reuse。 |
| test_support_code_map | standalone:`doc/test-support-code-map.zh.md` | aggregator、candidate helper、script 和 evidence summary 可定位。 | 若生产接入扩大代码面，刷新 map。 |
| phase_index | standalone:`doc/phases/README.zh.md` | Phase 000/010/020 状态已更新。 | PI1 开始前刷新 board availability。 |
| evaluation_diagnostic | standalone:`doc/pfhrgb-evaluation.zh.md` | 当前 `partial-production-candidate / PI1-plan-ready` 已记录。 | PI1 后升级为 production evaluation 或保留诊断分层。 |
| production_topic_doc | not_applicable with evidence | production 源码尚未修改，无 adopted production behavior。 | PI5 通过且用户确认采纳后再创建 / 刷新 `doc-rvv/features/pfhrgb-RVV.zh.md`。 |

## Target Granularity Audit

| target 类别 | 当前状态 | 证据 |
| --- | --- | --- |
| correctness aggregate | adopted | `run_test_compare` 同时覆盖 Std/RVV 三个 gtest。 |
| correctness aliases | merged into aggregate | 当前 case 数量少，gtest 名称足以定位；PI1 fallback 增加后再拆 alias。 |
| bench diagnostic aliases | adopted via case-filter | bench case label 包含 `component_pfhrgb_signature`、`candidate_pfhrgb_pair_batch_rvv`、`public_pfhrgb_k`、`public_pfhrgb_k_with_candidate`、`public_pfhrgb_k_with_candidate_reuse`。 |
| QEMU smoke aliases | adopted | QEMU 只用于 correctness / log-shape，不写性能结论。 |
| board smoke aliases | adopted | `check_board_ssh` 已恢复可达；board repeated 已运行。 |
| board repeated aliases | adopted | `board_repeated` 生成 5-run repeated summary。 |
| doctor / registry aliases | adopted | `evidence_doctor_repeated` 与 `log/evidence_registry.json` 已刷新。 |
| historical probe guarded aliases | not_applicable with evidence | 尚无 historical production probe 或 rollback target。 |

## Continue / Stop Decision

`continue_stop_decision`: stop at production checkpoint。

`stop_condition_hit`: continuing now requires modifying `features/include/pcl/features/impl/pfhrgb.hpp`, i.e. production source. `AGENTS.md` 默认不把普通 topic prompt 扩展成 production 源码修改授权；production integration loop 是用户可见生产补丁，PI5 还需要对称用户确认采纳或回滚。

`next_phase_default`: `020-pi1-production-integration-plan`。

默认下一动作：用户确认可以进入 PI1 后，按 `020-pi1-production-integration-plan/plan.zh.md` 做 production patch、direct correctness、fallback、asm、board repeated 和 Evidence Doctor。若用户暂不授权生产源码，则保留当前 topic-local diagnostic 资产。
