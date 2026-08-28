# Phase 010: line select diagnostic plan

## 阶段意图和边界

本阶段只为 `SampleConsensusModelLine<PointT>::selectWithinDistance` 建立 production-shaped diagnostic（生产形态诊断，测试专用代码模拟真实公开入口的数据形态）。目标是验证 line 的点到直线 cross3/squaredNorm（叉乘三维展开和平方范数）核在 direct indexed `indices_`、`PointXYZ`、float xyz AoS（结构数组布局）和 RVV 构建下，能保持公开标量入口的 `inliers` 顺序和 `error_sqr_dists_`（平方距离缓存）对应关系，并采集同边界 QEMU correctness（QEMU 正确性）、反汇编、board repeated（板卡重复性能测试）和 Evidence Doctor（证据体检）。

本阶段不修改 production（生产源码），不新增 public API（公开接口），不声明 `countWithinDistance` 已接 production，也不覆盖 `getDistancesToModel`、泛型点型、identity-index fast path（恒等索引快速路径）、空 indices 生产回退或上游 RANSAC 调用方。

## 当前状态清单

| area | 当前状态 |
| --- | --- |
| Phase 000 | `countWithinDistance` 诊断为 `partial-production-candidate`，candidate board median/min/max 为 `4.4569x / 4.4350x / 4.4920x`。 |
| production 状态 | `sac_model_line.hpp` 仍为标量实现；没有 line RVV dispatch（RVV 分流）。 |
| 测试资产 | 已有 `include/impl/sac_model_line_diagnostic.hpp`、`src/test_sac_model_line.cpp`、`src/bench_sac_model_line.cpp` 和 board / Evidence Doctor targets。 |
| select 语义 | 公开入口会先清空 `inliers` 和 `error_sqr_dists_`，按 `indices_` 顺序 push back 命中索引，并把对应平方距离写入 `error_sqr_dists_`。 |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| `select-vcompress-f32m2` | 复用 count 的 indexed xyz gather（索引离散加载）和 cross3 公式，用 mask（掩码）筛选阈值内点，再用 `vcompress`（向量压缩）保序写回 inliers 与平方距离。 | `vcompress` 和压缩距离暂存可能增加内存流量；float 中间量与公开 Eigen 标量路径在阈值附近可能产生微小差异。 |
| `count-indexed-gather-f32m2` | Phase 000 已正向，可作为公式和访存经验。 | count 没有输出容器写回，不能外推到 select 性能。 |
| `getDistances-sqrt-store` | 后续可复用 select/count 公式。 | sqrt 和 double dense store 语义不同，本阶段不关闭。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `select-vcompress-f32m2` | direct indexed `indices_` | `PointXYZ`, float xyz AoS, `Eigen::VectorXf` coefficients, threshold double | test-only `selectWithinDistanceCandidate` vs public `selectWithinDistance` | `make run_test_compare`，case 覆盖乱序 indices、调用前输出清理、inlier 顺序和误差对应关系 | `collect_repeated_board_select_evidence`，case label `public selectWithinDistance` / `diagnostic candidate selectWithinDistance` | 5-run bounded rerun 后判断 | `selectWithinDistanceCandidateRVV` | `run_repeated_board_select_evidence_doctor` | planned | 实现 GREEN、bench、asm、board |
| `getDistances-sqrt-store` | direct indexed `indices_` | `PointXYZ` | getDistances public/candidate | not_applicable in this phase | not_applicable | not_applicable | not_applicable | not_applicable | phase_deferred + unblocked | select 后进入 getDistances phase |
| `point-type-expansion` | direct indexed `indices_` | PointXYZ-like traits | count/select/getDistances | not_applicable in this phase | not_applicable | not_applicable | not_applicable | not_applicable | phase_deferred + unblocked | production scope 后续扩展 |

## 实现和测试动作

| action | 产物 | 命令 / 证据 | 完成判据 |
| --- | --- | --- | --- |
| RED test | `src/test_sac_model_line.cpp` | `make -C test-rvv/sample_consensus/sac_model_line run_test_compare` | 先因缺少 `selectWithinDistanceCandidate` 编译失败，证明测试能卡住 select candidate 缺口。 |
| GREEN candidate | `include/impl/sac_model_line_diagnostic.hpp` | `make -C ... run_test_compare` | Std/RVV 两个构建均通过 select same-chain（同构链路）对拍。 |
| bench scaffold | `src/bench_sac_model_line.cpp` | `make -C ... dump_bench_rvv` | bench 输出 public / diagnostic candidate select 两行，反汇编能归属到 candidate helper。 |
| board repeated | `collect_repeated_board_select_evidence` | 注入 `SSH_AUTH_SOCK` 后运行 5-run board repeated | 每轮 board gtest 通过，Std/RVV bench 日志可解析。 |
| manifest / doctor / registry | topic-local script、Makefile target | `record_repeated_board_select_evidence_state` 和 `repeated_select_evidence_status` | manifest、doctor Markdown / JSON 和 registry 新鲜度闭合。 |
| docs | result、roadmap、matrix、evaluation、role docs | Markdown 文档和 path-limited status scan | 下一轮短 prompt 可从 phase index 和 roadmap 恢复。 |

## Evidence Doctor 和 registry 规则

本阶段使用 `doc/phases/010-line-select-diagnostic/repeated-evidence-manifest.json` 作为 Evidence Doctor 输入。manifest 只收 `public selectWithinDistance` 和 `diagnostic candidate selectWithinDistance` 两行；public row 是 `summary_only_unknown` companion（伴随项），candidate row 是 `production_shaped_diagnostic`。

registry 默认路径仍为 `log/evidence_registry.json`。官方 target 覆盖 manifest / doctor 后必须运行 `record_repeated_board_select_evidence_state`，最终用 `repeated_select_evidence_status` 检查 `fresh`。

## 阶段完成条件

- `select-vcompress-f32m2` 至少需要 correctness、QEMU RVV path、asm attribution（反汇编归属）、board repeated 和 Evidence Doctor 才能进入 `partial-production-candidate`。
- 若 correctness 不通过，decision 为 `attempted / rejected`，不得进入 production。
- 若 board 为弱、负、中性或不稳定，只能说明 select diagnostic boundary 当前不支持候选；不得直接推出 `no-production` 或拒绝 bounded production probe（有界生产探针）。

## 板卡复跑预算和决策桶

板卡可用时使用 `SSH_AUTH_SOCK=<ssh-agent-socket>` 注入当前命令环境。默认 repeated budget 为 5 run，bench warmup 为 5 次，iterations 为 200。decision bucket：

| bucket | 口径 |
| --- | --- |
| positive-stable | median B/A >= 1.20x 且 min B/A >= 1.05x |
| weak-positive | 1.05x <= median B/A < 1.20x |
| neutral | 0.95x <= median B/A < 1.05x |
| negative | median B/A < 0.95x |
| unstable | run 间跨 bucket 或 Evidence Doctor 有未处理 Error |

## 继续 / 停止条件

默认继续到 correctness、bench build、asm、5-run board、Evidence Doctor、registry 和文档同步。只有 production 修改需要用户检查、板卡/工具链不可用、Evidence Doctor Error 未解决、dirty isolation 不安全或当前矩阵没有未阻塞动作时，才停止。

## 文档更新清单

- `README.zh.md`：更新当前结论、常用命令和可提交 summary evidence。
- `doc/sac_model_line-evaluation.zh.md`：加入 select EvidenceDecision 和 Traceability Map。
- `doc/testing-overview.zh.md`、`doc/correctness-tests.zh.md`、`doc/benchmark-and-evidence.zh.md`、`doc/optimization-evidence.zh.md`、`doc/test-support-code-map.zh.md`。
- `doc/optimization-roadmap.zh.md` 和 `doc/phases/optimization-matrix.zh.md`。
- `doc/phases/010-line-select-diagnostic/result.zh.md`。

## roadmap 同步动作

本阶段会把 `select-vcompress-f32m2` 从 `phase_deferred + unblocked` 推进到 `attempted / partial-production-candidate / rejected`，并保留 `getDistances-sqrt-store`、`point-type-expansion` 和需要用户授权的 count/select production integration 路线。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic；代码只在 `test-rvv` 中。 |
| A/B boundary | test helper：公开标量入口 vs 测试专用 select RVV candidate。 |
| 当前决策问题 | RVV-vs-scalar 是否值得进入后续 production probe。 |
| diagnostic 是否可外推到 production | unknown。它复用真实 input / indices / output 容器语义，但没有 production dispatch、fallback 和 protected helper 边界。 |
| comparison-boundary / baseline mismatch 风险 | yes。candidate 在测试派生类中，编译边界、内联形态和输出 buffer 组织可能不同于 production。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 只有 correctness、asm、board 异常解释闭合，且 production patch 能小范围 fallback 时才允许；否则不进入 PI1。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前没有已采用 line select RVV family；若后续比较 `vcompress` 与其它写回族，需要 production boundary 内 A/B。 |

## Phase Scope 与扩展队列

`validated_scope`：本阶段计划只覆盖 `selectWithinDistance`、direct indexed row source、`PointXYZ`、float xyz AoS、`Scalar` 由 `Eigen::VectorXf` 系数提供、乱序 indices 和中等规模 synthetic cloud。

`unvalidated_scope`：production dispatch、`getDistancesToModel`、空 indices、identity-index fast path、`PointXYZI` / RGB / normal 复合点型、自定义点型、非 RVV 构建 production fallback、真实 RANSAC 上游路径和 stick/parallel-line 派生语义。

`point_type_expansion_queue`：select candidate 正向后，先在 production plan 中冻结 PointXYZ-like traits gate，再补代表点型 correctness 和必要 board。

`phase_closeout_boundary`：本阶段最多关闭 `PointXYZ` direct indexed select diagnostic 矩阵条目；不能关闭 production adopted 或完整模板入口结论。
