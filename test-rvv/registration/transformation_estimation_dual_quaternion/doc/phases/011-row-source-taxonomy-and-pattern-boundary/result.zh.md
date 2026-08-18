# Phase 011 Result：row-source taxonomy and pattern boundary

## 当前结论

Phase 011 完成整理后重启。当前 topic 的活动命名固定为四类 production
`row_source_policy`：

- `ordered-cloud-pair`
- `source-indexed-cloud-pair`
- `dual-indexed-cloud-pair`
- `correspondence-pair`

`contiguous`、`local-window`、`strided` 被明确降级为
`row_source_policy=correspondence-pair` 下的 `index_pattern`。它们不是新的
production 数据流，也不是独立 row-source policy。

## 动作回填

| action | 状态 | 证据 / 产物 | 结论 |
| --- | --- | --- | --- |
| A1 taxonomy docs | done | README、testing overview、benchmark/evidence、roadmap、matrix、evaluation | 活动文档按 `row_source_policy / index_pattern / candidate_family / evidence_role` 解释 Phase 010。 |
| A2 test-support wording | done | `include/impl/tedq_adapters.hpp`、`src/test_tedq.cpp`、`src/bench_tedq.cpp` | 注释和 bench banner 不再把 index pattern 写成 row-source。 |
| A3 old naming cleanup | done | README、testing overview、benchmark/evidence | 活动说明使用四类 row-source policy；实际历史 evidence 路径不批量重命名。 |
| A4 next phase | done | phase README、roadmap、matrix | 下一恢复入口为 `012-point-type-layout-expansion`。 |

## 证据边界

本阶段没有新增性能 candidate，也没有改变 production header。它只整理证据坐标系：
Phase 008 仍是 correspondence direct index stream baseline；Phase 009 segment stream
仍为 rejected evidence；Phase 010 locality-aware production candidate 仍为 rejected
evidence。

## 继续 / 停止决定

- 当前阶段：`done / taxonomy adopted`。
- 当前 topic：`diagnostic / no-production`。
- `next_phase_default`：`012-point-type-layout-expansion`。
- Phase 012 优先覆盖 `correspondence-pair` direct index stream 的 `PointXYZI` 和
  `PointXYZRGB`，再决定是否扩大到 source-indexed / dual-indexed direct gather family。
