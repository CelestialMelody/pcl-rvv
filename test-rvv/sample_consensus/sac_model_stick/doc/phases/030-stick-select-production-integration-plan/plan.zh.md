# Phase 030: stick select production integration plan

## PI1 计划边界

本文件只记录 `SampleConsensusModelStick<PointT>::selectWithinDistance` 的 production integration plan（生产接入计划）。它不是 production patch（生产补丁），不授权修改 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_stick.hpp`，也不把 Phase 020 的 test-only diagnostic（测试专用诊断）提升为 production direct evidence（真实生产路径证据）。

进入 PI2 前必须得到用户明确授权。

## 候选范围

| 维度 | 冻结范围 |
| --- | --- |
| production entry | `SampleConsensusModelStick<PointT>::selectWithinDistance` |
| first production scope | `PointXYZ` 或 traits-gated（字段特征门禁）float xyz AoS direct indexed `indices_`，以 Phase 020 已验证的 `PointXYZ` 为首轮证据 |
| algorithm family | indexed xyz gather + cross3/squaredNorm + `< threshold^2` mask + `vcompress` 保序输出原始 index + compressed float squared-distance 转 double 写 `error_sqr_dists_` |
| fallback | 非 RVV 构建、非支持布局、`pcl::index_t` 非 32-bit signed、offset 超出 `u32` byte offset、其它未证明点型或 layout 均落回原标量语义 |
| excluded | `countWithinDistance` production patch、`getDistancesToModel`、identity fast path、泛型点型最终结论、真实 RANSAC 上游性能 |

## PI2-PI5 证据计划

| step | 需要产物 | 验收 |
| --- | --- | --- |
| PI2 production patch | `sac_model_stick.h/.hpp` 中窄范围 helper / dispatch，保留清晰 Standard fallback | 公开 API 不变；公开入口不保留大段重复标量主体；注释只说明 gate / fallback / `vcompress` 输出边界。 |
| PI3 correctness | production direct Std/RVV tests | 公开入口与 Standard helper 对拍，覆盖保序 inliers、`error_sqr_dists_`、无命中清空旧状态和 fallback。 |
| PI4 static evidence | `dump_bench_rvv` 或 dedicated asm target | production helper / public path 可归属 RVV 指令；若内联，manifest 记录真实承载边界。 |
| PI4 board evidence | production public repeated board | 5-run bounded budget；性能结论只来自板卡或目标硬件。 |
| PI4 doctor / registry | production manifest、Evidence Doctor、registry | Errors 必须修正或降级；Warnings / Suggestions 必须在 result 和 Handoff 解释。 |
| PI5 user checkpoint | 保留 patch，报告 diff、命令、证据和拟议下一步 | 无论 positive、weak、negative 或 unstable，都暂停等待用户确认采纳或回滚。 |

## diagnostic-to-production mismatch audit

Phase 020 candidate 在 test-only helper 中达到 positive-stable：median/min/max 为 `3.4432x / 3.4208x / 3.4761x`。它支持 bounded production probe，但不证明 production dispatch 已经存在。PI2 后必须在同一 production public boundary（生产公开边界）内重跑 correctness、asm、board repeated 和 Evidence Doctor。

## 当前停止条件

`pending_user_authorization_for_PI2`。没有用户明确授权前，worker 不修改 production 源码，不创建长期 `doc-rvv/sample_consensus/sac_model_stick-RVV.zh.md`，不把本计划写成 adopted production behavior。
