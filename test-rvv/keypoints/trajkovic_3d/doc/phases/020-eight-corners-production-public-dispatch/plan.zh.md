# Phase 020 Plan: eight corners production public dispatch

本阶段延续 Trajkovic 3D topic 的 production integration loop（生产接入闭环），把 `EIGHT_CORNERS`
response map（响应图）推进到真实公开入口 `Detector::compute(output)` 边界。阶段只验证
`method_=EIGHT_CORNERS`、`window_size_=3`、precomputed normals（预计算法线）、organized dense full cloud
（有组织稠密完整点云）且 `PointXYZ + pcl::Normal` float AoS layout（结构数组布局）满足时的 RVV 分流；
同时把既有 `FOUR_CORNERS` adopted scope（已采纳范围）当作回归控制一起重跑。

## S0 偏好冻结

- `preferences_loaded`: defaults=loaded；local_override=absent；prompt_override=loaded，用户允许“板卡测试有收益即可采纳”。
- `work_preferences`: 测试资产和诊断代码用详细中文注释；production 注释只保留维护边界、fallback（回退路径）、dispatch（分流逻辑）和数据布局。
- `documentation_policy`: closeout current-state-first（当前状态优先），长期 `doc-rvv` 只写 adopted production behavior（已采纳生产行为）和证据链。
- `evidence_policy`: summary-only；raw logs（原始日志）默认不提交。
- `commit_preferences`: 本轮不创建 commit。

## 阶段意图和边界

| 项目 | 本阶段覆盖 | 本阶段不覆盖 |
| --- | --- | --- |
| 入口 | `TrajkovicKeypoint3D::detectKeypoints()` 的 `EIGHT_CORNERS` response map | `normal estimation`、NMS RVV、其它 public API |
| 回归控制 | 既有 `FOUR_CORNERS` adopted scope 作为同边界回归控制 | 把 `FOUR_CORNERS` 当成新候选重做一遍 |
| 数据 | organized dense full cloud、precomputed normals、`PointXYZ + pcl::Normal`、`window_size_=3` | 非 dense、indices subset、点类型扩展、其它 window size |
| 证据角色 | production public + 回归控制 | 诊断证据替代 production 证据 |

## 当前状态清单

- `EIGHT_CORNERS` 在当前 production 源码里仍走标量分支。
- 现有 topic-local 文档和 Phase 010 只覆盖 `FOUR_CORNERS` production public。
- 2D 同类 topic 已证明 `EIGHT_CORNERS` 生产 public 路径可以与标量 fallback 并存；3D 只能借结构，不借结论。

## Experience Migration Audit

| sibling 经验维度 | sibling topic 里的机制 | 当前 topic 是否适用 | 状态 | 证据 / 理由 | 下一步 |
| --- | --- | --- | --- | --- | --- |
| row source | 2D response grid 也是 organized full public compute | 是 | adopted | 3D 同样是 organized dense full cloud；只换成 normal field 语义 | 保持相同 public entry 边界 |
| shared math pipeline | 2D 用 VL chunk 计算 4/8 邻域 response，再做阈值和复核 | 是 | adopted | 3D 的 8 邻域 normal 公式和组织方式同构，只是输入从 intensity 变成 normal diff | 复用同类 RVV chunk 组织 |
| staging / reduction | 2D 对 8 邻域使用分组临时量和最小值归并 | 是 | attempted | 3D 也需要 4 组 r/b/a/d 临时量，但 normal 语义不同，不能照搬变量名 | 以公式等价为准，不照搬布局 |
| formula / FMA | 2D 用 `vfadd` / `vfmul` / `vfmacc` 组织乘加树 | 是 | adopted | 3D 的 dot / quadratic 结构同样适合 FMA 组织 | 直接复用 FMA 形态 |
| evidence model | 2D 采用 correctness + asm + board repeated + doctor | 是 | adopted | 当前 topic 也需要同一证据链，只是 board case label 不同 | 按同级证据闭环 |
| production boundary | 2D public compute + fallback + scalar NMS | 是 | adopted | 3D 也保留 scalar NMS，只扩展 response map | 维持 fallback 边界 |
| point type / accessor | 2D intensity accessor 和 3D normal layout 不同 | 否 | rejected | 2D 的 intensity accessor 不能直接解释 3D normal layout | 只借结构，不借 accessor 结论 |

## 候选族和假设

| candidate family | 假设 | 风险 | 本阶段动作 |
| --- | --- | --- | --- |
| `eight-corners-response-rvv` | 8 邻域 normal 公式和 2D 8-corner 形态同构，适合 VL chunk 和 FMA 组织 | 公式更长，响应值浮点细差可能影响 NMS 排序 | 新增生产 RVV helper、gtest、asm gate 和 board repeated |
| `four-corners-regression` | 新 dispatch 不应回归已采纳 FOUR_CORNERS | shared helper 改动可能影响既有 board truth | 回归测试必须一起跑 |
| `point-type-expansion` | 若 8-corner 结果正向，下一步可考虑更宽点型 | 当前仍需独立 traits / layout 证据 | 继续留在 roadmap，不在本阶段实现 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `eight-corners-response-rvv` | organized full cloud with precomputed normals | `PointXYZ` + `pcl::Normal` / `float` / AoS | `Detector::compute(output)` with `method_=EIGHT_CORNERS` | `run_test_compare` with EIGHT_CORNERS and FOUR_CORNERS regression control | public bench `--mode public` | 5-run repeated board with production_public cases | `check_trajkovic_3d_rvv_asm` must match eight-corner helper | Errors / Warnings / Suggestions explicitly reviewed | planned | implement helper and tests |

## 实现和测试动作

| 动作 | 产物 | 完成判据 |
| --- | --- | --- |
| GREEN | `include/impl/trajkovic_3d_response.hpp` 新增 eight-corners helper | Std/RVV 对拍通过 |
| PROD | `keypoints/include/pcl/keypoints/impl/trajkovic_3d.hpp` 接入 `EIGHT_CORNERS` 分流 | 真实 `compute()` 命中 RVV 且 FOUR_CORNERS 不回归 |
| ASM | RVV test/bench binary 反汇编检查 | eight-corner helper 的 strided load / FMA / sqrt / divide 可归属 |
| BOARD | repeated board + Evidence Doctor + registry 刷新 | 5-run summary、doctor、manifest 都登记且可复核 |
| DOC | phase result、roadmap、matrix、evaluation、README、长期 `doc-rvv` | 证据若正向则同步刷新，若负向则只写诊断边界 |

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | 本阶段目标是 `production_public`，不是诊断证据。 |
| A/B boundary | `public_compute`，Std/RVV 两个二进制都走真实 `Detector::compute(output)`。 |
| 当前决策问题 | `RVV-vs-scalar`：EIGHT_CORNERS public path 是否值得进入 adopted scope。 |
| diagnostic 是否可外推到 production | 只能作为设计启发，不能直接外推。 |
| comparison-boundary / baseline mismatch 风险 | 有，8 邻域公式更长，NMS 对细小差异更敏感。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 若 board 不 positive，则保留证据并停下，不扩大到点型或 NMS。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前没有第二个 adopted 3D family；不需要 RVV-vs-RVV，但需要 Std/RVV public A/B。 |

## 阶段完成条件

- correctness / QEMU / asm / board / Evidence Doctor 全闭合。
- `FOUR_CORNERS` 回归控制保持与当前 adopted truth 一致。
- board positive 且证据可解释时，更新 roadmap、matrix、evaluation 和长期文档。

## Board 预算和决策桶

- 默认 5-run repeated board。
- `median >= 1.20x` 且无低于 `1.0x` 的 run：positive。
- `1.05x <= median < 1.20x`：weak-positive，需要结合复杂度再判断。
- `0.95x <= median < 1.05x`：neutral。
- `median < 0.95x`：negative。
- 方向摇摆且预算耗尽：unstable。

## Continue / Stop Decision

默认继续到实现和测试；若 `EIGHT_CORNERS` 没有值得继续的 board 信号，或者 board 访问异常，则停在本 phase 并只做收尾。

## 文档更新清单

- phase result
- optimization matrix
- optimization roadmap
- evaluation
- README
- `doc-rvv/keypoints/trajkovic_3d-RVV.zh.md`（仅在采纳后）

## Roadmap 同步动作

- 新增 `eight-corners-response-rvv` 的生产 public 证据。
- 保留 `point-type-expansion` 作为后续 phase。
- 如果 board 不 positive，明确把该路线降级为 deferred 或 rejected with evidence。
