# PPF Optimization Evidence

本文把 PPF 的候选族映射到代码、测试、板卡证据和最终 decision（决策）。长期 production 行为见
`doc-rvv/features/ppf-RVV.zh.md`；阶段流水见 `doc/phases/`。

| candidate family | 代码 / 文档入口 | 证据 | decision | 当前边界 |
| --- | --- | --- | --- | --- |
| same-chain scalar reference | `include/impl/ppf_reference.hpp`、`PPFReference.*` tests | Std/RVV correctness pass | adopted for diagnostics | 只作为 reference path（参考链路），不证明 production dispatch。 |
| SoA-staged pair-feature batch RVV | `include/impl/ppf_pair_batch_candidate.hpp`、`candidate_ppf_pair_feature_batch_rvv` | Phase 010 board speedup `0.80, 0.79, 0.79, 0.79, 0.81`，Doctor 有 degradation Error | rejected | 不接入 production；恢复条件是新的 direct-AoS 设计和 profile 证据。 |
| `alpha_m` closed-form formula | `include/impl/ppf_alpha_candidate.hpp`、`PPFAlphaM.ClosedFormMatchesEigenReference` | Phase 020 correctness pass | adopted for next diagnostic | 证明公式可对拍，不是 RVV 性能证据。 |
| test-only `alpha_m` batch RVV | `include/impl/ppf_alpha_candidate.hpp`、`candidate_ppf_alpha_m_batch_rvv` | Phase 030 board speedup `1.57, 1.57, 1.56, 1.57, 1.57` | diagnostic positive / superseded | 只授权有界 production probe，最终采纳看 production-public 数据。 |
| exact production `alpha_m` RVV | `features/include/pcl/features/impl/ppf.hpp`、`public_ppf_compute` | Phase 040 public speedup `1.35, 1.35, 1.37, 1.39, 1.38`，Doctor `0E/0W/2S` | adopted / superseded by wider gate | exact `PointXYZ + Normal + PPFSignature`。 |
| traits-gated point type expansion | `features/include/pcl/features/impl/ppf.hpp`、Phase 060 public cases | `PointXYZI + Normal` mean `1.384x`；`PointXYZ + PointNormal` mean `1.33x`；Doctor 均 `0E/0W/2S` | adopted production behavior | source xyz AoS traits + normal AoS + exact `PPFSignature`，float layout。 |
| evidence hardening | no code change yet | Doctor suggestions only | separate phase required | 归档增强，不是新优化。 |
| direct-AoS pair-feature revisit | no code change yet | Phase 010 SoA negative；无新 profile | deferred / separate phase required | 只有 profile 证明 `f1..f4` 仍是瓶颈且能避开 SoA staging 退化时恢复。 |

## 生产采用理由

当前采用的优化方式只 RVV 化 `alpha_m` 后段。`f1..f4` 继续使用 production 源码原本调用的
`pcl::computePairFeatures`，因此没有把 `features/src/ppf.cpp::computePPFPairFeature` 的语义混入
当前 public path。Phase 060 的生产接入后板卡数据证明，扩大后的 traits-gated public path 仍稳定快于
标量路径。

## 停止理由

当前 PPF topic 内没有新的高优先级未阻塞优化动作。继续微调 `alpha_m` staging 需要新的 profile 和
同边界 A/B；继续 pair-feature RVV 已被 Phase 010 的负向证据约束；PPFRGB / CPPF 是其它 caller topic，
不能由 PPF 数据外推。
