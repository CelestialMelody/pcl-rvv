# Phase 010 Production Integration Plan

## 阶段意图和边界

本阶段是 VFH（Viewpoint Feature Histogram，视点特征直方图）的 PI1 production integration plan
（生产接入计划）。它先把 Phase 000 的 `partial-production-candidate / PI1-plan-ready`
冻结成可审查的 production scope（生产范围）；Phase 030 完成后，生产探针候选优先级更新为
normal-centroid reduction + combined SPFH + viewpoint helper。PI1 本身不直接修改
`features/include/pcl/features/impl/vfh.hpp`。

当前授权边界仍是 `PointNormal -> VFHSignature308`、`float`、dense finite synthetic cloud、
默认 45/128 bin 布局、full-cloud sequential indices 和 public `VFHEstimation::compute()` 入口。
非 dense 输入、CVFH / OUR-CVFH callers、泛型点类型、`Scalar=double`、自定义 VFH 参数、
production dispatch（生产分流）和 `doc-rvv/features/vfh-RVV.zh.md` 长期主题文档都不在本阶段直接落地。

## 授权检查

| item | 状态 | 说明 |
| --- | --- | --- |
| production source 修改授权 | pending_user_confirmation | 当前 board 证据支持继续到 PI1，但还没有明确授权修改 `vfh.hpp`。 |
| PI5 采纳 / 回滚授权 | not_applicable_yet | 即便后续 PI1-PI5 跑完，也要先停在用户检查点。 |
| board availability | available | Phase 000、Phase 020 与 Phase 030 repeated board 已完成，Milkv-Jupiter 可用。 |
| dirty isolation | review_only | 只会继续碰 `test-rvv/features/vfh/**`、必要的 phase 文档和恢复入口。 |

## 生产补丁候选

| aspect | plan |
| --- | --- |
| scope | 先把生产候选收窄到当前 Phase 030 已验证的 dense finite `PointNormal` 和默认 VFH 参数；其它模板实例自然 fallback。 |
| helper split | 如果进入 PI2，优先把 `computePointSPFHSignature()` 的标量主体抽成 `Std` helper，再接 `RVV` helper；公开 `computeFeature()` 只做短路分流和回退。 |
| RVV work | 优先 probe `computeVFHSignatureCentroidsSPFHAndViewpointRVV` 对应的 reduction-combined 形态：xyz centroid 使用 common RVV，normal centroid、centroid-to-point pair math 与 viewpoint normal-dot preparation 走 RVV，histogram scatter 先保持标量顺序。 |
| fallback | 非 RVV build、非 dense、indices 不是 full-cloud sequential、点类型 / layout gate 不满足时走 Std。 |
| comments | production 注释只说明 gate、fallback、布局边界和 maintenance reason。 |

## 测试与证据动作

| action | 产物 | 验收 |
| --- | --- | --- |
| PI1-RED | 生产直连 gtest 或等价 smoke | 先证明当前 public `compute()` 还没有 production RVV 分流，并把后续 production gate 变成可失败条件。 |
| PI1-GREEN | 最小 production patch | 若用户确认修改 `vfh.hpp`，新增 bounded RVV 分流和 fallback。 |
| PI1-ASM | 生产 helper 反汇编归属 | 反汇编应能归到 production helper，而不只归到 test-only helper。 |
| PI1-BOARD | board smoke + repeated | 重新确认 public 入口 / candidate / baseline 在真实 production boundary 下的关系。 |
| PI5-CHECKPOINT | 保留 patch，等待用户检查 | 报告 diff、命令、board / Doctor 结果和采纳 / 回滚建议。 |

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production_public` / `production_detail`。Phase 000 / Phase 020 / Phase 030 只提供进入 PI1/PI2 的前置诊断。 |
| A/B boundary | 真实 `VFHEstimation::compute()` 与 production RVV helper / fallback。 |
| 当前决策问题 | 当前 public RVV path 是否能在真实生产边界里穿透 helper、fallback 和公开入口稀释。 |
| diagnostic 是否可外推到 production | 不能直接外推，必须重跑真实生产边界。 |
| comparison-boundary / baseline mismatch 风险 | 存在。Phase 000 / Phase 020 / Phase 030 的 test helper 与 public baseline 不是同一 production boundary。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 目前属于 positive diagnostic，但 production patch 仍需要用户确认；若后续 public baseline 仍接近 1x，则只保留 bounded probe，不做 clean adoption。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要。若未来出现多个 RVV family，必须同边界 A/B。 |

## 继续 / 停止条件

本阶段的默认恢复动作是先完成 PI1 计划文档，然后等待用户确认是否允许修改
`features/include/pcl/features/impl/vfh.hpp`。一旦确认，下一步就进入 PI2 production patch；
若不确认，则继续保留 Phase 000 / Phase 020 / Phase 030 的 diagnostic 资产和 PI1 计划，不把结果写成 adopted production。
