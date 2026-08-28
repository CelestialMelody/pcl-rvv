# Phase 010 Result: identity-index strided load

## 执行范围

本阶段验证 identity indices（恒等索引，`indices[i] == i`）能否用 strided load（跨步加载）
替代 indexed gather（离散加载）。实际 production（生产源码）接入范围已收窄为
`selectWithinDistanceRVV` 和 `countWithinDistanceRVV`；`getDistancesToModelRVV` 保持 gather-only
（只使用离散加载）实现。

| 维度 | 已验证范围 | 未验证范围 |
| --- | --- | --- |
| 入口 | `selectWithinDistance`、`countWithinDistance`、`getDistancesToModel` 的公开入口 correctness；Phase 010 只采纳 select/count load strategy。 | 其它 SAC 模型、normal-plane、sphere/circle、SAC 后处理。 |
| row source | direct indexed `indices_`；identity 与 shuffled 两种 bench mode。 | correspondence、双索引、其它 row source policy。 |
| 点型 / layout | `PointXYZ` 板卡性能；`PointXYZI` correctness 已由 Phase 000/010 测试覆盖。 | 更多 registered float xyz AoS 点型及非 AoS fallback runtime 证据。 |
| `Scalar` | `Eigen::VectorXf` float 系数，`double threshold` 转 float 比较。 | `Scalar=double` 或其它系数类型。 |
| 规模 | 板卡 repeated 使用 65536 点、200 iterations、5 warmup。 | 小规模阈值、其它数据分布和更大 run count。 |

## 计划动作回填

| action | 状态 | 命令 / 路径 | 结论 |
| --- | --- | --- | --- |
| bench mode split | done | `src/bench_sac_model_plane.cpp` | bench 第三个参数支持 `identity` / `shuffled`，输出 dataset label 和 warmup。 |
| correctness test | done | `make -C test-rvv/sample_consensus/sac_model_plane run_test_compare` | Phase 010 时 Std/RVV 两个构建各 4 个 gtest 通过；Phase 025 后当前套件已扩展为 7 个 gtest。 |
| production fast path | done with narrowing | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_plane.hpp` | select/count 使用 `sacModelPlaneRVVLoadXYZ(..., indices_may_be_identity=true, ...)`；getDistances 改回 `false`，保持 gather。 |
| asm attribution | done | `make -B -C test-rvv/sample_consensus/sac_model_plane dump_bench_rvv` | select/count 有 `vid.v`、`vmsne`、`vlsseg3e32` 和 `vluxei32`；getDistances 只有 `vluxei32`。 |
| board rerun | done | `log/board/phase-010/identity-stride-select-count/repeated/summary.md`、`log/board/phase-010/shuffled-stride-select-count/repeated/summary.md` | select/count 的 identity 收益提高且 shuffled 未退化；getDistances 不采纳 identity branch。 |
| Evidence Doctor | done with suggestions | `doc/phases/010-identity-index-strided-load/evidence-doctor.md` | adopted entries 无 Error/Warning；metadata Suggestions 不阻塞。 |

## Board performance

Phase 010 使用 production-public（真实公开入口）Std/RVV 对比，目标硬件为板卡，输入为
`PointXYZ`、65536 点、200 iterations、5 warmup。

| index mode | public entry | values | median | min | max | decision bucket |
| --- | --- | --- | ---: | ---: | ---: | --- |
| identity | `selectWithinDistance` | 3.3449x, 3.3724x, 3.3896x, 3.4120x, 3.4529x | 3.3896x | 3.3449x | 3.4529x | positive |
| identity | `countWithinDistance` | 2.1933x, 2.1822x, 2.1975x, 2.1969x, 2.2002x | 2.1969x | 2.1822x | 2.2002x | positive |
| identity | `getDistancesToModel` | 2.0321x, 2.1547x, 2.3645x, 2.6258x, 2.1353x | 2.1547x | 2.0321x | 2.6258x | not adopted for Phase 010 |
| shuffled | `selectWithinDistance` | 3.1471x, 3.2180x, 3.1896x, 3.1895x, 3.1590x | 3.1895x | 3.1471x | 3.2180x | positive |
| shuffled | `countWithinDistance` | 1.6663x, 1.6727x, 1.6761x, 1.6618x, 1.6649x | 1.6663x | 1.6618x | 1.6761x | positive |
| shuffled | `getDistancesToModel` | 2.3126x, 2.1626x, 2.4473x, 2.3740x, 2.2524x | 2.3126x | 2.1626x | 2.4473x | Phase 000 gather path retained |

和 Phase 000 相比，identity `countWithinDistance` 从 1.6678x 提升到 2.1969x，identity
`selectWithinDistance` 从 3.1965x 提升到 3.3896x；shuffled select/count 保持在 Phase 000
同级范围。`getDistancesToModel` 的 identity branch 试验没有稳定收益，且早期 mixed gate 让 shuffled
case 回落到 Phase 000 95% 阈值以下，因此当前 production 不采纳该入口的 identity 分支。

## Evidence Doctor

脚本输入：

- `log/board/phase-010/identity-stride-select-count/repeated/evidence_manifest.json`
- `log/board/phase-010/shuffled-stride-select-count/repeated/evidence_manifest.json`

结果摘要：

| index mode | result | 处理 |
| --- | --- | --- |
| identity | Errors=0, Warnings=1, Suggestions=6 | 唯一 Warning 是 `getDistancesToModel` long tail；该入口未采纳 Phase 010 fast path，不影响 select/count 窄采纳。 |
| shuffled | Errors=0, Warnings=0, Suggestions=6 | metadata Suggestions 是 taskset、governor、freq、temperature 和 binary hash 缺失；不阻塞当前 positive 桶。 |

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-public：真实公开入口 Std/RVV 对比。 |
| A/B boundary | `SampleConsensusModelPlaneBench` 调用公开入口；计时边界只包含入口调用，不包含输入构造。 |
| 当前决策问题 | RVV-family-selection：是否在已有 production RVV helper 内保留 identity load family。 |
| diagnostic 是否可外推到 production | 是，bench 直接走当前 production public entry；但只能外推到 `PointXYZ`、direct indexed、当前规模和板卡。 |
| comparison-boundary / baseline mismatch 风险 | select/count 的 identity/shuffled A/B 边界一致；getDistances 的 mixed gate 结果只用于拒绝该入口的新增分支。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | getDistances 已完成 bounded probe 并撤回；若重开，必须用同一 production boundary 的 RVV-vs-RVV detail A/B。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | select/count 当前采用的是已有 production RVV helper 内部 load strategy，Std/RVV public positive 加上 shuffled 不退化足以支持窄采纳；getDistances 缺少同边界稳定收益，不能采纳。 |

## EvidenceDecision

`current_decision=adopted_narrow_select_count`。

Phase 010 采纳 `selectWithinDistanceRVV` 和 `countWithinDistanceRVV` 的 identity-index strided load：
identity case 有明确收益，shuffled case 保持 gather fallback 且没有 material regression（实质退化）。
`getDistancesToModelRVV` 的 identity fast path 被拒绝，当前源码显式传入 `false`，只走 gather。

## 继续 / 停止判断

`continue_stop_decision=continue_available_but_phase_010_closed`。

Phase 010 自身已闭合；当前 topic 仍有未阻塞但独立的后续动作：

- `020-point-type-expansion`：更多 registered float xyz AoS 点型的 correctness、fallback、asm 和必要板卡证据。
- `evidence-registry-hardening`：补 taskset、governor、freq、temperature、binary hash 和 registry freshness check。

这些动作不改变 Phase 010 的采纳结论；恢复时应先从点型扩展 phase 进入。
