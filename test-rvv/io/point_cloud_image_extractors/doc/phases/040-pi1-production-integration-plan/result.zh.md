# Phase 040 Result: PI1 production-integration-plan

## 实际执行范围

本阶段完成 PI1 production integration plan（生产接入计划）和 gate（准入条件）冻结。production
源码未修改；`io/include/pcl/io/impl/point_cloud_image_extractors.hpp` 仍保持当前标量实现。

## 计划动作回填

| action | status | 证据 |
| --- | --- | --- |
| production shape scan | done | 已检查 `PointCloudImageExtractorFromRGBField<PointT>::extractImpl`、`PointCloudImageExtractorWithScaling<PointT>::extractImpl` 和 base `extract` 的 NaN post-pass。 |
| gate / fallback freeze | done | 本阶段 plan 固定 RGB exact `PointXYZRGB/RGBA` 和 scaling exact `PointXYZI intensity full-range` 的窄范围；其它路径 fallback。 |
| PI2-PI5 evidence plan | done | plan 已列 production direct tests、production bench labels、asm、board repeated 和 Evidence Doctor 处理策略。 |
| production mutation | not_applicable with evidence | 本阶段是 PI1 plan-only；继续到 PI2 会修改 production header，需要用户确认。 |

## 冻结的 PI2 候选范围

| candidate | production entry | frozen scope | status |
| --- | --- | --- | --- |
| `rgb_segment_store_v1` | `PointCloudImageExtractorFromRGBField<PointT>::extractImpl` | exact `PointXYZRGB` / `PointXYZRGBA`，字段为 `rgb` 或 `rgba`，32-bit load + `vsseg3e8` store | ready_for_PI2_after_user_confirmation |
| `scaling_reduction_v1` | `PointCloudImageExtractorWithScaling<PointT>::extractImpl` | exact `PointXYZI`，`field_name_ == "intensity"`，`SCALING_FULL_RANGE`，float field strided load + `vfredmin/vfredmax` | ready_for_PI2_after_user_confirmation |

## 生产错配风险和处理

| 风险 | PI1 处理 | PI2 / PI4 验证 |
| --- | --- | --- |
| helper-level diagnostic 不含真实 `PCLImage` resize / step 赋值 | 生产 helper 必须直接写 `PCLImage`，并保持 `encoding`、`width`、`height`、`step` 与 Std 一致 | production direct gtest 对拍 `PCLImage` 全字段和 `data`。 |
| RGB `rgb` / `rgba` 历史兼容语义 | PI2 继续使用 `getFieldIndex<PointT>("rgb")`，失败后查 `rgba`，并接受 `FLOAT32` / `UINT32` 单字段 | `PointXYZRGB` 和 `PointXYZRGBA` gate-hit tests。 |
| scaling generic field name 过宽 | PI2 只允许 exact `PointXYZI` + `"intensity"` + full-range；Z / curvature / fixed-factor fallback | gate-miss tests 单独覆盖 non-full-range、Z / curvature 和小规模或非 exact 点型。 |
| NaN black post-pass 被绕过 | RVV 只接 `extractImpl`，base `extract` 的 post-pass 保持标量 | production direct test 先命中 RVV `extractImpl`，再检查 NaN 像素清零。 |
| diagnostic speedup 外推过度 | PI1 只给 bounded production candidate，不给 adopted 结论 | PI4 repeated board 后重新 EvidenceDecision。 |

## EvidenceDecision

`evidence_decision=production-integration-plan-ready`。

这不是 adopted production behavior（已采用生产行为）。它只说明 PI2 的最小生产补丁范围已经可审查：
如果用户明确确认进入 production integration loop（生产接入闭环），下一阶段可以按本计划修改
`io/include/pcl/io/impl/point_cloud_image_extractors.hpp`，然后执行 PI3-PI5。

## Continue / Stop Decision

`continue_stop_decision=turn_stop_deferred with stop_condition_hit`。

停止条件：下一步 `PI2-production-patch` 会修改 production header，需要用户明确确认。当前没有生产
diff 可采纳，也没有生产 diff 需要回滚。

`next_phase_default=PI2-production-patch after explicit user confirmation`。
