# Phase 020: line getDistancesToModel diagnostic plan

## 阶段意图和边界

本阶段为 `SampleConsensusModelLine::getDistancesToModel` 建立 production-shaped diagnostic（生产形态诊断，测试专用代码尽量复用真实对象状态和入口数据流）。目标是证明 `PointXYZ + direct indexed indices_ + float xyz AoS` 边界下，测试专用 RVV candidate（候选实现）能和公开入口输出同一组 dense distance（连续距离数组），并在板卡 repeated bench（重复板卡性能测试）中给出可解释信号。

本阶段不修改 production（生产源码），不证明真实 public entry（公开入口）已有 RVV dispatch（RVV 分流），不扩大到泛型点类型、`Scalar=double`、其它 layout、其它 row source（行来源）或 production fallback（生产回退）矩阵。

## 当前状态清单

| area | current state | evidence |
| --- | --- | --- |
| production source | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_line.hpp` 未被本 topic 修改；`getDistancesToModel` 对 `indices_` 顺序逐点计算 `sqrt(cross3.squaredNorm())` 并写 `std::vector<double>`。 | production 源码 |
| Phase 000 | `countWithinDistance` diagnostic 为 `partial-production-candidate`，板卡 5-run median/min/max 为 `4.4569x / 4.4350x / 4.4920x`。 | `000-line-count-diagnostic/result.zh.md` |
| Phase 010 | `selectWithinDistance` diagnostic 为 `partial-production-candidate`，板卡 5-run median/min/max 为 `3.1010x / 3.0622x / 3.1434x`。 | `010-line-select-diagnostic/result.zh.md` |
| evidence registry | count/select repeated evidence 当前为 fresh。 | `make repeated_evidence_status && make repeated_select_evidence_status` |
| roadmap / matrix | `getDistances-sqrt-store` 已列为 `phase_deferred + unblocked`。 | `../optimization-roadmap.zh.md`, `../phases/optimization-matrix.zh.md` |

## 假设与候选族

| candidate family | hypothesis | risk / unknown |
| --- | --- | --- |
| `getDistances-sqrt-store` | 复用 count/select 的 indexed xyz gather（按索引离散加载 x/y/z）和 cross3 平方距离公式，RVV 负责逐点平方距离；dense 输出保留顺序写回。 | `sqrt`（平方根）和 `double` 写回可能主导成本；如果 RVV 只加速平方距离而后段标量转换过重，板卡收益可能弱或中性。 |

RVV 实现优先复用 `pcl::rvv_load::indexed_load3_f32m2`。本阶段允许先把 RVV 计算出的 float 平方距离暂存到 chunk buffer，再由标量 `std::sqrt` 写入 `double` 输出；如果编译环境提供稳定 RVV sqrt helper，可以作为后续候选，不在本阶段默认采用。

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `getDistances-sqrt-store` | direct indexed `indices_` | `PointXYZ`, float xyz AoS, `Eigen::VectorXf` coefficients, dense `std::vector<double>` output | test-only `getDistancesToModelCandidate` vs public `getDistancesToModel` | `run_test_compare`, `run_line_get_distances_tests` | `collect_repeated_board_get_distances_evidence` | planned 5-run board repeated | `getDistancesToModelCandidateRVV` | planned manifest / doctor / registry | planned |

## 实现和测试动作

| action | artifact / command | completion criterion |
| --- | --- | --- |
| RED test | `src/test_sac_model_line.cpp` 新增调用缺失 `getDistancesToModelCandidate` 的小样本和 bench-scale correctness case。 | `make run_line_get_distances_tests` 因候选入口缺失而失败。 |
| GREEN helper | `include/impl/sac_model_line_diagnostic.hpp` 新增 scalar fallback 和 RVV helper。 | QEMU RVV 侧 `run_line_get_distances_tests` 通过；公开入口和候选距离逐项在误差预算内一致。 |
| bench / manifest | `src/bench_sac_model_line.cpp`、`script/generate_line_board_evidence_manifest.py` 和 `Makefile` 增加 getDistances item、target 和 registry 入口。 | bench 输出可被 manifest wrapper 解析，doctor 可读取 getDistances comparison。 |
| asm | `make dump_bench_rvv` 后按符号扫描 `getDistancesToModelCandidateRVV`。 | 能归属 indexed load、FMA 或 multiply/add、store 指令；若 sqrt 没有向量化，文档写清 scalar sqrt 边界。 |
| board / doctor / registry | `SSH_AUTH_SOCK=<ssh-agent-socket> make collect_repeated_board_get_distances_evidence` 后运行 record/status。 | 5-run summary、manifest、doctor 和 registry fresh；Errors / Warnings / Suggestions 被解释。 |
| docs | 更新 phase result、roadmap、matrix、evaluation、role docs 和队列表。 | 每个文档只记录当前证据边界，不创建 production 长期主题文档。 |

## Evidence Doctor 和 registry 规则

本阶段 manifest 输出到 `doc/phases/020-line-get-distances-diagnostic/repeated-evidence-manifest.json`，doctor 输出到同目录 `repeated-evidence-doctor.md/json`。registry 继续使用 `log/evidence_registry.json`，run label 使用 `line-phase020-get-distances-repeated-board`。count、select、getDistances 的 record/status target 必须顺序执行，避免同一 registry 文件并发写入。

## 板卡复跑预算和决策桶

默认运行 5 次 repeated board。若 checksum、Evidence Doctor 或 B/A 方向触发异常，最多追加 1 次同边界确认复跑；没有用户授权时不扩大到 20-run。决策桶使用：

| bucket | condition |
| --- | --- |
| positive | 5-run `B/A` 全部大于 1，median 明显大于 1，doctor 无 Error。 |
| weak_positive | median 大于 1，但存在接近 1 的 run、明显长尾或 Warning。 |
| neutral | median 接近 1，不能支撑接入倾向。 |
| negative | median 小于 1 且不是单次污染。 |
| unstable | run 间跨越方向或追加预算后仍摇摆。 |

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | test helper；baseline 是 Std build 的 `getDistancesToModelCandidateScalar`，candidate 是 RVV build 的 `getDistancesToModelCandidateRVV`。 |
| 当前决策问题 | RVV-vs-scalar 和 implementation-shape。 |
| diagnostic 是否可外推到 production | 只能作为有界 production probe 的输入。它复用真实 `input_` / `indices_` 状态和公开入口公式，但 production 源码没有 dispatch / fallback 证据。 |
| comparison-boundary / baseline mismatch 风险 | 有。public companion timing 只作为伴随项；严格 B/A 只使用 diagnostic candidate 的 Std/RVV 同 wrapper 对比。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 允许条件是用户明确授权 PI1，且 PI1 写清 `getDistancesToModel` 与 count/select 的 production scope、fallback 和真实入口测试。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要。diagnostic positive 不能直接写成 clean adopted。 |

## Phase scope 与扩展队列

| scope | content |
| --- | --- |
| validated_scope | `getDistancesToModel`，direct indexed `indices_`，`PointXYZ`，float xyz AoS，`Eigen::VectorXf` model coefficients，dense `std::vector<double>` 输出，bench 规模 `65536`。 |
| unvalidated_scope | 泛型点类型、非标准 layout、`Scalar=double`、其它 row source、真实 production dispatch、fallback tests、生产 direct bench。 |
| point_type_expansion_queue | PI1 或独立点类型 phase 后再评估 `PointXYZI` / RGB / normal / custom PointXYZ-like traits。 |
| phase_closeout_boundary | 本阶段只能关闭 getDistances diagnostic 矩阵条目，不能关闭 production adoption 或泛型模板结论。 |

## 继续 / 停止条件

若 getDistances diagnostic positive 且 count/select 仍 positive，本 topic 的下一默认动作是请求用户授权 PI1 production integration plan。若结果 weak / neutral / negative / unstable，本阶段只降级 getDistances diagnostic boundary，不回滚 count/select 的 partial-production-candidate。只有 production 授权、工具 / 板卡阻塞、证据矛盾或 dirty isolation 风险会停止同轮 phase loop。

## 文档更新清单

本阶段结束后更新 `result.zh.md`、`README.zh.md`、`doc/sac_model_line-evaluation.zh.md`、`doc/testing-overview.zh.md`、`doc/correctness-tests.zh.md`、`doc/benchmark-and-evidence.zh.md`、`doc/optimization-evidence.zh.md`、`doc/test-support-code-map.zh.md`、`doc/optimization-roadmap.zh.md`、`doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md` 和 `doc-rvv/library-screening/sample_consensus/sample_consensus-function-evaluation-queue.zh.md`。没有 adopted production behavior 时不创建 `doc-rvv/sample_consensus/sac_model_line-RVV.zh.md`。
