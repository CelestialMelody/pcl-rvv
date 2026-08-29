# geometric_consistency Optimization Roadmap

## 当前边界

当前 topic 已经有窄范围 production RVV；路线图只记录还能继续尝试什么，不把当前
adopted helper 写回成更宽的自动结论。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `pairwise-consistency-rvv` | `clusterCorrespondences()` 内层 predicate | fixed consensus set + candidate j | 局部 predicate 已采纳到 production | gather / sqrt / early-exit 的开销已验证 | correctness、asm、board proxy、upstream correctness | adopted | next expansion only |
| `cluster-growth-rvv` | outer consensus growth loop | full cluster growth | 若要继续扩大 production 入口，再看外层 | `taken_corresps`、consensus growth 和 output order 风险高 | full production-direct bench / board / doctor | deferred | 新的 production-direct growth phase |
| `production patch` | 用户允许 board positive 后采纳 | production `clusterCorrespondences()` | 当前 narrow patch 已采纳 | 继续扩大必须先闭合新的 board / bench 边界 | production direct tests、asm、board proxy、doctor | adopted | 仅保留后续扩展路径 |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 000 | pairwise predicate 先于 cluster growth | 主流程里只有这段算术和局部控制流最清楚 | scalar reference / RVV candidate / asm / board / upstream correctness | high |
| 010 | full cluster growth diagnostic | cluster growth helper 已经正向，后续如要继续应新开 production-direct phase | growth helper / correctness / asm / board | medium |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| `cluster-growth-rvv` | 目前没有足够窄的 production-direct bench | 需要新 phase / 新 board target / 新证据边界 |
