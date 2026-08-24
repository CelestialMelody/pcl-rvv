# Phase 042 结果：matrix-local production probe

## EvidenceDecision

`positive_pending_user_judgment`。

本阶段把 `matrix-local-scale-simplification` 真正接入 production helper：`getTransformationFromCorrelation` 现在直接使用 `trace(R * H)` 计算 `sum_tt`，不再构造 `R4 * cloud_src_demean` 临时矩阵和逐列点积。这个改动保持数值等价，QEMU / board / registry 都通过，但 board 结论仍然是 weak-positive，不足以单独替代当前已采纳的 direct-fused 主线，因此是否把它作为一个独立提交点，交给你判断。

## 实际修改

| 文件 | 修改 | 证据角色 |
| --- | --- | --- |
| `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` | 用 `trace(R * H)` 替换 `R4 * cloud_src_demean` + 逐列点积。 | production helper simplification。 |
| `doc/phases/042-matrix-local-production-probe/*` | 新增生产探针阶段记录。 | 保存当前 probe 的边界和结果。 |

## 证据结果

| target | result |
| --- | --- |
| `make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare` | Std/RVV 8 tests passed。 |
| `ALLOW_QEMU_BENCH_COMPARE=1 make -C test-rvv/registration/transformation_estimation_svd_scale run_bench_compare BENCH_ARGS="--case-filter matrix-local-scale --iterations 3 --warmup-iterations 1"` | QEMU smoke 正常，`max_reference_error` 在 `1e-7` 量级。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_matrix_local_scale_repeated` | 5-run board repeated completed；summary / manifest / doctor / registry 已登记。 |

## board repeated 摘要

| case | median B/A | bucket |
| --- | ---: | --- |
| `matrix local scale simplification ordered-cloud-pair 4K` | `1.679x` | `positive` |
| `matrix local scale simplification ordered-cloud-pair 64K` | `1.171x` | `weak_positive` |
| `matrix local scale simplification ordered-cloud-pair 256K` | `1.176x` | `weak_positive` |

Evidence Doctor 结果为 `Errors=0`、`Warnings=2`、`Suggestions=0`。warning 仍来自 4K long-tail / group-outlier。

## 证据边界

| 维度 | 结论 |
| --- | --- |
| production source | 已修改；这是一个窄 helper probe。 |
| correctness | 正向。 |
| QEMU smoke | 正向；只证明日志形状与 manifest。 |
| board repeated | 正向，但仍是 weak-positive。 |
| current submission posture | 需要用户判断是否把这个小 patch 单独收口。 |

## optimization matrix 更新

`matrix-local-production-probe` 从 `not_started` 更新为 `positive_pending_user_judgment`。它是一个已经落地到源码的较小生产补丁，但板卡结论仍不足以自然替代当前主线，因此现在到了提交判断点。

## 下一步

要么把这个 probe 单独收口成一个提交点，要么保留在当前 worktree 里继续和其它候选一起评估。这里我建议交给你判断。
