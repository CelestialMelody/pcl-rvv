# Phase 000: line count diagnostic plan

## 阶段意图和边界

本阶段只为 `SampleConsensusModelLine<PointT>::countWithinDistance` 建立 production-shaped diagnostic（生产形态诊断，测试专用代码模拟真实公开入口的数据形态）。目标是证明 line 的点到直线 cross3/squaredNorm（叉乘三维展开和平方范数）核能在 direct indexed `indices_`、`PointXYZ`、`Scalar=float coefficients` 和 RVV 构建下与公开标量入口一致，并采集同边界 benchmark（性能测试）、QEMU correctness（QEMU 正确性）和反汇编证据。

本阶段不修改 production（生产源码），不新增 public API（公开接口），不声明 `selectWithinDistance`、`getDistancesToModel`、`PointXYZI` / RGB / normal 复合点型、空 indices、identity-index fast path（恒等索引快速路径）或 production dispatch（生产分流）已经覆盖。

## 当前状态清单

| area | 当前状态 |
| --- | --- |
| 源码 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_line.hpp` 三个主入口均为标量循环；`countWithinDistance` 每个 index 读取 xyz，归一化 line direction，再判断 `sqr_distance < threshold^2`。 |
| 筛选队列 | `doc-rvv/library-screening/sample_consensus/sample_consensus-function-evaluation-queue.zh.md` 将 `impl/sac_model_line.hpp` 列为顺序 5 新增候选，建议先做 direct count 对拍。 |
| 既有测试 | 上游 `test/sample_consensus/test_sample_consensus_line_models.cpp` 覆盖 RANSAC 和 sample validation，但没有 RVV direct same-chain（同构链路）对拍。 |
| topic 资产 | 本阶段创建 `test-rvv/sample_consensus/sac_model_line/`，采用 `src/`、topic-local docs 和 phase docs 布局。 |
| production 状态 | 未修改 production，`doc-rvv` production 长期主题文档不适用。 |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| `count-indexed-gather-f32m2` | 使用公共 xyz indexed load（索引离散加载）批量读取点字段，并用 RVV 展开 `(line_pt - point) x line_dir` 的三个分量，再用 mask popcount（掩码计数）得到 inlier 数。 | 标量路径通过 Eigen `cross3` 和 double threshold 比较；RVV 以 float 中间量对拍，阈值附近需要专门 case。 |
| `select-vcompress` | 若 count 证据正向，后续可为 select 尝试 `vcompress`（向量压缩）保序写回。 | 输出 `inliers` 和 `error_sqr_dists_` 语义更重，本阶段不关闭。 |
| `getDistances-sqrt-store` | 若 count/select 正向，后续可评估 sqrt + dense store。 | 每点 sqrt 和 double store 可能吞噬收益，参考 sphere 的 `getDistancesToModel` 负向历史，需独立证据。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| count-indexed-gather-f32m2 | direct indexed `indices_` | `PointXYZ`, float xyz AoS, model coeff float, threshold double | test-only candidate vs public `countWithinDistance` | `make run_test_compare`，case 覆盖乱序 indices、阈值边界和非单位方向系数 | `board_smoke` 或 repeated board compare，case label `public countWithinDistance` / `diagnostic candidate countWithinDistance` | 需要 5-run bounded rerun 后才能给性能结论 | `dump_bench_rvv` 中归属到 line candidate helper | `run_repeated_board_evidence_doctor` 或人工 doctor | planned | 实现 RED/GREEN 测试、bench、QEMU、asm、board |
| select-vcompress | direct indexed `indices_` | `PointXYZ` | select public/candidate | not_applicable in this phase | not_applicable | not_applicable | not_applicable | not_applicable | phase_deferred + unblocked | count 正向后进入 select phase |
| getDistances-sqrt-store | direct indexed `indices_` | `PointXYZ` | getDistances public/candidate | not_applicable in this phase | not_applicable | not_applicable | not_applicable | not_applicable | phase_deferred + unblocked | count/select 证据后单独评估 |

## 实现和测试动作

| action | 产物 | 命令 / 证据 | 完成判据 |
| --- | --- | --- | --- |
| RED test | `src/test_sac_model_line.cpp` | `make -C test-rvv/sample_consensus/sac_model_line run_test_compare` | 先因缺少 `countWithinDistanceCandidate` 编译失败，证明测试能卡住候选缺口。 |
| GREEN candidate | `src/test_sac_model_line.cpp` | 同上 | Std/RVV 两个构建均通过 direct same-chain count 对拍。 |
| bench scaffold | `src/bench_sac_model_line.cpp` | `make -C ... dump_bench_rvv`，板卡 `board_smoke` | bench 输出 public / diagnostic candidate 两行，QEMU 不作为性能结论。 |
| docs | README、evaluation、roadmap、matrix、result | Markdown 文档和 artifact tracking scan | 文档能让下一轮短 prompt 恢复。 |

## Evidence Doctor 和 registry 规则

本阶段如果生成 repeated board summary（重复板卡摘要），用 topic-local manifest 脚本生成 `doc/phases/000-line-count-diagnostic/repeated-evidence-manifest.json`，再运行 `test-rvv/script/evidence_doctor.py`。若本轮只完成 QEMU correctness / asm，Evidence Doctor 状态写 `not_run_no_board_summary`，不能给性能结论。

`log/evidence_registry.json` 当前不存在；创建 registry target 属于本阶段可选收敛项。若生成并引用 summary evidence，必须登记或在 result 中写明人工 freshness（新鲜度）检查路径。

## 阶段完成条件

- `count-indexed-gather-f32m2` 至少需要 correctness、QEMU RVV path、asm attribution（反汇编归属）和 board performance 才能进入 `partial-production-candidate`。
- 若 correctness 不通过，decision 为 `attempted / rejected`，不得进入 production。
- 若 correctness 通过但板卡收益弱、负向或不稳定，只能说明 diagnostic boundary 当前不支持生产取舍；是否允许 bounded production probe 要按 mismatch audit 回填。

## 板卡复跑预算和决策桶

板卡可用时使用 `SSH_AUTH_SOCK` 注入当前命令环境。默认 repeated budget 为 5 run，warmup 由 bench 程序内部固定，decision bucket 初始口径：

| bucket | 口径 |
| --- | --- |
| positive-stable | median speedup >= 1.20x 且 min speedup >= 1.05x |
| weak-positive | 1.05x <= median speedup < 1.20x |
| neutral | 0.95x <= median speedup < 1.05x |
| negative | median speedup < 0.95x |
| unstable | run 间跨 bucket 或 Evidence Doctor 发现未处理 Error |

## 继续 / 停止条件

默认继续到 correctness、bench build、asm 和有界 board 验证。只有 production 修改需要用户检查、板卡/工具链不可用、Evidence Doctor Error 未解决、dirty isolation 不安全或当前矩阵没有未阻塞动作时，才停止。

## 文档更新清单

- `README.zh.md`：topic 导航、常用命令和证据提交边界。
- `doc/sac_model_line-evaluation.zh.md`：函数级评估、Traceability Map 和 EvidenceDecision 主归属。
- `doc/optimization-roadmap.zh.md`：后续 count/select/getDistances 和点类型扩展候选。
- `doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md`、本 phase `result.zh.md`。

## roadmap 同步动作

本阶段会把 `count-indexed-gather-f32m2` 从 planned 推进到 attempted / partial-production-candidate / rejected，并保留 `select-vcompress`、`getDistances-sqrt-store`、`point-type-expansion` 三条后续路线。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic（生产形态诊断），代码只在 `test-rvv` 中。 |
| A/B boundary | test helper：公开标量入口 vs 测试专用 RVV candidate。 |
| 当前决策问题 | RVV-vs-scalar 是否值得进入后续 production probe。 |
| diagnostic 是否可外推到 production | unknown。它复用公开入口输入形态，但没有 production dispatch、fallback 和 protected helper 边界。 |
| comparison-boundary / baseline mismatch 风险 | yes。candidate 在测试派生类中实现，编译边界和内联形态可能不同于 production。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 只有 correctness、asm 和 board 异常解释闭合，且 production patch 能保持小范围 fallback 时才允许；否则不进入 PI1。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前没有已采用 line RVV family；若后续出现 identity fast path 或 vcompress 家族选择，需要同一 production boundary A/B。 |

## Phase Scope 与扩展队列

`validated_scope`：本阶段计划只覆盖 `countWithinDistance`、direct indexed row source、`PointXYZ`、float xyz AoS、`Scalar` 由 `Eigen::VectorXf` 系数提供、乱序 indices 和中等规模 synthetic cloud。

`unvalidated_scope`：`selectWithinDistance`、`getDistancesToModel`、空 indices、默认整云 identity indices、`PointXYZI` / RGB / normal 复合点型、自定义点型、非 RVV 构建 production fallback、真实 RANSAC 上游路径和 stick/parallel-line 派生语义。

`point_type_expansion_queue`：count candidate 正向后，先补 `PointXYZI` / `PointXYZRGB` / `PointXYZRGBA` / `PointXYZINormal` correctness，再决定是否做 dedicated board performance。

`phase_closeout_boundary`：本阶段最多关闭 `PointXYZ` direct indexed count diagnostic 矩阵条目；不能关闭 production adopted 或完整模板入口结论。
