# Optimization Roadmap

## 当前边界

vote generation helper 已完成诊断，但放回真实 `houghVoting()` 入口后，board
结果没有支撑采纳当前 production patch。`voteInt()` 的 no-interpolation 消融也
只给出接近阈值的弱信号。phase 030 补测类默认 `use_distance_weight=false`
配置后仍为 `neutral`，当前没有新的 unblocked 候选可直接继续推进。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `vote-generation-rvv` | `houghVoting()` 前半段 | scene vote 生成、min/max reduction | diagnostic 正向 | full 入口被后段成本吞没 | correctness、asm、board | attempted | `020-accumulator-scatter-audit` |
| `production-direct-vote-generation-rvv` | `houghVoting()` 公开入口 | 真实 production direct | 证明当前 patch 是否值得保留 | full 入口 neutral，5/5 < 1 | repeated board、doctor | neutral / not adopted | `020-accumulator-scatter-audit` |
| `accumulator-scatter-audit` | `HoughSpace3D::vote()` / `voteInt()` | Hough bin 写入 | no-interpolation 只接近持平 | `voteInt()` RVV 会涉及复杂状态写回 | 单独 diagnostic / ablation | attempted / stop | none |
| `production-direct-default-distance-weight-rvv` | 源码默认配置审计 | `houghVoting()` 默认 `use_distance_weight=false` | 排除 phase 010 非默认配置误判 | 5-run median `1.005x`，`1/5` 低于 1 | repeated board、doctor | neutral / not adopted | none |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| `010` | 对比 interpolation on/off | full production direct 吞掉 vote generation 收益，`voteInt()` 的 27 邻域写入可能是主成本 | board ablation / summary / doctor | high |
| `020` | 暂停 `voteInt()` RVV production 实现 | no-interpolation 只有 `1.002x` median，不能支撑复杂 scatter / voter tracking 改造 | 需要新的实现阶段计划和更强 profile | stop |
| `030` | 默认 distance weight production direct | phase 010 使用非默认 distance weight；源码默认是 `false` | board repeated / summary / doctor | stop |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| `point-type-expansion` | 当前 `PointXYZ` production direct 都没有正收益，先不扩点型 | full 入口或 scatter 方向出现正收益 |
| `doc-rvv-production-topic` | 当前没有 adopted production behavior | 后续 production direct 正向且采纳 |
| `voteInt-rvv-production` | 当前只有 no-interpolation 弱信号，且 `voteInt()` 写 `hough_space_` 与 `voter_ids_`，状态顺序和稀疏写入风险高 | 用户明确授权新的 production 实现阶段，且先有 profile / 消融支持 |
| `no-interpolation-no-distance-weight` | phase 020 和 phase 030 已分别证明 no-interpolation 与默认 distance weight 都只是 near-threshold neutral；组合会更窄且不改变 `voteInt()` 状态写回风险 | 用户明确要求覆盖全部参数组合，或后续 profile 显示该组合是实际热入口 |
