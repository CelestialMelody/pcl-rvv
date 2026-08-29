# Phase 000 Plan: current state and FOUR_CORNERS diagnostic

本阶段建立 TrajkovicKeypoint3D（3D Trajkovic 关键点）的 response-only diagnostic（只测响应图的诊断）边界。阶段先用测试专用 helper 对 `detectKeypoints()` 中 `FOUR_CORNERS` normal stencil（四邻域法线模板）做标量 / RVV 对拍、QEMU correctness（QEMU 正确性）和反汇编归属；如果板卡可用，再做 repeated board（重复板卡性能测试）并用 Evidence Doctor（证据体检）解释结果。

## S0 偏好冻结

- `preferences_loaded`: defaults=loaded；local_override=absent；prompt_override=loaded，用户本轮允许“板卡 production 测试有收益即可采纳”，覆盖默认 PI5 采纳暂停规则。
- `work_preferences`: 测试资产和诊断代码使用详细中文注释；production 注释若后续接入则只写维护边界、fallback（回退路径）、dispatch（分流逻辑）和数据布局。
- `documentation_policy`: closeout current-state-first（当前状态优先），长期 `doc-rvv` 只在 production direct（真实生产路径证据）收益成立并采纳后创建。
- `evidence_policy`: summary-only；raw logs（原始日志）默认不提交。
- `commit_preferences`: 本轮不创建 commit；topic 产物、证据摘要和 agent instruction patch 若后续提交需拆分。

## 阶段意图和边界

| 项目 | 本阶段覆盖 | 本阶段不覆盖 |
| --- | --- | --- |
| 入口 | `TrajkovicKeypoint3D::detectKeypoints()` 的 `FOUR_CORNERS` response map 公式 | `EIGHT_CORNERS`、NMS 排序 / occupancy / `push_back` |
| 数据 | organized `PointXYZ` + precomputed `pcl::Normal`，`width * height` 连续数组 | normal estimation 前置成本、indices subset、非 organized 输入 |
| 点类型 | `PointInT=PointXYZ`、`NormalT=pcl::Normal` 的测试边界 | 泛型 `PointInT` / `NormalT` traits 扩展 |
| 证据角色 | diagnostic / production-shaped 前置候选 | production adopted behavior |

## 当前状态清单

- 队列来源：`doc-rvv/library-screening/keypoints/keypoints-function-evaluation-queue.zh.md` 第 7 节仍标记 Trajkovic 3D 为“未开始”。
- production 源码：`keypoints/include/pcl/keypoints/impl/trajkovic_3d.hpp` 当前无 RVV 分流；热点是内部 `(width - half_window) * (height - half_window)` stencil。
- 本 topic 之前没有 `test-rvv/keypoints/trajkovic_3d` 目录，也没有当前 Handoff。
- 当前工作区已有无关 dirty：`test-rvv/.gitignore`、`doc-rvv/library-screening/recognition/recognition-retained-candidate-rescreen.zh.md` 和多个 recognition topic 未跟踪产物；本阶段不触碰。

## 候选族和假设

| candidate family | 假设 | 风险 | 本阶段动作 |
| --- | --- | --- | --- |
| `four-corners-response-rvv` | 4 个邻域法线的 dot / squared diff / sqrt / min 公式适合 VL chunk（可变向量长度分块）和 strided load（跨步加载） | finite gate（有限值门控）和 `getNormalOrNull()` 的 null normal 语义必须对齐；QEMU 不代表性能 | 新增测试 helper、gtest、asm gate 和板卡 bench |
| `eight-corners-response-rvv` | 8 邻域公式同构，但标量路径使用多个小 `std::vector<float>` | 公式更长，先不与 Phase 000 混合 | roadmap 暂缓到下一 phase |
| `production-dispatch` | 若 response-only 板卡收益稳定，可把 helper 接入 production `detectKeypoints()` | 必须先有 PI1 范围、fallback、真实入口测试和 production direct board | Phase 000 后按证据进入 PI1 |

## 优化矩阵

主矩阵路径：`test-rvv/keypoints/trajkovic_3d/doc/phases/optimization-matrix.zh.md`。

| candidate family | row source policy | point type / Scalar / layout | correctness / fallback target | bench / board | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `four-corners-response-rvv` | organized dense full image | `PointXYZ` + `pcl::Normal` / `float` / AoS | `run_test_compare` | planned board repeated | `check_trajkovic_3d_rvv_asm` | planned | planned |

## 实现和测试动作

| 动作 | 产物 | 完成判据 |
| --- | --- | --- |
| RED | `src/test_trajkovic_3d.cpp` 引用尚未实现的 response helper | `make -C test-rvv/keypoints/trajkovic_3d run_test_rvv` 因缺少 helper 实现失败 |
| GREEN | `include/impl/trajkovic_3d_response.hpp` 实现 Std/RVV response helper | `run_test_compare` 通过 |
| ASM | RVV test binary 反汇编检查 | strided load、multiply、sqrt 指令可归属到 helper |
| BOARD | 板卡 repeated bench 与 Evidence Doctor | 5-run 或本阶段预算内 repeated summary 有 decision bucket |

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | Phase 000 是 `diagnostic`，后续 production 接入必须转为 `production-public` 或 `production-detail`。 |
| A/B boundary | 当前是 test helper；不是 public overload。 |
| 当前决策问题 | 先回答 `RVV-vs-scalar` response map 是否值得进入 PI1。 |
| diagnostic 是否可外推到 production | 只能外推为“值得生产探针”；不能直接外推为 adopted production。 |
| comparison-boundary / baseline mismatch 风险 | 有，Phase 000 不包含 normal estimation 和 NMS。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 只有源码风险很低、真实入口计时边界可控且用户仍授权时才允许；否则保留诊断结论。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前没有已有 Trajkovic 3D RVV family，不需要 RVV-vs-RVV；但需要 production direct Std/RVV。 |

## 阶段完成条件

- correctness / QEMU / asm / board / Evidence Doctor 均闭合且收益稳定：进入 PI1 production integration plan（生产接入计划）。
- diagnostic 为 weak-positive：按用户本轮偏好，若 production direct 后仍有板卡收益可采纳；Phase 000 只推进到 PI1，不在诊断边界采纳。
- diagnostic neutral / negative / unstable：记录不建议接 production 的当前证据，并把 EIGHT_CORNERS 或 production-shaped profile 放入 roadmap。

## 下一阶段默认入口

默认恢复动作：完成 Phase 000 RED/GREEN/ASM/BOARD 后更新 result、matrix、roadmap 和 Handoff。若 board response-only positive，则创建 PI1，并在 PI1 能冻结范围时继续 PI2-PI5；本轮用户已授权 PI5 收益成立即可采纳并创建正式 `doc-rvv/keypoints/trajkovic_3d-RVV.zh.md`。
