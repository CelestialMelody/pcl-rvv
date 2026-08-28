# Phase 020: stick select diagnostic plan

## 阶段意图和边界

本阶段只为 `SampleConsensusModelStick<PointT>::selectWithinDistance` 建立 production-shaped diagnostic（生产形态诊断，测试专用代码模拟真实公开入口的数据形态）。目标是验证 `PointXYZ`、direct indexed row source（直接索引行来源）、float xyz AoS（结构数组）布局下，RVV（RISC-V Vector，可变长向量扩展）候选能复刻公开入口的 inlier（内点）保序输出和 `error_sqr_dists_` 平方距离记录语义，并采集 QEMU correctness（QEMU 正确性）、反汇编、板卡 bench（性能测试）和 Evidence Doctor（证据体检）证据。

本阶段不修改 production（生产源码），不新增 public API（公开接口），不证明 `countWithinDistance` 的 production patch 已授权，也不覆盖 `getDistancesToModel`、默认整云 identity indices（恒等索引）、泛型点类型或真实 production dispatch（生产分流）。

## 当前源码语义

`selectWithinDistance` 先验证 model coefficients（模型系数），把系数 0-2 当第一个端点、3-5 当第二个端点，归一化 `line_pt2 - line_pt1`。入口清空并预留 `inliers` 与 protected（受保护成员）`error_sqr_dists_`，然后按 `indices_` 顺序扫描点云。每个点计算 `dir.cross3(line_dir).squaredNorm()`；若平方距离 `< threshold^2`，追加原始点云 index 到 `inliers`，并把同一个平方距离以 `double` 追加到 `error_sqr_dists_`。

本阶段必须保持：

| 语义点 | 验收要求 |
| --- | --- |
| 输出顺序 | `inliers` 顺序必须等于标量路径按 `indices_` 扫描时的命中顺序。 |
| 输出值 | 输出的是 `(*indices_)[i]` 原始点云 index，不是子集内位置 `i`。 |
| 距离记录 | `error_sqr_dists_` 个数和顺序与 `inliers` 一一对应；阈值附近使用公开入口同一 `< threshold^2` 规则。 |
| stick 入口差异 | `selectWithinDistance` 没有 `nr_i/nr_o` 外圈惩罚，不能复用 count 的返回策略。 |

## 候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| `select-indexed-gather-f32m2-compress` | 复用 Phase 000 的 indexed xyz load（索引离散加载）和平方距离公式，用 mask（掩码）判断 inlier，再通过 `vcompress`（向量压缩）保序暂存命中 index 和平方距离，最后缩短输出容器。 | 输入来自 `indices_`，输出必须 gather 原始 index 后 compress；若错误使用 `vid + i` 会输出子集位置。`error_sqr_dists_` 是 `double` 容器，若 RVV 只写 float staging，需要逐项转成 double 并在 correctness 中与公开入口对拍。 |
| `select-mask-scalar-append` | 先只用 RVV 计算 mask 和平方距离，再按 lane 标量 append 输出，降低初版实现风险。 | 标量 append 可能吞噬收益；若 bench 负向，只能说明该 staging family 不支持接 production，不能否定其它 `vcompress` 形态。 |
| `select-production-probe` | 若 diagnostic candidate 正向，后续可规划有界 production probe。 | production 源码修改需要用户明确授权；Phase 020 不能自行进入 PI2。 |

本阶段默认实现第一候选；如果编译器或 intrinsic 支持阻塞，再降级到第二候选并在 result 中写明 evidence boundary（证据边界）变窄。

## 优化矩阵增量

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| select-indexed-gather-f32m2-compress | direct indexed `indices_` | `PointXYZ`, float xyz AoS, `Eigen::VectorXf`, `double threshold` cast to float threshold squared | test-only candidate vs public `selectWithinDistance` | `make run_test_compare`；case 覆盖乱序 indices、阈值边界、全部不命中和 `error_sqr_dists_` 顺序 | board repeated compare，case label `public selectWithinDistance` / `diagnostic candidate selectWithinDistance` | 5-run bounded budget 后才给性能结论 | `selectWithinDistanceCandidateRVV` 中应有距离公式、mask、compress 或等价 staging 指令 | `run_repeated_board_evidence_doctor` | planned | 先执行 RED/GREEN，再跑 QEMU、asm 和板卡 |

## 实现和测试动作

| action | 产物 | 命令 / 证据 | 完成判据 |
| --- | --- | --- | --- |
| RED test | `src/test_sac_model_stick.cpp` | `make -C test-rvv/sample_consensus/sac_model_stick run_test_compare` | 先因缺少 `selectWithinDistanceCandidate` 或 accessor 编译失败，证明测试能卡住 select 入口缺口。 |
| GREEN candidate | `include/impl/sac_model_stick_diagnostic.hpp` | 同上 | Std/RVV 两个构建均通过 public/candidate select 对拍。 |
| bench 扩展 | `src/bench_sac_model_stick.cpp` | `make -C ... dump_bench_rvv`，板卡 repeated | bench 输出 count 和 select 的 public / diagnostic candidate 行，QEMU 不作为性能结论。 |
| manifest / doctor | `script/generate_stick_board_evidence_manifest.py`、Phase 020 summary evidence | `make -C ... record_repeated_board_evidence_state && make -C ... repeated_evidence_status` | 新 run label、doctor 和 registry 指向 Phase 020 文档，且 freshness（新鲜度）检查通过。 |
| docs | README、evaluation、roadmap、matrix、phase index、queue | Markdown 文档和 `git diff --check` | 文档区分 count PI 授权边界和 select diagnostic 结果。 |

## 板卡复跑预算和决策桶

板卡可用时使用 `SSH_AUTH_SOCK=/run/user/$(id -u)/keyring/ssh` 注入命令环境。默认 repeated budget（有界复跑预算）为 5 run，warmup 由 bench 程序内部固定，decision bucket（决策桶）沿用 Phase 000：

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
| evidence role | `production-shaped diagnostic`；代码只在 `test-rvv` 派生类中。 |
| A/B boundary | test helper：公开标量入口 vs 测试专用 RVV candidate。 |
| 计时边界 | 只包含入口调用、输出容器 clear/reserve/write 和 `error_sqr_dists_` 写回；不包含点云、indices 和系数构造。 |
| row source | direct indexed `indices_`；本阶段不覆盖 identity full cloud 或其它 row source。 |
| 当前决策问题 | select RVV staging 是否值得后续规划 bounded production probe。 |
| diagnostic 是否可外推到 production | unknown。它复用真实对象状态和公开入口语义，但没有 production helper、dispatch 和 fallback 边界。 |
| comparison-boundary / baseline mismatch 风险 | yes。candidate 在测试派生类中实现，编译和内联边界可能不同于 production。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 只有 correctness、asm、board 异常解释闭合，且能提出更接近 production 的 bounded probe 时才允许；否则保留为 attempted diagnostic。 |
| clean adoption 是否需要 production direct | yes。必须有 PI2-PI5 和用户确认，Phase 020 不能直接产生 adopted production behavior。 |

## Phase Scope 与扩展队列

`validated_scope`：计划只覆盖 `selectWithinDistance`、direct indexed `indices_`、`PointXYZ`、float xyz AoS、`Eigen::VectorXf` 系数、`double threshold` 转 float 阈值平方、synthetic stick-distance cloud 和保序 inlier / error 输出。

`unvalidated_scope`：`getDistancesToModel`、production `countWithinDistance` patch、空 / 默认整云 identity indices、`PointXYZI` / RGB / RGBA / normal 复合点型、自定义点型、`Scalar=double`、真实 RANSAC 上游路径和 production fallback。

`point_type_expansion_queue`：若 select diagnostic 正向，下一阶段先保留 `PointXYZ` production probe 计划；泛型点类型扩展必须另建 phase，并补 traits / offset / fallback correctness、反汇编、板卡和 Evidence Doctor。

`phase_closeout_boundary`：本阶段最多关闭 `PointXYZ` direct indexed select diagnostic 矩阵条目；不能关闭 production adopted 或完整模板入口结论。

## 继续 / 停止条件

默认继续到 correctness、bench build、asm、5-run board repeated、Evidence Doctor 和 registry。只有 production 修改、板卡 / 工具链不可用、Evidence Doctor Error 未解决、dirty isolation 不安全或当前矩阵没有未阻塞动作时，才停止。若 Phase 020 得到正向诊断信号，也只能写 PI1 production plan 或 handoff，不能自行进入 PI2 production patch。
