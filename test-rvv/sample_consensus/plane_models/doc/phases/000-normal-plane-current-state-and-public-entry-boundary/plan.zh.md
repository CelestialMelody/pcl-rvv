# Normal-plane 当前状态与公开入口边界 Phase Plan

## 阶段意图和边界

本阶段针对 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_plane.hpp` 已有 RVV 代码做收敛，不重新筛选 sample_consensus 模块，也不扩大到 plane/sphere/circle 其它 topic。当前测试资产仍位于历史目录 `test-rvv/sample_consensus/plane_models`；本阶段先把 normal-plane 的公开入口边界、fallback（回退路径）和证据缺口写清，再用最小测试补齐生产形态证据。

本阶段要证明：

- `SampleConsensusModelNormalPlane<PointXYZ, Normal>` 的公开入口 `selectWithinDistance`、`countWithinDistance`、`getDistancesToModel` 在 `__RVV10__` 下能走已有 RVV helper，并与标准 helper 保持当前容差内一致。
- 不满足 RVV layout gate（布局验收条件）的点类型或法线类型仍能通过公开入口回退到标准路径。
- 当前 board（板卡）性能证据只覆盖 `PointXYZ + Normal`、连续 indices、`Scalar` 接口为 `double threshold` 但内部 RVV `float` 算术，不外推到泛型点类型全集。

本阶段不证明：

- `PointXYZINormal`、`PointNormal` 或任意 PointXYZ-like / Normal-like 组合的完整 production 结论。
- indices 非法值、大于 32-bit byte offset（字节偏移）的极端点云或其它 row source policy（行来源策略）。
- 新 RVV family（实现族）优于已有 RVV helper；本阶段只收敛已有 helper 的公开入口证据。

## 当前状态清单

| 对象 | 当前事实 | 路径 |
| --- | --- | --- |
| production 源码 | 已有 `selectWithinDistanceRVV`、`countWithinDistanceRVV`、`getDistancesToModelRVV`，公开入口用 `RVVXYZFloatLayout<PointT>`、`RVVNormalFloatLayout<PointNT>` 和 curvature field gate 分流。 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_plane.hpp` |
| production 声明 | 三个 RVV protected helper 只在 `__RVV10__` 下声明。 | `sample_consensus/include/pcl/sample_consensus/sac_model_normal_plane.h` |
| correctness 测试 | 已有 protected helper 对拍和上游 RANSAC 回归；公开入口是否命中 RVV / fallback 没有单独验收。 | `test-rvv/sample_consensus/plane_models/test_sample_consensus_plane_models.cpp` |
| bench | 已有 protected helper bench，输出 Std/RVV 独立日志供 compare 脚本汇总。 | `test-rvv/sample_consensus/plane_models/bench_sac_normal_plane.cpp` |
| board 证据 | 历史 board compare 显示三入口加速，但没有 Evidence Doctor 结构化 manifest。 | `test-rvv/sample_consensus/plane_models/output/board/analyze_bench_compare.log` |
| 长期文档 | `doc-rvv` 已记录已有实现，但片段仍保留旧 offset 写法，需后续 freshness check。 | `doc-rvv/sample_consensus/selectWithinDistance_getDistancesToModel_RVV.zh.md` |
| 队列 | sample_consensus 函数评估队列把 normal-plane 列为第一条“已有实现，需收敛”。 | `doc-rvv/library-screening/sample_consensus/sample_consensus-function-evaluation-queue.zh.md` |

## 假设与候选族

| candidate family | 假设 | 本阶段动作 |
| --- | --- | --- |
| existing `f32m2` helper | 现有 RVV helper 已覆盖主逐点热点，问题主要是公开入口证据和 fallback 证据不足。 | 补 public-entry-shaped correctness。 |
| stronger AoS layout gate | 生产分流只用 field semantics gate（字段语义门禁）可能弱于实际 gather helper 的 AoS byte-offset 假设。 | 先审计现有 trait；若测试暴露编译或 fallback 缺口，再改 production gate。 |
| Evidence Doctor manifest | 当前 board summary 能人工阅读，但 Evidence Doctor 输入不完整。 | 若本阶段跑 board compare，则生成最小 manifest 并运行 doctor；若只跑 QEMU correctness，记录缺口。 |
| test support split | `plane_models` 混合 plane 和 normal-plane 测试，根目录长文件不符合当前 `src/` + `include/impl` 质量门槛。 | 本阶段记录为 `phase_deferred + unblocked`，下一 phase 默认处理结构拆分，除非本阶段必须先修 production。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| existing `f32m2` normal-plane RVV | ordered indices over `indices_` | `PointXYZ + Normal`, threshold double cast to float, AoS gather | public `select/count/getDistances` + protected helpers | `run_test_rvv` plus new public-entry tests | `run_board_bench_compare` if tests pass | planned, board available | `dump_bench_rvv` | planned if board summary refreshed | planned | 补公开入口和 fallback 测试 |
| scalar fallback under RVV build | ordered indices over `indices_` | unsupported normal curvature layout or unsupported point layout | public entries only | compile/run fallback test in RVV build | not applicable | not applicable | not applicable | not_applicable | planned | 用 unsupported layout case 验证 |
| test support structure | not applicable | current mixed `plane_models` layout | test/doc assets | doc suite and artifact tracking | not applicable | not applicable | not applicable | not_applicable | phase_deferred | 下一 phase 做 structure split / doc suite |

## 实现和测试动作

| action | 产物 | 命令 / 证据 | 完成判据 |
| --- | --- | --- | --- |
| A1 current-state docs | phase plan、phase README、roadmap、matrix、evaluation 初稿 | `git status --short --untracked-files=all -- <topic paths>` | phase plan 先于代码 / 测试修改存在 |
| A2 public-entry RED test | `test_sample_consensus_plane_models.cpp` | `make -C test-rvv/sample_consensus/plane_models run_test_rvv` | 新测试先在当前实现或当前测试结构下暴露缺口；若测试立即通过，改为记录为 coverage gap，不改 production |
| A3 production or test fix | production hpp 或测试资产 | 同一 `run_test_rvv` | 公开入口 / fallback 证据闭合 |
| A4 QEMU std/rvv compare | 输出日志 | `make -C test-rvv/sample_consensus/plane_models run_test_compare` | Std/RVV correctness 通过；QEMU 不作性能结论 |
| A5 asm attribution | asm dump | `make -C test-rvv/sample_consensus/plane_models dump_bench_rvv` | 能在 RVV bench binary 中定位 normal-plane RVV 指令 |
| A6 board repeated budget | board logs / summary | `make -C test-rvv/sample_consensus/plane_models run_board_bench_compare fetch_board_logs`，最多 3 轮，若 bucket 稳定即停 | 板卡性能 bucket 为 positive / weak / neutral / negative / unstable |
| A7 result and handoff | phase result、roadmap、matrix、evaluation、Handoff | 文档路径和 git status | 每个 action 回填 `done / partial / deferred / blocked` |

## Evidence Doctor 和 registry 规则

当前 topic 没有 `log/evidence_registry.json`。本阶段如刷新 board summary，必须先检查 `test-rvv/script/evidence_doctor.py` 是否能消费现有 compare summary；若不能，写人工 Evidence Doctor 摘要，至少列 Errors / Warnings / Suggestions，并把 registry 缺口列为后续结构 phase。raw logs 默认不提交。

## 板卡复跑预算和决策桶

板卡在当前 prompt 中确认可用。本阶段预算为最多 3 轮 `run_board_bench_compare fetch_board_logs`。三入口 speedup 均大于 1.2x 为 `positive`；1.05x 到 1.2x 为 `weak-positive`；0.95x 到 1.05x 为 `neutral`；小于 0.95x 为 `negative`；三轮内方向反复跨 bucket 为 `unstable`。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | 本阶段新增测试目标是 production-public（真实公开入口）correctness；现有 protected helper bench 是 production-detail helper 形态。 |
| A/B boundary | correctness 是 public overload；board bench 当前是 protected helper wrapper，不等同完整公开入口 dispatch。 |
| 当前决策问题 | RVV-vs-scalar correctness / fallback correctness；若跑 board，性能问题仍是已有 RVV helper 是否值得保留。 |
| diagnostic 是否可外推到 production | protected helper correctness 不可单独外推；public-entry test 通过后才能证明公开入口 dispatch/fallback。 |
| comparison-boundary / baseline mismatch 风险 | 有。bench wrapper 预分配并直接调用 protected helper，不能覆盖 public entry 的 layout gate 和非覆盖类型 fallback。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前已有 production patch；若 board 退化，需要停在 PI5 类用户确认边界，不能自行回滚。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 本阶段不做新 family 选择，因此不需要 RVV-vs-RVV A/B；若改 helper family，必须补。 |

## 继续 / 停止条件

继续条件：公开入口测试、fallback、QEMU correctness、asm、board 或文档结构中任一项仍为 `phase_deferred + unblocked`。停止条件只允许是 production 采纳 / 回滚需要用户确认、板卡或工具不可达、Evidence Doctor Error 无法解释、dirty isolation 不安全，或矩阵与 roadmap 无授权未阻塞动作。

默认下一 phase：若本阶段不命中 production 修复阻塞，进入 `010-normal-plane-test-support-structure`，把 mixed `plane_models` 根目录长文件迁移 / 拆分为当前配置期望的 `src/`、`include/`、`include/impl` 和 topic-local doc suite。

## 文档更新清单

- `test-rvv/sample_consensus/plane_models/doc/phases/README.zh.md`
- `test-rvv/sample_consensus/plane_models/doc/phases/optimization-matrix.zh.md`
- `test-rvv/sample_consensus/plane_models/doc/optimization-roadmap.zh.md`
- `test-rvv/sample_consensus/plane_models/doc/sac_model_normal_plane-evaluation.zh.md`
- `test-rvv/sample_consensus/plane_models/doc/phases/000-normal-plane-current-state-and-public-entry-boundary/result.zh.md`
- Handoff 只在最终输出中给出，除非需要跨对话恢复再落盘到 work log。
