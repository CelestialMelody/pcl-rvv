# ISS 3D Optimization Roadmap

## 当前边界

当前 topic 来自 `doc-rvv/library-screening/keypoints/keypoints-retained-candidate-rescreen.zh.md`
的 ISS 3D retained candidate（保留候选）。Phase 000 证明 `getScatterMatrix`
局部核的 component ablation（组件消融）为 positive；Phase 010 证明真实 public
`compute()` 边界只有 neutral 弱正向，因此 production patch 不采纳；当前 production 源码保持标量，probe 只作为历史证据保留。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| f64 vector reduction scatter | 当前源码中每邻域点 6 个 double 累加项；上一轮 f32 RVV checksum 不一致 | indexed neighbor list, `PointXYZ`, AoS | 减少每邻域点标量乘加与循环开销，同时保留 double 累加语义 | gather 成本、f64 RVV 吞吐、reduction 顺序误差、生产公开入口中的 search / Eigen 稀释 | QEMU correctness、asm、board smoke、bounded repeated board、Evidence Doctor | attempted_positive diagnostic | 000-current-state-and-scatter-diagnostic completed |
| f32 accumulation scatter | 上一轮临时候选 | indexed neighbor list, `PointXYZ`, AoS | RVV 单精度更快 | checksum 与生产 double 累加不一致，不能作为 production-ready 语义 | 仅保留为 historical negative；不继续推进，除非另设 relaxed numeric topic | rejected with evidence | none |
| contiguous neighbor fast path | bench case 中连续索引局部性更好 | neighbor list 近似连续或可提前判定连续 | 可能降低 gather 成本 | production search 返回的邻域顺序通常不保证连续；判定成本可能抵消收益 | case-filter A/B、production-shaped input audit、board repeated | rejected_for_current_production | none |
| production bounded probe | 用户本轮允许正向生产证据后自行采纳 | `ISSKeypoint3D::getScatterMatrix` 的生产 detail | 判断 diagnostic 收益是否能穿透真实生产调用 | 公开入口仍包含 search、EVD、NMS；模板点型和 fallback gate 需谨慎 | PI1 plan、production direct correctness/fallback、asm、board production bench、Evidence Doctor | removed_no_production | none |
| nonzero-output public input | Phase 010 public checksum 为 0，输出语义弱 | public `compute()` with better synthetic or fixture cloud | 验证弱正向是否在真实 keypoint 输出下仍存在 | 需要设计输入并重新跑 board repeated；可能只是测试输入改进，不改变实现 | new phase plan、QEMU smoke、board repeated、doctor | turn_stop_deferred with stop_condition_hit | needs user decision because current production probe is not adopted |
| profile-guided broader hotspot search | public compute 稀释 scatter 收益 | search / EVD / NMS / output | 找到更大端到端收益点 | 会扩大到新的函数族或 topic | profile on board、new screening/evaluation | rejected_for_current_topic | separate topic if requested |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 000 | f64 vector reduction replaces earlier f32 candidate | board smoke 暴露 checksum mismatch；生产 scatter 使用 double 累加 | 重新跑 correctness、asm、board smoke/repeated | high |
| 010 | production reduction shape micro-opt | production helper 对称填矩阵时重复规约 c01/c02/c12 | RED/green shape gate、QEMU、asm、production repeated board | completed |
| 010 | public compute nonzero-output case | 当前 public checksum 为 0，弱化输出语义覆盖 | 新输入设计和 repeated board | blocked_by_user_decision |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| f32 accumulation scatter | 与生产 double scatter 语义不一致，checksum mismatch 会污染 production 判断。 | 只有用户明确接受 relaxed numeric boundary，或另开近似数值 topic。 |
| search / Eigen / NMS wide vectorization | 当前 retained candidate 建议先隔离 scatter；直接扩大到全 pipeline 会混入邻域搜索、求解器和输出顺序风险。 | scatter production direct 证据正向或 profile 证明其它阶段是更高收益热点。 |
| current production scatter patch adoption | production public median 1.014x，manifest bucket 为 neutral；收益不足以抵消长期维护和未覆盖点型成本。 | 用户明确接受 near-threshold 风险并决定重新引入，或新 production public case / profile 提供更强收益。 |

## Roadmap Default Recovery Queue

| order | action | status | recovery condition |
| --- | --- | --- | --- |
| 1 | Phase 000 f64 scatter diagnostic closeout。 | completed | result、matrix、evaluation 和 evidence registry 已同步。 |
| 2 | Phase 010 production bounded probe。 | completed_neutral_not_adopted | public compute repeated board 为 neutral。 |
| 3 | Topic-local doc suite、evaluation、Handoff。 | completed | 当前文档套件已补齐；`doc-rvv` not_applicable。 |
| 4 | 提交 no-production closeout。 | ready_for_commit | bounded budget 用完且收益 neutral；production patch 已移除，当前 topic 无未阻塞自动优化动作。 |
