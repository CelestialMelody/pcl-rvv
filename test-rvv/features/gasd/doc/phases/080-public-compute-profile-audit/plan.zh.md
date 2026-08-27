# Phase 080 plan: public compute profile audit

## 阶段意图和边界

本阶段只做 public compute profile audit（公开入口性能剖析审计），判断完整
`GASDEstimation::computeFeature` 和 `GASDColorEstimation::computeFeature` 的公开入口耗时是否仍
支持继续寻找新的 RVV（RISC-V Vector，可变长度向量）实现族。

本阶段不修改 `features/include/pcl/features/impl/gasd.hpp`，不接入 production patch（生产补丁），也不把
Phase 060 已经稳定负向的 staged shape family（分阶段暂存形态）恢复为生产候选。

## 当前状态清单

| item | current state |
| --- | --- |
| production file | `features/include/pcl/features/impl/gasd.hpp` 当前不改 |
| current decision | Phase 070 为 `no-production for current staged shape family` |
| known positive diagnostics | Phase 010 shape projection median `1.690x`；Phase 020 color hue median `1.920x`；Phase 030 trilinear staging median `1.950x` |
| known negative diagnostics | Phase 050 Eigen-backed write median `0.820x`；Phase 060 production-shaped shape combined median `0.590x` |
| existing bench cases | `public_gasd_shape_compute`、`public_gasd_color_compute` 已在 `src/bench_gasd.cpp` 中存在 |
| board availability | 当前会话用户确认板卡可用 |

## 假设与候选族

本阶段验证的不是某个 RVV candidate 是否更快，而是完整公开入口的 baseline cost shape（基线耗时形态）。

| hypothesis | reason | decision impact |
| --- | --- | --- |
| public shape compute 耗时很小或与 Phase 060 负向一致 | Eigen write / descriptor object boundary 可能主导，继续当前 staged family 价值低 | 保持暂停，不进入 production integration loop |
| public shape compute 明显显示完整入口有大成本空间 | 可能存在未覆盖的新实现族，例如 transform/profile-first 或写回布局重构 | 后续可另开 profile-guided phase |
| public color compute 明显比 shape compute 更大 | color path 或 `INTERP_QUADRILINEAR` 可能值得单独 follow-up | 只形成独立 follow-up，不外推当前 shape closeout |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | test / bench | board | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| public shape compute profile | public shape entry | `PointXYZ` / `float` / production descriptor | `public_gasd_shape_compute` repeated board | required | not_applicable | Evidence Doctor summary | pending |
| public color compute profile | public color entry | `PointXYZRGBA` / `float` / production descriptor | `public_gasd_color_compute` repeated board | required | not_applicable | Evidence Doctor summary | pending |

## 实现和测试动作

| action | artifact / command | expected evidence |
| --- | --- | --- |
| P1 跑 shape public compute repeated board | `make -C test-rvv/features/gasd board_repeated REPEATED_BOARD_TAG=phase080_public_shape_compute REPEATED_BOARD_TITLE="GASD public shape compute profile repeated board summary" REPEATED_BOARD_RUN_LABEL=gasd_phase080_public_shape_compute_repeated REPEATED_BOARD_CASE_NAME=public_gasd_shape_compute GASD_REPEATED_BENCH_ARGS="--case-filter public_gasd_shape_compute --points 4096 --shape-half-grid 4 --hists-size 1 --repeat 16 --iterations 10 --warmup 2"` | 5-run board summary |
| P2 跑 shape Evidence Doctor | 同上参数运行 `evidence_doctor_repeated` | Errors / Warnings / Suggestions |
| P3 跑 color public compute repeated board | `make -C test-rvv/features/gasd board_repeated REPEATED_BOARD_TAG=phase080_public_color_compute REPEATED_BOARD_TITLE="GASD public color compute profile repeated board summary" REPEATED_BOARD_RUN_LABEL=gasd_phase080_public_color_compute_repeated REPEATED_BOARD_CASE_NAME=public_gasd_color_compute GASD_REPEATED_BENCH_ARGS="--case-filter public_gasd_color_compute --points 4096 --shape-half-grid 3 --color-half-grid 2 --hists-size 12 --repeat 16 --iterations 10 --warmup 2"` | 5-run board summary |
| P4 跑 color Evidence Doctor | 同上参数运行 `evidence_doctor_repeated` | Errors / Warnings / Suggestions |
| P5 文档回填 | `result.zh.md`、roadmap、matrix、README / phase index | 继续 / 停止判断 |

## Evidence Doctor 和 registry 规则

本阶段只提交 summary-level（摘要级）证据路径，不提交 raw board logs（原始板卡日志）。Evidence Doctor 的
Errors / Warnings / Suggestions 必须写入阶段结果；若 repeated summary 不完整或 checksum 不一致，阶段降级为
blocked / inconclusive（受阻 / 无结论）。

## 板卡复跑预算和决策桶

| item | value |
| --- | --- |
| repeated runs | 5 |
| warmup / iterations | `--warmup 2 --iterations 10` |
| repeat | `16`，避免公开入口阶段运行时间过长 |
| positive / negative | public Std/RVV 没有 production patch 时只用于 cost-shape profile，不作 RVV speedup 采纳 |
| unstable | range 跨桶或 Doctor warning/error 指向不稳定时降级为 `profile-inconclusive` |

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-public profile（公开生产入口性能剖析），不是 production patch 证据 |
| A/B boundary | public overload benchmark；Std/RVV 二进制当前应等价，因为 production 未改 |
| 当前决策问题 | 是否有足够公开入口成本空间支持另开新实现族 |
| diagnostic 是否可外推到 production | only cost-shape yes；不能外推成 RVV adoption |
| comparison-boundary / baseline mismatch 风险 | yes；Std/RVV 当前没有真实实现差异，性能比值只作为测量稳定性参考 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes，但需要用户授权 PI1 scope |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes；本阶段不进入 adoption |

## 继续 / 停止条件

- 如果 public profile 不显示新的高价值热点方向，维持 Phase 070 的 `stop_for_user_review_no_production_closeout`。
- 如果 shape public compute 显示显著成本空间，但 Phase 060 仍负向，则下一步只能是 profile-guided new family
  phase，不能复用当前 staged shape family。
- 如果 color public compute 明显高于 shape 且用户关心 color interpolation，则把
  `INTERP_QUADRILINEAR` 写成独立 follow-up phase。

## 文档更新清单

完成后更新：

- `080-public-compute-profile-audit/result.zh.md`
- `doc/phases/README.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `README.zh.md`

`doc-rvv/features/gasd-RVV.zh.md` 仍不适用，除非未来有 adopted production behavior（已采纳生产行为）或 PI5
生产证据闭环。
