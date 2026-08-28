# Phase 050 Plan: full-chain-timing-and-production-eligibility

## 阶段意图和边界

本阶段把 Phase 040 的 linearized map copy（线性化图拷贝）正向信号放回 production-shaped full-chain split（生产形态分阶段计时）中审计。目标不是直接修改 `recognition/src/linemod.cpp`，而是回答：如果只把 `EnergyMaps -> LinearizedMaps` 这一段替换为 RVV stride-load copy（跨步加载拷贝），在包含 energy generation、8-bin linearization、score accumulation 和 threshold scan 的测试链路中是否仍有足够收益，值得进入 PI1 production integration plan（生产接入计划）。

本阶段只修改 `test-rvv/recognition/linemod_template_scoring` 下的测试资产、bench、manifest wrapper 和 topic-local 文档；production 源码保持 untouched（未触碰）。本阶段不证明真实 `LINEMOD::matchTemplates` public entry（公开入口）已分流，也不覆盖 `LINEMOD_USE_SEPARATE_ENERGY_MAPS`、NMS（非极大值抑制）、averaged detection（邻域加权检测）或 semi-scale（半尺度不变）路径。

## 当前状态清单

| item | current state |
| --- | --- |
| negative subkernels | Phase 010 threshold scan median `0.601x`；Phase 020 accumulation-only median `0.862x`；Phase 030 energy map median `0.954x` |
| positive subkernel | Phase 040 linearized map copy median `2.160x`，0/5 退化，Evidence Doctor `Errors=0 / Warnings=0 / Suggestions=0` |
| production state | 无 production patch；Phase 040 只是 `partial-production-candidate` |
| board availability | 当前 prompt 明确板卡可用；本阶段需要 5-run board repeated 和 Evidence Doctor |

## Validated / Unvalidated Scope

| scope type | content |
| --- | --- |
| validated_scope | synthetic `QuantizedMap` byte stream -> 8 energy maps -> 8 bins × 64 linearized maps；score accumulation 从 linearized maps 中选取 `nr_maps` 个连续 byte maps；threshold scan 保持标量顺序语义 |
| unvalidated_scope | 真实 modality 对象、template feature/bin 分布、`LinearizedMaps` 类内 aligned allocation、`getOffsetMap(feature.x,feature.y)` 偏移、NMS、averaging、semi-scale、public dispatch |
| point_type_expansion_queue | not applicable；本阶段输入是图像字节 map，不涉及 PCL point type（点类型）或 `Scalar` |
| phase_closeout_boundary | 只能决定是否建议进入 PI1 或继续 full-chain / production-direct 诊断；不能 clean-adopt production |

## 候选族和对比边界

| candidate | A/B boundary | changed part | unchanged part | expected signal |
| --- | --- | --- | --- | --- |
| full-chain split with RVV linearized copy only | test helper / production-shaped diagnostic | RVV build 使用 `linearizeEnergyMapsRVV`；Std build 使用 `linearizeEnergyMapsStd` | energy generation、score accumulation、score scan 在两侧都使用标量 helper，避免把已知负向 RVV 子核混入 | stage-level copy 仍 positive；full-chain total 若 positive / weak-positive，进入 PI1 eligibility audit |

## 实现和测试动作

| action | artifact | expected evidence | completion |
| --- | --- | --- | --- |
| RED：新增 multi-bin linearized copy correctness | `src/test_linemod_template_scoring.cpp` | helper 缺失导致编译失败 | failure observed before helper edit |
| GREEN：实现 multi-bin helper | `include/impl/linemod_template_scoring_candidates.hpp` | 8 bin × 64 maps layout 与标量 reference 一致 | `run_test_compare` pass |
| 扩展 bench harness | `src/bench_linemod_template_scoring.cpp` | 输出 full-chain split stage labels、total label、`linearized_checksum` | parseable logs |
| 增加 Phase 050 Make targets | `Makefile` | collect / manifest / doctor / registry / freshness target 指向 Phase 050 证据路径 | target exists and runs |
| 扩展 manifest wrapper | `script/generate_linemod_template_scoring_evidence_manifest.py` | 识别 Phase 050 labels 和边界 metadata | summary + manifest |
| 反汇编检查 | `make dump_full_chain_split_bench_rvv` | 专用二进制包含 linearized copy 的 `vlse8.v` / `vse8.v` | asm summary |
| 板卡 repeated | 5-run budget | full-chain split 和 stage labels 的 decision bucket | summary + doctor |

## Evidence Doctor 和 Registry

本阶段 summary 主归属为 `doc/phases/050-full-chain-timing-and-production-eligibility/full-chain-split-repeated-summary.md`，manifest 为 `full-chain-split-repeated-evidence-manifest.json`，Evidence Doctor 输出为 `full-chain-split-repeated-evidence-doctor.md/json`。registry run label 使用 `phase050-full-chain-split-repeated-20260828`。

## 板卡复跑预算和决策桶

默认 5-run。`median speedup >= 1.05` 为 `positive`，`1.00-1.05` 为 `weak_positive`，`0.97-1.00` 为 `neutral`，`<0.97` 为 `negative`；若 full-chain total 方向跨越 1 或 Evidence Doctor 指出影响决策桶的长尾，最多再做一次同边界确认复跑。若 5-run 已稳定落入一个桶，不追加复跑。

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `production_shaped_diagnostic` |
| A/B boundary | `test_helper` |
| 当前决策问题 | `implementation-shape` and `RVV-vs-scalar` for linearized-copy-only full-chain split |
| diagnostic 是否可外推到 production | `unknown`；full-chain split 仍用 synthetic 数据和 helper，不覆盖真实 public entry、object allocation、template feature offset 或 detection sink |
| comparison-boundary / baseline mismatch 风险 | `yes`；A/B 只改变 copy helper，但整体链路不是 production direct |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | `yes`；若 full-chain total 不正向，则只记录 partial candidate，不进入 PI1；若正向且 doctor clean，可进入 PI1 计划，但 PI2 production patch 仍需用户或 PI1 gate 授权边界 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 若后续存在多个 copy family，需要；当前只比较 scalar copy 与 RVV stride-copy |

## 继续 / 停止条件

若 Phase 050 full-chain total 正向且 Evidence Doctor clean，默认下一步是 PI1 production integration plan，冻结真实 production helper、dispatch、fallback、step/layout gate 和 production direct tests；但仍不在本阶段修改 production。若 full-chain total 中性或负向，默认继续 profile / alternative copy family 或把 Phase 040 保留为 isolated positive，不接 production。只有命中工具失败、证据矛盾、dirty isolation 不安全、用户授权边界或 roadmap 无未阻塞动作时才停止。

## 文档更新清单

本阶段完成后更新本 `result.zh.md`、`doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md`、`doc/optimization-roadmap.zh.md`、`doc/linemod_template_scoring-evaluation.zh.md`、`README.zh.md`、筛选队列和 Handoff。没有 adopted production behavior 或 PI5 生产证据闭环，因此不创建 `doc-rvv/recognition/linemod_template_scoring-RVV.zh.md`。
