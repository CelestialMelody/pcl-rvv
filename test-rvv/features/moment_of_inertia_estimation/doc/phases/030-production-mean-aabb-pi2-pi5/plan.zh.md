# Phase 030 plan: production mean/AABB PI2-PI5

## 阶段意图和边界

本阶段把 phase 000 中收益最高、生产改动最小的 mean/AABB（质心与轴对齐包围盒）规约接入真实 `MomentOfInertiaEstimation<PointT>::compute()` 路径，完成 PI2-PI5 的有界生产探针。公开 API 不变，只修改当前 topic 的测试资产、生产私有 helper 声明 / 实现和 topic-local 证据文档。

本阶段不接入 phase 010 projected covariance fusion（投影协方差融合），也不把 diagnostic helper-only 结果外推成 adopted production behavior（已采用生产行为）。

## 当前状态清单

| item | status | path |
| --- | --- | --- |
| phase 000 diagnostic | board median 2.082x，bucket `positive` | `log/board/repeated_phase000_reduction_diagnostic/summary.md` |
| phase 010 diagnostic | board median 1.247x，bucket `positive` | `log/board/repeated_phase010_projected_covariance_diagnostic/summary.md` |
| PI1 plan | completed；允许 bounded production probe | `doc/phases/020-pi1-production-integration-plan/plan.zh.md` |
| current handoff | `partial-production-candidate`，PI2 已由用户本轮授权 | `tmp/rvv-work-logs/features/moment_of_inertia_estimation/current-handoff/current-handoff.zh.md` |
| production scalar shape | `computeMeanValue()` 内部完成 mean/AABB | `features/include/pcl/features/impl/moment_of_inertia_estimation.hpp` |

## 假设与候选族

候选族为 `production mean/AABB RVV helper`：在 `__RVV10__` 下，用 `RVVXYZAoSFloatLayout<PointT>` 做 PointXYZ-like traits gate（类似 PointXYZ 的 xyz 单 float AoS 布局门控），用公共 `pcl::rvv_load::indexed_load3_f32m2` 做 indexed gather（按索引 gather 加载），向量规约 x/y/z 的 sum/min/max。gate 失败、非 RVV 构建或 32-bit byte offset 不安全时自然回到 `computeMeanValueStd()`。

本阶段的性能假设是：虽然 mean/AABB 只是完整 `compute()` 的一部分，但它位于每次 public compute 的前置必经路径，且 phase 000 证明该规约族在 helper boundary（helper 边界）上有足够强的收益，值得一次 production public（真实公开入口）探针。

## 优化矩阵

| candidate family | row source | point type / Scalar / layout | test | bench / board | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| production mean/AABB RVV helper | `indices_` indexed cloud | PointXYZ-like traits gate，`float`，AoS，32-bit byte offset | private helper hit test；public `compute()` getters 对拍；fallback compile/run | public `moi_public_compute` repeated board | production helper / bench RVV binary 中出现 gather + reduction 指令 | production manifest + Evidence Doctor | PI5 决定保留、回滚或继续探针 |

## 实现和测试动作

1. RED：在 `src/test_moi.cpp` 增加只在 `__RVV10__` 下编译的 private-helper 命中测试，先调用计划中的 `computeMeanValueRVV()`；实现前应编译失败。
2. GREEN：把 `computeMeanValue()` 拆为 `computeMeanValueStd()` 与 `computeMeanValueRVV()`，RVV helper 只在 gate 成立时写入 `mean_value_` 和 AABB 并返回 `true`。
3. 正确性：新增 public `MomentOfInertiaEstimation<pcl::PointXYZ>::compute()` getter 对拍，覆盖 shuffled indices 和 tail；非 RVV Std 构建仍走标量。
4. bench：在 `bench_moi.cpp` 增加 `moi_public_compute` case，计时完整 public `compute()`，不复用 helper-only timing。
5. 证据：新增 phase030 repeated board target、summary / manifest / Evidence Doctor / registry 入口。
6. 文档：更新 phase result、matrix、roadmap、evaluation、README 和 Handoff。正式 `doc-rvv` 只在 PI5 用户确认采纳后创建。

## Evidence Doctor 和 registry 规则

production direct summary 使用 `log/board/repeated_phase030_public_compute_production/summary.md`、`evidence_manifest.json` 和 `evidence_doctor.md`。Evidence Doctor 出现 Error 时不得关闭 PI5；Warning 必须解释或降级；Suggestion 可记录到下一阶段。

## 阶段完成条件

阶段完成不是自动采纳。PI5 必须报告：

- production diff 和实际 public entry；
- `run_test_compare`、asm/QEMU smoke、board repeated、Evidence Doctor 和 registry 结果；
- public compute 是否真的有收益；
- 推荐保留、回滚或继续拆分下一生产候选。

## 板卡复跑预算和决策桶

默认 5 runs，`points=65536`、`iterations=5`、`warmup=1`，case `moi_public_compute`。若 median >= 1.10 且 B/A < 1 为 0，则 `positive`；median >= 1.03 且低于 1 的 run 不超过 1 个为 `weak_positive`；0.97 到 1.03 为 `neutral`；低于 0.97 为 `negative`；方向摇摆则 `unstable`。预算耗尽仍摇摆时暂停在 PI5，不反复加跑。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | phase000/010 是 diagnostic；本阶段新增 production-public |
| A/B boundary | phase000 helper-only；phase030 public `MomentOfInertiaEstimation::compute()` |
| 当前决策问题 | RVV-vs-scalar production public 是否值得保留 |
| diagnostic 是否可外推到 production | 只能作为进入 bounded production probe 的依据，不能替代 public evidence |
| comparison-boundary / baseline mismatch 风险 | 有；完整 `compute()` 包含 covariance、Eigen solver、angle scan 和 allocation |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 已有 diagnostic 为 positive；production 若非 positive，停在 PI5 报告，不自行回滚 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前生产无既有 adopted RVV family；public Std/RVV positive 可支持“当前 patch 值得采纳”，但仍需用户 PI5 确认 |

## 继续 / 停止条件

若 phase030 public compute positive 或 weak-positive 且实现小、fallback 简单，PI5 建议保留并等待用户确认；确认后再做 production closeout 和正式 `doc-rvv`。若 neutral / negative / unstable，暂停并说明回滚或继续拆分的选择。若测试、板卡、traits gate 或 dirty isolation 阻塞，则更新 Handoff 为 blocked 或 turn-stop deferred。

## 文档更新清单

本阶段结束前更新：

- `doc/phases/030-production-mean-aabb-pi2-pi5/result.zh.md`
- `doc/phases/README.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/moment_of_inertia_estimation-evaluation.zh.md`
- `tmp/rvv-work-logs/features/moment_of_inertia_estimation/current-handoff/current-handoff.zh.md`

正式 `doc-rvv/features/moment_of_inertia_estimation-RVV.zh.md` 延后到用户确认采纳后。
