# Phase 030 计划：PI5 user confirmation packet

## Phase 目标和授权边界

本阶段只整理 Phase 010 production integration loop（生产接入闭环）在 PI5 用户检查点需要人工确认的信息。目标是让用户不用回读所有 phase 文档，就能看到当前 production patch（生产补丁）的源码范围、真实公开入口、fallback（回退路径）边界、可复现证据、Evidence Doctor（证据体检）异常和可选决策。

本阶段不修改 `registration/include/pcl/registration/transformation_estimation_svd_scale.h` 或 `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp`，不把 `production_patch_positive_pending_user_confirmation` 改成 adopted production behavior（已采纳生产行为），不创建 `artifact_layout.topic_doc_template` 解析出的 production 长期主题文档，也不回滚当前 production patch。

## 当前 evidence baseline

| source | 当前事实 |
| --- | --- |
| Phase 010 result | `direct-fused-scale-accum` 已接入 ordered public overload，证据支持采纳，但 PI5 等待用户确认。 |
| production diff | scale 子类新增 ordered overload override；RVV 构建下按 `Scalar=float`、dense、layout-gated xyz AoS、等长、`nr_points >= 16` 尝试 RVV fused accumulation，失败回父类路径。 |
| QEMU correctness | 当前 `run_test_compare` 已在 Phase 020 rerun 到 Std/RVV 7 tests。 |
| ASM attribution | production ordered overload 符号内可见 RVV strided load、FMA、ordered reduction 和 `vsetvli`。 |
| board production direct | `production_public_scale_ordered_cloud_pair_repeated` 5-run summary 为 positive；4K/64K/256K median B/A = `26.123x` / `33.860x` / `33.066x`。 |
| Evidence Doctor | production direct board doctor：`Errors=0`、`Warnings=1`；4K group_outlier 需要按 size 分开解释，不阻塞当前窄范围 positive。 |
| roadmap / matrix | 默认恢复入口是 PI5 用户确认；generic point type、row source 和 matrix-local production probe 都停在该边界。 |

## 本轮假设

- 一个集中确认包可以降低 PI5 人工判断成本，并避免下一轮 worker 把“继续”误读为 adopted 或 rollback。
- 生产证据已经足够支持“建议采纳当前窄 production patch”，但最终状态必须由用户确认。
- 若用户确认采纳，下一阶段应进入 adoption closeout：更新 EvidenceDecision、创建 / 刷新 production 长期主题文档、同步 README / evaluation / matrix / roadmap，并准备 topic commit 边界。

## 执行步骤

| step | action | completion evidence |
| --- | --- | --- |
| 1 | 新增本 phase plan。 | 本文件存在，并写清不越过 PI5。 |
| 2 | 新增本 phase result / confirmation packet。 | `result.zh.md` 汇总源码 diff、证据、风险、选项和下一步。 |
| 3 | 同步 phase index 和 roadmap 默认恢复入口。 | `doc/phases/README.zh.md` 与 `doc/optimization-roadmap.zh.md` 指向 Phase 030 确认包。 |
| 4 | 运行 freshness / markdown / diff whitespace 检查。 | `evidence_status`、relative link check、`git diff --check` 通过或记录失败原因。 |

## 暂停条件和回滚条件

如果发现 production source diff 与 Phase 010 result 不一致、registry stale、summary 缺失、Evidence Doctor 出现 error，停止并先刷新证据，不输出采纳建议。若用户明确说“不采纳 / 回滚”，另建 rollback/no-production phase，回滚只限当前 production patch，并保留 topic-local probe 证据。
