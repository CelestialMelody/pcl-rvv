# Phase 007 Plan：indexed direct-gather family expansion

## 阶段意图和边界

Phase 006 已证明 source-indexed-cloud-pair 的 direct indexed gather 在 test-rvv 内相对
staged ordered reuse 为 positive。本阶段继续 RVV 优化搜索，补齐 dual-indexed-cloud-pair
和 correspondence-pair 的 direct-gather family comparison。

本阶段仍然只修改
`test-rvv/registration/transformation_estimation_dual_quaternion` 的 test-support、bench、
脚本和 topic-local 文档，不修改
`registration/include/pcl/registration/impl/transformation_estimation_dual_quaternion.hpp`，
不重开 production integration loop。

## 术语合同

- `ordered-cloud-pair`：source[i] 与 target[i] 一一配对，使用完整点云，不含显式 index。
- `source-indexed-cloud-pair`：source[indices[i]] 与 target[i] 配对。
- `dual-indexed-cloud-pair`：source[source_indices[i]] 与 target[target_indices[i]] 配对。
- `correspondence-pair`：由 correspondence 的 query / match 两列形成点对。
- `direct gather`：只描述 indexed row ingress，不是所有 RVV 优化的总称。
- 活动 filter 和新的人类可读 label 使用 `ordered-cloud-pair`。

## 动作和完成判据

| action | 内容 | 完成判据 |
| --- | --- | --- |
| A1 terminology | 将当前测试对象、bench dataset 和 topic-local 文档改为 `ordered-cloud-pair` | active docs / new logs 使用 `ordered-cloud-pair` |
| A2 dual direct gather | source / target 两侧分别使用 index vector 和 `vluxei32`，共用现有 C1/C2 + solve | Std/RVV correctness 对拍通过，stats 明确 `used_gather=true`、`used_staging=false` |
| A3 correspondence direct gather | correspondence 转成双侧 index stream 后调用 dual direct gather | 非 identity correspondence correctness 对拍通过；不把 correspondence 语义外推 |
| A4 family bench | 输出三种 indexed policy 的 staged/direct pair | 同一 RVV binary、同一公式、solve、checksum 和 timer boundary |
| A5 board evidence | bounded 5-run、20 iterations、5 warm-up | 每个 policy 独立 summary / manifest / doctor；checksum 一致，方向稳定才保留结论 |

## 候选优先级

1. dual-indexed direct gather：两侧均为显式 index，最直接验证 gather ingress 是否能替代双侧 staging。
2. correspondence direct gather：复用 dual gather，但额外保留 query/match 展开语义边界。
3. hybrid / tiled gather：只有 direct gather 在某一 policy 不稳定时才进入，不预先扩大实现复杂度。

## 早停条件

- 任意 policy correctness 失败：保留 staged candidate，停止该 policy 的 board 采样。
- checksum 或 Evidence Doctor 不一致：先修证据合同，不形成性能结论。
- positive 只表示 test-rvv implementation family comparison，不授予 production adoption。
- production 仍保持标量；若要重开 production，必须另行取得 PI1 授权。
