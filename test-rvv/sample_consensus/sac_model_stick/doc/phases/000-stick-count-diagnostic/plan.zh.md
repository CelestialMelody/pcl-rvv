# Phase 000: stick count diagnostic plan

## 阶段意图和边界

本阶段只为 `SampleConsensusModelStick<PointT>::countWithinDistance` 建立 production-shaped diagnostic（生产形态诊断，测试专用代码模拟真实公开入口的数据形态）。目标是证明 stick 的点到线段方向 cross3/squaredNorm（叉乘三维展开和平方范数）核可以在 direct indexed row source（直接索引行来源）、`PointXYZ`、float xyz AoS（结构数组）布局和 RVV 构建下与公开标量入口一致，并采集 QEMU correctness（QEMU 正确性）、反汇编和板卡 bench（性能测试）证据。

本阶段不修改 production（生产源码），不新增 public API（公开接口），不声明 `selectWithinDistance`、`getDistancesToModel`、泛型点类型、默认整云 identity indices（恒等索引）、production dispatch（生产分流）或 stick 的其它不一致系数语义已经覆盖。

## 当前状态清单

| area | 当前状态 |
| --- | --- |
| 源码 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_stick.hpp` 三个距离入口均为标量循环。`countWithinDistance` 把系数 0-2 当作端点 1、3-5 当作端点 2，归一化 `line_pt2 - line_pt1` 后按 `sqr_distance < threshold^2` 计入 `nr_i`，按 `threshold^2 <= sqr_distance < 4 * threshold^2` 计入 `nr_o`，最终返回 `nr_i <= nr_o ? 0 : nr_i - nr_o`。 |
| 筛选队列 | `doc-rvv/library-screening/sample_consensus/sample_consensus-function-evaluation-queue.zh.md` 将 `impl/sac_model_stick.hpp` 列为顺序 6 新增候选，要求覆盖 `nr_i/nr_o` 边界。 |
| 既有测试 | 当前没有 `test-rvv/sample_consensus/sac_model_stick/`。line topic 的测试工程形态可作为结构参照，但 stick 的双计数和系数语义必须独立对拍。 |
| topic 资产 | 本阶段创建 `test-rvv/sample_consensus/sac_model_stick/`，使用 `src/`、`include/impl/`、topic-local docs 和 phase docs 布局。 |
| production 状态 | 未修改 production；`artifact_layout.topic_doc_template` 解析出的 `doc-rvv` 长期生产文档当前不适用。 |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| `count-indexed-gather-f32m2-dual-count` | 使用公共 xyz indexed load（索引离散加载）批量读取点字段，RVV 展开 `(point - line_pt1) x normalized(line_pt2 - line_pt1)`，分别 popcount 内圈和外圈 mask，再复刻 `max(nr_i - nr_o, 0)`。 | stick 不是普通 inlier count；若只复用 line 的单阈值计数会错误高估。阈值附近和 `4 * threshold^2` 外圈边界必须有专门 case。 |
| `select-vcompress` | count 证据正向后，后续可为 `selectWithinDistance` 尝试 `vcompress`（向量压缩）保序写回。 | 输出 `inliers` 和 `error_sqr_dists_` 语义更重，本阶段不关闭。 |
| `getDistances-sqrt-penalty` | 后续可评估 `getDistancesToModel` 的 sqrt + outlier penalty。 | 源码里该入口把系数 3-5 当方向，而 count/select 把 3-5 当第二端点；必须单独审计，不能复用本阶段结论。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| count-indexed-gather-f32m2-dual-count | direct indexed `indices_` | `PointXYZ`, float xyz AoS, `Eigen::VectorXf` model coefficients, `double threshold` cast to float threshold squared | test-only candidate vs public `countWithinDistance` | `make run_test_compare`；case 覆盖乱序 indices、内圈 / 外圈 / 远外圈和返回 0 的 `nr_i <= nr_o` 边界 | `board_smoke` 或 repeated board compare，case label `public countWithinDistance` / `diagnostic candidate countWithinDistance` | 需要 5-run bounded rerun 后才能给性能结论 | `dump_bench_rvv` 中归属到 stick candidate helper | `run_repeated_board_evidence_doctor` 或人工 doctor | planned | 先执行 RED/GREEN，再跑 QEMU、asm 和板卡 |
| select-vcompress | direct indexed `indices_` | `PointXYZ` | select public/candidate | not_applicable in this phase | not_applicable | not_applicable | not_applicable | not_applicable | phase_deferred + unblocked | count 正向后单独建 select phase |
| getDistances-sqrt-penalty | direct indexed `indices_` | `PointXYZ` | getDistances public/candidate | not_applicable in this phase | not_applicable | not_applicable | not_applicable | not_applicable | phase_deferred + unblocked | 先审计系数语义，再决定是否做 sqrt candidate |

## 实现和测试动作

| action | 产物 | 命令 / 证据 | 完成判据 |
| --- | --- | --- | --- |
| RED test | `src/test_sac_model_stick.cpp` | `make -C test-rvv/sample_consensus/sac_model_stick run_test_compare` | 先因缺少 `countWithinDistanceCandidate` 或 diagnostic helper 编译失败，证明测试能卡住候选缺口。 |
| GREEN candidate | `include/impl/sac_model_stick_diagnostic.hpp` | 同上 | Std/RVV 两个构建均通过 direct same-chain（同构链路）count 对拍。 |
| bench scaffold | `src/bench_sac_model_stick.cpp` | `make -C ... dump_bench_rvv`，板卡 `board_smoke` | bench 输出 public / diagnostic candidate 两行，QEMU 不作为性能结论。 |
| docs | README、evaluation、roadmap、matrix、result | Markdown 文档和 artifact tracking scan | 文档能让下一轮短 prompt 恢复。 |

## Evidence Doctor 和 registry 规则

本阶段如果生成 repeated board（重复板卡）summary，先用 topic-local manifest 脚本生成 `doc/phases/000-stick-count-diagnostic/repeated-evidence-manifest.json`，再运行 `test-rvv/script/evidence_doctor.py`。若本轮只完成 QEMU correctness / asm，Evidence Doctor（证据体检）状态写 `not_run_no_board_summary`，不能给性能结论。

`log/evidence_registry.json` 当前不存在；创建 registry target 属于本阶段可选收敛项。若生成并引用 summary evidence（摘要证据），必须登记或在 result 中写明人工 freshness（新鲜度）检查路径。

## 阶段完成条件

- `count-indexed-gather-f32m2-dual-count` 至少需要 correctness、QEMU RVV path、asm attribution（反汇编归属）和 board performance（板卡性能）才可能进入 `partial-production-candidate`。
- 若 correctness 不通过，decision 为 `attempted / rejected`，不得进入 production。
- 若 correctness 通过但板卡收益弱、负向或不稳定，只能说明 diagnostic boundary（诊断边界）当前不支持生产取舍；是否允许 bounded production probe（有界生产探针）要按 mismatch audit 回填。

## 板卡复跑预算和决策桶

板卡可用时使用 `SSH_AUTH_SOCK` 注入当前命令环境。默认 repeated budget（有界复跑预算）为 5 run，warmup 由 bench 程序内部固定，decision bucket（决策桶）初始口径：

| bucket | 口径 |
| --- | --- |
| positive-stable | median speedup >= 1.20x 且 min speedup >= 1.05x |
| weak-positive | 1.05x <= median speedup < 1.20x |
| neutral | 0.95x <= median speedup < 1.05x |
| negative | median speedup < 0.95x |
| unstable | run 间跨 bucket 或 Evidence Doctor 发现未处理 Error |

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic，代码只在 `test-rvv` 中。 |
| A/B boundary | test helper：公开标量入口 vs 测试专用 RVV candidate。 |
| 当前决策问题 | RVV-vs-scalar 是否值得进入后续 production probe。 |
| diagnostic 是否可外推到 production | unknown。它复用公开入口输入形态，但没有 production dispatch、fallback 和 protected helper 边界。 |
| comparison-boundary / baseline mismatch 风险 | yes。candidate 在测试派生类中实现，编译边界和内联形态可能不同于 production。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 只有 correctness、asm、board 异常解释闭合，且 production patch 能保持小范围 fallback 时才允许；否则不进入 PI1。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前没有已采用 stick RVV family；若后续出现 identity fast path、select vcompress 或其它 family selection，需要同一 production boundary A/B。 |

## Phase Scope 与扩展队列

`validated_scope`：本阶段计划只覆盖 `countWithinDistance`、direct indexed row source、`PointXYZ`、float xyz AoS、`Eigen::VectorXf` 系数、乱序 indices 和 synthetic stick-distance cloud。

`unvalidated_scope`：`selectWithinDistance`、`getDistancesToModel`、空 indices、默认整云 identity indices、`PointXYZI` / RGB / normal 复合点型、自定义点型、非 RVV 构建 production fallback、真实 RANSAC 上游路径、以及 `getDistancesToModel` 的不一致系数解释。

`point_type_expansion_queue`：count candidate 正向后，先补 `PointXYZI` / `PointXYZRGB` / `PointXYZRGBA` / `PointXYZINormal` correctness，再决定是否做 dedicated board performance。

`phase_closeout_boundary`：本阶段最多关闭 `PointXYZ` direct indexed count diagnostic 矩阵条目；不能关闭 production adopted 或完整模板入口结论。

## 继续 / 停止条件

默认继续到 correctness、bench build、asm 和有界 board 验证。只有 production 修改需要用户检查、板卡 / 工具链不可用、Evidence Doctor Error 未解决、dirty isolation 不安全或当前矩阵没有未阻塞动作时，才停止。
