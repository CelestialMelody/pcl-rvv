# Phase 060 结果：active-z tail / table-lookup compression A/B

## 实际执行范围

本阶段按计划只尝试 `getActiveVoxelsZRVV()` 内部的 finite-collapse single-buffer
候选：删除 `finite_flags` staging（暂存）数组，把非 finite lane 在 RVV 端合并为
`cubeindex=0`，标量尾段只查 `edgeTable[cube_indices[lane]]`。

候选只用于同边界 A/B；最终没有保留到 production。当前源码已经回到 Phase 050 adopted
generic production truth：`cube_indices + finite_flags` 双缓冲 active-cell prepass。

## 动作回填

| action | status | evidence | result |
| --- | --- | --- | --- |
| Phase plan | done | `plan.zh.md` | 修改 production 前已写明范围、采纳条件、RVV-vs-RVV A/B 和回退条件。 |
| baseline board capture | done | `log/board/phase060_rvv_ab/summary.md` | Phase 050 adopted RVV baseline：`23.1890 / 23.1497 / 23.2548 ms/iter`；raw run logs 只作为本地采集来源，不默认提交。 |
| finite-collapse candidate | attempted / reverted | `surface/include/pcl/surface/impl/marching_cubes.hpp` 临时 patch | 候选删除一组 `vse32` finite flag store；因收益 neutral，已按计划回退。 |
| QEMU correctness on candidate | done | `make -C test-rvv/surface/marching_cubes run_test_compare` | Std/RVV 各 6 tests passed，包含 NaN skip、generic point types 和 fallback。 |
| candidate asm | done | `make -C test-rvv/surface/marching_cubes clean_bench_rvv && make -C test-rvv/surface/marching_cubes dump_bench_rvv` | 强制重建后确认 candidate 版本的 `getActiveVoxelsZRVV()` 只有一处 `vse32.v` staging store。 |
| candidate board A/B | done / neutral | `log/board/phase060_rvv_ab/summary.md` | RVV-vs-RVV median `1.007x`，values `1.002x, 1.007x, 1.012x`，bucket `neutral`；raw run logs 只作为本地采集来源，不默认提交。 |
| Evidence Doctor | done | `log/board/phase060_rvv_ab/evidence_doctor.md` | `Errors=0, Warnings=1, Suggestions=1`；warning 是 3-run low count，suggestion 是 near-threshold。 |
| production revert verification | done | `make -C test-rvv/surface/marching_cubes run_test_compare`、`dump_bench_rvv` | 回到 adopted 源码后 Std/RVV 各 6 tests passed；asm 刷新为双 staging store 形态。 |

## RVV-vs-RVV board 结果

| case | baseline RVV ms | candidate RVV ms | candidate speedup |
| --- | ---: | ---: | ---: |
| pair1 / `mc_prod_xyz_64` | `23.1890` | `23.1528` | `1.002x` |
| pair2 / `mc_prod_xyz_64` | `23.1497` | `22.9928` | `1.007x` |
| pair3 / `mc_prod_xyz_64` | `23.2548` | `22.9812` | `1.012x` |

汇总：median `1.007x`，min `1.002x`，max `1.012x`，checksum match。按本阶段计划，
该结果属于 `neutral`，不能作为新 RVV family 的 clean adoption（干净采纳）依据。

## Evidence Doctor 处理

- `Errors=0`：同边界 RVV-vs-RVV manifest 已通过严格 A/B 检查。
- `Warnings=1 / low_run_count`：本阶段预算是 3-run 初筛；该 warning 限制结论强度，不影响
  neutral / no-adoption 决策。
- `Suggestions=1 / near_threshold_ba`：median 距离 1.0 太近，不能把 `1.007x` 写成稳定收益。
  处理动作是回退候选，不扩大到四个 generic 点型，也不进入 5-run production adoption。

## diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | production-detail RVV-family-selection |
| A/B boundary | 同一 `performReconstruction()` public wrapper；baseline 是 Phase 050 adopted RVV，candidate 是 finite-collapse RVV |
| 当前决策问题 | 新 RVV family 是否优于已采用 active-cell prepass |
| diagnostic 是否可外推到 production | 可用于 synthetic public boundary 的 family selection；不能外推 Hoppe/RBF 真实输入分布 |
| comparison-boundary / baseline mismatch 风险 | 已用 RVV-vs-RVV 同 case / 同 iterations 控制；Std/RVV 日志只作 regression smoke |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段已在 production detail 内探测；neutral 后不继续扩大 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 已补齐；结果不支持采纳 |

## 决策

`finite-collapse-single-buffer` 结论为 `attempted / neutral / not adopted`。

理由：

- 正确性通过，说明语义可行；
- asm 证明能减少一组 finite flag staging store；
- 但同边界 RVV-vs-RVV 只有 `1.007x` median，低于本阶段 weak-positive 阈值；
- Doctor 明确提示 near-threshold，继续扩大 runs 也只是在噪声附近消耗预算；
- 生产源码保持 Phase 050 已采纳形态，避免为了不可证明收益改变 current truth。

## Continue / Stop 决策

- `continue_stop_decision`：当前 topic 回到 `adopted generic production`，Phase 060 关闭为 attempted。
- `stop_condition_hit`：同边界 active-z tail 候选已闭合，roadmap / matrix 中没有当前授权范围内、
  高优先级且未阻塞的下一优化动作。
- `next_phase_default`：ready_for_review；若后续恢复，优先条件不是继续微调 finite-collapse，而是先有
  profile 或 5-run/20-run evidence 表明 active-z tail 仍是瓶颈。

后续只保留带恢复条件的方向：

- full RVV table lookup / `vcompress`：需要 profile 证明 `edgeTable` 标量尾段仍显著，且能接受更复杂实现。
- Hoppe/RBF 真实输入分布：需要单独 phase 或子 topic，因为会扩大到子类 voxelization 分布。
- triangle emission RVV：需要新的输出策略和 push_back / polygon 构造边界，不属于本阶段继续动作。
