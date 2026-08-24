# 050 production detail ablation result

## 当前结论

本阶段完成 DON detail ablation（细节消融）闭环。结果没有产生可直接恢复 production probe（生产探针）的新实现族：

- `finite-only-no-mask`：median `0.721x`，`B/A < 1 = 5/5`，稳定负向。finite mask（有限值掩码）不是主要退化根因；去掉 mask 后 RVV 仍明显慢。
- `no-sqrt-store-zero-curvature`：median `1.125x`，`B/A < 1 = 1/5`，弱正向但有 Warning。它去掉了 production 语义中的 curvature `sqrt`，只能说明 `vfsqrt` / curvature 计算是成本中心，不能作为 production 候选。
- `normal-only-no-curvature-store`：median `1.025x`，`B/A < 1 = 1/5`，near-threshold（接近阈值）且有 Suggestion。只写 normal 三分量也不足以形成稳定收益。

EvidenceDecision（证据决策）：`stop_no_worthwhile_production_direction`。在保持当前 DON production 语义（normal 差、非有限置零、curvature 严格 `sqrt` 写回）的前提下，没有值得继续推进到 production 的 RVV 优化方向。

## 实际执行范围

| 计划动作 | 状态 | 证据路径 | 结论 |
| --- | --- | --- | --- |
| phase plan | done | `doc/phases/050-production-detail-ablation/plan.zh.md` | 已先于实现冻结 diagnostic 边界 |
| detail-ablation helpers | done | `include/impl/don_core.hpp` | 新增 finite-only no-mask、no-sqrt zero-curvature、normal-only 三类 test-only helper |
| bench cases | done | `src/bench_don.cpp` | `--case-filter don_ablate_all` 输出三类消融 case |
| repeated board target | done | `Makefile` | 新增 `run_board_don_ablation_repeated` 及 summary / doctor / registry target |
| QEMU correctness | done | `make -C test-rvv/features/don run_test_compare` | Std/RVV build 各 5 个 gtest 通过 |
| QEMU smoke / log shape | done | `run_bench_std` / `run_bench_rvv` / `analyze_bench_compare`，`BENCH_ARGS='--points 4096 --iterations 1 --warmup-iterations 1 --case-filter don_ablate_all'` | 三个 case 均可运行并被 compare script 解析；QEMU timing 不作为性能证据 |
| asm attribution | done | `make -C test-rvv/features/don dump_bench_rvv`，`build/asm/riscv/bench_don_rvv.asm` | filtered asm 出现 `vlse32/vfsub/vmerge/vfsqrt/vsse32` 等 RVV 指令 |
| board repeated | done | `make -C test-rvv/features/don run_board_don_ablation_repeated` | 5-run summary 已生成 |
| Evidence Doctor / registry | done | `log/board/repeated_phase050_detail_ablation/evidence_doctor.md`，`log/evidence_registry.json` | Doctor: Errors=1, Warnings=3, Suggestions=1；registry fresh |

## 证据链

| case | role | median | min | max | B/A < 1 | Doctor | 解释 |
| --- | --- | ---: | ---: | ---: | ---: | --- | --- |
| `don_ablate_finite_only_no_mask` | diagnostic | `0.721x` | `0.713x` | `0.768x` | `5/5` | Error: `ba_degradation_frequency` | 去掉 finite mask 后仍稳定退化，说明当前 AoS strided load/store 与计算组织本身不够强 |
| `don_ablate_no_sqrt_store_zero_curvature` | diagnostic | `1.125x` | `0.984x` | `1.152x` | `1/5` | Warnings: degradation + long-tail | 去掉 `sqrt` 后有弱正向，说明 curvature `sqrt` 是成本中心；但该 case 写零 curvature，破坏 production 语义 |
| `don_ablate_normal_only` | diagnostic | `1.025x` | `0.975x` | `1.082x` | `1/5` | Warning + near-threshold suggestion | 只写三分量收益接近阈值，不能支撑 production 维护成本 |

QEMU smoke 的 Std/RVV checksum 在三个 case 中均一致；这只证明同一消融 case 的两侧输出合同一致，不证明消融 case 等于 production 语义。

## Diagnostic-to-Production Mismatch Audit 回填

| question | result |
| --- | --- |
| evidence role | `diagnostic` |
| A/B boundary | `test helper detail ablation` |
| 当前决策问题 | `implementation-shape`：是否存在值得恢复 production probe 的新实现族 |
| diagnostic 是否可外推到 production | 不能直接外推。唯一弱正向 case 去掉了 production 的 curvature `sqrt` 语义 |
| comparison-boundary / baseline mismatch 风险 | 已由 phase 030 证明存在：helper-only positive 不能直接外推 public entry |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不允许。当前没有保持 production 语义且稳定 positive 的 detail candidate |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 是；本阶段没有进入 clean adoption |

## Optimization matrix 更新

| candidate family | row source policy | point type / Scalar / layout | scope and entry | board evidence | Evidence Doctor | decision | next action |
| --- | --- | --- | --- | --- | --- | --- | --- |
| finite-only no-mask | ordered normal cloud, finite input only | `pcl::Normal` / float / AoS | test helper detail ablation | median `0.721x`, `B/A < 1 = 5/5` | Errors=1 | `rejected` | 不继续 |
| no-sqrt zero-curvature | ordered normal cloud | `pcl::Normal` / float / AoS | test helper detail ablation | median `1.125x`, `B/A < 1 = 1/5` | Warnings=2 | `semantic_mismatch_not_production_candidate` | 只有用户明确允许近似 / 改变 curvature 语义时才另开 topic |
| normal-only | ordered normal cloud | `pcl::Normal` / float / AoS | test helper detail ablation | median `1.025x`, `B/A < 1 = 1/5` | Warning=1, Suggestion=1 | `rejected_near_threshold` | 不继续 |

## 停止条件

本阶段命中停止条件：没有保持当前 production 语义且值得继续推进的 RVV production 方向。后续可恢复条件只有两类：

- 用户明确授权改变或近似 curvature 语义，例如探索 approximate sqrt（近似平方根）并接受新的误差预算。
- 未来工具链 / RVV math helper 提供严格或可证明误差预算的 faster sqrt helper，并先通过同边界 correctness、asm、board repeated 和 Evidence Doctor。

在上述条件出现前，DON topic 应暂停为 no-production，features 队列可推进下一主题。
