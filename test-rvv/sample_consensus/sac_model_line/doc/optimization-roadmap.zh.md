# sac_model_line Optimization Roadmap

## 当前边界

当前已完成 Phase 000：`countWithinDistance` 的 `PointXYZ + direct indexed indices_ + float xyz AoS` production-shaped diagnostic。Phase 010 已完成 `selectWithinDistance` 的同边界 production-shaped diagnostic。Phase 020 的 `getDistancesToModel` scalar-sqrt 形状为 negative diagnostic；Phase 030 的 `getDistancesToModel` vfsqrt 形状为 partial-production-candidate。Phase 040 已把 count/select/getDistances vfsqrt 接入 production 并完成接入后 production direct board repeated。Phase 050 已把 `getDistancesToModelRVV` 的写回从 scratch + 标量 lane store 改成 `vfwcvt + vse64` direct double store；Phase 060 已把 `selectWithinDistanceRVV` 的压缩平方误差写回也改成 `vfwcvt + vse64` direct double store。Phase 070 尝试 identity-index strided load（恒等索引跨步加载）后，strict RVV-vs-RVV A/B（同一生产边界内 RVV 实现族对比）显示 select 退化且 shuffled 控制组不稳，因此已拒绝并回退到 Phase 060 gather-only load family。三入口当前均为 positive-stable，当前窄范围已按本轮采纳条件写成 adopted production behavior。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `count-indexed-gather-f32m2` | 当前源码 cross3/squaredNorm 主循环 | `countWithinDistance` direct indexed `PointXYZ` | Phase 060 production public board median `4.8068x` | 其它点型 / identity fast path 未证明 | production direct tests、asm、board repeated、doctor | adopted production behavior | closed for current boundary |
| `select-vcompress-scratch-scalar-store` | count 共享距离公式 | `selectWithinDistance` direct indexed `PointXYZ` | Phase 050 production public board median `3.1014x` | 保留 scratch + 标量 lane double store | Phase 050 production direct tests、asm、board repeated、doctor | historical adopted baseline, replaced | closed by Phase 060 |
| `select-vcompress-vse64-error-store` | 用户指出 line select 仍有标量 lane loop，normal-plane sibling 已有 direct double store | `selectWithinDistance` direct indexed `PointXYZ` | Phase 060 production public board median `3.2460x`，RVV avg 比 Phase 050 约快 `1.024x` | 收益小，Phase 050/060 不是逐 run 同步 A/B；其它点型 / identity fast path 未证明 | production direct tests、asm、board repeated、doctor | adopted production behavior | closed for current boundary |
| `getDistances-sqr-rvv-scalar-sqrt-store` | count 共享距离公式 | `getDistancesToModel` | 尝试复用平方距离核 | sqrt 与 double store 主导，5-run 全部退化 | Phase 020 board repeated、asm、doctor | rejected with evidence | not_applicable |
| `getDistances-vfsqrt-scratch-scalar-store` | Phase 040 production integration | `getDistancesToModel` | production public board median `3.4036x` | 仍有 scratch store 和标量 lane double store | Phase 040 production direct tests、asm、board repeated、doctor | historical adopted baseline, replaced | closed by Phase 050 |
| `getDistances-vfsqrt-vse64-store` | 用户反馈和 normal-plane sibling store 形态 | `getDistancesToModel` | Phase 050 production public board median `4.2237x`，RVV avg 比 Phase 040 约快 `1.19x` | 其它点型 / identity fast path 未证明；Phase 040/050 不是逐 run 同步 A/B | production direct tests、asm、board repeated、doctor | adopted production behavior | closed for current boundary |
| `line-production-public-dispatch` | Phase 040/050/060 production integration | public count/select/getDistances | 接入后 public board medians `4.8068x` / `3.2460x` / `4.0460x` | 长期文档 freshness 需验证 | S11 production doc-rvv closeout、freshness check | adopted production behavior | closeout after Phase 060 verification |
| `identity-index-strided-load` | plane/circle sibling 经验和 line fallback 反思 | full-cloud identity `indices_` 或等价顺序索引 | identity 下 count/getDistances 有弱正向 | identity select median `0.9967x` 且 4/5 退化；shuffled getDistances median `0.9959x` 且 3/5 退化；doctor 有 Errors | Phase 070 strict RVV-vs-RVV A/B、asm、board repeated、doctor | rejected with evidence | closed by Phase 070 |
| `point-type-expansion` | PCL 模板入口 | count/select/getDistances | 扩大覆盖范围 | traits / offset / layout gate | representative point type correctness 和必要 board | turn_stop_deferred with stop_condition_hit | production scope 或 dedicated diagnostic 授权后扩展 |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 000 | 先为 count 做 PI1 生产接入计划。 | diagnostic candidate 收益稳定，且 count 无输出容器写回。 | production fallback、真实入口 test、production asm 和 board repeated。 | high |
| 000 | select 和 getDistances 分开推进。 | 它们共享公式但输出语义不同。 | 各自独立 correctness、bench 和 Evidence Doctor。 | medium |
| 010 | count/select 可合并做 PI1 scope 审计。 | 两个入口均在同一 direct indexed `PointXYZ` diagnostic boundary 下 positive-stable，但 production dispatch 和 fallback 仍未证明。 | production direct correctness、fallback、asm、board repeated 和 Evidence Doctor。 | high |
| 010 | `getDistancesToModel` 仍需单独 sqrt/store 诊断。 | select 的 `vcompress` 正向不能外推到 dense double distance 输出。 | QEMU correctness、sqrt/store bench、asm、board repeated 和 doctor。 | medium |
| 020 | scalar-sqrt 形状不再作为 production probe 候选。 | 5-run B/A 全部小于 1，说明 RVV 平方距离后回到标量 sqrt / dense store 的形状不能支撑当前 diagnostic boundary。 | 不需要继续同形状复跑；保留为 rejected evidence。 | high |
| 020 | 尝试 vfsqrt 形状。 | Phase 020 的退化集中在 sqrt / store 后处理边界，仓库已有直接 `__riscv_vfsqrt_v_f32m2` 使用先例。 | QEMU correctness、asm、5-run board repeated、doctor 和 registry。 | high |
| 030 | count/select/getDistances 可以合并做 PI1 scope 审计。 | 三个入口均已有同一 `PointXYZ + direct indexed indices_` diagnostic positive family，但 production dispatch、fallback 和真实公开入口 direct bench 未证明。 | PI1 计划、production direct correctness、fallback、asm、board repeated 和 Evidence Doctor。 | high |
| 040 | 三入口 production patch 已有接入后正向板卡证据，S11 production doc-rvv closeout 使用 post-integration 数据。 | production direct 已消除 diagnostic / public boundary mismatch；长期文档必须使用接入后数据。 | `doc-rvv/sample_consensus/sac_model_line-RVV.zh.md`、freshness check。 | high |
| 040 | identity-index strided load 仍可作为后续性能探索，但不应混入当前采纳检查点。 | 它会改变 production family，需要 RVV-vs-RVV detail A/B；当前证据只证明 gather-style public RVV 快于标量。 | 新 phase plan、identity/shuffled case、production/detail A/B、board repeated 和 doctor。 | medium |
| 050 | `getDistancesToModelRVV` 的 direct double store 成为当前生产写回形态。 | 用户指出 normal-plane 已用 `vfwcvt + vse64`，Phase 050 证据显示 line 同边界也有收益。 | Phase 050 result、production direct manifest、Evidence Doctor、asm 和 freshness check。 | high |
| 060 | `selectWithinDistanceRVV` 的 compressed error double store 也可直接 `vfwcvt + vse64`。 | 用户指出 line RVV for 循环里仍有标量 lane loop；源码确认它位于 select 压缩距离写回，而 normal-plane sibling 已有 RVV direct store。 | Phase 060 result、production direct manifest、Evidence Doctor、asm 和 freshness check。 | high |
| 060 | identity-index strided load 成为下一个可执行性能探索。 | getDistances 和 select 的明确标量写回热点都已去掉；剩余同 topic 性能方向主要是减少 direct identity `indices_` 下的 gather 成本。 | Phase 070 plan、identity/shuffled bench mode、RVV-vs-RVV detail A/B、board repeated 和 doctor。 | medium |
| 070 | identity-index strided load 不进入当前 production。 | 同边界 A/B 后，identity 输入只给 count/getDistances 弱正向，select 退化；shuffled 控制组也有退化频率 Error。 | 已回退 production patch；保留 Phase 070 manifest / doctor / result 作为拒绝证据。 | closed |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| QEMU bench compare | QEMU timing 不作为性能证据。 | 只在需要日志形状 smoke 时显式记录 `qemu_smoke_only`。 |
| full template production adoption | 当前只验证 `PointXYZ`。 | 完成 generic point type strategy（泛型点类型策略）和 fallback matrix 后再扩大。 |
| identity-index strided load | Phase 070 strict A/B 显示 select identity 退化、shuffled 控制组不稳，未满足三入口 adoption bucket。 | 只有出现更便宜的 identity 检测或入口拆分策略，且先冻结“只对 count/getDistances 生效、不影响 select”的新 production boundary，才可重开。 |

## 默认恢复动作

`next_phase_default`: `ready_for_review_after_phase070_rejection` for the current `PointXYZ + direct indexed indices_ + float xyz AoS` performance boundary. `identity-index-strided-load` 已由 Phase 070 拒绝；`getDistances` store shape 已由 Phase 050 的 `vfwcvt + vse64` 形态关闭，`select` compressed error store 已由 Phase 060 的 `vfwcvt + vse64` 形态关闭。`point-type-expansion` 属于更宽点型 / layout 范围，需要专门 scope。
