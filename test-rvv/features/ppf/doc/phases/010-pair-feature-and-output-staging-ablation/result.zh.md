# Phase 010 Result: Pair Feature And Output Staging Ablation

## 阶段结论

Phase 010 已完成计划内 test-only RVV candidate（测试专用 RVV 候选）、correctness（正确性）、
QEMU smoke（QEMU 小型验证，只检查构建 / 日志形状）、asm attribution（反汇编归属）和
5-run board repeated benchmark（板卡重复性能测试）。结论是：

- `pair-feature batch RVV` 进入 `attempted / negative`。候选正确性成立，但板卡同边界 5-run 全部退化，B/A values 为 `0.80x, 0.79x, 0.79x, 0.79x, 0.81x`。
- 该结果只覆盖 diagnostic / component ablation（诊断 / 组件消融）边界，不能外推成 production direct（真实生产路径证据）。
- 当前不建议进入 production integration loop（生产接入闭环）。继续修改 production 会扩大权限，且当前 Evidence Doctor（证据体检）已经给出候选退化 Error。

## 执行动作回填

| 动作 | 状态 | 证据 | 结论 |
| --- | --- | --- | --- |
| 写 RED candidate 测试 | done | `src/test_ppf.cpp::PPFCandidate.PairFeatureBatchRVVComputesProductionLikeOutput` | Phase 010 候选测试已进入 correctness 集合。 |
| 实现 RVV candidate | done | `include/impl/ppf_pair_batch_candidate.hpp`、`include/ppf.h` | RVV 构建下批量计算 `f1..f4`；`alpha_m` 保持标量 reference；非 RVV 构建回退 reference。 |
| 跑 correctness | done | `make -C test-rvv/features/ppf run_test_compare` | Std/RVV 两侧 3 个测试通过；误差预算 `2e-3`。 |
| 增加 bench | done | `src/bench_ppf.cpp`、`Makefile` | bench 输出 dataset、iterations、warmup、case timing 和 checksum。 |
| QEMU bench smoke | done | `log/qemu/run_bench_rvv.log` | 只证明 RVV bench 可运行和日志形状，不作为性能证据。 |
| asm attribution | done | `build/asm/riscv/bench_ppf_rvv.asm` | `computePPFPairFeatureBatchRVV` 符号及 `vle32`、`vse32`、`vfmacc`、`vfsqrt`、`vfdiv`、`vmerge`、`vsetvli` 等 RVV 指令出现。 |
| board repeated | done | 历史 Phase 010 repeated run；数值保留在本文。 | 5-run 全部显示候选退化，decision bucket 为 `negative`。 |
| Evidence Doctor | done | 历史 Phase 010 Evidence Doctor；结论保留在本文。 | 结果为 `Errors=1, Warnings=1, Suggestions=8`；Error 是候选 5/5 退化。 |

## 板卡结果

本阶段使用 `REPEATED_BOARD_RUNS=5`，bench 参数为
`--side 28 --index-count 64 --repeat 8 --iterations 8 --warmup 2`。B/A 这里沿用 analyzer 的
Std/RVV speedup（`Std avg / RVV avg`），`>1` 表示 RVV 构建更快，`<1` 表示 RVV 构建更慢。

| case | run values | mean Std ms | mean RVV ms | decision bucket |
| --- | --- | --- | --- | --- |
| `component_ppf_reference` | `1.01, 1.01, 1.01, 1.00, 1.01` | `415.5076` | `412.4548` | neutral / near-threshold；只作为基线 sanity。 |
| `candidate_ppf_pair_feature_batch_rvv` | `0.80, 0.79, 0.79, 0.79, 0.81` | `415.9726` | `523.1078` | negative。 |
| `public_ppf_compute` | `1.00, 1.00, 1.00, 0.99, 1.00` | `390.7378` | `391.1456` | neutral；当前 production 没有 RVV dispatch，因此不是生产 RVV 证据。 |

run log 中 candidate RVV checksum 以默认 `std::cout` 精度显示为 `6.80557e+11`，Std/reference 侧显示为
`6.80558e+11`。这不是完整 checksum equality（校验和相等性）证据；当前 correctness 证据仍以
`run_test_compare` 的逐字段误差预算为准。后续若继续 bench harness，应提高 checksum 输出精度或把
checksum 数值写入 manifest，避免 reviewer 只能从 6 位科学计数法判断数值边界。

## Evidence Doctor 处理

Evidence Doctor 当时输入是默认 repeated output dir（重复输出目录）。该默认目录后来被 Phase 040
production-public run 覆盖；因此本阶段负向数值以本文和 matrix 的历史记录为准，不把当前
`log/board/repeated` 下的文件作为 Phase 010 的可提交证据路径。

| severity | signal | case | 处理 |
| --- | --- | --- | --- |
| Error | `ba_degradation_frequency` | `candidate_ppf_pair_feature_batch_rvv` | 解释为同边界 repeated board 负向证据；不进入 production probe。 |
| Warning | `ba_degradation_frequency` | `public_ppf_compute` | 该 case 只是当前 public baseline smoke，不能写成 production RVV 证据。 |
| Suggestion | `environment_metadata_missing`、`binary_identity_missing`、`near_threshold_ba` | 多个 case | 不阻塞 negative 结论，但降低环境复现解释能力；下一轮可补 taskset / governor / freq / temperature / binary hash。 |

Evidence Doctor 的 Error 不表示一定存在 correctness bug；它说明当前候选 5/5 退化，不能支持 production
performance（生产性能）或 production integration（生产接入）结论。

## Diagnostic 到 Production Mismatch Audit

| question | actual result |
| --- | --- |
| evidence role | diagnostic / component ablation。 |
| A/B boundary | test helper vs test helper；`public_ppf_compute` 只是 public baseline smoke。 |
| 当前决策问题 | RVV-vs-scalar 和 implementation-shape。 |
| diagnostic 是否可外推到 production | no。候选仅 RVV 化 `f1..f4`，`alpha_m` 仍标量，且 board repeated negative。 |
| comparison-boundary / baseline mismatch 风险 | yes。candidate 是混合边界；public smoke 当前没有 production RVV dispatch。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前 negative，不允许自动进入 bounded production probe。若用户后续强制生产探针，需要另写 PI1 计划并重新冻结范围。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes；当前没有 production boundary 内的新 family positive。 |

## 负向归因

本阶段只能给出受证据约束的归因假设。候选为 SoA staging（数组结构暂存）+ RVV pair-feature math
标量 `alpha_m` 混合形态；5-run 负向可能来自 staging 内存流量、额外 vector buffer、`atan2_RVV_f32m2`
近似成本、`vsetvli` / LMUL m2 开销、寄存器压力，或 `alpha_m` 标量后段仍是主成本。当前没有 profile
（剖析）或更细组件消融，不能把退化单因归咎于某一项。

## 结构和文档审计

| area | 状态 | 证据 / 说明 | 下一步 |
| --- | --- | --- | --- |
| test/bench source layout | adopted | 当前使用 `src/test_ppf.cpp`、`src/bench_ppf.cpp`，符合配置解析出的 `src/` 布局。 | none |
| aggregator and internal helpers | adopted | `include/ppf.h` 聚合入口，`include/impl/ppf_reference.hpp` 与 `include/impl/ppf_pair_batch_candidate.hpp` 拆分 reference / candidate。 | 若 Phase 020 增加 `alpha_m` 候选，再按职责新增 internal header。 |
| script and manifest | adopted | `script/generate_ppf_evidence_manifest.py` 生成 repeated board manifest，Evidence Doctor 可读。 | 下一轮补 checksum 数值和 binary hash 字段。 |
| target granularity | partial | 已有 `run_test_compare`、`run_bench_rvv`、`dump_bench_rvv`、`board_repeated`、`evidence_doctor_repeated`；细分 case-filter target 尚未拆。 | Phase 020 若继续新增候选，补 dedicated bench alias。 |
| topic-local docs | partial | README、evaluation、roadmap、phase index、matrix 和本 result 已存在；独立 testing overview / benchmark evidence / code map 暂未拆。 | 当前 negative 候选不要求立刻 closeout；若准备 ready-for-review，先开 structure-parity-doc-suite phase。 |
| production topic doc | not_applicable with evidence | 无 adopted production behavior、production patch 或 PI5 证据闭环。 | 不创建 `doc-rvv/features/ppf-RVV.zh.md`。 |
| evidence registry | partial | Phase 010 当时生成过 manifest / doctor，但默认 output dir 后续被 Phase 040 覆盖；topic-local registry target 尚未接入 Makefile。 | Phase 070 已把 current production evidence 切到 Phase 040 / 060 summary-only 路径；历史 negative 数值保留在本文。 |

## Continue / Stop Decision

Phase 010 本身闭合，`pair-feature batch RVV` 不进入 production。当前 topic 仍有授权范围内的未阻塞后续动作：

1. Phase 020 `alpha_m` formula audit：先审计 `alpha_m` 标量公式是否有可拆分、可稳定对拍的 RVV 片段；如果成本/复杂度不支持，再拒绝而不是直接写 kernel。
2. 若 Phase 020 发现 `alpha_m` 不建议 RVV 化，可转入 structure-parity-doc-suite closeout，把当前诊断证据链和 no-production 判断整理到 topic-local doc suite。
3. direct-AoS gather（直接结构数组离散加载）暂不作为默认下一步，因为 Phase 010 的 staged candidate 已经明显负向，且 direct-AoS 仍无法解决 `alpha_m` 主成本未知的问题。

`next_phase_default=020-alpha-m-formula-audit`。
