# Phase 040 Plan: production closeout 与 identity-index strided load 前沿

## 阶段意图和边界

本阶段先把 Phase 000 的 `selectWithinDistance` / `countWithinDistance` production patch（生产补丁）从
PI5 checkpoint（生产证据检查点）同步为 adopted production behavior（已采用生产行为）：用户已确认
“板卡上的测试结果如果显示有收益即可采纳”，且要求接入后的正式 `doc-rvv` 文档使用接入后板卡测试数据。

完成 S11 production closeout 后，本阶段继续尝试下一条未阻塞优化方向：identity-index strided load
（恒等索引跨步加载）。它只作用于 `indices_` 为 `0..N-1` 的整云 / 恒等索引 chunk，目标是在
`selectWithinDistanceRVV` 和 `countWithinDistanceRVV` 中避免不必要的 indexed gather（按索引离散加载）。
乱序或非恒等索引必须继续走现有 gather 路径，不能退化正确性或把 shuffled（乱序）case 写成 identity 收益。

## 当前状态清单

| area | 当前状态 |
| --- | --- |
| Phase 000 production direct | select median 1.6702x，count median 1.4012x，Evidence Doctor 0/0/0。 |
| Phase 020 getDistances | 当前 test-only `RVV sqr + scalar sqrt/store` helper 被诊断拒绝，production 保持标量。 |
| Phase 030 doc suite | topic-local README、evaluation、testing overview、correctness、benchmark/evidence、optimization evidence、code map 已补齐。 |
| production source | `selectWithinDistance` 和 `countWithinDistance` 已有 `__RVV10__`、x/y float layout、signed 32-bit index、u32 byte offset gate。 |
| identity frontier | roadmap 中仍为 deferred；当前用户采纳 Phase 000 后不再阻塞。 |

## Phase Scope 与扩展队列

| 字段 | 范围 |
| --- | --- |
| validated_scope | `PointXYZ` / float x-y AoS，`indices_` 为 direct indexed；identity A/B 只覆盖 `selectWithinDistance` 和 `countWithinDistance`。 |
| unvalidated_scope | `getDistancesToModel` production RVV、更多 PointXYZ-like 点型 board、`Scalar=double`、非 float x/y layout、超大 offset fallback、其它 row source。 |
| point_type_expansion_queue | Phase 040 不扩点型；PointXYZI 已有 correctness，dedicated board performance 仍需后续 phase。 |
| phase_closeout_boundary | Phase 040 只能关闭 Phase 000 production closeout 和 identity-index 当前实现族的 evidence decision。 |

## 候选族和假设

identity-index fast path 复用当前 topic 内的 x/y 单字段 wrapper：

- 若整个 `indices_` 首尾满足 `indices[0] == 0` 且 `indices[N-1] == N-1`，每个 VL chunk 再用 `vid + start` 检查是否完全恒等。
- 恒等 chunk 用 `strided_load_field_f32m2<PointT, pcl::fields::x/y>` 从 `points_base + i * sizeof(PointT)` 直接加载。
- 非恒等 chunk 继续用现有 `byte_offsets_u32m2` + `indexed_load_field_f32m2` gather 路径。

预期收益来自减少 identity case 中两次 `vluxei32` 和 offset 计算。风险是 chunk 级 identity 检查自身有成本；
若 identity 数据很少或 shuffled case 误命中，收益会消失甚至退化。

## 优化矩阵

| candidate family | row source policy | point type / layout | test | bench / board | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Phase 000 select/count gather | direct indexed shuffled | `PointXYZ` / x-y float AoS | 已通过 | board repeated positive | 已归属 | 0/0/0 | adopted |
| identity-index strided load | direct indexed identity | `PointXYZ` / x-y float AoS | `run_test_compare` + identity correctness | 需要 RVV-vs-RVV A/B：identity 新旧实现，另看 shuffled non-regression | helper 符号内应出现 `vlse32`，且保留 gather fallback | 需要 manifest / manual doctor | planned |
| getDistances current helper | direct indexed shuffled | `PointXYZ` / x-y float AoS | 已通过 | B/A median 0.6590x | 已归属 | Error=1 / Warning=1 | rejected current family |

## 实现和测试动作

| action | 产物 | 预期证据 |
| --- | --- | --- |
| S11 production closeout | `doc-rvv/sample_consensus/sac_model_circle-RVV.zh.md` 和 topic-local 文档 pending -> adopted 更新 | 文档以 Phase 000 接入后板卡数据为当前生产证据。 |
| RED asm gate | Makefile target `check_identity_strided_asm` | 当前实现应失败，因为 `countWithinDistanceRVV` / `selectWithinDistanceRVV` 只含 gather。 |
| production implementation | `impl/sac_model_circle.hpp` identity 检测和 x/y load helper | identity chunk 命中 strided load；shuffled chunk 仍 gather。 |
| bench input mode | `src/bench_sac_model_circle.cpp` 第三参数 `identity|shuffled` | 同一 binary 可构造 identity 和 shuffled 两类 board A/B 输入。 |
| evidence manifest extension | `script/generate_circle_board_evidence_manifest.py` 支持 identity/shuffled mode | 后续 repeated board 可区分 production-public Std/RVV 与 RVV-family-selection。 |

## Evidence Doctor 和 registry 规则

Phase 000 的 production direct evidence 已登记为 `circle-phase000-production-select-repeated-board`。identity
候选属于 RVV-family-selection（RVV 实现族选择），不能用 Std/RVV positive 直接 clean-adopt；若本阶段能采集板卡，
必须记录同一 production boundary 内新旧 RVV 的 detail A/B，或在 result 中把 identity 实现标为
`implementation-family comparison pending`。

## 板卡复跑预算和决策桶

identity A/B 默认 5-run repeated board，200 iterations，5 warmup。决策桶：

- `positive`：identity select/count median 均大于 1.05x，且 shuffled non-regression 无稳定退化。
- `weak-positive`：median 在 1.00x 到 1.05x，需结合实现复杂度和 Evidence Doctor。
- `neutral`：0.98x 到 1.02x 且无稳定方向。
- `negative`：median 小于 0.98x 或 `B/A < 1` 频率高。
- `unstable`：5-run 内跨桶摇摆，预算耗尽后不继续无限复跑。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | Phase 000 closeout 是 production-public；identity 候选需要 production-detail / RVV-vs-RVV。 |
| A/B boundary | Phase 000 是 public overload Std/RVV；identity 应比较同一 public overload 下旧 RVV gather 与新 RVV identity fast path。 |
| 当前决策问题 | Phase 000 是否采纳已由用户确认；identity 是 RVV-family-selection。 |
| diagnostic 是否可外推到 production | identity 必须直接进 production boundary 验证，不能靠 test-only helper 外推。 |
| comparison-boundary / baseline mismatch 风险 | 有。若只跑 Std/RVV，会混入标量基线变化，不能证明新 RVV family 优于已采纳 gather family。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 可以保留实现为 bounded probe，但不得写 clean adopted；若 shuffled 退化或 correctness 失败，应回退 identity 改动。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 需要。 |

## 继续 / 停止条件

默认继续到 identity 实现和本地验证。若交叉编译 / QEMU / 板卡不可用，阶段可以停在
`turn_stop_deferred with stop_condition_hit`，但必须保留 RED/GREEN、本地可复现命令和下一步板卡 A/B target。
若本地 correctness 或 asm gate 失败且无法修复，则回退 identity 改动，只保留 Phase 000 adopted closeout。

## 文档更新清单

本阶段需同步：

- `doc-rvv/sample_consensus/sac_model_circle-RVV.zh.md`
- `README.zh.md`
- `doc/sac_model_circle-evaluation.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/phases/README.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/phases/040-production-closeout-and-identity-frontier/result.zh.md`
- current Handoff
