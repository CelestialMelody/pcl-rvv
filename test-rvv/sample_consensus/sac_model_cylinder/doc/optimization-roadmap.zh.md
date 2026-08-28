# sac_model_cylinder optimization roadmap

## 当前边界

当前 topic 已完成三入口 production adoption（生产采纳）：`countWithinDistance`、`selectWithinDistance` 和
`getDistancesToModel` 在真实公开入口下接入 RVV（RISC-V Vector，可变长向量），并通过 production direct
5-run board repeated、QEMU correctness、反汇编归属和 Evidence Doctor。Phase 040 已把接入后板卡证据扩展到
`PointXYZ + Normal`、`PointXYZI + Normal`、`PointXYZRGB + Normal` 和 `PointXYZ + PointNormal` 四组代表点型。

采纳范围覆盖 direct indexed `indices_`、上述四组代表点型、float xyz/normal AoS（结构数组）布局、
`Eigen::VectorXf` model coefficients 和 65536 点 shuffled adjacent pairs bench case。自定义点型全集、
`Scalar=double`、identity-index 专门优化、真实 workload profile 和其它 public entry 仍需单独 phase 或 profile 触发。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| count/select indexed-gather + radial-norm + normal-angle | normal-plane 法线角度、line/stick 轴向几何结构经验 | direct indexed `indices_`, `PointXYZ + Normal` | count mask popcount 与 select `vcompress` 覆盖主循环 | `sqrt/acos` 成本、近阈值误差、early gate 等价性 | correctness、QEMU、asm、board、Doctor | adopted | completed in Phase 020 |
| getDistances full-RVV dense double store | circle/line/stick full-RVV sqrt + double store 经验 | public `getDistancesToModel` dense output | 避免 scalar sqrt/acos/store 循环 | 全量 normal angle 成本、float/double 误差、checksum policy | correctness、bench、asm、board、Doctor | adopted | completed in Phase 030 |
| point-type / layout expansion | 当前 production gate 是 traits-based，Phase 040 补了代表点型证据 | `PointXYZI + Normal`、`PointXYZRGB + Normal`、`PointXYZ + PointNormal` | 扩大常见点型命中范围 | 不证明自定义点型全集和未测复合点型性能 | representative correctness、asm、board、Doctor | adopted | completed in Phase 040 |
| identity-index strided load A/B | plane/circle/line 经验显示不能默认升级 | identity `indices_` workload | 如果真实 workload 以 identity 为主，可能减少 gather 成本 | sibling 中多次负向或窄正向，可能伤害通用 shuffled case | 同一 production boundary RVV-vs-RVV A/B、board、Doctor | deferred | 仅 profile 触发 |
| optimizeModelCoefficients staging | 源码中 inliers -> Eigen arrays 后进入 LM helper | optimization stage | 可能减少数组组装成本 | solver 可能主导，距离三入口已更直接 | 上游 profile、component ablation | not_applicable now | profile prerequisite |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 020 | production direct environment metadata 补齐 | repeated board 已稳定，但 manifest 中 taskset / governor / freq / temperature / binary hash 仍未记录。 | board wrapper 增加 metadata 采集；不改变当前 decision bucket。 | medium |
| 030 | getDistances checksum policy 固化 | 浮点距离 hash 曾误报 checksum mismatch，逐项数值应由 gtest 负责。 | 保留 bench-shaped correctness；manifest checksum 只保护 dense vector size。 | completed |
| 030 | point-type expansion | 三入口已正向，下一步最自然的覆盖面扩展是更多 source / normal 点型。 | 新 phase plan、代表点型、fallback、asm、5-run board 和 Doctor。 | high if continuing this topic |
| 040 | 代表点型扩展已闭合 | `PointXYZI + Normal`、`PointXYZRGB + Normal` 和 `PointXYZ + PointNormal` 均为 positive，Doctor 0/0/0。 | 若要继续扩大到 `PointXYZRGBA`、`PointXYZINormal` 或自定义点型，应先有调用价值或 profile 触发。 | completed |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| identity-index strided load | 已完成 sibling 主题中多次显示 identity strided load 不是默认升级；当前 shuffled production 结果已强正向。 | 真实 workload/profile 显示 identity indices 占主导，且 gather 成本成为主要瓶颈。 |
| optimizeModelCoefficients staging | 每次模型优化只对 inliers 组装 Eigen arrays 并调用 LM solver，不是当前三入口距离核。 | profile 显示 optimize staging 接近主成本，且不与 Eigen solver 优化重复。 |
| cone 自动继承 cylinder | cone 比 cylinder 多 height normalize、opening angle 和 cone normal 合成，不能直接外推。 | 以 cylinder 正向为来源启动 cone 独立评估。 |
| arbitrary custom point types | Phase 040 只批准三组代表点型；自定义点型虽然可能满足 traits gate，但不同 offset、stride 和 normal 组合仍需独立证据。 | 有真实调用点或用户点名点型时，新建窄 point-type phase。 |

## 默认恢复队列

| next action | 状态 | 恢复条件 | 停止条件 |
| --- | --- | --- | --- |
| `040-point-type-expansion` | completed / adopted | 已闭合代表点型 correctness、asm、5-run board 和 Doctor。 | 无。 |
| `identity-index-A/B` | deferred | 有真实 workload/profile 或用户点名 identity indices。 | 无 profile 时不默认运行。 |
| `cone 法线距离诊断` | next-topic candidate | cylinder 三入口 production 正向后，cone 依赖条件已解除。 | 需要新 topic phase，不属于 cylinder closeout。 |
