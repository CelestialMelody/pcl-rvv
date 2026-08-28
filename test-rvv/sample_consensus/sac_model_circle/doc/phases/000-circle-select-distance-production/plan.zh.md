# Phase 000: circle2d select/count 生产候选计划

## 阶段意图和边界

本阶段针对 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle.hpp` 的
`SampleConsensusModelCircle2D<PointT>` 建立独立 topic。当前 production（生产源码）已经有
`countWithinDistanceRVV`，但 `selectWithinDistance` 仍直接执行标量循环；`getDistancesToModel`
需要每个点写回精确 `sqrt` 距离，先不接入 production。

阶段目标是把 `selectWithinDistance` 的 shell mask（壳层掩码，平方距离落在内外半径之间）候选做成
真实 public entry（公开入口）生产分流：RVV（RISC-V Vector，可变长向量）负责 x/y gather（按索引离散加载）
和平方距离 mask，命中的 inlier 顺序和 `error_sqr_dists_` 精确距离写回保持标量语义。阶段范围只覆盖
direct indexed `indices_`、registered single-float x/y 点型、`Eigen::VectorXf` float 系数、
`double threshold` 转 float 后的双边界比较和 32-bit byte offset（32 位字节偏移）可表达的点云规模。

本阶段不覆盖 `getDistancesToModel` 生产接入、circle3d、`Scalar=double`、非 registered float x/y layout、
自定义非 AoS 布局或其它 SAC 后处理。

## S0 偏好冻结

| 字段 | 本轮状态 |
| --- | --- |
| preferences_loaded | defaults: loaded；local_override: absent；prompt_override: 用户要求作为 RVV worker 开启并持续推进 circle topic，板卡可用且需注入 `SSH_AUTH_SOCK`。 |
| work_preferences | 测试资产和 diagnostic（诊断代码）使用详细中文注释；production 注释克制，只解释 fallback（回退路径）、dispatch（分流逻辑）、数值和布局边界；文档 current-state-first（当前状态优先）；evidence logs（证据日志）默认 summary-only。 |
| commit_preferences | 默认不创建 commit；topic 产物、summary evidence、raw logs 和 agent instruction patch（agent 指令改动）拆分审查。 |
| dirty_isolation | 工作区已有非本 topic 的 `.agents`、sphere、segmentation 等改动；本阶段只触碰 circle production 文件、`test-rvv/sample_consensus/sac_model_circle/**` 和 sample_consensus 队列表。 |

## 当前状态清单

| area | 当前状态 | 证据 |
| --- | --- | --- |
| production count | `countWithinDistance` 在 `__RVV10__` 且 x/y float layout gate 成立时调用 `countWithinDistanceRVV`。 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle.hpp` |
| production select | `selectWithinDistance` 只有标量循环，尚无 `selectWithinDistanceStandard` / `selectWithinDistanceRVV` 分层。 | 同上 |
| production getDistances | 稠密写回 `std::vector<double>`，每点需要 `sqrt`，本阶段暂不接入。 | 同上 |
| 历史测试 | `quadric_models` 有 Circle2D count smoke / perf，但没有独立 phase、roadmap、matrix，也没有 select production direct。 | `test-rvv/sample_consensus/quadric_models/test_sample_consensus_quadric_models.cpp` |
| 独立 topic scaffold | 本阶段新建 `sac_model_circle` topic-local Makefile、board.mk、测试源码和 phase 文档。 | `test-rvv/sample_consensus/sac_model_circle/**` |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| select RVV gather + scalar exact error writeback | circle2d 只读 x/y，比 sphere 少一个 z gather；select 用 RVV 快速判定 shell，命中后按顺序写回 index 和精确 `sqrt` 误差。 | 如果 inlier 命中率高，标量 `sqrt` tail（尾段）仍可能限制收益；需要板卡 bench 才能判断。 |
| count RVV regression | 既有 `countWithinDistanceRVV` 应作为回归保留。 | 旧 `quadric_models` 允许小 count 差异；独立 topic 应先用确定性边界 case 收紧 correctness。 |
| getDistances RVV candidate | 可复用平方距离 RVV，但每点必须 `sqrt` 并写 `double`。 | sphere 同类路径已有负向证据；本阶段先记为 deferred（暂缓），不改 production。 |

## Phase Scope 与扩展队列

| question | answer |
| --- | --- |
| validated_scope | `SampleConsensusModelCircle2D` 的 `selectWithinDistance` / `countWithinDistance`，`PointXYZ` correctness 和后续板卡性能；`PointXYZI` 只做 correctness 扩展。 |
| unvalidated_scope | `getDistancesToModel`、更多 PointXYZ-like 点型、非 float x/y layout、自定义点型、超大 cloud 32-bit byte offset gate、circle3d、`Scalar=double`。 |
| point_type_expansion_queue | 先用 `PointXYZI` 证明 traits gate 不只 exact `PointXYZ`；`PointXYZRGB/RGBA/INormal` 可在后续 phase 补 correctness 和必要 board。 |
| phase_closeout_boundary | 只能关闭本阶段 explicit 测试覆盖的 select/count 组合，不能关闭 getDistances 或整个 circle 模板族。 |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| RED 测试 | `src/test_sac_model_circle.cpp` 调用 `selectWithinDistanceStandard` / `selectWithinDistanceRVV` | RVV 构建先因缺少 helper 声明失败，证明测试能抓住当前生产缺口。 |
| PI2 production patch | `sac_model_circle.h` 和 `impl/sac_model_circle.hpp` 新增 select Std/RVV helper，并让 public entry 在 RVV gate 成立时短路。 | Std 构建和 RVV 构建 correctness 均通过；非 RVV 自然走 Standard。 |
| QEMU correctness | `make -C test-rvv/sample_consensus/sac_model_circle run_test_compare` | Std/RVV 日志均通过；QEMU 只作为正确性和日志形状证据。 |
| asm attribution | `make -C test-rvv/sample_consensus/sac_model_circle dump_bench_rvv` 或测试二进制反汇编替代 | `selectWithinDistanceRVV` 或内联范围出现 RVV load / mask / compress 或 store 指令；若没有 bench 源，记录为 partial。 |
| board smoke / bench | `SSH_AUTH_SOCK=<agent-socket> make -C ... board_smoke` | 板卡可达时执行；若当前没有 bench 源，本阶段先只跑 board correctness，bench 作为下一 phase。 |

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | 本阶段目标是 production direct（真实生产路径证据）；RED 测试和 QEMU 先建立 correctness gate。 |
| A/B boundary | public overload（公开重载）和 protected helper；板卡 bench 后才比较 public Std/RVV。 |
| 当前决策问题 | RVV-vs-scalar 和 implementation-shape（实现形态）。 |
| diagnostic 是否可外推到 production | 旧 `quadric_models` count 只能作为历史诊断，不外推到 select production。 |
| comparison-boundary / baseline mismatch 风险 | 有。旧测试混合多个 quadric 模型且只覆盖 count；本 topic 用独立 public entry 测试重建边界。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 允许：范围限 select/count，且 production patch 保留到 PI5 用户检查点；若板卡性能不成立，不自动回滚。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要比较新旧 RVV family，因为 select 当前没有 adopted RVV family；若后续尝试 `vcompress` 变体，则需要 RVV-vs-RVV detail A/B。 |

## 板卡复跑预算和决策桶

板卡可用性由当前用户说明和 `SSH_AUTH_SOCK` 注入确认。性能阶段默认 5-run repeated board（重复板卡测试），
decision bucket（决策桶）使用：`positive >= 1.20x`、`weak-positive 1.05x..1.20x`、`neutral 0.95x..1.05x`、
`negative < 0.95x`、跨桶摇摆为 `unstable`。本阶段如果尚未有 bench 源，只关闭 correctness 和 scaffold，
把 board performance 留给下一 phase，而不是写成 production adopted。

## Continue / Stop

本阶段完成后默认继续到 `010-circle-bench-and-board-evidence`：补 bench 源、板卡 repeated summary、
Evidence Doctor（证据体检）和 registry（证据登记）。只有 production direct correctness 或生产补丁不可编译、
板卡不可达、dirty isolation 不安全、或继续需要扩大到 `getDistancesToModel` / 其它 topic 时，才允许停止。
