# Phase 020 Production Integration Plan

## 阶段意图和边界

本阶段是 PI1 production integration plan（生产接入计划），只冻结生产探针边界，不修改 `features/include/pcl/features/impl/pfh.hpp`。Phase 010 的 `pfh-pair-feature-staged-rvv` 在 test helper boundary（测试 helper 边界）上有稳定 `2.33x-2.35x` diagnostic positive（诊断正向），足以支持进入有界生产探针审计。

## 候选生产范围

| 项目 | 冻结范围 |
| --- | --- |
| production entry | `pcl::PFHEstimation<PointInT, PointNT, PointOutT>::computePointPFHSignature` 内部 helper 级别；公开 API 不变。 |
| 初始点类型 | phase-local exact `PointNormal -> PointNormal -> PFHSignature125` 或等价已证明点型；泛型 traits 扩展不在同一补丁内默认完成。 |
| Scalar / histogram | `float` histogram，`nr_split=5` 首先闭合；其它 split 只做 correctness/fallback，不默认采纳性能。 |
| row source | fixed neighborhood indices；不覆盖其它来源语义。 |
| 候选 family | staged pair-feature RVV：scalar pair staging、RVV tuple math、scalar ordered histogram scatter。 |
| 不触碰路径 | public API、KdTree search、indices 构造、其它 PFH/FPFH/VFH callers、长期 `doc-rvv` 主题文档。 |

## Fallback 与 gate 计划

| gate | PI1 决策 |
| --- | --- |
| `__RVV10__` | 非 RVV 构建保留原标量路径。 |
| 点类型 / layout | 生产补丁若只做 exact `PointNormal`，其它模板实例必须自然 fallback；若改做泛型，必须先证明 `x/y/z/normal_x/normal_y/normal_z` 单 float、POD、standard-layout、offset、stride 和 alignment。 |
| 规模 | pair count 过小或 `indices.size() < 2` 走原路径；阈值需由 PI4 bench 决定。 |
| finite / degenerate pair | 保持 production 有限点检查和零距离 / 零 cross norm pair 的跳过语义。 |
| histogram scatter | 本轮不向量化 scatter；必须保持 pair order 和 bin clamp。 |
| math helper | 使用 `atan2_RVV_f32m2`；`acos(abs())` 分支继续用 `abs(angle1) < abs(angle2)` 等价替换。 |

## PI2-PI5 验证计划

| 阶段 | 必须闭合的证据 |
| --- | --- |
| PI2 production_patch | 最小生产 diff；保留原标量 helper 或清晰 fallback；production 注释只解释边界。 |
| PI3 production_direct_tests | 真实 production helper / public entry correctness；非 RVV fallback；非覆盖点型 fallback；degenerate pair；bin boundary。 |
| PI4 production_evidence_rerun | `run_test_compare`、`dump_bench_rvv`、board smoke、5-run repeated、Evidence Doctor；manifest 必须标记 production boundary。 |
| PI5 decision | 无论证据正负都停在用户检查点；用户确认前不采纳、不回滚。 |

## Diagnostic-To-Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | Phase 010 是 diagnostic；PI4 才能产生 production-detail 或 production-public evidence。 |
| A/B boundary | 计划从 test helper boundary 迁移到 production detail helper boundary。 |
| 当前决策问题 | 是否允许 staged family 做有界 production probe；不决定最终采纳。 |
| diagnostic 是否可外推到 production | 可外推为“值得试探”，不可外推为“已经 production-ready”。 |
| comparison-boundary / baseline mismatch 风险 | staging 位置、helper inline、production object state、public search/output 稀释都可能改变收益。 |
| bounded production probe 条件 | 用户明确授权 PI2；patch 不扩大公开 API；fallback 全覆盖；PI4 repeated board 与 Doctor 闭合。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 如果只有 staged family，可先以 production Std/RVV 判断；若引入 direct-load family，则需要 RVV-vs-RVV family comparison。 |

## Point-Type Expansion Queue

| scope | status | resume condition |
| --- | --- | --- |
| exact `PointNormal` | planned for first production probe | 用户授权 PI2 后补 production direct tests 和 board evidence。 |
| PointNormal-like traits | deferred | 需要 normal field traits / AoS layout gate 审计和 fallback tests。 |
| mixed `PointInT` / `PointNT` | deferred | 需要分别审计 source xyz 与 normal cloud xyz/normal 字段访问；不能从 `PointNormal` 外推。 |
| `Scalar=double` 或非 float histogram | not_applicable for first probe | 当前 PFH histogram 是 float；如源码入口变化再复查。 |

## Stop Conditions

本阶段之后若继续到 PI2，必须先获得用户明确授权，因为 PI2 会修改 production 源码。其它暂停条件包括：fallback gate 无法隔离、exact 点型收窄无法在模板入口安全表达、production direct correctness 失败、asm 无法归属、board evidence 不可用或 Evidence Doctor Error 不能降级。

## 默认下一步

`next_phase_default`: 等待用户确认是否进入 PI2-PI5 production integration loop。确认后从本计划恢复，按冻结范围做最小 production patch。
