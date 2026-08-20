# Phase 109 Plan: dual-indexed-exact-public-variance

## 阶段意图和边界

本阶段复核当前已接入 production（生产源码）的 dual-indexed exact
`PointXYZ -> PointXYZ` RVV dispatch 在 20-run board repeated（板卡重复测试）下的方差。
Phase 107 的同边界 family A/B（实现族 A/B 对比）为 positive，但 4K 有 `1/5` below-1
caveat；Phase 109 只回答这个 caveat 是否稳定、是否需要调整生产 gate。

本阶段不证明也不修改：

- 不扩大到 dual-indexed generic PointXYZ-like 点类型。
- 不恢复 correspondence RVV dispatch。
- 不改变 source-indexed generic Phase 103/104 guarded / Phase 106 negative 结论。
- 不修改 `registration/include/pcl/registration/impl/transformation_estimation_2D.hpp`，除非 20-run
  结果明确要求进入后续生产 gate 决策点；即使如此，也先停在用户决策点。

## 当前状态清单

| 项目 | 当前事实 |
| --- | --- |
| production source | `registration/include/pcl/registration/impl/transformation_estimation_2D.hpp` 已包含 exact dual-indexed `PointXYZ -> PointXYZ` dispatch。 |
| Phase 107 correctness | `make -C test-rvv/registration/transformation_estimation_2D run_test_compare`：Std/RVV `84/84`。 |
| Phase 107 QEMU / asm | `record_qemu_dual_indexed_family_ab_state`：Doctor `0/0/0`；`production_public_dual_indexed_boundary` 50 RVV lines。 |
| Phase 107 board | `dual_indexed_family_ab_phase107_repeated`：4K/64K/256K B/A `1.085x / 1.691x / 1.678x`；4K `1/5` below-1；Doctor `0/3/0`。 |
| 当前 caveat | 64K/256K 收益强；4K 接近阈值，需 20-run 方差复核。 |

## 假设与候选族

候选族只有当前 production detail family A/B：

- A：materialize selected source/target rows，再走当前 ordered public RVV。
- B：当前 direct dual-indexed public RVV。

假设：Phase 107 的 4K `1/5` below-1 可能是小规模测量方差；若 20-run 仍有较高 below-1
频率，则当前 exact dual-indexed dispatch 可能需要 size threshold（规模阈值）或继续保留 caveat。

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | test | bench / board | asm | Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| current direct dual-indexed public RVV vs materialize+ordered public RVV | dual-indexed-cloud-pair | exact `PointXYZ -> PointXYZ`, `Scalar=float`, dense finite, valid source/target indices, size >= 16 | `run_test_compare` | `dual-indexed-family-ab` 20-run board under independent label `dual_indexed_family_ab_phase109_variance_repeated` | `production_public_dual_indexed_boundary` | QEMU and board Evidence Doctor | positive / weak-positive / unstable / gate-review |

## 实现和测试动作

1. 写入 Phase 109 plan，并补 Phase 109 专用 board registry target，避免新证据登记到 Phase 093。
2. 运行 correctness：`make -C test-rvv/registration/transformation_estimation_2D run_test_compare`。
3. 运行 QEMU smoke / asm / Doctor / registry：`record_qemu_dual_indexed_family_ab_state`，并在 result 中说明该 QEMU 目录是路径命中和反汇编证据，不作为性能结论。
4. 使用独立 board label 和 evidence dir：

   ```bash
   make -C test-rvv/registration/transformation_estimation_2D \
     run_board_bench_dual_indexed_family_ab_phase109_repeated \
     TE2D_BOARD_REPEATED_RUNS=20 \
     TE2D_BOARD_DUAL_INDEXED_FAMILY_AB_RUN_LABEL=dual_indexed_family_ab_phase109_variance_repeated
   ```

5. 运行 Evidence Doctor、registry、`evidence_status` 和 `git diff --check`。
6. 同步 `result.zh.md`、phase README、roadmap、optimization matrix、evaluation、`doc-rvv` 和 current Handoff。

## Evidence Doctor 和 registry 规则

- board summary：`log/board/dual_indexed_family_ab_phase109_variance_repeated/summary.md`
- board manifest：`log/board/dual_indexed_family_ab_phase109_variance_repeated/evidence_manifest.json`
- board Doctor：`log/board/dual_indexed_family_ab_phase109_variance_repeated/evidence_doctor.md`
- registry run label：`board-te2d-dual-indexed-family-ab-repeated-phase109-variance`
- QEMU Doctor Error 必须修复或降级；board Doctor Error 不能 clean close。
- Warning 必须解释是否影响 4K caveat、64K/256K positive 结论或生产 gate。

## 板卡复跑预算和决策桶

| 桶 | 判断口径 |
| --- | --- |
| positive | 4K/64K/256K median 均 `> 1.05x`，且每个规模 `B/A<1` 频率不超过 `1/20`，无 Doctor Error。 |
| weak-positive | 4K median `> 1.0x` 但 `<= 1.05x`，或 4K `B/A<1` 频率 `2/20` 到 `3/20`；64K/256K 仍稳定 positive。 |
| unstable | 4K `B/A<1` 频率 `>= 4/20`，或 Doctor 报告和数值方向冲突但 64K/256K positive。 |
| gate-review | 4K median `< 1.0x`、64K/256K 任一规模 below-1 频率高，或 Doctor Error 未能解释。 |

预算固定为 20-run；若该预算后 4K 仍摇摆，不无限复跑，写成 `unstable` 或 `gate-review`
并停在用户决策点。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production-detail`。 |
| A/B boundary | 同一 RVV binary 内的 public overload detail helper：direct dual-indexed public RVV vs materialize+ordered public RVV。 |
| 当前决策问题 | 已接入 production path 的 caveat 复核；不是新 family clean adoption。 |
| diagnostic 是否可外推到 production | 本阶段不是 diagnostic，而是 production-detail same-boundary A/B；可用于评估当前 exact dual-indexed dispatch 的小规模稳定性。 |
| comparison-boundary / baseline mismatch 风险 | 低；A/B 仍需注意 materialize path 和 direct path 计时边界不同，但都是当前 public RVV family 内的对照。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不新增 probe；若结果不稳，只进入 production gate review，不自动回滚。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 已满足同边界 family A/B，本阶段是方差强化。 |

## 阶段完成条件

本阶段完成需要：

- correctness pass；
- QEMU smoke / asm / Doctor 完成；
- 20-run board repeated 使用独立 label / evidence dir 完成；
- Evidence Doctor 异常解释完成；
- registry fresh；
- result、matrix、roadmap、evaluation、`doc-rvv` 和 Handoff 同步；
- 若证据要求改变 production gate，停在用户决策点，不自动回滚或提交。

## 继续 / 停止条件

若 Phase 109 为 positive 或 weak-positive，后续默认转向新的 bounded optimization phase：
source-indexed generic negative-case investigation 或 correspondence new bounded candidate。若 Phase 109 为
unstable / gate-review，下一步是 production gate decision package，需用户确认是否调整 dual-indexed size gate。
