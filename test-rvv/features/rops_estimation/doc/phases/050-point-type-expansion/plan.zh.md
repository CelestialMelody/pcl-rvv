# Phase 050 Plan: point-type-expansion

## 阶段意图和边界

Phase 050 计划建立时，Phase 040 的 production detail path 已接入、PI5 用户确认尚未完成；用户后续确认板卡正向即可采纳后，
本阶段结果已经作为 adopted production behavior 的点型扩展证据。目标是把
`rotateCloudRVV()` 和 `getDistributionMatrixRVV()` 从 exact `pcl::PointXYZ` gate 改为公共
`pcl::rvv::RVVXYZAoSFloatLayout<PointInT>` gate，并用接入后的 production-detail board（板卡）数据证明
代表性点型仍有收益。

本阶段仍不修改 public API（公开接口），不进入完整 public `computeFeature()` benchmark，不优化 LRF、
central moments 或 descriptor normalization。`rotateCloud()` 会构造 `PointInT` rotated cloud，但 production
标量路径也只写 xyz；后续 `getDistributionMatrix()` 只读取 xyz。因此本阶段只批准“xyz 字段参与当前 RVV stage”
这一窄范围，不声明额外字段语义已被完整优化。

| 维度 | 本阶段范围 |
| --- | --- |
| production 边界 | `features/include/pcl/features/impl/rops_estimation.hpp` 的 private helper dispatch。 |
| 点型 / Scalar | `RVVXYZAoSFloatLayout<PointInT>`，代表性测试和板卡 case 至少覆盖 `PointXYZI` 与 `PointNormal`；`PointOutT` 仍为 `pcl::Histogram<135>`。 |
| 数据布局 | PCL traits 注册 xyz、三个字段为单个 float、POD standard-layout、`sizeof(PointT)==sizeof(POD)`、字段 offset 和 stride 满足 AoS float 访问。 |
| 运行时 gate | `cloud.size() >= 16`、`cloud.is_dense == true`、`number_of_bins_ > 0`、projection 为 0/1/2、AABB extent 非零。 |
| 不覆盖范围 | 完整 public `computeFeature()`、真实 mesh local surface、LRF、central moments、normal / intensity / RGB 等额外字段输出语义、`Scalar=double` 和自定义点型逐个板卡覆盖。 |

## 当前状态清单

| 项 | 当前状态 | 路径 / 证据 |
| --- | --- | --- |
| Phase 040 exact `PointXYZ` | adopted production behavior | `040-production-integration-plan/result.zh.md`；board median `1.700x`，Doctor `0E/0W/2S`，用户确认板卡正向即可采纳。 |
| production gate | traits-gated after Phase 050 | `rotateCloudRVV()` / `getDistributionMatrixRVV()` 已改为 `RVVXYZAoSFloatLayout<PointT>` gate。 |
| 泛型策略 | loaded | `doc-rvv/rvv/RVV Generic Point Type Strategy.zh.md`：写出 `PointT` 输出时不能只凭 xyz traits 外推额外字段语义。 |
| board status | available | 当前会话已完成 Phase 040 repeated board。 |

## 实现和测试动作

| id | 动作 | 产物 | 完成判据 |
| --- | --- | --- | --- |
| A1 typed RED tests | `src/test_rops_estimation.cpp` | RVV build 下 `PointXYZI` / `PointNormal` production helper trace 命中；在 exact-only gate 下应失败。 |
| A2 traits-gated production patch | `features/include/pcl/features/impl/rops_estimation.hpp` | 移除 exact `PointXYZ` 限制，保留 `RVVXYZAoSFloatLayout<PointT>`、dense 和规模 gate；非覆盖 layout 仍 fallback。 |
| A3 typed bench case | `src/bench_rops_estimation.cpp`、summary script / Makefile | `rops_production_pointxyzi_rotate_distribution_pipeline` 和 `rops_production_pointnormal_rotate_distribution_pipeline` 可独立 case-filter。 |
| A4 correctness / asm | `run_test_compare`、QEMU smoke、`dump_bench_rvv` | Std/RVV correctness 通过，typed production helper 符号或 bench binary 中有 RVV 指令归属。 |
| A5 board repeated / Doctor | `log/board/phase050-*` | 两个代表性点型 5-run board bucket 为 positive 或 weak_positive 且 Doctor 无 Error。 |
| A6 PI5 checkpoint docs | evaluation、roadmap、matrix、phase index、Handoff | 记录接入后 typed board 数据；用户确认后创建长期 `doc-rvv`，但不把代表性点型外推到所有自定义点型。 |

## Board 复跑预算和决策桶

每个 typed case 初始 run budget：

| 参数 | 计划值 |
| --- | --- |
| run count | 5 |
| points | 65536 |
| bins | 5 |
| iterations | 20 |
| warm-up iterations | 3 |
| repeat | 8 |

决策桶：

- `positive`：median >= 1.15 且 0/5 退化。
- `weak_positive`：median >= 1.05 且退化不超过 1/5。
- `neutral`：median 0.95-1.05。
- `negative`：median < 0.95。
- `unstable`：方向跨桶、checksum 不一致、Doctor Error 或长尾明显；最多一次同边界确认复跑。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production_detail` |
| A/B boundary | production private helper dispatch，typed bench wrapper 只负责调用真实 `ROPSEstimation<PointT, Histogram<135>>::rotateCloud/getDistributionMatrix` |
| 当前决策问题 | 已采用的 private helper RVV path 是否可扩到常见 PointXYZ-like typed scope |
| diagnostic 是否可外推到 production | 不外推；本阶段直接采集接入后的 production-detail typed board 数据 |
| comparison-boundary / baseline mismatch 风险 | 中等；仍不是完整 public descriptor，且额外字段语义只按“当前 RVV stage 不消费”裁剪 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 若某个 typed case 不正向，只回退该 typed / traits 扩展，不影响 Phase 040 exact `PointXYZ` |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要；这是同一 RVV helper 的 typed scope 扩展，不是新 RVV family 选择 |

## Optimization Matrix

| candidate family | row source policy | point type / Scalar / layout | correctness / fallback target | bench / board target | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| PointXYZ-like production detail expansion | transformed local cloud -> rotated cloud -> bins | `RVVXYZAoSFloatLayout<PointInT>` / float AoS；代表 `PointXYZI`、`PointNormal` | planned typed production trace and scalar-reference compare | planned phase050 typed repeated board | planned bench RVV asm | planned phase050 typed Doctor | planned |

## Continue / Stop 条件

`continue_stop_decision`：Phase 050 完成后，若 typed board 结果 positive / weak-positive 且 Doctor 无 Error，
刷新 evaluation、roadmap、matrix、phase result 和 Handoff，并停在 PI5 用户检查点等待采纳 / 回滚确认。若两个代表性点型都成立，当前 topic
内没有更高优先级、低风险且值得默认连续推进的优化方向；descriptor normalization、完整 public compute 和 strict evidence
metadata hardening 分别作为后续选择，不在同一 phase 顺手扩大。

`stop_condition_hit`：只有 typed correctness 失败、fallback 无法隔离、board/tool 不可用、Doctor Error、
typed board 结果 negative / unstable，或扩展需要修改 public API / 其它 topic 时停止。
