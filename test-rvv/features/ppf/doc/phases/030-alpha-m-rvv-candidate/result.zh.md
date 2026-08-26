# Phase 030 Result: Alpha M RVV Candidate

## 实际执行范围

本阶段按 `plan.zh.md` 执行 test-only `alpha_m` batch RVV candidate（测试专用批量
RVV 候选）。`f1..f4` 继续调用当前 production（生产源码）实际使用的
`pcl::computePairFeatures`，只把 `alpha_m` 的 closed-form formula（闭式公式）后段交给
RVV `atan2_RVV_f32m2` 和向量代数计算。

本阶段没有修改 `features/include/pcl/features/impl/ppf.hpp`，也没有接入 production
dispatch（生产分流）。

validated_scope：`PointXYZ + Normal`、float、AoS（结构数组）、sequential indices、ordered
`index_i x input_` row source（行来源）。

unvalidated_scope：泛型点类型、其它 normal point type、`Scalar=double`、非连续 / 乱序
indices、`PPFRGB` / `CPPF`、真实 production dispatch、完整 fallback matrix（回退矩阵）和
near `-x` normal 边界。

## 动作回填

| 动作 | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| RED: alpha_m RVV 输出测试 | done | 历史 RED：`PPFCandidate.AlphaMBatchRVVComputesProductionLikeOutput` 编译失败，缺少 `computePPFAlphaMBatchRVV`。 | 测试先于 helper，符合本阶段 TDD（测试驱动开发）记录。 |
| GREEN: 实现 RVV candidate | done | `include/impl/ppf_alpha_candidate.hpp`、`include/ppf.h`。 | RVV 构建下批量计算 alpha；非 RVV 构建回退 reference。 |
| correctness | done | `make -C test-rvv/features/ppf run_test_compare`。 | Std/RVV QEMU correctness 均通过 5 个测试。 |
| QEMU bench smoke | done | `make -C test-rvv/features/ppf BENCH_ARGS="--side 8 --index-count 4 --repeat 1 --iterations 1 --warmup 0 --case-filter candidate_ppf_alpha_m_batch_rvv" run_bench_rvv`。 | 只证明构建、case-filter 和日志形状；不作为性能证据。 |
| asm attribution | done | `make -C test-rvv/features/ppf dump_bench_rvv`；`bench_ppf_rvv.full.asm` 中 `computePPFAlphaMBatchRVV` 符号范围包含 `vle32.v`、`vfdiv.vv`、`vfsqrt.v`、`vmerge.vvm`、`vfnmsac.vv`、`vfmacc.vv` 和 `vse32.v`。 | RVV 指令能归属到本阶段 candidate wrapper 的符号范围；仍是 diagnostic asm evidence（诊断反汇编证据）。 |
| board repeated | done | `make -C test-rvv/features/ppf REPEATED_BOARD_RUNS=5 BENCH_ARGS="--side 28 --index-count 64 --repeat 8 --iterations 8 --warmup 2" board_repeated evidence_doctor_repeated`。 | `candidate_ppf_alpha_m_batch_rvv` 5-run speedup 为 `1.57, 1.57, 1.56, 1.57, 1.57`，decision bucket 为 positive。 |

## Board 证据摘要

| case | Std mean ms | RVV mean ms | speedup values | decision bucket | 证据角色 |
| --- | ---: | ---: | --- | --- | --- |
| `candidate_ppf_alpha_m_batch_rvv` | 415.5008 | 264.6650 | `1.57, 1.57, 1.56, 1.57, 1.57` | positive | component diagnostic（组件诊断） |
| `candidate_ppf_pair_feature_batch_rvv` | 416.0366 | 521.0482 | `0.80, 0.80, 0.79, 0.80, 0.81` | negative | historical diagnostic（历史诊断） |
| `public_ppf_compute` | 390.7082 | 391.0638 | `1.00, 1.00, 0.99, 1.00, 1.00` | neutral | public smoke baseline（公开入口基线 smoke） |

证据路径说明：Phase 030 当时使用默认 `log/board/repeated` output dir（输出目录）。该目录后来被
Phase 040 production-public run 覆盖，因此当前提交边界不把该目录里的文件当作 Phase 030 的证据。
本阶段 alpha candidate 的数值作为 historical diagnostic（历史诊断）保留在本文和 optimization matrix；
当前 production adoption（生产采纳）只依赖 Phase 040 / 060 接入后的 production-public summary。

## Evidence Doctor 结果

Evidence Doctor 当时输入为默认 repeated output dir。由于该目录后续被 Phase 040 覆盖，本节只保留
历史 Doctor 结论，不再把当前 `log/board/repeated` 文件解释为 Phase 030 的 Doctor 输出。

结果：`Errors=1, Warnings=1, Suggestions=10`。

| 严重级别 | finding | 处理动作 |
| --- | --- | --- |
| Error | `candidate_ppf_pair_feature_batch_rvv` 5/5 退化，B/A 为 `0.80, 0.80, 0.79, 0.80, 0.81`。 | 该 finding 属于 Phase 010 历史候选；继续保持 rejected / negative，不影响 Phase 030 alpha candidate 的 positive 桶，但阻止把 pair-feature batch RVV 写成 production-ready。 |
| Warning | `public_ppf_compute` 1/5 低于 1，median 约 1.00。 | public case 是当前 production baseline smoke；production 没有 RVV dispatch，因此该 warning 只说明公开入口 Std/RVV 构建差异接近噪声，不支持 production RVV 结论。 |
| Suggestions | alpha / pair-feature / reference / public case 缺少 taskset、governor、freq、temperature 和 binary hash；reference/public 还有 near-threshold 建议。 | 不阻塞本阶段 diagnostic decision；若进入 PI1 或扩大生产探针，应补 board 环境 metadata 和二进制身份。 |

## Diagnostic 到 Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `diagnostic` / `component-ablation`。 |
| A/B boundary | test helper vs test helper；`public_ppf_compute` 只是当前公开入口 smoke，不是 production RVV。 |
| 当前决策问题 | `RVV-vs-scalar` for `alpha_m` 后段，以及 implementation-shape（实现形态）是否值得进入生产接入计划。 |
| diagnostic 是否可外推到 production | partial / unknown。alpha 后段在当前 synthetic ordered row source 上稳定 positive，但真实 `PPFEstimation::computeFeature` 仍有泛型点类型、indices、fallback、异常输入和 dispatch 维护边界。 |
| comparison-boundary / baseline mismatch 风险 | yes。candidate 把 `f1..f4` 保持标量，只替换 alpha 后段；public smoke 没有 RVV dispatch，不能当 production direct。 |
| diagnostic weak / negative / neutral / unstable 时 bounded production probe | 当前 alpha candidate 是 positive，因此允许提出 bounded production probe；probe 仍需用户确认 PI1 范围。pair-feature batch RVV 是 negative，不进入该 probe。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes。PI1-PI5 需要真实 production patch、production direct correctness / fallback、asm attribution 和 board repeated；若存在多个 RVV family，还需要同一 production boundary 内的 RVV-vs-RVV A/B。 |

## EvidenceDecision

Phase 030 的 EvidenceDecision 是 `partial-production-candidate / wait-for-PI1-authorization`。

理由：

- correctness 已在 Std/RVV QEMU 两侧通过。
- QEMU bench smoke 只证明日志形状，没有被写成性能结论。
- 反汇编能把关键 RVV 指令归属到 `computePPFAlphaMBatchRVV` 符号范围。
- Milkv-Jupiter 5-run board repeated 在 diagnostic A/B 边界下稳定 positive。
- Evidence Doctor 没有针对 `candidate_ppf_alpha_m_batch_rvv` 给出 degradation Error；未闭合项是环境 metadata 和 binary identity 建议。
- 当前 production 源码没有 RVV dispatch，本阶段证据不能自动升级为 adopted production behavior（已采用生产行为）。

## Continue / Stop Decision

本阶段已闭合。默认下一动作不是继续改 test-only helper，而是在用户确认后进入
PI1 production integration plan（生产接入计划）。在确认前不修改 production。

`stop_condition_hit`：继续推进到 PI1 / PI2 会触碰 production integration loop（生产接入闭环）和
`features/include/pcl/features/impl/ppf.hpp`，需要用户显式确认生产探针范围。

`next_phase_default`：等待用户确认是否进入 PI1；建议范围为 `PointXYZ + Normal`、float、AoS、
ordered `index_i x input_` row source，只接 `alpha_m` 后段，不接 Phase 010 pair-feature batch RVV。
