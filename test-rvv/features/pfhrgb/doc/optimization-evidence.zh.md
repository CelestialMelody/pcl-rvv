# PFHRGB Optimization Evidence

本文索引 PFHRGB candidate family（候选实现族）的证据和取舍。搜索空间和下一阶段排序见
`doc/optimization-roadmap.zh.md`；阶段流水见 `doc/phases/`。

## 当前结论摘要

| 状态 | candidate |
| --- | --- |
| adopted as test baseline | `pfhrgb-scalar-reference-scaffold` |
| adopted production behavior | `pfhrgb-production-direct-probe` exact-gated public path |
| positive production-shaped diagnostic | `pfhrgb-public-with-candidate-diagnostic`、`pfhrgb-staging-reuse` |
| attempted / negative-current-rerun | `pfhrgb-color-pair-batch-rvv` helper-only case |
| attempted / neutral-negative | `pfhrgb-component-baseline` |
| applicable | production long-term doc `doc-rvv/features/pfhrgb-RVV.zh.md` |

## 优化方式总表

| candidate family | 代码路径 | 测试路径 | bench / board evidence | decision | 边界 |
| --- | --- | --- | --- | --- | --- |
| `pfhrgb-scalar-reference-scaffold` | `include/impl/pfhrgb_reference.hpp` | `PFHRGBReference.ComputesPublicDescriptorLikeProductionHelper` | not_applicable | adopted as test baseline | 只作为 oracle（参考答案），不接 production。 |
| `pfhrgb-color-pair-batch-rvv` | `include/impl/pfhrgb_pair_batch_candidate.hpp` | `PFHRGBCandidate.PairBatchRVVComputesHistogramCloseToReference` | helper-only current median `0.98x`，Doctor Error | attempted / negative-current-rerun | 数值正确，但 helper-only 性能不稳定。 |
| `pfhrgb-public-with-candidate-diagnostic` | same candidate helper + public-shaped wrapper | `PFHRGBCandidate.PublicShapedCandidateComputesDescriptorsCloseToEstimator` | current median `1.21x` | production-shaped context | 证明 KSearch-shaped wrapper 下候选仍有价值，但采纳依据转为 production-public。 |
| `pfhrgb-staging-reuse` | `PairBatchWorkspace` + reusable wrapper | `PFHRGBCandidate.ReusablePublicShapedCandidateComputesDescriptorsCloseToEstimator` | current median `1.24x` | adopted shape input | 生产路径采用 workspace 复用思路，仍以真实 public case 作为收益证据。 |
| `pfhrgb-production-direct-probe` | `features/include/pcl/features/impl/pfhrgb.hpp` | `PFHRGBProduction.ExactPointTypePublicEntryMatchesScalarReference`、`PFHRGBProduction.NonExactSourcePointTypeKeepsScalarFallbackSemantics` | `public_pfhrgb_k` median `1.27x`，0/5 低于 1 | adopted production behavior | 只覆盖 exact `PointXYZRGBNormal` / `PFHRGBSignature250` / `nr_split=5` / KSearch public boundary。 |

## 标量路径与 RVV 路径差异

标量 production path（生产路径）在 `computePointPFHRGBSignature` 中遍历有向点对，调用
`computeRGBPairFeatures` 生成几何 tuple 和 RGB ratio，再按 250-bin histogram 顺序 scatter。当前 RVV
candidate 只接管 pair tuple 和 RGB ratio 的批量算术；histogram scatter 保持标量顺序，避免 bin conflict
（直方图桶冲突）和非结合累加风险。Phase 030 的 reusable workspace 复用 staging（分阶段暂存）和 tuple
buffers，减少 public-shaped 外层循环中的逐点分配。

## 结论边界

当前已采纳的生产方向是 exact `PointXYZRGBNormal` / `float` / `nr_split=5` / synthetic KSearch。
不能把接入后 production-public 结论外推到泛型 RGB traits、`Scalar=double`、radius search、OMP 或真实数据集。
继续优化的候选只剩两类：泛型点型扩展，或在 profile 证明 histogram scatter / staging 仍是瓶颈后再做新实现族。
当前证据中 helper-only case 为负向，因此没有值得立刻继续的同边界微优化。
