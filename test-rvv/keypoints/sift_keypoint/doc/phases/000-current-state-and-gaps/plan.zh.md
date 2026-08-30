# Phase 000 Plan: current state and gaps

本阶段先把 SIFT keypoint 的 scale-space 热段拆成可测、可解释、可继续推进的诊断边界。
优先回答 `computeScaleSpace()` 里的 Gaussian weight loop 是否值得先做 RVV candidate；
`findScaleSpaceExtrema()` 先保留 scalar reference。当前阶段已经进入 production-public probe
（生产公开入口探针）：如果真实 `SIFTKeypoint::compute()` 的 Std/RVV 输出 fingerprint
（正确性指纹）不一致，先补 public output trace（公开入口输出跟踪）和容差对拍，再决定是否修
production patch 或降级证据边界。

## S0 偏好冻结

- `preferences_loaded`: defaults=loaded；local_override=absent；prompt_override=loaded。
- `work_preferences`: 诊断和测试资产用详细中文注释；production 注释若后续接入则只解释边界、fallback、dispatch 和数据布局。
- `documentation_policy`: current-state-first；当前不创建正式 `doc-rvv` 长期文档。
- `evidence_policy`: summary-only；raw logs 默认不提交。
- `commit_preferences`: 本轮不创建 commit。

## 阶段意图和边界

| 项目 | 本阶段覆盖 | 本阶段不覆盖 |
| --- | --- | --- |
| 入口 | `computeScaleSpace()`、`findScaleSpaceExtrema()` 的诊断边界；production-public probe 覆盖真实 `SIFTKeypoint::compute()` | 不把 public probe 直接外推成所有点型 / 所有输入分布 |
| 数据 | synthetic sorted neighborhood batches；`PointXYZI` organized dense public cloud | 不同点型 traits 扩展、indices / 非 organized 输入 |
| 证据角色 | diagnostic / profile prerequisite；production-public correctness probe | 未通过 correctness 前不能写 production adopted behavior |

## 当前状态清单

- 生产源码还停留在 `keypoints/include/pcl/keypoints/impl/sift_keypoint.hpp` 的原始实现。
- 本 topic 之前没有独立的 `test-rvv/keypoints/sift_keypoint` scaffold。
- 当前工作区仍有无关 dirty：`doc-rvv/library-screening/recognition/recognition-retained-candidate-rescreen.zh.md` 等文件；本阶段不触碰。

## 候选族和假设

| candidate family | 假设 | 风险 | 本阶段动作 |
| --- | --- | --- | --- |
| `gaussian-weight-loop-rvv` | Gaussian 权重、numerator / denominator 规约适合做 RVV candidate | early break、exp 语义和 lane tail 需要解释 | 新增 helper、gtest、bench、asm gate |
| `public-output-trace` | public benchmark checksum 不一致可能来自输出差异或 fingerprint 口径过严 | 若关键点集合或 scale 发生变化，production correctness 不闭合 | 增加 public trace 输出、对比脚本和 board trace target |
| `extrema-scan-rvv` | DoG 上的局部极值扫描可能是第二个候选 | 邻域选择和 search tree cost 更复杂 | 先暂缓 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | correctness / fallback target | bench / board | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `gaussian-weight-loop-rvv` | synthetic sorted neighbor batches | synthetic float batches / `float` / AoS-ish | `run_test_compare` | planned repeated board | `check_sift_keypoint_rvv_asm` | planned | in progress |
| `public-output-trace` | organized dense public cloud | `PointXYZI -> PointWithScale` / `float` / organized input | board Std/RVV trace compare | no-warmup 1-run board trace only | production bench asm reuse | trace compare, then doctor | planned |

## 实现和测试动作

| 动作 | 产物 | 完成判据 |
| --- | --- | --- |
| RED | helper declarations + gtest/bench | 首次 make 失败，暴露缺失实现 |
| GREEN | `include/impl/sift_keypoint_scale_space.hpp` | helper 对拍通过 |
| ASM | bench RVV binary 反汇编 | 能看到 `expf_RVV_f32m2` / `vfredosum` / `vle32` |
| BOARD | repeated board summary / manifest / doctor | 5-run 或预算内 repeated evidence 稳定 |
| PUBLIC_TRACE | public bench 输出 `PointWithScale` trace 并对比 Std/RVV | 说明 checksum mismatch 是语义差异、容差内差异，还是 fingerprint 口径问题 |

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `diagnostic` + `production-public`，后者只用于真实公开入口 Std/RVV 对拍 |
| A/B boundary | `test_helper` + `public overload` |
| 当前决策问题 | 先判断 `RVV-vs-scalar` 的 Gaussian weight loop 是否值得继续；随后判断 production patch 是否保持 public 输出语义 |
| diagnostic 是否可外推到 production | 诊断证据仅能外推为“值得继续问 production”；production-public trace 才能回答当前 public patch 的正确性 |
| comparison-boundary / baseline mismatch 风险 | 有；diagnostic 没有 radiusSearch 和完整 detector pipeline，public probe 只覆盖 `PointXYZI` organized dense case |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 需要先完成当前 helper 证据，再看板卡和用户授权 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前没有 adopted RVV family，不需要 |

## 下一阶段默认入口

默认恢复动作：先补 helper 和 test/bench 让 `run_test_compare` 进入可复核状态，再补 asm 和 repeated board 证据。如果 helper 结果显示收益，但完整 SIFT 入口仍有稀释，就继续把 `findScaleSpaceExtrema()` 和 public smoke 拆成下一 phase。
