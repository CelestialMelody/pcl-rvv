# Phase 000: sphere select/getDistances 诊断结果

## 当前结论

本阶段完成了 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_sphere.hpp` 的独立 sphere topic scaffold（脚手架）、QEMU correctness（QEMU 正确性验证）、RVV 反汇编检查、板卡 smoke（小型验证）和 5-run repeated board summary（重复板卡摘要）。本阶段没有修改 production（生产源码）。

EvidenceDecision（证据决策）如下：

- `countWithinDistance`：已有 production RVV path（生产 RVV 路径）在本 topic 的 `PointXYZ` direct indexed `indices_` 边界下重新得到稳定正向证据，5-run median speedup 为 `3.5192x`，min/max 为 `3.4878x / 3.5435x`，manifest 记录 `countWithinDistanceRVV` 反汇编 RVV 指令数为 `19`。
- `selectWithinDistance` 测试专用候选：当前 `RVV squared-distance + scalar output` 设计是 `partial-production-candidate`（局部生产候选）。它在 production-shaped diagnostic（生产形态诊断）中 5-run median speedup 为 `1.4216x`，min/max 为 `1.4141x / 1.4369x`，checksum 一致；但候选 helper 被内联，manifest 对 `selectWithinDistanceCandidateRVV` 的符号级 `rvv_instr_count=0`，只能写成“二进制内联区域有 RVV 指令”，不能写成热点符号归属已闭合。
- `getDistancesToModel` 测试专用候选：当前 `RVV squared-distance + scalar sqrt/store` 设计被拒绝为当前实现族的 production 候选。5-run median speedup 为 `0.7781x`，min/max 为 `0.7605x / 0.7906x`，所有 run 退化。负向信号只拒绝当前测试 helper 形态，不证明所有未来 `getDistancesToModel` RVV 设计都不可行。
- `public selectWithinDistance` 与 `public getDistancesToModel`：当前 production 仍是标量。Std/RVV public 对比只说明两个构建中的现有公开入口近似中性或轻微退化，不能被写成 RVV 生产收益。

## 计划回填

| action | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| 新增独立 topic Makefile 和 board.mk | done | `test-rvv/sample_consensus/sac_model_sphere/Makefile`、`board.mk` | 已提供 Std/RVV test、bench、board 和 manifest 入口。 |
| 新增 correctness harness | done | `make -C test-rvv/sample_consensus/sac_model_sphere run_test_compare` | Std/RVV 两侧各 3 个测试通过，覆盖 shell 边界、`PointXYZI` layout 和测试专用候选对拍。 |
| 新增 bench harness | done | `src/bench_sac_model_sphere.cpp`，board `run_board_bench_compare` | 输出 public select/count/getDistances 以及 diagnostic candidate select/getDistances 五个 bench item。 |
| QEMU correctness | done | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` | QEMU 只支撑正确性和日志形状，不支撑性能结论。 |
| asm attribution（反汇编归属） | partial | `make -C test-rvv/sample_consensus/sac_model_sphere dump_bench_rvv`，`build/asm/riscv/bench_sac_model_sphere_rvv.full.asm` | `countWithinDistanceRVV` 有符号级 RVV 指令归属；测试候选在内联区域可见 gather / FMA / store 指令，但 topic-local manifest 当前无法把这些指令归到候选符号。 |
| board evidence（板卡证据） | done | `SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_sphere board_smoke`，另做 5-run repeated 手工预算 | 板卡可达；5-run 结果见 repeated manifest 和 repeated Evidence Doctor。 |
| Evidence Doctor（证据体检） | done with errors explained | `repeated-evidence-manifest.json`、`repeated-evidence-doctor.md` | 3 个 Error 和 1 个 Suggestion 已解释并用于降级 / 拒绝对应证据边界。 |

## 重复板卡数据

输入为 `PointXYZ`、`65536` 点、`200` iterations、`5` warmup iterations、5-run repeated。数值为 Std/RVV speedup 或 candidate Std/RVV speedup；性能结论只来自板卡。

| case | evidence role | median | min | max | decision bucket | 当前解释 |
| --- | --- | ---: | ---: | ---: | --- | --- |
| public `selectWithinDistance` | production-shaped diagnostic / public scalar stability | `1.0021x` | `0.9828x` | `1.0102x` | neutral | 当前公开入口两侧都走标量，接近 1 是预期；2/5 低于 1，不能写成收益。 |
| public `countWithinDistance` | production direct（真实生产路径证据） | `3.5192x` | `3.4878x` | `3.5435x` | positive-stable | 现有 count RVV 生产路径稳定正向。 |
| public `getDistancesToModel` | production-shaped diagnostic / public scalar stability | `0.9864x` | `0.9597x` | `1.0035x` | neutral-to-negative | 当前公开入口两侧都走标量；4/5 低于 1，只说明 public scalar 对比不稳定，不说明 RVV 已退化。 |
| diagnostic candidate `selectWithinDistance` | production-shaped diagnostic | `1.4216x` | `1.4141x` | `1.4369x` | positive-stable | 测试专用 RVV 平方距离 + 标量输出候选值得进入有界 PI1 生产接入计划。 |
| diagnostic candidate `getDistancesToModel` | production-shaped diagnostic | `0.7781x` | `0.7605x` | `0.7906x` | negative | 当前 scratch store + 标量 sqrt 写回设计不值得进入 production probe（生产探针）。 |

## Evidence Doctor 处理

当前有效 Evidence Doctor 报告是 `test-rvv/sample_consensus/sac_model_sphere/doc/phases/000-sphere-select-distance-diagnostic/repeated-evidence-doctor.md`。早期 `evidence-doctor.md` 只解析 Markdown summary，`comparisons=0`，已降级为历史辅助检查，不能作为当前结论依据。

| finding | severity | 处理动作 | 对结论的影响 |
| --- | --- | --- | --- |
| public `selectWithinDistance` 2/5 低于 1 | Error | 降级为 public scalar stability 观察，不写成 RVV 收益。 | 不阻塞测试专用 select 候选；阻止把 public select Std/RVV 写成 production positive。 |
| public `getDistancesToModel` 4/5 低于 1 | Error | 降级为 public scalar stability 观察。 | 不作为 `getDistancesToModel` RVV 负向证据；真实负向来自测试专用候选行。 |
| diagnostic candidate `getDistancesToModel` 5/5 低于 1 | Error | 拒绝当前 candidate family，后续需要 RVV sqrt/helper 或消融后再恢复。 | 阻止将当前 getDistances 候选推进 production。 |
| public `selectWithinDistance` near threshold | Suggestion | 不再扩大本阶段 run budget；该行不是候选收益。 | 保留为稳定性说明。 |

## Diagnostic 到 production mismatch audit 回填

| question | result |
| --- | --- |
| evidence role | `selectWithinDistance` 候选和 `getDistancesToModel` 候选都是 production-shaped diagnostic；`countWithinDistance` 是已有 production direct。 |
| A/B boundary | 候选行为 `test_helper`，public count 行为 `public_overload`。 |
| 当前决策问题 | 对 `selectWithinDistance` 是 RVV-vs-scalar 候选筛选；对 `getDistancesToModel` 是当前 implementation-shape（实现形态）是否值得继续。 |
| diagnostic 是否可外推到 production | 不能直接外推。`selectWithinDistance` 只允许进入 PI1 计划，PI2-PI5 仍需真实 production patch、fallback、direct tests、asm 和 board 重跑。 |
| comparison-boundary / baseline mismatch 风险 | 存在。Std/RVV 是两个构建，候选 helper 不在 production dispatch 内；public select/getDistances 行不是 RVV 候选收益。 |
| 弱 / 负 / 中性 diagnostic 是否允许 bounded production probe | `selectWithinDistance` 当前不是弱 / 负 / 中性，而是 positive-stable，可允许 PI1 有界生产计划；`getDistancesToModel` 当前 negative，不允许按当前实现族进入生产探针。 |
| clean adoption 是否需要 production boundary 证据 | 需要。任何 production 采纳必须经过 PI1-PI5 和用户确认；若后续出现多个 select RVV family，还要补同一 production boundary 内 RVV-vs-RVV detail A/B。 |

## 范围边界

| scope type | 当前状态 |
| --- | --- |
| validated_scope | `PointXYZ` / `PointXYZI` correctness；`PointXYZ` 65536 direct indexed `indices_` board repeated；float xyz AoS layout；`Eigen::VectorXf` 系数；`double threshold` 转 float 后 shell 边界。 |
| unvalidated_scope | `PointXYZRGB` / `PointXYZRGBA` / normal 复合点型、自定义 registered xyz 点型、`Scalar=double`、非 AoS 或非 single-float xyz layout、超大 cloud 的 32-bit byte offset gate、production dispatch 新增、circle / normal-sphere 外推。 |
| point_type_expansion_queue | PI1 只能计划 `RVVXYZFloatLayout<PointT>` 或更强 AoS gate 的 fallback；后续若采纳生产补丁，单独补 `PointXYZRGB` / `PointXYZRGBA` / 自定义 registered xyz correctness、asm、board 和 Evidence Doctor。 |
| phase_closeout_boundary | Phase 000 只关闭独立 topic scaffold、测试专用候选 correctness、board diagnostic evidence 和 count 生产路径复核；不关闭 select/getDistances production。 |

## 文档套件和结构审计

当前 topic 已有 `src/` 测试 / bench 源码、topic-local script、evaluation、roadmap、phase index 和 matrix；但缺少 README navigation、testing overview、correctness tests、benchmark/evidence、optimization evidence 和 test support code map 独立 role 文档。由于本 topic 已有 board summary 和 Evidence Doctor，doc-suite role inventory（文档套件职责清单）不能省略。

| role | 当前状态 | decision | next action |
| --- | --- | --- | --- |
| topic_navigation | 缺少 `test-rvv/sample_consensus/sac_model_sphere/README.zh.md` | phase_deferred + unblocked | Phase 010 补 README 和证据白名单。 |
| testing_overview | 缺少独立文档 | phase_deferred + unblocked | Phase 010 补运行入口分类和 target 粒度审计。 |
| correctness_tests | 仅源码注释承载 | phase_deferred + unblocked | Phase 010 补 TEST 字典。 |
| benchmark_and_evidence | 仅 phase result / manifest 承载 | phase_deferred + unblocked | Phase 010 补 bench label、board、doctor 和提交边界。 |
| optimization_evidence | matrix 已有初始条目但缺少 role 文档 | phase_deferred + unblocked | Phase 010 补候选到证据映射。 |
| test_support_code_map | evaluation 有 Traceability Map，但缺少代码地图 | phase_deferred + unblocked | Phase 010 补测试支撑代码地图。 |
| production_topic_doc | 无用户确认采纳的 select/getDistances production 行为 | not_applicable with evidence | 不创建 `doc-rvv/sample_consensus/sac_model_sphere-RVV.zh.md`。 |

## Evidence freshness 和 registry

- `repeated-evidence-manifest.json` 与 `repeated-evidence-doctor.md` 是当前 truth；早期单次 board log 只作为历史输入。
- 本 topic 尚无 `log/evidence_registry.json`，因此当前 `evidence_registry_status=not_available`。人工 freshness check 使用 `repeated-20260827-phase000/run-01..run-05`、manifest、doctor 和 `git status --short --untracked-files=all -- test-rvv/sample_consensus/sac_model_sphere`。
- raw board logs 位于 `test-rvv/sample_consensus/sac_model_sphere/log/board/`，默认 local-only；本阶段可提交候选只包括 topic 文档、测试源码、Makefile、script、manifest 和 doctor summary。

## Continue / stop decision

Phase 000 当前完成，`stop_condition_hit=none`。因为 doc suite role inventory 仍有当前 topic 授权范围内的 `phase_deferred + unblocked` 项，下一阶段默认不是 `ready_for_review`，而是：

```text
next_phase_default: 010-structure-parity-doc-suite
```

Phase 010 完成后，若无新的结构阻塞，默认进入 `020-select-production-integration-plan`，只为 `selectWithinDistance` 写 PI1 计划；`getDistancesToModel` 当前 candidate family 保持 rejected。
