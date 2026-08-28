# sac_model_circle Optimization Roadmap

## 当前边界

当前 topic 覆盖 `SampleConsensusModelCircle2D<PointT>` 的 2D 圆距离入口。Phase 000 已把 `selectWithinDistance` 接入 RVV production dispatch（生产分流），并重新验证 `countWithinDistanceRVV`；Phase 080/090 又把 `selectWithinDistance` 命中点误差写回从标量 `sqrt` tail（尾段）推进为 compressed lanes（压缩后的有效向量通道）上的 `vfsqrt + vfwcvt + vse64` 写回。用户已确认接入后板卡收益可以作为采纳依据，因此当前 select/count/getDistances 都是 adopted production behavior（已采用生产行为）。长期 production 文档为 `doc-rvv/sample_consensus/sac_model_circle-RVV.zh.md`。

`getDistancesToModel` 已重开并采纳 full-RVV（完整 RVV）实现族。Phase 020 的 `RVV sqr distance + scalar sqrt + dense double store` test-only candidate（仅测试使用候选）在板卡上稳定退化，只拒绝该旧实现族。Phase 050 改用 `vfsqrt + vfwcvt + vse64` 后 diagnostic positive；Phase 060 已接入 production（生产源码），接入后 public Std/RVV 也为 positive-stable；Phase 070 已按用户确认把该路径写成 adopted production behavior。

Phase 040 尝试 identity-index strided load（恒等索引跨步加载），但 RVV-vs-RVV（两个 RVV 实现族直接比较）strict A/B 结果对 select/count 都是 negative，因此该分支已被拒绝。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| select RVV gather + scalar exact error writeback | 当前 circle2d 源码和 sphere select 经验 | `selectWithinDistance` direct indexed `indices_` | 少一个 z gather，可能比 sphere select 更便宜 | 命中率高时 scalar `sqrt` tail 仍可能成为主成本 | RED/green correctness、asm、board repeated、Evidence Doctor | superseded by Phase 090 | 保留为 historical baseline；当前 production truth 是 full-RVV error tail。 |
| select compressed error full-RVV tail | Phase 070 反思、用户指出 RVV sqrt、line/stick compressed error write-back 经验 | `selectWithinDistance` direct indexed `indices_` | 消除 `vcompress` 后逐 active lane 标量 `sqrt` / double store 成本 | float `vfsqrt` 与标量 double sqrt 不是 bitwise 相同，需要 tolerance correctness 和接入后 production direct | Phase 080 RVV-vs-RVV detail A/B、Phase 090 production direct correctness / asm / board / Evidence Doctor | adopted by Phase 090 | 无；正式 `doc-rvv` 已使用 Phase 090 接入后板卡数据。 |
| existing count RVV regression | 当前 production helper | `countWithinDistance` | 保留已有正向路径，防止 select 改动破坏 count | 旧 quadric 测试曾允许小差异；需确定性 correctness | QEMU correctness、asm、board repeated | adopted | 无；随 select/count closeout 保留。 |
| getDistances squared-distance RVV + scalar sqrt store | sphere 负向经验和当前源码 | `getDistancesToModel` | 理论上减少 x/y gather 和平方距离成本 | 每点必须 `sqrt` 和 double store；Phase 020 证明当前 helper 还引入 buffer / lane loop 成本 | RED/GREEN、QEMU correctness、asm、5-run board diagnostic、Evidence Doctor | rejected with diagnostic evidence；superseded by Phase 050/060 full-RVV family | 不再复跑旧形态；该负向不能外推到 full-RVV production probe。 |
| getDistances vfsqrt full-RVV store | 用户指出 RVV 有 sqrt 指令；normal_plane dense write-back 经验 | `getDistancesToModel` | 消除 Phase 020 的标量 `sqrt` / 临时 buffer 后处理成本 | float `vfsqrt` 与标量 double sqrt 不是 bitwise 相同，需要 tolerance correctness gate | Phase 050 diagnostic、Phase 060 production direct correctness / asm / board / Evidence Doctor | adopted by Phase 070 | 无；正式 `doc-rvv` 已使用 Phase 060 接入后板卡数据。 |
| identity-index strided load | plane identity fast path 经验 | select/count on cloud-only identity indices | 避免 gather 成本 | chunk 级 identity 检测和额外指令可能超过 gather 节省 | RVV-vs-RVV A/B、asm、board | rejected with strict A/B evidence | 无；只有新的低成本 identity 检测或不同访存组织能解释 Phase 040 退化时恢复。 |
| structure parity doc suite | workflow doc-suite quality bar | topic-local README、evaluation、testing / evidence / code map docs | 改善 reviewer 恢复和证据归属，不改变性能 | 不能把文档补齐写成 production adoption | artifact tracking、diff check、evidence status | adopted in Phase 030 | 无后续文档结构动作。 |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 000 | `getDistancesToModel` companion row 暂不进入 production manifest | 它没有手写 RVV production path，5-run 观测接近中性且 4/5 低于 1；混入 Phase 000 manifest 会阻塞 select/count 的生产证据角色。 | 后续若恢复该入口，需要单独拆 `sqrt` 和 double store，不能从 select/count 外推。 | completed by Phase 020 rejection |
| 000 | `PointXYZI` correctness 已覆盖但未做 board performance | 当前 traits gate 不只 exact `PointXYZ`，但板卡性能只证明 `PointXYZ`。 | 若要扩大 production performance 结论，需要 dedicated board 和 Evidence Doctor；不影响当前 `PointXYZ` production adoption。 | deferred by scope |
| 020 | 当前 getDistances RVV sqr + scalar sqrt/store helper 不值得进入 production probe | 5-run board diagnostic 的 B/A median 为 0.6590x，5/5 低于 1；Evidence Doctor 报 1 个退化 Error 和 1 个“指令更少但更慢” Warning。 | 若未来恢复，应先拆 `sqrt` / double store / temporary buffer（临时缓冲）成本，或引入不同实现族；不能直接复用本 helper。 | low until new family |
| 030 | 文档套件和 handoff 已从聊天摘要迁回 topic-local 文件 | 恢复扫描发现 evaluation 和 current handoff 缺失；复杂 topic 已有 production direct、board summary 和 Evidence Doctor，必须有稳定 doc role 主归属。 | 已补齐 README、evaluation、testing overview、correctness、benchmark/evidence、optimization evidence 和 code map。 | completed |
| 040 | identity fast path 不值得保留 | Phase 040 同边界 RVV-vs-RVV A/B 中 select median 0.9914x、count median 0.9698x，5/5 全部低于 1。 | 需要新的低成本 identity 检测、不同 load 组织或不同硬件证据才可重开。 | rejected |
| 050 | `getDistancesToModel` 可用 full-RVV sqrt/store 恢复 | Phase 020 的负向集中在标量 sqrt/store 后处理；`normal_plane` 已有 `vfwcvt + vse64` dense double 写回形态。 | production direct correctness、asm、5-run board repeated 和 Evidence Doctor。 | completed by Phase 060 |
| 060 | 接入后 public getDistances 仍正向 | 5-run production public B/A median 1.4737x，Evidence Doctor 0/0/0。 | Phase 070 用户确认后刷新正式 `doc-rvv`。 | completed by Phase 070 |
| 070 | production closeout 后发现 select 命中后仍有 scalar sqrt tail | `selectWithinDistance` 当时先 `vcompress` 命中 lane，再标量 `sqrt` 写 `error_sqr_dists_`；Phase 000 select median 已有 1.6702x，因此必须先做 RVV-vs-RVV detail A/B。 | Phase 080 已完成 detail A/B，并触发 Phase 090 production probe。 | completed by Phase 080/090 |
| 080 | select full-RVV error tail 候选正向 | Phase 080 RVV-vs-RVV B/A median 1.6078x，说明压缩后继续做 `vfsqrt + vfwcvt + vse64` 比旧 adopted RVV family 更快。 | 接入后 public production correctness、asm、5-run board repeated 和 Evidence Doctor。 | completed by Phase 090 |
| 090 | 接入后 public select 仍正向 | Phase 090 production direct B/A median 2.5700x，Evidence Doctor 0/0/0，且 public select item checksum 一致。 | 正式 `doc-rvv` 使用 Phase 090 接入后板卡数据。 | completed |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| circle3d projection | 不属于本 topic，且投影 / normalize / double 语义更复杂。 | 回到队列表顺序 9，另开 `sac_model_circle3d` topic。 |
| getDistances RVV sqr + scalar sqrt/store current helper | Phase 020 诊断负向：B/A median 0.6590x，min/max 0.6528x / 0.6629x，5/5 低于 1。 | 已由 Phase 050 full-RVV 新实现族取代；不要再复跑旧形态。 |
| selectWithinDistance old scalar error tail | Phase 080/090 已证明 full-RVV error tail 更快，并已接入 production。 | 仅当未来发现数值或其它硬件退化时，用 Phase 000 historical baseline 做回退比较。 |
| identity-index strided load current helper | Phase 040 strict A/B 负向：select median 0.9914x，count median 0.9698x，5/5 都低于 1。 | 只有新的 identity 检测或 load 组织能解释并规避退化时恢复；当前生产源码保持 gather-only。 |
| topic-local doc suite | Phase 030 已补齐 README、evaluation、testing overview、correctness、benchmark/evidence、optimization evidence 和 code map；Phase 040 已创建 production `doc-rvv`。 | 仅在新 phase、新证据或 production diff 变化时刷新。 |
