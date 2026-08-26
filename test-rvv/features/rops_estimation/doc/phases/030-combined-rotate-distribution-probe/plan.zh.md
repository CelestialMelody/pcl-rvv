# Phase 030 Plan: combined-rotate-distribution-probe

## 阶段意图和边界

本阶段验证 combined production-shaped diagnostic（组合生产形态诊断）：同一个 timing boundary（计时边界）内先执行 `rotateCloud()` 对 synthetic transformed local cloud（合成已变换局部点云）生成 rotated cloud（旋转点云）和 AABB（轴对齐包围盒），再对 XY / XZ / YZ 三个 projection（投影）生成 distribution matrix（分布矩阵）。

本阶段仍不修改 production（生产源码）。它不覆盖 LRF、KdTree local surface、central moments、descriptor normalization、public dispatch、真实 mesh 输入或模板点型扩展。它只回答：Phase 010 的 binning 正向和 Phase 020 的 rotate + AABB 正向放进同一组合链路后，是否仍支持进入 PI1 production integration plan（生产接入计划）。

## 当前状态清单

| area | current state | path |
| --- | --- | --- |
| Phase 010 | distribution matrix diagnostic positive；5-run board median 1.440x，0/5 退化 | `doc/phases/010-distribution-matrix-ablation/result.zh.md` |
| Phase 020 | rotateCloud + AABB diagnostic positive；5-run board median 1.220x，0/5 退化 | `doc/phases/020-rotate-projection-ablation/result.zh.md` |
| current helper | 已有 `rotateCloudRVV()` 和 `getDistributionMatrixRVV()`；均为 test-only helper | `include/impl/rops_components.hpp` |
| production path | `computeFeature()` 每次旋转后依次执行 `rotateCloud()`、3 个 `getDistributionMatrix()` 和 `computeCentralMoments()` | `features/include/pcl/features/impl/rops_estimation.hpp` |

## 假设与候选族

| candidate family | hypothesis | risk | validation |
| --- | --- | --- | --- |
| sequential combined helper | 复用 Phase 020 rotate helper 和 Phase 010 distribution helper，先保留 production-like rotated cloud buffer，再生成 3 个矩阵 | 两个 component 单独 positive，但组合后可能被 rotated cloud 写回、row/col staging、matrix scatter 和 cache 行为稀释 | production oracle 对拍、QEMU smoke、asm、board repeated |
| future fused projection | 直接在旋转后对 projection binning，减少 rotated cloud 写回或复用 buffer | 语义和 staging 生命周期更复杂，可能改变 AABB / projection 复用边界 | 若 sequential combined negative / weak，再作为后续 phase |

## 优化矩阵

本阶段矩阵主路径见 `../optimization-matrix.zh.md`。Phase 030 只能把 `combined rotate + distribution matrix` 从 `planned` 推进到 `attempted-positive-diagnostic`、`attempted-weak/neutral/negative-diagnostic`、`rejected with evidence` 或 `phase_deferred + unblocked`；不能关闭完整 ROPS descriptor topic。

## 实现和测试动作

| action | artifact / command | expected evidence | completion criteria |
| --- | --- | --- | --- |
| A1 RED test | `src/test_rops_estimation.cpp` 调用尚不存在的 `rops::rotateCloudAndDistributionMatricesRVV()` | `make run_test_rvv` 编译失败 | failure 来自缺少 candidate symbol |
| A2 helper implementation | `include/impl/rops_components.hpp` | Std fallback 串联 `rotateCloudStd()` + `getDistributionMatrixStd()`；RVV 串联 `rotateCloudRVV()` + `getDistributionMatrixRVV()` | `run_test_compare` 通过 |
| A3 combined bench | `src/bench_rops_estimation.cpp` 新增 `rops_rotate_distribution_pipeline` case | QEMU smoke checksum match；板卡可解析 | bench 不把 QEMU timing 当性能证据 |
| A4 asm attribution | `dump_test_rvv` / `dump_bench_rvv` | combined bench 二进制里同时出现 rotate 和 distribution 相关 RVV 指令 | result 写清归属边界 |
| A5 board repeated | `run_board_bench_compare` 5-run + `doctor_phase030_board_summary` | board summary / manifest / Evidence Doctor | bucket 按下方规则判定 |

## Board 复跑预算与决策桶

默认先跑 1 次 board smoke，再跑 5-run repeated：`points=65536`、`bins=5`、`iterations=20`、`warmup=3`、`repeat=8`、`case-filter=rops_rotate_distribution_pipeline`。若 median >= 1.15 且 0/5 退化，decision bucket 为 positive；median >= 1.05 且退化不超过 1/5 为 weak_positive；0.95-1.05 为 neutral；median < 0.95 为 negative；方向跨桶或长尾明显则 unstable。除非 Evidence Doctor 出现 Error、checksum 不一致或方向反转，不无限复跑。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | same bench wrapper；Std build scalar fallback vs RVV build test-only combined helper |
| 当前决策问题 | implementation-shape；combined helper 是否支持进入 PI1 production integration plan |
| diagnostic 是否可外推到 production | unknown。它比单 component 更接近 production，但仍缺 LRF、local surface、moments、normalization、public dispatch 和模板点型 |
| comparison-boundary / baseline mismatch 风险 | yes。production 中 rotate / projection 嵌在 keypoint × axis × rotation 循环中，buffer 生命周期和 allocator 行为可能不同 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | weak_positive 以上才考虑 PI1；neutral / negative 先做 fused projection 或 descriptor normalization phase，不直接 production patch |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes；本阶段没有 production patch |

## Phase scope 与未验证范围

`validated_scope`：计划覆盖 `pcl::PointXYZ` / float / AoS synthetic finite transformed local cloud，4 个 axis-angle case，rotated cloud + AABB + 3 个 distribution matrices，QEMU Std/RVV correctness，asm attribution 和 board repeated combined diagnostic。

`unvalidated_scope`：完整 ROPS descriptor、LRF、真实 mesh local surface 分布、central moments fusion、descriptor normalization、其它点型、production dispatch、不同 rotations / bins / support radius。

`point_type_expansion_queue`：本阶段不扩点型；若进入 PI1，再为 exact type gate 与 PointXYZ-like traits 建独立 phase。

## 继续 / 停止条件

若 combined probe positive，下一步默认进入 PI1 production integration plan，但 PI5 后采纳 / 回滚必须等待用户确认。若 combined probe weak_positive，先审计 production buffer 生命周期再决定是否 PI1。若 neutral / negative，默认继续 fused rotate-to-bin 或 descriptor normalization phase，而不是凭单 component positive 直接改 production。若板卡不可用、checksum mismatch 或 Evidence Doctor Error，先修复 / 降级 / 记录 blocker。

## 文档更新清单

完成阶段后更新 `result.zh.md`、`optimization-matrix.zh.md`、`optimization-roadmap.zh.md`、`rops_estimation-evaluation.zh.md` 和 phase `README.zh.md`。production 长期主题文档仍不适用，除非后续真实接入 production 并通过 PI5 用户检查点。
