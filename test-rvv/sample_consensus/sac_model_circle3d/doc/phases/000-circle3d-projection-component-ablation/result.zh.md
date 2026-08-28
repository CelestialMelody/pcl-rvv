# Phase 000 Result: circle3d projection component ablation

## 执行范围

本阶段按 `plan.zh.md` 覆盖 `SampleConsensusModelCircle3D<PointT>` 的 count/select 投影距离核。实际验证边界是 test-only candidate（仅测试使用候选）、direct indexed `indices_`、`PointXYZ`、float xyz AoS（结构数组）、`Scalar=float` 系数、规模 65536、板卡 5-run repeated benchmark（重复性能测试）和 Evidence Doctor（证据体检）。

production（生产源码）未修改；`getDistancesToModel`、泛型点类型、`Scalar=double`、非标准 layout、其它 row source（行来源）和 production dispatch（生产分流）均不在本阶段证明范围内。

## 动作回填

| action | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| RED correctness test（预期失败的正确性测试） | done | `make -C test-rvv/sample_consensus/sac_model_circle3d run_circle3d_phase000_tests`，候选方法未实现时 link 失败。 | 测试能捕获缺失的 count/select candidate 入口。 |
| 实现 test-only candidate | done | `include/impl/sac_model_circle3d_candidates.hpp` | RVV 构建下 gated helper 使用 xyz gather、投影、`vfsqrt`、mask、`vcpop` 和 `vcompress`；非 RVV 或退化投影回退 public 标量入口。 |
| QEMU correctness（正确性） | done | `make -C test-rvv/sample_consensus/sac_model_circle3d run_test_compare` | Std/RVV 均 2/2 通过；QEMU 不作为性能证据。 |
| QEMU bench log-shape（日志形状）smoke | done | `make -C test-rvv/sample_consensus/sac_model_circle3d run_bench_rvv BENCH_ARGS='1024 2 1' ALLOW_QEMU_BENCH_COMPARE=0` | 日志包含 `warmup_iterations`，只证明 bench 二进制可运行和输出字段可解析。 |
| asm attribution（反汇编归属） | done | `make -C test-rvv/sample_consensus/sac_model_circle3d check_projection_asm` | `countWithinDistanceProjectionRVV` 和 `selectWithinDistanceProjectionRVV` 符号内能归属 `vluxei32.v`、`vfmacc`、`vfnmsac`、`vfsqrt.v`；select 另有 `vcompress.vm`。 |
| board repeated（板卡重复测试） | done | `make -C test-rvv/sample_consensus/sac_model_circle3d collect_projection_repeated_board_evidence` | 5-run，65536 点，200 次计时迭代，20 次 warm-up。raw logs 在 `log/board/repeated-20260828-phase000-circle3d-projection/run-*/`，默认不提交。 |
| Evidence Doctor / registry | done | `make -C test-rvv/sample_consensus/sac_model_circle3d record_projection_board_evidence_state` | 生成并登记 manifest / doctor，见下方证据路径。 |

## 当前证据路径

| artifact | path | role |
| --- | --- | --- |
| manifest（证据清单） | `test-rvv/sample_consensus/sac_model_circle3d/doc/phases/000-circle3d-projection-component-ablation/projection-repeated-evidence-manifest.json` | 机器可读 repeated board summary。 |
| Evidence Doctor Markdown | `test-rvv/sample_consensus/sac_model_circle3d/doc/phases/000-circle3d-projection-component-ablation/projection-repeated-evidence-doctor.md` | reviewer 可读异常信号摘要。 |
| Evidence Doctor JSON | `test-rvv/sample_consensus/sac_model_circle3d/doc/phases/000-circle3d-projection-component-ablation/projection-repeated-evidence-doctor.json` | 可登记 summary artifact。 |
| evidence registry（证据登记表） | `test-rvv/sample_consensus/sac_model_circle3d/log/evidence_registry.json` | 当前 summary artifact 的 freshness（新鲜度）登记。 |
| asm dump | `test-rvv/sample_consensus/sac_model_circle3d/build/asm/riscv/bench_sac_model_circle3d_rvv.full.asm` | 本地生成反汇编，不默认提交。 |

## 板卡性能摘要

本阶段的性能口径是同一 RVV binary（二进制）内 public helper 与 test-only candidate 的 B/A，其中 B/A = public ms / candidate ms；大于 1 表示 candidate 更快。

| component | B/A values | mean | median | min / max | decision bucket |
| --- | --- | ---: | ---: | ---: | --- |
| `countWithinDistance` public vs projection candidate | 0.5697, 0.5729, 0.5713, 0.5725, 0.5701 | 0.5713 | 0.5713 | 0.5697 / 0.5729 | negative，5/5 退化。 |
| `selectWithinDistance` public vs projection candidate | 1.1150, 1.1441, 1.1279, 1.1342, 1.1258 | 1.1294 | 1.1279 | 1.1150 / 1.1441 | positive，5/5 正向。 |

所有 run 的 count、select checksum（校验和）与 gtest 对拍一致。板卡日志持续出现 `Clock skew detected`，但不影响本阶段二进制执行、日志抓取或 checksum；它作为环境 warning 保留在 Handoff，不作为 correctness failure（正确性失败）。

## Evidence Doctor 结果

当前 doctor 结果：Errors=1，Warnings=0，Suggestions=0。

唯一 Error 是 `countWithinDistance` 的 `ba_degradation_frequency`：5/5 B/A 低于 1，且集中在 0.57x 左右。处理动作是拒绝“count/select 合并候选”作为 production candidate（生产候选），并禁止把 count 方向写成正向或 pending-positive。

`selectWithinDistance` 没有 doctor finding，5/5 B/A 正向；但它仍然只是 component ablation（组件消融）证据，不能直接证明 production dispatch、fallback gate（回退门禁）或泛型点类型已经可采纳。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | component ablation / diagnostic（诊断）。 |
| A/B boundary | 同一 RVV binary 内 public overload companion vs test-only helper；不是真实 production dispatch。 |
| 当前决策问题 | 当前投影 RVV code shape 是否值得进入 bounded production probe（有界生产探针）。 |
| diagnostic 是否可外推到 production | count 不能；select 只能作为“可请求生产探针”的输入，不能作为 clean adoption（干净采纳）证据。 |
| comparison-boundary / baseline mismatch 风险 | yes。baseline 走 public helper，candidate 走派生类 test helper；timer boundary 和 wrapper 不同。 |
| weak / negative / neutral / unstable 时是否允许 bounded production probe | count: no，本候选在当前边界稳定负向；select: yes，但必须由用户明确授权进入 production integration loop（生产接入闭环）。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes。真正保留生产补丁前还需要 PI1-PI5、production direct（真实生产路径）correctness / fallback / asm / repeated board 和再次 EvidenceDecision（证据决策）。 |

## Doc Suite Role Inventory

| role | 状态 | 证据 / 路径 |
| --- | --- | --- |
| topic_navigation | `standalone:test-rvv/sample_consensus/sac_model_circle3d/README.zh.md` | README 指向当前结论、常用命令、证据路径和 `doc-rvv` 不适用性。 |
| testing_overview | `standalone:test-rvv/sample_consensus/sac_model_circle3d/doc/testing-overview.zh.md` | 覆盖 correctness、QEMU smoke、asm、board repeated、doctor / registry target。 |
| correctness_tests | `standalone:test-rvv/sample_consensus/sac_model_circle3d/doc/correctness-tests.zh.md` | 两个 gtest 的输入、断言和证明边界已列出。 |
| benchmark_and_evidence | `standalone:test-rvv/sample_consensus/sac_model_circle3d/doc/benchmark-and-evidence.zh.md` | bench 参数、run label、summary artifact、提交边界已列出。 |
| optimization_evidence | `standalone:test-rvv/sample_consensus/sac_model_circle3d/doc/optimization-evidence.zh.md` | count/select 分别记录 attempted / partial-production-candidate gate。 |
| optimization_roadmap | `standalone:test-rvv/sample_consensus/sac_model_circle3d/doc/optimization-roadmap.zh.md` | 下一动作需要用户确认 production probe 或另开新候选族。 |
| test_support_code_map | `standalone:test-rvv/sample_consensus/sac_model_circle3d/doc/test-support-code-map.zh.md` | 聚合头、candidate、test、bench、script 和 evidence output 均可定位。 |
| phase_index / matrix | `standalone:test-rvv/sample_consensus/sac_model_circle3d/doc/phases/README.zh.md` / `doc/phases/optimization-matrix.zh.md` | Phase 000 状态、默认恢复动作和矩阵已更新。 |
| evaluation_diagnostic | `standalone:test-rvv/sample_consensus/sac_model_circle3d/doc/sac_model_circle3d-evaluation.zh.md` | 当前 EvidenceDecision 和 Traceability Map 主归属。 |
| production_topic_doc | `not_applicable with evidence` | 没有 adopted production behavior、用户确认保留的 production patch 或 PI5 生产证据；不创建 `doc-rvv/sample_consensus/sac_model_circle3d-RVV.zh.md`。 |

## 结构和目标粒度审计

| area | current shape scan | quality bar / calibration | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| test/bench source layout | `src/test_sac_model_circle3d.cpp`、`src/bench_sac_model_circle3d.cpp` | 符合 `artifact_layout` 的 `src/` 布局。 | adopted | QEMU 和 board 均使用同一 source。 | 无。 |
| aggregator and internal helpers | `include/sac_model_circle3d.h` + `include/impl/sac_model_circle3d_candidates.hpp` | 当前 topic 只有一个 candidate family，单内部头可审查。 | adopted | code map 已列职责。 | 若进入 production probe，再拆 production helper 与 test helper 分工。 |
| target granularity | correctness aggregate、phase alias、QEMU smoke、asm check、board repeated、doctor / registry target 都存在。 | 当前没有 historical probe。 | adopted | `Makefile` target 可复现。 | production direct target 需要用户授权生产补丁后新增。 |
| topic-local docs | README、testing overview、correctness、benchmark/evidence、optimization evidence、roadmap、code map、phase suite 和 evaluation 已存在。 | 符合 doc-suite quality bar 的最小拆分。 | adopted | 本文件完成 artifact tracking 入口。 | 无。 |
| evidence freshness | manifest / doctor / registry 已生成；初次无预热 summary 被有预热 batch 覆盖为当前 truth。 | 文档引用当前 summary artifact。 | adopted | `projection_evidence_status` 需要在文档更新后重跑确认。 | 重跑 freshness check。 |
| production long-term docs | 无 `doc-rvv` topic 文档。 | no-production / diagnostic 不默认创建。 | not_applicable with evidence | 当前无生产补丁。 | 用户确认 select production probe 后再进入 PI1。 |

## EvidenceDecision

本阶段 decision 是 `partial-production-candidate gate for selectWithinDistance only`：

- `countWithinDistance` 的 projection RVV candidate 在当前边界 `rejected with evidence`。5-run 有预热 board B/A 均为 0.57x 左右，不建议按该 code shape 接入 count production。
- `selectWithinDistance` 的 projection RVV candidate 在当前边界 `partial-production-candidate`。它有 correctness、asm 和有预热板卡 repeated 正向，但仍需要用户确认后才能进入 production integration loop。
- 合并的 count/select projection candidate 作为一个 family 不采纳；后续如果继续，应拆成 select-only production probe 或另建 count-specific formula / reduction 候选，而不是把本阶段 count 结果藏在 select 正向后面。

## Continue / Stop Decision

`continue_stop_decision = turn_stop_deferred with stop_condition_hit`

停止条件：继续推进到有意义的下一步需要修改 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle3d.hpp` 生产源码，建立 `selectWithinDistance` 的真实 dispatch / fallback / production direct evidence；AGENTS.md 明确短 prompt 不默认授权生产源码修改，PI5 前后也需要用户检查点。当前 worker 因此停在用户确认点。

`next_phase_default = 010-selectWithinDistance-production-probe`，仅在用户明确授权进入 production integration loop 后启动。若用户不授权 production probe，可另开 `010-count-formula-ablation`，但当前证据不建议把 count 合并候选继续推向生产。
