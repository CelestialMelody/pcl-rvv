# Phase 001 Plan: weighted-spfh-33 diagnostic candidate

## 阶段意图和边界

本阶段只在 `test-rvv/features/fpfh` 内实现 `weighted-spfh-33` 的 test-only RVV candidate
（仅测试 RVV 候选），用于判断 `weightPointSPFHSignature` 的 33-bin 加权累加是否存在清晰优化空间。
本阶段不修改 `features/include/pcl/features/impl/fpfh.hpp`，不创建 production dispatch（生产分流），
不覆盖泛型点类型、非连续 row source（行来源）或 OMP。

## Phase scope 与扩展队列

| item | scope |
| --- | --- |
| validated_scope | `PointNormal -> FPFHSignature33`、`Scalar=float`、11+11+11 bins、dense sequential SPFH rows、`dists[0] == 0` skip semantics。 |
| unvalidated_scope | arbitrary remapped row indices、KSearch public path direct adoption、search surface != input、indices subset、generic point layouts、`Scalar=double`、OMP。 |
| point_type_expansion_queue | 若进入 production probe，先补 PointXYZ + Normal、PointNormal exact、custom xyz-like traits 的 correctness / fallback。 |
| phase_closeout_boundary | 本阶段只能关闭 test-only dense-row component candidate；不能关闭 production adoption。 |

## 实现和测试动作

| action | artifact / command | completion |
| --- | --- | --- |
| A1 RED correctness test | 新增 `FPFHCandidate.DenseRowsRVVWeightsSPFHLikeProductionHelper` | 先看到缺少 candidate API 的编译失败。 |
| A2 GREEN test-only candidate | `include/impl/fpfh_weighted_candidate.hpp`, `include/fpfh.h` | Std 构建回退标量 reference；RVV 构建仅在 dense sequential 33-bin 边界走 RVV kernel。 |
| A3 bench label | `src/bench_fpfh.cpp` 的 `candidate_weighted_spfh_dense_rows` | 独立 label 不污染 Phase 000 baseline case。 |
| A4 QEMU correctness | `make -C test-rvv/features/fpfh run_test_compare` | Std/RVV 通过 5 个 tests。 |
| A5 asm attribution | `make -C test-rvv/features/fpfh dump_bench_rvv` + `addr2line` | 关键 RVV 指令归到 candidate header。 |
| A6 board repeated + Doctor | `board_repeated` 写入 `log/board/phase001-weighted-candidate/repeated`; `evidence_doctor_repeated` 指向同目录 | candidate label 稳定正向，Doctor 异常解释清楚。 |

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `component-candidate` diagnostic；不是 production evidence。 |
| A/B boundary | Std build fallback scalar reference vs RVV build test-only dense-row helper。 |
| 当前决策问题 | 判断 33-bin weighted SPFH component 是否值得进入 PI1 production integration plan。 |
| diagnostic 是否可外推到 production | 只能外推到“dense sequential row”组件实现空间。真实 production 会把 KSearch 邻居映射到 SPFH row，row indices 可能非连续。 |
| comparison-boundary / baseline mismatch 风险 | 高。candidate 的 scalar baseline 是 test-only reference，不是当前 production helper；public path 还没有调用 candidate。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 若 candidate 稳定正向但 public path 仍未变快，可以允许 PI1 计划，但必须先冻结 row-source gate 和 fallback。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-scalar / RVV-vs-RVV A/B | 需要。Phase 001 positive 只触发用户检查点，不自动采纳。 |

## 板卡复跑预算和决策桶

本阶段固定 5-run repeated，不追加复跑。`candidate_weighted_spfh_dense_rows` 若 5/5 都高于 1.10x，
且 Doctor 对该 label 无 error，则判为 `strong_diagnostic_positive`。若旧 baseline case 报错，
只影响旧 case 的 production 解释，不覆盖 candidate label 的组件候选结论。
