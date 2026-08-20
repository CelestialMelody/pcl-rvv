# Phase 060 active-z tail / table-lookup compression A/B plan

## 阶段意图和边界

本阶段只优化 `surface/include/pcl/surface/impl/marching_cubes.hpp` 中已采用的
`getActiveVoxelsZRVV()` active-cell prepass（活跃体素预扫描）。目标是减少 z-lane
尾段的 staging（暂存）内存流量：把非 finite lane 在 RVV 端折叠成 `cubeindex=0`，
尾段只保留 `cube_indices` 一个缓冲并继续用标量 `edgeTable` 判断 active cell。

本阶段不改变公开 API，不扩大当前 production gate，不改 `createSurface()` 三角输出，不恢复
edge interpolation RVV，不覆盖 Hoppe/RBF 真实输入分布、`Scalar=double` 或新的 row source。

## 当前状态清单

| item | current state |
| --- | --- |
| production gate | `pcl::rvv::RVVXYZAoSFloatLayout<PointNT>::value` |
| adopted implementation | RVV 扫描 z-lane cube index 和 finite flag，active z 回到标量 `getNeighborList1D()` / `createSurface()` |
| fallback | 非 `__RVV10__` 或非 gate 点类型走 `reconstructSurfaceStd()` |
| correctness | `make -C test-rvv/surface/marching_cubes run_test_compare` 已在 Phase 050 通过 Std/RVV 各 6 tests |
| asm | `make -C test-rvv/surface/marching_cubes dump_bench_rvv` 已刷新 |
| board anchor | generic `PointXYZ/PointXYZI/PointXYZRGB/PointXYZRGBA` repeated 均为 positive，Evidence Doctor 均 0/0/0 |
| unblocked next action | active-z tail / table-lookup compression A/B |

## 假设与候选族

| candidate | hypothesis | risk |
| --- | --- | --- |
| finite-collapse-single-buffer | 删除 `finite_flags` 落内存，把非 finite lane 的 cube index merge 成 0；标量尾段只查 `edgeTable[cube_indices[lane]]` | 收益可能很小，因为 public path 仍受 `createSurface()` 输出主导 |
| full vector table / compress | 用 RVV gather `edgeTable` 并 `vcompress` 输出 active z | 实现形态更复杂，table gather 和压缩开销可能吞掉收益；本阶段只在第一候选正向或 profile 指向尾段时恢复 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| finite-collapse-single-buffer | dense-grid-cell | `RVVXYZAoSFloatLayout` / `float` / AoS xyz | `performReconstruction()` public path, active-cell prepass detail | `run_test_compare` Std/RVV 6 tests；fallback case 不变 | public Std/RVV smoke；同边界 RVV-baseline/RVV-candidate A/B | 3-run bounded repeated，优先 `mc_prod_xyz_64`，必要时扩到 generic quartet | `dump_bench_rvv` 检查 RVV scan 仍存在且少一组 finite flag store | Evidence Doctor Errors 必须为 0；Warning/Suggestion 要解释 | planned |

## 实现和测试动作

1. 保存当前 adopted RVV binary 作为 RVV baseline，或在改动前运行板卡 baseline 日志，作为同边界 RVV-vs-RVV A/B 的左侧。
2. 修改 `getActiveVoxelsZRVV()`：保留 finite mask，但用 `vmerge` 把非 finite lane 的 `cube` 置 0；删除 `finite_flags` 缓冲和 store。
3. 跑 QEMU correctness：`make -C test-rvv/surface/marching_cubes run_test_compare`。
4. 刷新反汇编：`make -C test-rvv/surface/marching_cubes dump_bench_rvv`，确认 active-cell RVV 指令仍归属当前路径。
5. 跑板卡有界 A/B：先 `mc_prod_xyz_64` 3-run；若 bucket 为 positive 或 weak-positive，再扩 generic quartet；若 neutral / negative 且 correctness 通过，候选标为 attempted，不接入。
6. 生成 summary 和 Evidence Doctor；若现有 summary 脚本不足以表达 RVV-vs-RVV，同步补 topic-local summary manifest。
7. 更新 result、matrix、roadmap、evaluation、长期 `doc-rvv` 和 Handoff。

## Evidence Doctor 和 registry 规则

- 证据策略：summary-only；raw board logs 不默认提交。
- RVV-vs-RVV A/B 必须在 manifest 中写清 baseline 为 Phase 050 adopted RVV，candidate 为 Phase 060 finite-collapse RVV。
- public Std/RVV 只作为 regression smoke，不作为 family selection 采纳依据。
- Evidence Doctor `Errors > 0` 时不得关闭本阶段；Warnings/Suggestions 必须解释或降级。

## 阶段完成条件

| result | condition |
| --- | --- |
| adopted | correctness 通过，asm 归属成立，RVV-vs-RVV repeated 为 positive 且 doctor 0 Error，并且收益足以抵消代码复杂度 |
| attempted | correctness 通过但 RVV-vs-RVV neutral / weak / negative，或收益太小不值得保留 |
| rejected | correctness 失败、语义不等价或 doctor Error 无法修复 |
| blocked | 板卡 / 工具不可用，或 dirty isolation 无法安全归属 |

## 板卡复跑预算和决策桶

- baseline / candidate 各 3 run 起步，`--case-filter mc_prod_xyz_64`，默认 12 iterations、2 warmup。
- `median >= 1.03` 且 `min >= 0.99`：weak-positive，可考虑扩到四个 generic 点型。
- `median >= 1.10` 且 `min >= 1.03`：positive，可作为接入候选。
- 全部在 `0.99 ~ 1.03`：neutral，默认不保留新 family，除非代码更简单且不降低证据。
- `median < 0.99` 或 checksum mismatch：negative / rejected。
- 预算耗尽仍摇摆则标为 unstable，交给 reviewer / 用户判断。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-detail for RVV-vs-RVV；production-public Std/RVV 只作 regression smoke |
| A/B boundary | `performReconstruction()` public wrapper，baseline/candidate 都命中同一 `reconstructSurfaceRVV()` family |
| 当前决策问题 | RVV-family-selection |
| diagnostic 是否可外推到 production | yes for synthetic public path；不能外推到 Hoppe/RBF 真实输入分布 |
| comparison-boundary / baseline mismatch 风险 | RVV-vs-RVV 必须同一板卡、同一 case、同一 iterations；Std/RVV 不用于 family selection |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段已经在 production detail 内探测；弱或中性默认不扩大实现 |
| clean adoption 是否需要 RVV-vs-RVV detail A/B | yes，本阶段必须补齐 |

## 文档更新清单

- `result.zh.md`：记录 QEMU、asm、board、doctor、decision bucket 和继续 / 停止条件。
- `optimization-matrix.zh.md`：新增 finite-collapse-single-buffer 行。
- `optimization-roadmap.zh.md`：关闭或重排 active-z tail candidate；记录 full vector table / compress 是否继续。
- `marching_cubes-evaluation.zh.md` 和 `doc-rvv/surface/marching_cubes-RVV.zh.md`：只有候选保留或证据改变 production truth 时同步。
- `current-handoff.zh.md`：更新 phase loop 状态和默认恢复入口。

## 继续 / 停止条件

若 finite-collapse 关闭后仍有同边界、低风险、未阻塞的 active-z table gather / compress 候选，并且本阶段证据显示 prepass tail 仍值得优化，继续创建 Phase 070。若收益 neutral / negative 或收益无法和代码复杂度匹配，则本 topic 停在 adopted generic production，后续只保留 Hoppe/RBF 输入分布、`Scalar=double` 或 triangle emission RVV 的单独 phase 条件。
