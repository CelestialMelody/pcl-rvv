# Phase 070 Result: line identity-index strided load

## 当前结论

本阶段尝试 `identity-index-strided-load`（恒等索引跨步加载）实现族：当 `indices_`
在当前 VL chunk（可变向量长度分块）内等于连续下标时，用 `strided_load3_f32m2`
替换 indexed gather（按索引离散加载）。该候选没有采纳到当前 production behavior（生产行为）。

5-run board strict A/B（同一生产边界内 RVV-vs-RVV 严格对比）显示：identity 输入下
`countWithinDistance` 和 `getDistancesToModel` 只有弱正向，`selectWithinDistance` 明确退化；
shuffled 控制组又出现多项退化频率 Error。按本阶段计划中的决策桶，候选为
`rejected with evidence`。production 源码已回到 Phase 060 的 gather-only RVV load family，
保留 `selectWithinDistanceRVV` 和 `getDistancesToModelRVV` 的 `vfwcvt + vse64`
direct double store（直接 double 写回）。

## 实际执行范围

| 计划项 | 实际状态 | 证据 |
| --- | --- | --- |
| production load helper | attempted then reverted | 曾在 `sac_model_line.hpp` 中加入 identity / gather load helper；A/B 后因证据不足回退到 gather-only。 |
| correctness identity cases | done | `run_test_compare` 当前 Std/RVV 各 9 个 gtest 通过，包含 identity indices 公开入口对拍。 |
| bench input mode | done | `bench_sac_model_line.cpp` 支持 `identity` / `shuffled` 第三个参数；默认仍是 shuffled。 |
| asm gates | done | 候选在回退前通过 `check_identity_strided_asm`；回退后 `check_production_asm` 强制重建通过，确认当前 production 不再要求 `vls*`。 |
| Makefile A/B target | done with guard | Phase 070 A/B target 和 manifest 入口保留；默认拒绝重生成历史候选证据，需显式设置 `ALLOW_PHASE070_IDENTITY_REFRESH=1`。 |
| manifest / Evidence Doctor | done | identity 与 shuffled 两组 manifest / doctor / registry 均生成并登记。 |

## Board Strict A/B Summary

`B/A` 这里表示 `gather-only RVV baseline / identity-strided RVV candidate`。大于 1 表示候选更快。

### Identity indices

| public entry | gather-only avg ms | identity-strided avg ms | B/A values | median / min / max | degradation count |
| --- | ---: | ---: | --- | --- | ---: |
| `countWithinDistance` | 0.367697 | 0.363201 | `1.0063x, 1.0182x, 1.0075x, 1.0214x, 1.0084x` | `1.0084x / 1.0063x / 1.0214x` | 0/5 |
| `selectWithinDistance` | 0.749138 | 0.757514 | `0.9979x, 0.9719x, 1.0020x, 0.9765x, 0.9967x` | `0.9967x / 0.9719x / 1.0020x` | 4/5 |
| `getDistancesToModel` | 0.547717 | 0.541417 | `0.9986x, 1.0105x, 1.0218x, 1.0143x, 1.0132x` | `1.0132x / 0.9986x / 1.0218x` | 1/5 |

### Shuffled control

| public entry | gather-only avg ms | identity-strided avg ms | B/A values | median / min / max | degradation count |
| --- | ---: | ---: | --- | --- | ---: |
| `countWithinDistance` | 0.370267 | 0.369140 | `1.0136x, 1.0037x, 0.9999x, 1.0048x, 0.9934x` | `1.0037x / 0.9934x / 1.0136x` | 2/5 |
| `selectWithinDistance` | 0.744438 | 0.741547 | `0.9836x, 0.9982x, 1.0077x, 1.0213x, 1.0090x` | `1.0077x / 0.9836x / 1.0213x` | 2/5 |
| `getDistancesToModel` | 0.545262 | 0.546088 | `1.0133x, 0.9916x, 0.9959x, 1.0000x, 0.9918x` | `0.9959x / 0.9916x / 1.0133x` | 3/5 |

## Evidence Doctor 和 Registry

Evidence paths:

- `test-rvv/sample_consensus/sac_model_line/doc/phases/070-line-identity-index-strided-load/identity-repeated-evidence-manifest.json`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/070-line-identity-index-strided-load/identity-repeated-evidence-doctor.md`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/070-line-identity-index-strided-load/identity-repeated-evidence-doctor.json`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/070-line-identity-index-strided-load/identity-shuffled-repeated-evidence-manifest.json`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/070-line-identity-index-strided-load/identity-shuffled-repeated-evidence-doctor.md`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/070-line-identity-index-strided-load/identity-shuffled-repeated-evidence-doctor.json`

Evidence Doctor 结果：

- identity A/B：Errors=1 / Warnings=1 / Suggestions=2。Error 来自 `selectWithinDistance` 4/5 run 低于 1。
- shuffled A/B：Errors=3 / Warnings=0 / Suggestions=2。三条 public entry 都有退化频率 Error。

`log/evidence_registry.json` 已登记两组 Phase 070 summary evidence（摘要证据）。该 registry 是
local-only metadata（本地元数据），不默认提交。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-detail / strict A/B（生产细节严格对比）。 |
| A/B boundary | 同一 public overload 下，`PCL_RVV_LINE_DISABLE_IDENTITY_STRIDED` 编译出的 gather-only RVV baseline 对默认 identity-strided RVV candidate。 |
| 当前决策问题 | RVV-family-selection：identity load family 是否比 Phase 060 gather family 更值得保留。 |
| diagnostic 是否可外推到 production | 不使用 diagnostic 外推；本阶段直接在 production helper 上做有界 probe，并用宏生成旧 RVV family 基线。 |
| comparison-boundary / baseline mismatch 风险 | strict A/B 已消除 Std/RVV baseline mismatch；但 manifest 的 asm instruction count 字段不完整，asm 归属以 `check_identity_strided_asm` 输出为准。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段 probe 已完成；结果落入 negative / unstable 风险，不采纳并回退 production patch。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 需要，且本阶段结果没有满足 clean adoption。 |

## 矩阵更新

| candidate family | decision | 理由 |
| --- | --- | --- |
| `identity-index-strided-load` | rejected with evidence | identity 输入没有三入口全正，`selectWithinDistance` median 为 `0.9967x` 且 4/5 退化；shuffled 控制组三入口均触发退化频率 Error。 |
| `count-indexed-gather-f32m2` | adopted production behavior | Phase 060 production direct 仍是当前 truth；回退后 `check_production_asm` 强制重建通过。 |
| `select-vcompress-vse64-error-store` | adopted production behavior | Phase 060 已采纳，Phase 070 不改变该写回形态。 |
| `getDistances-vfsqrt-vse64-store` | adopted production behavior | Phase 050/060 已采纳，Phase 070 不改变该写回形态。 |

## 继续 / 停止判断

本阶段完成。当前 `PointXYZ + direct indexed indices_ + float xyz AoS` production boundary
内，已尝试并关闭的同边界优化方向包括：

- count/select/getDistances indexed gather RVV production dispatch；
- `getDistancesToModelRVV` 的 `vfsqrt + vfwcvt + vse64` direct double store；
- `selectWithinDistanceRVV` 的 `vcompress + vfwcvt + vse64` compressed error double store；
- identity-index strided load，同边界 A/B 后拒绝。

`point-type-expansion` 仍存在，但它会扩大点型 / layout 证据边界，当前 Handoff 将其归为
separate scope（单独范围）。在当前同边界性能搜索内，暂无新的 high-priority unblocked candidate。
默认下一状态是 `ready_for_review_after_phase070_rejection`；若后续继续，应先明确是否进入点型扩展 scope。
