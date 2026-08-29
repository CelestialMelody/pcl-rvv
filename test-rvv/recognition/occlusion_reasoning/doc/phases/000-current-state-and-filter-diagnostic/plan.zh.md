# Phase 000 Plan: current-state-and-filter-diagnostic

## 阶段意图和边界

本阶段建立 `ZBuffering::filter()` 的 pre-production diagnostic（接入生产前诊断）。
阶段只证明 raw depth buffer 输入下，投影、bounds mask（边界掩码）、invalid depth
过滤、阈值比较和保序 indices 输出能否由 RVV candidate（RVV 候选链路）保持
same-chain correctness（同构链路正确性）并在板卡上显示收益。

本阶段不修改 production，不接入 `ZBuffering` 类成员分流，不优化 `computeDepthMap()`、
smooth window 或 `copyPointCloud`。

## 当前状态清单

| 项目 | 当前事实 | 路径 |
| --- | --- | --- |
| 队列入口 | 执行清单第 7 项，状态未启动 | `doc-rvv/library-screening/recognition/recognition-function-evaluation-queue.zh.md` |
| production 源码 | `filter()` 逐点投影并读取 `depth_[u * cy_ + v]` | `recognition/include/pcl/recognition/impl/hv/occlusion_reasoning.hpp` |
| depth map 风险 | `computeDepthMap()` 初始化长度和写入 stride 与 filter 读取 stride 不一致 | 同上 |
| topic 资产 | 新建 `test-rvv/recognition/occlusion_reasoning` | 本目录 |
| production 状态 | 未修改 | `git status --short` |

## Phase Scope 与扩展队列

- `validated_scope`：test-only `ProjectionPoint{x,y,z}`，`Scalar=float`，raw `float`
  depth buffer，方形和非整 VL 点数，filter diagnostic。
- `unvalidated_scope`：真实 `ModelT/SceneT` 泛型字段布局、`computeDepthMap()`、
  `filter(model, filtered)` 的 `copyPointCloud` 成本、smooth window、非 RVV build production
  fallback 和上游 `hypotheses_verification` 调用。
- `point_type_expansion_queue`：PI1 前需要读取泛型点类型策略；若接 production，优先只覆盖
  PCL 已显式实例化的 `PointXYZ -> PointXYZ` / `PointXYZ -> PointXYZRGB` 或证明 xyz traits gate。
- `phase_closeout_boundary`：只关闭 `projection-filter-rvv` diagnostic 矩阵条目。

## 假设与候选族

| candidate family | 假设 | 风险 | 本阶段状态 |
| --- | --- | --- | --- |
| `projection-filter-rvv` | 批量计算 `u/v`、depth index 和 keep predicate 可减少逐点投影成本 | gather depth 和保序 append 仍可能主导 | planned |
| `projection-filter-vcompress` | 后续可用 `vcompress` 压缩 keep indices | 实现复杂，需独立 A/B | deferred |
| `compute-depth-map-semantics-fix` | 生产接入前需要先冻结 depth map 构建语义 | 修复可能改变历史行为 | planned-risk |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `projection-filter-rvv` | model point stream + raw depth buffer | `ProjectionPoint`, float, AoS + raw depth | diagnostic helper | `run_test_compare` | `run_bench_rvv` smoke / board repeated | 5-run planned | vector projection instructions | planned | planned | write RED |

## 实现和测试动作

| action | 产物 / 命令 | 预期证据 | 完成判据 |
| --- | --- | --- | --- |
| RED gtest | `make -C test-rvv/recognition/occlusion_reasoning run_test_rvv` | RVV build 因 candidate 返回 `ScalarFallback` 失败 | failure observed |
| GREEN candidate | `include/impl/occlusion_reasoning_candidates.hpp` | Std/RVV correctness 通过 | `run_test_compare` pass |
| QEMU smoke | `run_qemu_smoke` | 构建、correctness 和日志形状 | pass，不写性能结论 |
| asm | `check_occlusion_filter_rvv_asm` | RVV 指令可归属到 bench helper | pass 或降级 |
| board repeated | `board_repeated` | 5-run Std/RVV A/B | decision bucket stable |
| Evidence Doctor | `evidence_doctor_repeated` | Errors / Warnings / Suggestions | Error 为 0 或降级 |

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | test helper |
| 当前决策问题 | RVV-vs-scalar diagnostic |
| diagnostic 是否可外推到 production | unknown；它复刻 filter 核心谓词，但不包含真实 `depth_` 构建、类成员 dispatch 或 `copyPointCloud` |
| comparison-boundary / baseline mismatch 风险 | yes；raw depth buffer 避开了 `computeDepthMap()` 当前索引风险 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes，但只在 PI1 能冻结 depth map 语义和 fallback 后允许 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前没有已采纳 RVV family；如后续尝试 vcompress，需要同边界 A/B |

## Evidence Doctor 和 Registry

本阶段 repeated board 输出由 `script/generate_occlusion_reasoning_evidence_manifest.py`
生成 summary 和 manifest，再调用 `test-rvv/script/evidence_doctor.py`。registry 路径为
`log/evidence_registry.json`。

## 板卡复跑预算和决策桶

默认 5-run，`BENCH_ARGS="65536 150 150 200 5"`。若 Evidence Doctor 暴露方向接近阈值、
长尾或 `B/A < 1`，最多追加 1 组同边界确认复跑。bucket：
`positive >= 1.20`、`weak_positive [1.05,1.20)`、`neutral [0.95,1.05)`、
`negative < 0.95`、跨桶为 `unstable`。

## 继续 / 停止条件

板卡当前由用户说明可用，因此本阶段不以“需要板卡验证”为停止理由。只有构建工具失败、
板卡不可达、Evidence Doctor Error 未解、生产语义 bug 需要用户判断或 dirty isolation 不安全时，
才停止。若 Phase 000 positive，下一阶段默认进入 PI1 production integration plan；若 weak / neutral /
negative，优先尝试 `vcompress` 或 depth-map 语义审计后再决定是否停。

## 文档更新清单

本阶段更新 README、evaluation、testing overview、correctness tests、benchmark/evidence、
optimization evidence、test support code map、roadmap、matrix、phase result 和 Handoff。
production 未接入前不创建 `doc-rvv`。
