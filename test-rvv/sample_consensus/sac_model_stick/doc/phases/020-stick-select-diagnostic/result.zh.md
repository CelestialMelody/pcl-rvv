# Phase 020: stick select diagnostic result

## 当前结论

`select-indexed-gather-f32m2-compress` 已完成本阶段 diagnostic（诊断）闭环。它只证明 `SampleConsensusModelStick<PointT>::selectWithinDistance` 在 `PointXYZ`、direct indexed row source（直接索引行来源）、float xyz AoS（结构数组）布局和 test-only candidate（测试专用候选）边界下，RVV（RISC-V Vector，可变长向量扩展）路径可以复刻公开入口的 inlier（内点）保序输出和 `error_sqr_dists_` 平方距离记录，并在板卡 repeated bench（重复性能测试）中表现为 positive-stable（稳定正向）。

EvidenceDecision（证据决策）：`partial-production-candidate`。当前没有 production patch（生产补丁），没有 adopted production behavior（已采用生产行为），也不能把本阶段诊断证据写成 production direct（真实生产路径证据）。

## 执行范围回填

| action | status | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| Phase plan | done | `doc/phases/020-stick-select-diagnostic/plan.zh.md` | 冻结 `PointXYZ`、direct indexed `indices_`、test-only select candidate 范围。 |
| RED test | done | `make -C test-rvv/sample_consensus/sac_model_stick run_test_compare` 先因缺少 `selectWithinDistanceCandidate` 编译失败 | 测试能卡住 select candidate 缺口；不是事后只验证已有行为。 |
| GREEN candidate | done | `make -C test-rvv/sample_consensus/sac_model_stick run_test_compare` | Std/RVV 两个构建均通过 4 个 gtest；新增 case 覆盖乱序 indices、命中/未命中混合、`error_sqr_dists_` 顺序和无命中时清空旧状态。 |
| bench / asm | done | `make -C test-rvv/sample_consensus/sac_model_stick dump_bench_rvv` | bench 输出 public/candidate count 和 select 四行；`selectWithinDistanceCandidateRVV` 符号可见 RVV 指令。 |
| board repeated evidence | done | `SSH_AUTH_SOCK=/run/user/$(id -u)/keyring/ssh make -C test-rvv/sample_consensus/sac_model_stick collect_repeated_board_evidence` | 5-run budget 用完，select candidate decision bucket 稳定为 positive-stable。 |
| Evidence Doctor / registry | done | `make -C test-rvv/sample_consensus/sac_model_stick record_repeated_board_evidence_state` | Phase 020 生成并登记 select-only manifest、doctor md、doctor json；Errors=0，Warnings=1，Suggestions=1。 |

## 正确性与路径证据

QEMU correctness（QEMU 正确性验证）通过 `run_test_compare` 覆盖 4 个测试，其中 Phase 020 新增：

- `SelectCandidateMatchesPublicEntryAndErrorDistances`：公开入口和 test-only candidate 对拍，输入含乱序 `indices_`、命中 / 未命中混合点，验证 `inliers` 顺序和 `error_sqr_dists_` 一一对应。
- `SelectCandidateClearsStaleErrorDistancesWhenNoInliers`：先制造旧 `error_sqr_dists_`，再用零阈值触发无命中，验证 candidate 清空旧输出状态。

反汇编归属来自 `build/asm/riscv/bench_sac_model_stick_rvv.full.asm`。`selectWithinDistanceCandidateRVV` 符号内可见 `vfmacc.vv`、`vmflt.vf`、`vcpop.m` 和两处 `vcompress.vm`，对应平方距离、阈值 mask、命中计数、原始 index 和平方距离压缩暂存。manifest 统计 select candidate 行的 RVV 指令数为 36；public select 行也在 RVV build 中有 RVV 指令计数，但该行仍是公开入口弱对照，不能说明 production dispatch 已接入新候选。

## 板卡 repeated 结果

板卡为 Milkv-Jupiter。bench 数据集是 synthetic sac_model_stick direct indexed dual-count cloud，规模 65536，iterations 为 200，warmup 为 5。raw board logs（原始板卡日志）留在 `log/board/repeated-20260828-phase020/run-*/`，默认不进入提交边界。

| case | Std ms values | RVV ms values | speedup min / median / max | bucket | 证据角色 |
| --- | --- | --- | --- | --- | --- |
| public `selectWithinDistance` | 2.215921, 2.196753, 2.203695, 2.208677, 2.238810 | 2.204709, 2.234530, 2.194879, 2.208211, 2.210671 | 0.9831x / 1.0040x / 1.0127x | neutral / weak cross-check | production-shaped diagnostic 中的公开入口对照；production 源码未接 select RVV。 |
| diagnostic candidate `selectWithinDistance` | 2.181857, 2.182000, 2.186541, 2.190400, 2.181015 | 0.636417, 0.633276, 0.629014, 0.636157, 0.637571 | 3.4208x / 3.4432x / 3.4761x | positive-stable | test-only candidate 的 RVV-vs-scalar 诊断证据。 |

本阶段的有效性能结论只来自第二行 diagnostic candidate。public 行说明当前 production public entry（生产公开入口）尚未接入 select RVV，Std/RVV build 在公开入口上基本等价。

## Evidence Doctor

输入：

- `doc/phases/020-stick-select-diagnostic/repeated-evidence-manifest.json`
- `doc/phases/020-stick-select-diagnostic/repeated-evidence-doctor.md`
- `doc/phases/020-stick-select-diagnostic/repeated-evidence-doctor.json`

结果：Errors=0，Warnings=1，Suggestions=1。

Warning `ba_degradation_frequency` 和 Suggestion `near_threshold_ba` 都落在 public `selectWithinDistance` 行。该行不是新 RVV candidate 的同边界收益；它只反映 production public entry 当前仍没有 RVV 分流，且 Std/RVV build 的微小差异接近噪声。处理动作是把 public 行降级为 weak cross-check（弱交叉检查），不把它写成生产收益。candidate 行没有 Evidence Doctor Error 或 Warning，可以支撑 `partial-production-candidate`，但仍不能替代 production direct 证据。

## Evidence Registry 与新鲜度

`log/evidence_registry.json` 已登记 Phase 020 的 3 个摘要证据文件，run label 为 `stick-phase020-repeated-board`，case filter 为 `stick-select-phase020-repeated`。登记文件为：

- `doc/phases/020-stick-select-diagnostic/repeated-evidence-manifest.json`
- `doc/phases/020-stick-select-diagnostic/repeated-evidence-doctor.md`
- `doc/phases/020-stick-select-diagnostic/repeated-evidence-doctor.json`

后续复跑如果改变 speedup、decision bucket、Evidence Doctor 数量或证据角色，本文件、evaluation、roadmap、matrix 和 Handoff 必须刷新。raw run 目录不默认提交。本轮采集时远端临时输出目录仍使用了旧 `phase000-stick-count-run-*` 字面量；本地证据目录、manifest、registry 和文档均使用 Phase 020 路径，Makefile 已修正后续远端输出标签。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production-shaped diagnostic`。测试派生类复用真实 `input_` / `indices_` 和 `error_sqr_dists_` 状态，但代码只在 `test-rvv`。 |
| A/B boundary | diagnostic candidate 行是 `test helper` 边界；public 行是公开入口弱对照，不是 production RVV 分流。 |
| 计时边界 | 只包含入口调用、输出容器 clear/reserve/write 和 `error_sqr_dists_` 写回；不包含点云、indices 和系数构造。 |
| 当前决策问题 | RVV indexed gather + `vcompress` 是否值得进入 bounded production probe（有界生产探针）。 |
| diagnostic 是否可外推到 production | 部分可外推到公式、访存、mask 和压缩输出方向：candidate 使用真实点云、真实 indices 和同一 select 语义。但它没有 production dispatch / fallback / protected helper 边界，因此不能直接当作生产证据。 |
| comparison-boundary / baseline mismatch 风险 | 存在。candidate 在测试派生类中实现，编译、内联和符号边界不同于 production。public 行的 weak 结果不能反驳 candidate 的局部正向，也不能证明生产收益。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段 candidate 为 positive-stable，允许规划 PI1；若后续 production direct 变弱或退化，必须停在 PI5 用户检查点，不能自动采纳或回滚。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前 stick 没有已采用 select RVV family，所以 PI2-PI5 后可用 production public Std/RVV 判断是否采纳本 family。若新增 identity fast path 或其它 family selection，必须补同一 production boundary 内的 RVV-vs-RVV A/B。 |

## Phase Scope 与扩展队列

已验证范围：`selectWithinDistance`、direct indexed `indices_`、`PointXYZ`、float xyz AoS、`Eigen::VectorXf` 系数、`double threshold` 转 float 阈值平方、规模 65536 的 synthetic stick-distance bench、保序 `inliers` 和 `error_sqr_dists_`。

未验证范围：production `selectWithinDistance` dispatch、production `countWithinDistance` patch、`getDistancesToModel`、空 / 默认整云 identity indices、`PointXYZI` / `PointXYZRGB` / `PointXYZRGBA` / `PointXYZINormal` / 自定义点型、`Scalar=double`、真实 RANSAC 上游路径和 production fallback。

`point_type_expansion_queue`：若用户授权并且 PI1/PI2 保持窄范围，下一轮可以先只做 `PointXYZ` production probe；泛型点类型扩展必须另建 phase，读取 `doc-rvv/rvv/RVV Generic Point Type Strategy.zh.md`，补 traits / offset / fallback correctness、反汇编和板卡证据。

## Doc Suite 与测试支撑审计

测试支撑仍位于 `src/`、`include/impl/` 和 topic-local `script/`；没有旧 `test_support/` 目录，没有 legacy pointer（旧路径指针）或 compatibility alias（兼容别名）。新增 select 后 helper、test、bench 和 script 仍未触发 800/1000 行拆分阈值；职责边界可由文件头注释、case label 和 manifest metadata 审查。

## EvidenceDecision 与继续 / 停止判断

Phase 020 decision：`partial-production-candidate`。

继续动作：创建 `030-stick-select-production-integration-plan/plan.zh.md`，只冻结 select production integration loop（生产接入闭环）的候选范围、fallback / dispatch、泛型点类型 gate 和 production direct 证据计划。

stop condition：修改 production 源码需要用户明确授权。本阶段不会自行修改 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_stick.hpp`。若继续当前 topic 而不改 production，下一个未阻塞动作是 `getDistancesToModel` 系数语义审计 / diagnostic phase。
