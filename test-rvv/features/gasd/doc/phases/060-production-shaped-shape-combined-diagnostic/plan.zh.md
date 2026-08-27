# Phase 060 plan: production-shaped shape combined diagnostic

## 阶段意图和边界

本阶段把前面分散的 shape path（形状路径）诊断合成一个 production-shaped diagnostic（生产形态诊断）：
baseline（基线）使用生产相似的 scalar direct loop（标量直接循环），逐样本计算 projection、`dbin`、
trilinear weights（三线性权重）并写 `std::vector<Eigen::VectorXf>` histogram，最后执行 shape copy。
candidate（候选）使用 RVV（RISC-V Vector，可变长度向量）projection staging、RVV trilinear staging、
scalar Eigen histogram write 和 RVV shape copy。

本阶段仍不修改 `features/include/pcl/features/impl/gasd.hpp`。它不覆盖 alignment transform（对齐变换）、
public `computeFeature` dispatch（公开入口分流）、`indices_`、descriptor object state（描述子对象状态）、
`INTERP_NONE`、`INTERP_QUADRILINEAR` 或 color path（颜色路径）。它只回答：在 test-only helper boundary
（测试 helper 边界）里，上游 projection 收益能否抵消 Phase 050 暴露的 Eigen write 退化。

## validated_scope / unvalidated_scope

| field | scope |
| --- | --- |
| validated_scope | `PointXYZ` synthetic shape samples、`float`、production-like normalization、`INTERP_TRILINEAR`、`std::vector<Eigen::VectorXf>` shape histogram grid、shape descriptor copy |
| unvalidated_scope | public `computeFeature` dispatch、alignment transform、`indices_` row source、`INTERP_NONE`、`INTERP_QUADRILINEAR`、color path、其它 PointT traits、`Scalar=double` |
| phase_closeout_boundary | 只能关闭 test-only production-shaped shape combined helper 的 correctness、asm、board diagnostic 条目 |

## 当前状态清单

| area | state | evidence |
| --- | --- | --- |
| Phase 010 shape projection staging | diagnostic-positive | `doc/phases/010-shape-sample-projection-diagnostic/result.zh.md` |
| Phase 040 flat histogram write | diagnostic-weak-positive | `doc/phases/040-histogram-write-probe/result.zh.md` |
| Phase 050 Eigen-backed histogram write | diagnostic-negative | `doc/phases/050-eigen-backed-histogram-write-probe/result.zh.md` |
| production state | 未修改 production | `git status -- features/include/pcl/features/impl/gasd.hpp` |
| board | 当前会话确认可用 | Phase 050 repeated board 已完成 |

## 假设与候选族

| hypothesis | candidate | risk | validation |
| --- | --- | --- | --- |
| projection staging 的收益可能抵消 Eigen write 退化 | `computeShapeDescriptorTrilinearRVVStaged` | 额外 staging buffer 和 scalar Eigen write 可能仍让组合路径退化 | same-chain correctness、QEMU smoke、asm、board repeated、Doctor |
| scalar baseline 应更接近 production | `computeShapeDescriptorTrilinearStd` 直接逐样本计算并写 Eigen histogram | 若 baseline 也拆成 staging，会人为放大 RVV 候选收益 | 代码审计和 result 中 baseline boundary 说明 |
| copy boundary 可以作为组合候选的一部分 | candidate 末尾使用已有 RVV shape copy helper | copy 只占小段，不能掩盖写回退化 | bench / board summary 单独写不能证明 public dispatch |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| production-shaped shape combined diagnostic | synthetic shape sample loop | `PointXYZ` / `float` / Eigen histogram + descriptor buffer | planned `make run_test_compare` | planned `candidate_shape_combined_rvv` | pending | pending | pending | phase_deferred + unblocked |

## 实现和测试动作

| action | artifact / command | expected evidence | completion criteria |
| --- | --- | --- | --- |
| C1 scalar combined reference | `include/impl/gasd_reference.hpp` | direct projection + direct Eigen histogram write + scalar shape copy | 与 production shape loop 公式同构，baseline 不共享 staging |
| C2 RVV combined candidate | `include/impl/gasd_copy_candidate.hpp` | RVV projection staging + RVV trilinear staging + scalar Eigen write + RVV copy | `run_test_compare` 通过 |
| C3 correctness test | `src/test_gasd.cpp` | `GASDShapeCombined` 对拍 | 输出 descriptor buffer 与 scalar reference 一致 |
| C4 bench case | `src/bench_gasd.cpp` | case `candidate_shape_combined_rvv` 输出 timing / checksum | QEMU 只作 smoke，性能等 board |
| C5 manifest metadata | `script/generate_gasd_evidence_manifest.py` | case metadata 写清 production-shaped diagnostic 和 timer boundary | Evidence Doctor 不误判为 copy case |
| C6 asm | `make dump_bench_rvv` | projection / trilinear staging / copy helper 命中 RVV 指令 | 写入 result |
| C7 board repeated + Doctor | `make board_repeated ... --case-filter candidate_shape_combined_rvv ...`、`make evidence_doctor_repeated` | 5-run summary + manifest + doctor | Errors 解释或降级 |
| C8 文档回填 | result、matrix、roadmap、evaluation、README | 当前 phase 可恢复 | 写清继续 / 停止条件 |

## Evidence Doctor 和 registry 规则

新增 case `candidate_shape_combined_rvv`：evidence role 为 `production-shaped diagnostic`，A/B boundary 为
`test helper`，timer boundary 为
`shape projection plus trilinear Eigen histogram accumulation plus shape descriptor copy`。当前仍没有正式
`log/evidence_registry.json`；本阶段结束时先用 manifest + artifact tracking 承担 freshness（新鲜度）
检查，并把 registry 接入保留为结构增强项。

## 板卡复跑预算和决策桶

预算为 5 次 repeated board run，iterations=10、warmup=2。decision bucket：
`positive` >= 1.10x 且无反向；`weak_positive` 为 median 1.03x-1.10x 且反向不超过 1/5；
`neutral` 为 0.97x-1.03x；`negative` < 0.97x；跨桶摇摆标 `unstable`。

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | test helper：scalar direct shape combined helper vs RVV staged shape combined helper |
| 当前决策问题 | RVV-vs-scalar diagnostic；判断 shape path 是否还有组合收益 |
| diagnostic 是否可外推到 production | no；仍缺 public dispatch、alignment transform、indices 和真实 `computeFeature` object state |
| comparison-boundary / baseline mismatch 风险 | yes；synthetic cloud 已是 transformed-like 输入，不覆盖 transform / indices / output object setup |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes，但需要用户授权 production integration loop；负向时默认先做 profile / no-production closeout 审计 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes |

## 继续 / 停止条件

默认继续到 C1-C8。合法停止条件：helper 无法在当前工具链编译、QEMU correctness 失败且无法修复、
板卡不可达、Evidence Doctor Error 无法解释或降级、或继续需要 production 授权。

如果本阶段为 `positive` 或稳定 `weak_positive`，下一步不是自动 production patch，而是准备 PI1
production integration plan（生产接入计划）并等待用户检查候选范围。若为 `neutral` / `negative`，下一步
默认进入 topic-local no-production closeout / profile 恢复条件审计，而不是继续堆叠更远离 production 的局部候选。
