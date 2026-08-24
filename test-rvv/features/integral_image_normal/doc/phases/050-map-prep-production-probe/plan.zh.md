# Phase 050 Plan: Map-prep Production Probe

## 阶段意图和边界

本阶段在用户授权后进入 PI2-PI5 production integration loop（生产接入闭环），只把 Phase 000 已稳定正向的 map-prep 前缀接入 `features/include/pcl/features/impl/integral_image_normal.hpp`。本阶段不接入 average 3D gradient diff-buffer，因为 Phase 030/040 的 production-shaped diagnostic（生产形态诊断）为 weak / unstable。

## PI2 范围

| dimension | scope |
| --- | --- |
| production entry | `IntegralImageNormalEstimation<PointInT, PointOutT>::computeFeature(PointCloudOut&)` 的 depth-change map 和 distance-map initialization 前缀 |
| point type / layout | `pcl::rvv::RVVXYZAoSFloatLayout<PointInT>` 成立的 `PointInT`，实际只读取 z 字段；其它点型 fallback |
| `Scalar` | `float` z 字段；不覆盖 `double` 或非 float z |
| row source | full organized image；`indices_` 只影响后续 output path，不改变 map-prep 输入 |
| kept scalar | distance transform 两遍传播、normal query / output、border policy、viewpoint flip |
| forbidden expansion | 不修改 public API、不改 `computeFeatureFull/Part`、不接 `initAverage3DGradientMethod()` diff-buffer、不触碰其它 topic |

## 实现动作

| action | artifact | completion criterion |
| --- | --- | --- |
| A1 production helper | `integral_image_normal.hpp` | 新增 Std/RVV map-prep helper；non-RVV 和 gate 不满足时走 Std |
| A2 production direct correctness | `src/test_integral_image_normal.cpp` | 公开 `compute()` 后的 distance map / normals 与同语义 reference 一致 |
| A3 production direct bench | `src/bench_integral_image_normal.cpp` | 新增真实 `IntegralImageNormalEstimation<PointXYZ, Normal>::compute()` bench label |
| A4 manifest / doctor | topic-local manifest + Evidence Doctor | production-direct case metadata 与 summary 刷新 |
| A5 docs | phase result、matrix、roadmap、evaluation、doc-rvv | PI5 后用 production board 数据更新正式文档 |

## Evidence Doctor 和 registry

本阶段板卡目标使用 5-run repeated。production direct case 必须进入 manifest，evidence role 写成 `production_direct`，A/B boundary 为 public `compute()`。若 production direct 低于 1.0、退化频率高、checksum 不一致、asm 无法归属或 registry stale，则停在 PI5，保留 patch 等待用户决定采纳或回滚。

## 板卡复跑预算和决策桶

- runs：5。
- positive：5-run 全部 `B/A > 1.0` 且 mean / median 明确正向。
- weak-positive：mean / median 正向但存在少量退化；可按用户偏好作为采纳候选，但必须在 PI5 明示风险。
- negative / unstable：退化频率高或长尾明显；不自动回滚，PI5 等待用户确认。

## Diagnostic-to-production mismatch audit

| question | plan |
| --- | --- |
| evidence role | production-direct for new public compute bench；历史 map-prep 是 diagnostic |
| A/B boundary | public `IntegralImageNormalEstimation::compute()` for production cases |
| 当前决策问题 | RVV-vs-scalar：接入后的真实公开入口是否值得保留 |
| diagnostic 是否可外推 production | 不直接外推；只作为 PI2 候选来源 |
| comparison-boundary risk | 有，生产 direct 可能被 distance transform / normal output 稀释 |
| weak / negative 时是否允许 bounded production probe | 本阶段已是 bounded probe；PI5 后由用户决定保留或回滚 |
| clean adoption 是否需要 RVV-vs-RVV detail A/B | 不需要；当前没有已采用的 production RVV family |

## 继续 / 停止条件

本阶段默认连续推进 PI2-PI5。PI5 后必须暂停：若 production-direct 板卡有收益，建议采纳并创建 / 更新 `doc-rvv/features/integral_image_normal-RVV.zh.md`；若无收益或不稳定，报告 production diff 和证据，等待用户确认是否回滚。
