# PFHRGB Optimization Roadmap

## 当前边界

当前 topic 目标是 `features/include/pcl/features/impl/pfhrgb.hpp`。生产源码已接入 PFHRGB RVV 路径；
当前采纳证据来自接入后的 production-public（生产公开入口）板卡结果。当前窄范围是 exact
`pcl::PointXYZRGBNormal`、`float`、AoS xyz / normal / rgb 字段、默认 `nr_split=5`、KSearch public
boundary（公开 K 近邻搜索边界）和 `pcl::PFHRGBSignature250` 输出。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `pfhrgb-scalar-reference-scaffold` | 当前 PFHRGB production source | `computePointPFHRGBSignature` same-chain tests | 建立后续 candidate 的 correctness gate（正确性验收） | 颜色 ratio 和 pair order 语义可能复刻错误 | Std/RVV gtest 对拍、degenerate pair、颜色除零、public finite descriptor | adopted | closed in `000-current-state-and-scaffold` |
| `pfhrgb-color-pair-batch-rvv` | PFH direct-AoS 成功模式与复筛清单 | all-pairs geometry + RGB ratio 前算术 | 历史 positive 已被当前 rerun 降级；helper-only median `0.98x` | staging、histogram scatter、public outer loop 可能吞掉收益 | candidate correctness、asm、board repeated、Evidence Doctor | attempted / negative-current-rerun | 不再单独作为 production value |
| `pfhrgb-public-k-dilution-check` | 复筛清单 production-value evaluation | `PFHRGBEstimation::compute` synthetic KSearch path | 接入前用于判断 public boundary 是否会稀释 helper 收益 | 历史阶段仅作接入前背景，不能覆盖接入后的 production-public truth | public-like correctness、board repeated、Doctor | historical baseline | superseded by production-public rerun |
| `pfhrgb-public-with-candidate-diagnostic` | Phase 000 mismatch audit | topic-local public-shaped outer loop + candidate helper | 当前 5-run median `1.21x`，说明候选收益在 KSearch-shaped wrapper 中仍可见 | 不是 production dispatch，接入后只保留为实现形态背景 | correctness、case-filter bench、asm、board repeated、Evidence Doctor | production-shaped context | closed by production-public rerun |
| `pfhrgb-staging-reuse` | Phase 010 reflection | topic-local public-shaped wrapper | 当前 5-run median `1.24x`，比非复用 wrapper `1.21x` 略高 | 仍不是最终采纳证据；采纳看 `public_pfhrgb_k` | correctness、bench case、asm、board repeated、Evidence Doctor | adopted shape input / diagnostic context | closed by production-public rerun |
| `pfhrgb-production-direct-probe` | 后续 PI1 候选 | production `computePointPFHRGBSignature` RVV helper | 接入后 `public_pfhrgb_k` 5-run median `1.27x`，0/5 低于 1 | 只覆盖 exact 点型和 `nr_split=5`；环境 metadata 仍缺 taskset / governor / freq / temperature | production direct correctness、fallback、asm、board production bench、Evidence Doctor | adopted production behavior | closed in `020-pi1-production-integration-plan` |
| `pfhrgb-doc-suite-parity` | Phase loop maturity audit | topic-local docs only | 让 reviewer 不依赖聊天上下文即可恢复 evidence、case 和 helper 边界 | 不改变性能；需要避免复制 raw log 和生产过度声明 | role docs、phase result、artifact tracking scan | adopted | closed in `040-structure-parity-doc-suite` |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 000 | `public-with-candidate` 探针 | pair-batch candidate 曾正向，但公开入口中性；需要同一 public-shaped 计时边界判断收益是否被 search/output 稀释。 | correctness、bench case、asm、5-run board repeated、Evidence Doctor | closed |
| 010 | PI1 exact-gated production probe | public-with-candidate diagnostic 稳定正向，但 wrapper 不是 production dispatch；需要真实生产边界消除 mismatch。 | production direct correctness、fallback、asm attribution、board repeated、Evidence Doctor | closed |
| 010 | staging reuse / allocation ablation | wrapper 每个点都构造 staging；如果 PI1 不正向，可能需要判断分配和暂存是否主因。 | reusable workspace、same-boundary board A/B | closed positive in Phase 030 |
| 030 | helper-only degradation audit | 当前 rerun 显示 component/helper case 退化，但 public-shaped wrapper 正向。 | Phase 030 result 中降级旧 helper-only claims；PI1 只把 helper 作为实现组件。 | closed for diagnostic |
| 040 | doc-suite parity | repeated evidence 改变旧结论，且 topic 已有多 candidate、Doctor、board summary 和 script。 | standalone role docs、artifact tracking、Handoff refresh | closed |
| 020 | point-type expansion audit | exact-gated production 已正向，但泛型 RGB / normal traits 尚未验证。 | 新 phase 中的 traits / offset / fallback correctness、asm、board repeated 和 Evidence Doctor | deferred |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| `pfhrgb-generic-rgb-traits-production` | 当前没有 traits / offset / fallback 证据证明泛型 RGB / normal 点型可安全接入；继续会扩大到新的 point-type expansion scope。 | 需要扩大生产覆盖时，新建 `050-point-type-expansion`，读取泛型点类型策略并补 traits / offset / fallback / board 证据。 |
| `pfhrgb-histogram-scatter-rvv` | histogram scatter 有 bin conflict（直方图 bin 冲突）和顺序累加风险；PFH 也保留标量 scatter。 | 只有 profile 或 component ablation 显示 scatter 是主瓶颈时恢复。 |
| `pfhrgb-helper-only-production-value` | 当前 rerun 中 helper-only case median `0.98x` 且 5/5 低于 1，Doctor 报 Error。 | 只有更细 profile 或新 implementation family 证明 helper-only 边界可稳定正向时恢复。 |

## 默认恢复动作

`next_phase_default`: none inside current exact-gated production scope。

Phase 020 已完成接入后 correctness、ASM、board repeated、Evidence Doctor 和 `doc-rvv` closeout。
若要继续，只建议在用户明确需要更宽覆盖时进入 `050-point-type-expansion`；当前同边界 helper-only
结果为负向，没有值得立刻推进的微优化候选。
