# Phase 010 plan: projected covariance fusion diagnostic

## 阶段意图和边界

本阶段验证 `getProjectedCloud()` + projected `computeCovarianceMatrix()` 是否能融合成不写临时 projected cloud（投影点云）的 RVV diagnostic（诊断）helper。目标边界是 `PointXYZ`、`float`、ordered indexed cloud（按 `indices` 顺序遍历的点云）、single normal vector（单个法向量）和 plane through mean（经过质心的平面）。本阶段仍不修改 production（生产源码）。

## 当前状态清单

| area | current state | path |
| --- | --- | --- |
| phase 000 | reduction summary diagnostic positive，board median 2.082x，Evidence Doctor Errors=0 | `doc/phases/000-current-state-and-reduction-diagnostic/result.zh.md` |
| production scalar path | `getProjectedCloud()` 先为每个角度分配并写 `projected_cloud`，随后 projected covariance 再遍历该云 | `features/include/pcl/features/impl/moment_of_inertia_estimation.hpp` |
| current helper | 已有 indexed xyz load 和 reduction helper，可复用 | `include/impl/moi_reductions.hpp` |

## 假设与候选族

对中心化点 `r = p - mean`，投影到法向量 `n` 的平面后：

```text
r_projected = r - dot(r, n) * n
covariance = sum(r_projected * r_projected^T) / (count - 1)
```

这等价于 production 当前 `getProjectedCloud(normal, mean)` 后再用 `mean_value_` 求 covariance。预期收益来自避免每个角度的 projected cloud resize/write 和第二次读 projected cloud。

## 优化矩阵

| candidate family | scope | required evidence | decision |
| --- | --- | --- | --- |
| projected covariance fusion | one normal vector，helper-only diagnostic | RED/green correctness、QEMU `run_test_compare`、asm、board repeated、Evidence Doctor | planned |

## 实现和测试动作

| action | artifact / command | expected evidence | completion criteria |
| --- | --- | --- | --- |
| B1 test-first 红灯 | 扩展 `src/test_moi.cpp`，调用尚未实现的 `computeProjectedCovarianceRVV` | `make run_test_rvv` 编译失败 | 失败原因是 candidate 缺失 |
| B2 Std/RVV projected covariance helper | `include/impl/moi_reductions.hpp` | Std/RVV same-chain（同构链路）对拍 | `run_test_compare` 通过 |
| B3 bench case | 扩展 `src/bench_moi.cpp` case-filter `moi_projected_covariance` | QEMU smoke + board repeated | 输出 checksum 和 us/iter |
| B4 asm / board / doctor | `dump_bench_rvv`、`run_board_moi_repeated` 或 phase010 target | gather/reduction 指令、5-run summary、Errors=0 | phase result 回填 |

## Evidence Doctor 和 registry 规则

沿用 `log/evidence_registry.json`。若复用 repeated target，应将 `case-filter`、run label 和 doc-ref 指向 phase 010，避免覆盖 phase 000 的 current truth。

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | diagnostic |
| A/B boundary | test helper：materialized projection Std vs fused projected covariance RVV |
| 当前决策问题 | RVV-vs-scalar diagnostic；是否减少 projection allocation 和 projected covariance 成本 |
| diagnostic 是否可外推到 production | unknown；公式等价但 production 每个 angle 还会调用 Eigen eccentricity 求解 |
| comparison-boundary / baseline mismatch 风险 | yes；helper-only bench 不含 allocation resize 策略和完整 angle loop |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 如果 phase 000 positive 但 phase 010 negative，仍可考虑只接入 reduction summary 相关生产片段；否则先做 production-shaped profile |
| clean adoption 是否需要 production boundary 内 RVV-vs-RVV detail A/B | yes |

## continue / stop conditions

默认继续 B1-B4。停止条件：公式对拍失败、Evidence Doctor Error 无法修复、板卡工具失败、或继续需要修改 production 源码进入 PI1 授权边界。
