# Phase 000 Plan: current-state and common covariance audit

## 阶段意图和边界

本阶段启动 `features/normal_3d` RVV topic。目标是证明 `NormalEstimation::computeFeature`
是否已经通过 common 层 `computeMeanAndCovarianceMatrix` 的 dense indexed RVV 路径覆盖主要可向量化片段。
本阶段不修改 production 源码，不创建长期 `doc-rvv` production 文档，不证明 OMP 入口。

## 当前状态清单

| area | 当前事实 |
| --- | --- |
| production | `normal_3d.hpp` 没有本地 `__RVV10__` 分支；`computePointNormal` 调用 common centroid/covariance helper。 |
| reused RVV | `common/include/pcl/common/impl/centroid.hpp` 已有 dense full-cloud 与 dense indexed `computeMeanAndCovarianceMatrixRVV`。 |
| tests | 上游 `test/features/test_normal_estimation.cpp` 覆盖 normal correctness；本 topic 新建独立 synthetic plane tests。 |
| benchmark | 上游 `benchmarks/features/normal_3d.cpp` 依赖外部 PCD；本 topic 新建 synthetic component/public bench，便于板卡复跑。 |
| production doc | no adopted production behavior，`doc-rvv/features/normal_3d-RVV.zh.md` not_applicable。 |

## Phase scope 与扩展队列

| item | scope |
| --- | --- |
| validated_scope | `PointXYZ -> Normal`、dense surface、indexed neighborhood、`Scalar=float` covariance 输出、synthetic plane grid、KSearch 16/32。 |
| unvalidated_scope | `PointXYZRGB` / `PointNormal` / custom xyz-like 点型、非 dense surface、NaN/Inf 邻域过滤语义、OMP、真实 PCD dataset、`Scalar=double`、其它 search policy。 |
| point_type_expansion_queue | 若 Phase 000 positive，后续 phase 需要补 PointXYZRGB 或 PointNormal-like traits 的 fallback / correctness / bench / asm / board 证据。 |
| phase_closeout_boundary | 只能关闭 common covariance reuse audit，不能关闭 normal_3d 模板入口的泛型 production 结论。 |

## 优化矩阵

| candidate family | row source / input | point type / Scalar / layout | correctness | bench / board | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| reuse common dense indexed covariance RVV | KSearch neighborhood indices | `PointXYZ`, float xyz AoS, dense | planned `run_test_compare` | planned board smoke / compare | planned `dump_bench_rvv` | planned | planned |
| normal_3d local flip/output RVV | public normal output loop | `PointXYZ -> Normal` | not_yet_covered | not_yet_covered | not_yet_covered | not_yet_covered | deferred pending Phase 000 evidence |

## 实现和测试动作

| action | artifact / command | completion |
| --- | --- | --- |
| A1 scaffold topic harness | `Makefile`, `board.mk`, `src/test_normal_3d.cpp`, `src/bench_normal_3d.cpp` | files compile or errors are recorded in result |
| A2 QEMU correctness | `make -C test-rvv/features/normal_3d run_test_compare` | Std/RVV pass |
| A3 asm attribution | `make -C test-rvv/features/normal_3d dump_bench_rvv` | RVV lines present and attributable to common covariance path or inlined boundary |
| A4 board smoke / compare | `make -C test-rvv/features/normal_3d board_smoke` | board test pass and compare summary exists |
| A5 Evidence Doctor readiness | inspect summary / registry availability | Doctor run or metadata-incomplete downgrade recorded |

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic（生产形态诊断）；bench uses public `NormalEstimation` but not a new production dispatch. |
| A/B boundary | public overload compiled Std vs RVV, where RVV difference is currently inherited from common covariance helper. |
| 当前决策问题 | RVV-vs-scalar for current public path, not RVV-family-selection. |
| diagnostic 是否可外推到 production | 部分可以：公开入口真实调用 production header；但 synthetic input 和 common helper reuse 不能证明所有点型 / search 情况。 |
| comparison-boundary / baseline mismatch 风险 | 中等：search 和 Eigen solver 可能稀释 covariance RVV；component case 和 public case 必须分开看。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes, only if component evidence shows covariance is fast but public path is diluted by a small normal_3d-local bottleneck. |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前不适用；没有新增 RVV family。若新增 normal_3d-local RVV family，则需要同边界 A/B。 |

## 板卡复跑预算和决策桶

Phase 000 先跑 board smoke / compare。若单次结果 positive 或 weak-positive，下一步补 5-run repeated summary；
若 negative 或 neutral，先做 component/public 边界解释，不直接 no-production。若 5-run budget 内方向摇摆，
标为 unstable 并降级 EvidenceDecision。

## 文档更新清单

- 更新 `doc/normal_3d-evaluation.zh.md` 的证据表。
- 写 `result.zh.md`，回填每个 action 的命令、证据路径和 EvidenceDecision。
- 更新 `doc/phases/optimization-matrix.zh.md` 和 `doc/optimization-roadmap.zh.md`。
- 若没有 adopted production behavior，继续不创建 `doc-rvv/features/normal_3d-RVV.zh.md`。

## 继续 / 停止条件

默认下一步是执行 A2-A5。只有板卡不可达、工具链失败、证据矛盾、dirty isolation 不安全，或继续需要 production patch 用户确认时才停止。
