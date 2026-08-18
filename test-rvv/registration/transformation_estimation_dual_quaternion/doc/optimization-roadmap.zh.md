# 优化路线图

## 当前边界

Phase 016 后，当前生产提交候选保留三类 row source policy（行来源策略）的 RVV path：
`ordered-cloud-pair`、`source-indexed-cloud-pair` 和 `dual-indexed-cloud-pair`。
`correspondence-pair` production RVV 已移除，后续只作为 diagnostic（诊断）和历史负向 probe 输入。

## 候选搜索空间

| candidate family | applies to | required evidence | status | next phase |
| --- | --- | --- | --- | --- |
| ordered-cloud-pair C1/C2 RVV accumulation | ordered-cloud-pair / `Scalar=float` / dense xyz AoS | correctness、QEMU smoke、asm、retained board repeated、doctor | `retained_production_candidate` | 等用户提交或取消接入 |
| source-indexed direct gather production path | source-indexed-cloud-pair / valid 32-bit indices | path-hit、fallback、QEMU smoke、board repeated、doctor | `retained_production_candidate` | 等用户提交或取消接入 |
| dual-indexed direct gather production path | dual-indexed-cloud-pair / valid 32-bit source+target indices | path-hit、fallback、QEMU smoke、board repeated、doctor | `retained_production_candidate` | 等用户提交或取消接入 |
| correspondence direct index stream production path | correspondence-pair | production direct board repeated + doctor | `removed_from_production` | 另开专项或等待真实 workload 分布证据 |
| correspondence segment-load | correspondence-pair | Phase 009 board repeated | `rejected_with_evidence` | 不继续 |
| correspondence locality-aware dispatch | correspondence-pair | Phase 010 locality ablation | `rejected_with_evidence` | 需真实 workload 证据才可重开 |
| point type / layout expansion | `PointXYZI` / `PointXYZRGB` diagnostic | Phase 012 / 013 correctness、QEMU、board | `diagnostic_positive_with_warnings` | 不阻塞当前三类 production patch |

## 默认恢复队列

1. Phase 016 closeout：done；生产范围已收敛到三类 retained RVV。
2. 提交前检查：运行 `make evidence_status`、`git diff --check`，并扫描 stale wording。
3. 用户判断：提交当前接入，或取消接入并回滚 production patch / 长期文档。
4. 后续可选 topic：correspondence-pair 真实 workload 采样或新的 bounded production probe。

## 阶段反思

Phase 015 的四类 production public 证据把 diagnostic-to-production mismatch（诊断到生产错配）问题闭合了：
前三类在真实 public overload 中稳定 positive，correspondence 在中大规模 negative。Phase 016 因此不再把
correspondence 作为 retained production path，而是把它降回独立专项。这个结果也说明 row source policy
仍需要独立批准；ordered/source/dual 的正向不能外推到 correspondence。
