# Phase 000: stick count diagnostic result

## 当前结论

`count-indexed-gather-f32m2-dual-count` 已完成本阶段诊断闭环。它只证明 `SampleConsensusModelStick<PointT>::countWithinDistance` 在 `PointXYZ`、direct indexed row source（直接索引行来源）、float xyz AoS（结构数组）布局和测试专用 candidate（候选实现）边界下，RVV（RISC-V Vector，可变长向量扩展）路径可以复刻 stick 的 `nr_i / nr_o` 双计数语义，并在板卡 repeated bench（重复性能测试）中表现为 positive-stable（稳定正向）。

EvidenceDecision（证据决策）：`partial-production-candidate`。当前没有 production patch（生产补丁），没有 adopted production behavior（已采用生产行为），也不能把本阶段诊断证据写成 production direct（真实生产路径证据）。

## 执行范围回填

| action | status | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| RED test | done | `make -C test-rvv/sample_consensus/sac_model_stick run_test_compare` 曾因 `impl/sac_model_stick_diagnostic.hpp` 缺失失败 | 测试能卡住候选入口缺失，不是事后只验证已有行为。 |
| GREEN candidate | done | `make -C test-rvv/sample_consensus/sac_model_stick run_test_compare` | Std/RVV 两个构建均通过 2 个 gtest；覆盖内圈、外圈、远外圈、乱序 indices 和 `nr_i <= nr_o` 返回 0。 |
| bench scaffold | done | `make -C test-rvv/sample_consensus/sac_model_stick dump_bench_rvv` | bench 输出 public count 与 diagnostic candidate 两行；QEMU 只用于构建 / 反汇编，不写性能结论。 |
| board repeated evidence | done | `SSH_AUTH_SOCK=/run/user/$(id -u)/keyring/ssh make -C test-rvv/sample_consensus/sac_model_stick collect_repeated_board_evidence` | 5-run budget 用完，candidate decision bucket 稳定为 positive-stable。 |
| Evidence Doctor / registry | done | `make -C test-rvv/sample_consensus/sac_model_stick record_repeated_board_evidence_state` | 生成并登记 manifest、doctor md、doctor json；Errors=0，Warnings=1，Suggestions=1。 |
| docs | done | 本文件、evaluation、roadmap、matrix、phase index、README | 当前证据和下一阶段入口已回填。 |

## 正确性与路径证据

QEMU correctness（QEMU 正确性验证）通过 `run_test_compare` 覆盖 2 个测试：

- `CountCandidateMatchesPublicEntryWithInnerOuterPenalty` 对拍公开入口和测试专用 candidate，输入同时包含内圈、外圈和远外圈点。
- `CountCandidateReturnsZeroWhenOuterBandDominates` 覆盖外圈数量不少于内圈时返回 0 的 stick 专属边界。

反汇编归属来自 `build/asm/riscv/bench_sac_model_stick_rvv.full.asm`。`countWithinDistanceCandidateRVV` 符号内可见 `vfmacc.vv`、`vmflt.vf`、`vmandn.mm` 和两处 `vcpop.m`，对应平方距离、内外阈值 mask（掩码）和内外计数。manifest 统计 public entry 行的 RVV 指令数为 36，candidate helper 行的 RVV 指令数为 27；public 行的计时只是当前 RVV build 中公开入口仍走标量主体的弱对照，不能当作生产加速。

## 板卡 repeated 结果

板卡为 Milkv-Jupiter。bench 数据集是 synthetic sac_model_stick direct indexed dual-count cloud，规模 65536，iterations 为 200，warmup 为 5。raw board logs（原始板卡日志）留在 `log/board/repeated-20260828-phase000/run-*/`，默认不进入提交边界。

| case | Std ms values | RVV ms values | speedup min / median / max | bucket | 证据角色 |
| --- | --- | --- | --- | --- | --- |
| public `countWithinDistance` | 1.766935, 1.738414, 1.746477, 1.754700, 1.759985 | 1.740628, 1.739192, 1.739014, 1.740333, 1.740355 | 0.9996x / 1.0083x / 1.0151x | neutral / weak cross-check | production-shaped diagnostic 中的公开入口对照；生产源码未接 RVV。 |
| diagnostic candidate `countWithinDistance` | 1.752654, 1.737772, 1.741970, 1.753166, 1.756931 | 0.415630, 0.419461, 0.419929, 0.419746, 0.419423 | 4.1429x / 4.1767x / 4.2169x | positive-stable | test-only candidate 的 RVV-vs-scalar 诊断证据。 |

本阶段的有效性能结论只来自第二行 diagnostic candidate。public 行说明当前 production public entry（生产公开入口）尚未接入 RVV，Std/RVV build 在公开入口上基本等价。

## Evidence Doctor

输入：

- `doc/phases/000-stick-count-diagnostic/repeated-evidence-manifest.json`
- `doc/phases/000-stick-count-diagnostic/repeated-evidence-doctor.md`
- `doc/phases/000-stick-count-diagnostic/repeated-evidence-doctor.json`

结果：Errors=0，Warnings=1，Suggestions=1。

Warning `ba_degradation_frequency` 和 Suggestion `near_threshold_ba` 都落在 public `countWithinDistance` 行。该行不是新 RVV candidate 的同边界收益；它只反映生产公开入口当前仍没有 RVV 分流，且 Std/RVV build 的微小差异接近噪声。处理动作是把 public 行降级为 weak cross-check（弱交叉检查），不把它写成生产收益。candidate 行没有 Evidence Doctor Error 或 Warning，可以支撑 `partial-production-candidate`，但仍不能替代 production direct 证据。

## Evidence Registry 与新鲜度

`log/evidence_registry.json` 已登记 3 个摘要证据文件，run label 为 `stick-phase000-repeated-board`，case filter 为 `stick-count-phase000-repeated`。登记文件为：

- `doc/phases/000-stick-count-diagnostic/repeated-evidence-manifest.json`
- `doc/phases/000-stick-count-diagnostic/repeated-evidence-doctor.md`
- `doc/phases/000-stick-count-diagnostic/repeated-evidence-doctor.json`

当前文档引用这些路径；后续复跑如果改变 speedup、decision bucket、Evidence Doctor 数量或证据角色，本文件、evaluation、roadmap、matrix 和 Handoff 必须刷新。raw run 目录不默认提交。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production-shaped diagnostic`。测试派生类复用真实 `input_` / `indices_` 状态，但代码只在 `test-rvv`。 |
| A/B boundary | diagnostic candidate 行是 `test helper` 边界；public 行是公开入口弱对照，不是 production RVV 分流。 |
| 当前决策问题 | RVV-vs-scalar 是否值得进入 bounded production probe（有界生产探针）。 |
| diagnostic 是否可外推到 production | 部分可外推到公式和访存方向：candidate 使用真实点云、真实 indices 和同一 `countWithinDistance` 语义。但它没有 production dispatch / fallback / protected helper 边界，因此不能直接当作生产证据。 |
| comparison-boundary / baseline mismatch 风险 | 存在。candidate 在测试派生类中实现，编译、内联和符号边界不同于 production。public 行的 weak 结果不能反驳 candidate 的局部正向，也不能证明生产收益。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段 candidate 为 positive-stable，允许规划 PI1；若后续 production direct 变弱或退化，必须停在 PI5 用户检查点，不能自动采纳或回滚。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前 stick 没有已采用 RVV family，所以 PI2-PI5 后可用 production public Std/RVV 判断是否采纳本 family。若新增 identity fast path、select `vcompress` 或其它 family selection，必须补同一 production boundary 内的 RVV-vs-RVV A/B。 |

## Phase Scope 与扩展队列

已验证范围：`countWithinDistance`、direct indexed `indices_`、`PointXYZ`、float xyz AoS、`Eigen::VectorXf` 系数、`double threshold` 转 float 阈值平方、规模 65536 的 synthetic stick-distance bench。

未验证范围：`selectWithinDistance`、`getDistancesToModel`、空 indices、默认整云 identity indices、`PointXYZI` / `PointXYZRGB` / `PointXYZRGBA` / `PointXYZINormal` / 自定义点型、`Scalar=double`、production fallback、真实 RANSAC 上游路径、以及 `getDistancesToModel` 中系数 3-5 作为方向的语义差异。

`point_type_expansion_queue`：若用户授权并且 PI1/PI2 保持窄范围，下一轮可以先只做 `PointXYZ` production probe；泛型点类型扩展必须另建 phase，读取 `doc-rvv/rvv/RVV Generic Point Type Strategy.zh.md`，补 traits / offset / fallback correctness、反汇编和板卡证据。

## Doc Suite 与测试支撑审计

| area | current shape scan | quality bar | decision | evidence / next action |
| --- | --- | --- | --- | --- |
| topic navigation | `README.zh.md` 存在 | 入口、常用命令、证据白名单、当前边界 | adopted | 已更新当前结论和 PI1 入口。 |
| testing overview / correctness / benchmark / code map | 合并在 evaluation 与源码文件注释 | 当前 topic 只有 2 个 gtest、1 个 bench wrapper、1 个 topic-local script | merged | 合并不会阻碍 reviewer 定位；若 PI2 接入 production，再拆成独立 role docs。 |
| phase suite | `doc/phases/README.zh.md`、Phase 000 plan/result、matrix | 可恢复阶段计划和结果 | adopted | Phase 000 已闭合，Phase 010 plan 已创建。 |
| production topic doc | `doc-rvv/sample_consensus/sac_model_stick-RVV.zh.md` 不存在 | 只在 adopted production behavior 或 PI5 用户确认后适用 | not_applicable with evidence | 当前无 production patch。 |
| artifact tracking | 当前 topic 新文件均未跟踪 | README/evaluation/phase 引用的文件必须进入 topic artifact boundary | partial | 提交前需按 topic-only 边界 stage；raw logs 和 build 排除。 |

测试支撑形态：源码位于 `src/`，内部 helper 位于 `include/impl/`，没有旧 `test_support/` 目录，没有 legacy pointer（旧路径指针）或 compatibility alias（兼容别名）。当前 helper 180 行、test 105 行、bench 143 行、script 296 行，未触发 800/1000 行拆分阈值。

## EvidenceDecision 与继续 / 停止判断

Phase 000 decision：`partial-production-candidate`。

继续动作：创建 `010-stick-count-production-integration-plan/plan.zh.md`，只冻结 production integration loop（生产接入闭环）的候选范围、fallback / dispatch、泛型点类型 gate 和 production direct 证据计划。

stop condition：修改 production 源码需要用户明确授权。本轮已完成 PI1 计划，但没有授权进入 PI2 production patch，因此停止在 `pending_user_authorization_for_PI2`，不自行修改 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_stick.hpp`。
