# Phase 070 Plan: line identity-index strided load

## 阶段意图和边界

本阶段尝试 `identity-index-strided-load`（恒等索引跨步加载）：当 `indices_`
在当前 VL chunk（可变向量长度分块）内等于 `i, i+1, ...` 时，`countWithinDistanceRVV`、
`selectWithinDistanceRVV` 和 `getDistancesToModelRVV` 改用 `strided_load3_f32m2`
（三字段跨步加载）读取 `PointXYZ` 风格 AoS（数组结构）点；非恒等 chunk 继续使用当前 indexed
gather（按索引离散加载）路径。

本阶段不修改 public API（公开接口），不扩大到其它点型、`Scalar`、layout 或 row source（行来源），
不改变已采纳的 `vcompress + vfwcvt + vse64` 和 `vfsqrt + vfwcvt + vse64` 写回形态。性能决策是
RVV-family-selection（RVV 实现族选择），必须比较同一 production boundary（生产边界）内的
gather-only RVV baseline（只用离散加载的 RVV 基线）和 identity-strided RVV candidate
（恒等索引跨步加载候选），不能只看 public Std/RVV（公开入口标量 / RVV）对比。

## 当前状态清单

| area | 当前状态 |
| --- | --- |
| Phase 060 production truth | public count/select/getDistances 板卡 median 为 `4.8068x` / `3.2460x` / `4.0460x`，Evidence Doctor 0/0/0。 |
| current production load family | 三个 RVV helper 都每个 chunk 先加载 `indices_`，再用 `byte_offsets_u32m2` + `indexed_load3_f32m2` gather 读取 xyz。 |
| store family | `selectWithinDistanceRVV` 已用 `vcompress + vfwcvt + vse64` 写 `error_sqr_dists_`；`getDistancesToModelRVV` 已用 `vfsqrt + vfwcvt + vse64` 写 dense double output（连续 double 输出）。 |
| sibling experience | base plane 有 identity chunk 检查和 strided load helper；circle 曾做 strict A/B 后负向，说明本阶段必须独立测试 line，不能直接采纳。 |
| bench shape | 当前 bench 只构造 shuffled adjacent pairs（相邻交换乱序）；需要增加 `identity|shuffled` 模式。 |

## Phase Scope 与扩展队列

| 字段 | 范围 |
| --- | --- |
| validated_scope | `SampleConsensusModelLine<PointXYZ>` 的三条 public entry，float xyz AoS，direct indexed `indices_`，identity 与 shuffled 两类 indices。 |
| unvalidated_scope | `PointXYZI`、RGB/RGBA、normal、自定义点型，非 AoS layout，`Scalar=double`，其它 row source，真实 RANSAC 上游端到端性能。 |
| point_type_expansion_queue | 本阶段不扩点型；若 identity 候选 adopted，后续仍需为更多点型做 dedicated correctness / asm / board。 |
| phase_closeout_boundary | 只关闭 identity-index load family 在当前 `PointXYZ + direct indexed` 边界下是否采用；不关闭泛型点型或上游工作流。 |

## Sibling Experience Migration Audit

| sibling 经验维度 | sibling topic 里的机制 | 当前 topic 是否适用 | 状态 | 证据 / 理由 | 下一步 |
| --- | --- | --- | --- | --- | --- |
| identity 检查 | base plane 用首尾 quick check（快速检查）和 chunk 内 `vid + start` 对比。 | 适用。line 同样有 direct indexed `indices_`，且三入口共用 xyz 加载。 | planned | 可复用思想，不复制 plane 公式。 | 在 line 本地 detail helper 中实现。 |
| strict A/B | circle 用禁用 identity 的 RVV baseline 对默认 RVV candidate。 | 适用。Phase 070 是实现族选择。 | planned | public Std/RVV positive 不能证明新 RVV family 优于 Phase 060。 | 新增 gather-only RVV bench binary 和 manifest。 |
| negative sibling | circle identity A/B 负向。 | 只能作为风险，不作为 line 结论。 | deferred as risk | line 的 cross3 + vfsqrt / vcompress 成本不同，gather 占比可能不同。 | 用 line 板卡 A/B 决策。 |
| point type expansion | plane 后续扩大更多 AoS 点型 correctness。 | 本阶段不适用。 | deferred | 当前用户目标是继续当前性能搜索，不扩大点型范围。 | 另开 point-type phase。 |

## 候选族和假设

候选实现：

- 新增 line 本地 `indicesMayBeIdentity`、`indicesAreIdentityChunk` 和 `loadXYZ` helper。
- 正常 RVV 构建：identity chunk 使用 `strided_load3_f32m2<sizeof(PointT), kX, kY, kZ>`；非 identity chunk 使用现有 `indexed_load3_f32m2`。
- 测试专用 gather baseline：用 `PCL_RVV_LINE_DISABLE_IDENTITY_STRIDED` 编译 RVV bench，使同一源码可生成旧 gather-only family。

收益假设：identity 输入下减少 offset 计算和三字段 indexed gather 成本。风险：chunk 检查本身增加整数向量比较和 mask popcount；如果硬件上 `vlse32` 对当前 AoS stride 不优，或 shuffled 控制组额外检查成本明显，候选可能中性或负向。

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Phase 060 gather-based RVV | direct indexed shuffled | `PointXYZ`, float xyz AoS | public count/select/getDistances | already pass | Phase 060 production board | positive-stable | helper contains `vlux` | 0/0/0 | adopted baseline |
| identity-index strided load | direct indexed identity | `PointXYZ`, float xyz AoS | public count/select/getDistances | `run_test_compare` plus identity cases | `collect_identity_repeated_board_evidence` | planned strict RVV-vs-RVV | helper should contain `vlse32` and keep `vlux` fallback | planned | pending |
| shuffled non-regression | direct indexed shuffled | `PointXYZ`, float xyz AoS | public count/select/getDistances | existing shuffled tests | `collect_identity_shuffled_repeated_board_evidence` | planned strict RVV-vs-RVV | helper should still contain `vlux` fallback | planned | pending |

## 实现和测试动作

| action | 产物 | 预期证据 |
| --- | --- | --- |
| production load helper | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_line.hpp` | 三入口共用 identity / gather load 选择，fallback 仍是现有标量和 u32 offset gate。 |
| correctness identity cases | `test-rvv/sample_consensus/sac_model_line/src/test_sac_model_line.cpp` | identity indices 下 public entry 与 diagnostic candidate / Standard 语义一致。 |
| bench input mode | `test-rvv/sample_consensus/sac_model_line/src/bench_sac_model_line.cpp` | 第三个参数支持 `identity|shuffled`，默认保持 shuffled。 |
| asm gates | `script/check_line_production_asm.py` 和新增 identity asm 检查 | 默认 RVV helper 中出现 `vlse32`；gather baseline 仍出现 `vlux`。 |
| Makefile A/B target | `test-rvv/sample_consensus/sac_model_line/Makefile` | 可构建 / 部署 default RVV candidate 和 gather-only RVV baseline，并收集 identity / shuffled repeated board。 |
| manifest extension | `script/generate_line_board_evidence_manifest.py` | 生成 strict A/B manifest，区分 identity 和 shuffled non-regression。 |

## Evidence Doctor 和 registry 规则

本阶段新增两组 summary evidence（摘要证据）：

- identity strict A/B：`doc/phases/070-line-identity-index-strided-load/identity-repeated-evidence-*`
- shuffled non-regression strict A/B：`doc/phases/070-line-identity-index-strided-load/identity-shuffled-repeated-evidence-*`

两组 manifest 必须设置 `strict_ab: true`，`build_comparison` 为 `rvv_vs_rvv_repeated`，baseline 是
`PCL_RVV_LINE_DISABLE_IDENTITY_STRIDED` 编译出的 gather-only RVV，candidate 是默认 production RVV。
registry 记录为 `strict_ab` evidence role（证据角色）。

## 板卡复跑预算和决策桶

默认 5-run repeated board，每轮 65536 points、200 iterations、5 warmup。决策桶：

- `positive`：identity 模式三入口 median 均大于 `1.03x`，且 shuffled 控制组没有稳定小于 `0.99x` 的退化。
- `weak-positive`：identity 模式主要入口大于 `1.00x` 但不到 `1.03x`，需要结合 shuffled 成本和维护复杂度判断。
- `neutral`：`0.98x` 到 `1.02x` 且无稳定方向。
- `negative`：identity 或 shuffled 任一关键入口 median 小于 `0.98x`，或 5-run 中多数小于 `1.0x`。
- `unstable`：5-run 内跨桶摇摆；预算耗尽后不无限复跑。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-detail / strict A/B。 |
| A/B boundary | 同一 public overload 下，gather-only RVV baseline 对 default identity-strided RVV candidate。 |
| 当前决策问题 | RVV-family-selection：identity load family 是否比 Phase 060 gather family 更值得保留。 |
| diagnostic 是否可外推到 production | 不使用 diagnostic 外推；候选直接进入 production helper，并用测试宏生成旧 RVV family 基线。 |
| comparison-boundary / baseline mismatch 风险 | 有。若只看 Std/RVV public rows，会混入标量基线，不能证明新 family 优于旧 RVV family。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段本身就是有界 production probe；若证据不是 positive，必须把候选标为 rejected / attempted，并按结果决定是否回退。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 需要；identity 和 shuffled 两组都要解释。 |

## 继续 / 停止条件

默认继续到实现、本地 correctness、asm、板卡 repeated、Evidence Doctor 和 registry。若本地 correctness 或 asm 无法修复，回退候选生产改动，只保留 Phase 060 adopted 状态。若板卡不可用，阶段可停在 `turn_stop_deferred with stop_condition_hit`，但必须留下可复现 target 和未完成证据路径。

## 文档更新清单

本阶段完成后同步：

- `test-rvv/sample_consensus/sac_model_line/doc/phases/070-line-identity-index-strided-load/result.zh.md`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/optimization-matrix.zh.md`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/README.zh.md`
- `test-rvv/sample_consensus/sac_model_line/doc/optimization-roadmap.zh.md`
- `test-rvv/sample_consensus/sac_model_line/doc/benchmark-and-evidence.zh.md`
- `test-rvv/sample_consensus/sac_model_line/doc/optimization-evidence.zh.md`
- `test-rvv/sample_consensus/sac_model_line/doc/sac_model_line-evaluation.zh.md`
- `doc-rvv/sample_consensus/sac_model_line-RVV.zh.md`，仅当 identity candidate 最终 adopted。
- current Handoff。
