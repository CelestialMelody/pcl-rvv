# Phase 040 Production RVV Probe Plan

## 阶段意图和边界

本阶段按用户确认进入 VFH（Viewpoint Feature Histogram，视点特征直方图）的 PI2-PI5
production integration loop（生产接入闭环）。生产补丁只接入 Phase 030 已验证的
reduction-combined RVV（RISC-V Vector，可变长度向量）路线：xyz centroid（坐标质心）、
normal centroid（法线质心）、centroid-to-point pair math（从质心到点的点对数学）和
viewpoint normal-dot preparation（视点法线点积分箱准备）进入 RVV；histogram scatter
（直方图离散累加）保持标量顺序。

覆盖范围收窄到 `VFHEstimation::computeFeature()` 的默认 VFH public entry（公开入口）：
`PointInT` 具有单个 `float` xyz 字段、`PointNT` 具有单个 `float` normal 字段，输出为
`VFHSignature308`，dense full-cloud sequential indices（全点云顺序索引），默认 bin 布局，
`normalize_bins=true`、`normalize_distances=false`、`size_component=false`，且未使用给定
centroid / normal。非 RVV 构建、非覆盖点型、非 dense normals、子集 indices、自定义参数、
CVFH / OUR-CVFH 的给定 centroid 或 normal 路径均 fallback（回退）到原标量主体。

## 用户采纳口径

接入 production 后必须重新运行 production direct（真实生产路径直连）测试、反汇编和板卡
benchmark（性能测试）。正式 `doc-rvv/features/vfh-RVV.zh.md` 只在接入后的板卡数据仍显示收益时创建，
并以 production direct 板卡数据为准，不沿用 Phase 030 test-only diagnostic（测试专用诊断）数字作为
正式性能结论。

## 实现和测试动作

| action | 产物 | 验收 |
| --- | --- | --- |
| PI2-RED | `src/test_vfh.cpp` 新增 production helper hit / fallback gtest。 | RVV 构建在 helper 未实现时失败或不能命中 production RVV helper。 |
| PI2-GREEN | `features/include/pcl/features/impl/vfh.hpp` 新增 bounded `pcl::detail` RVV helper。 | public `compute()` 输出与 reference 在误差预算内；非覆盖参数 fallback。 |
| PI3-BENCH | `src/bench_vfh.cpp` 新增 `production_vfh_compute_default` case。 | board repeated 可以直接比较接入后的 public Std/RVV。 |
| PI4-ASM | `dump_bench_rvv`。 | 反汇编能归到 production helper 或其 inline 区域，而不是只归到 test-only helper。 |
| PI4-BOARD | `board_repeated` 到 `log/board/repeated-production-phase040`。 | 5-run production direct 板卡结果决定是否值得采纳。 |
| PI5-DOCTOR | Evidence Doctor 使用 production summary manifest。 | `Errors=0`；Warning / Suggestion 已解释或降级。 |
| DOC | evaluation、roadmap、matrix、phase result；若 production direct 有收益则创建 `doc-rvv/features/vfh-RVV.zh.md`。 | 文档以接入后的板卡数据为准，清楚区分 diagnostic 与 production evidence（生产证据）。 |

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | Phase 040 的最终判断使用 `production-public`，Phase 000-030 只作为候选来源。 |
| A/B boundary | 真实 `VFHEstimation::compute()` public overload，Std build 对 RVV build。 |
| 当前决策问题 | `RVV-vs-scalar`，判断接入后的 public RVV path 是否值得保留。 |
| diagnostic 是否可外推到 production | 不直接外推；本阶段必须以 production direct board 数据重判。 |
| comparison-boundary / baseline mismatch 风险 | 通过新增 production bench label 和 manifest metadata 降低；旧 diagnostic case 不参与正式采纳数字。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段已由用户授权 bounded probe；若 production direct 无收益或不稳定，停在 PI5 用户检查点。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 若只比较接入后 public Std/RVV，可按用户口径用 production-public 收益决定采纳；若再选择多个 RVV family，另补 RVV-vs-RVV detail A/B。 |

## 继续 / 停止条件

板卡可用时继续跑到 PI5。若 production direct 5-run 稳定显示收益，创建正式 `doc-rvv/features/vfh-RVV.zh.md`
并把 VFH 状态更新为 adopted production behavior（已采纳生产行为）的候选；若无收益、退化或 Evidence Doctor
出现未修复 Error，则暂停并报告当前 patch、证据和回退建议。
