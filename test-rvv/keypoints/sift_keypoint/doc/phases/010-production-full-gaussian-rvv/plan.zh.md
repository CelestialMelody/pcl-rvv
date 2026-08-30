# Phase 010 Plan: production full Gaussian RVV

本阶段处理已经存在但未采纳的 `sift_keypoint.hpp` production patch（生产补丁）。Phase 000
的 diagnostic（诊断）helper 在板卡上显示 `computeScaleSpace()` 的 Gaussian 权重循环有明显收益，
但阶段入口处的旧 production-public（真实公开入口）summary 显示 `SIFTKeypoint::compute()` 轻微退化。因此本阶段
只尝试把 production 内层改成和 diagnostic helper 同边界的完整 Gaussian RVV 累加；若 production
direct（真实生产入口直连）板卡仍无收益，则不采纳并保留 no-production / rollback 结论。

本文件保留阶段开始前的计划和历史观察；阶段结束后的当前结论以
`result.zh.md`、`doc/sift_keypoint-evaluation.zh.md` 和
`doc-rvv/keypoints/sift_keypoint-RVV.zh.md` 为准。实际采纳形态是 RVV weight staging
（权重暂存）加 scalar tail（标量尾段），不是 full vector reduction（完整向量规约）。

## 阶段范围

| 项目 | 本阶段覆盖 | 本阶段不覆盖 |
| --- | --- | --- |
| 入口 | `pcl::SIFTKeypoint<PointInT, PointOutT>::computeScaleSpace()` 通过 public `compute()` 间接命中 | 不改 public API，不改 `findScaleSpaceExtrema()` |
| 点类型 / 输出 | `PointXYZI -> PointWithScale` public bench case | 不外推到所有 `PointInT`、其它 intensity field 类型或自定义点类型 |
| 数据布局 | organized dense synthetic cloud；`radiusSearch` 仍由原 search tree 完成 | 不优化 indexed / 非 organized / 真实业务分布 |
| RVV 片段 | 对已排序 `nn_dist` 和预取的 field value 做 VL chunk、`expf_RVV_f32m2` 和 vector reduction（向量规约） | 不把 `radiusSearch`、downsample、extrema scan 和 output assembly RVV 化 |

## 当前证据

| 证据 | 路径 | 结论 |
| --- | --- | --- |
| Phase 000 diagnostic board | `test-rvv/keypoints/sift_keypoint/log/board/repeated_phase000_scale_space_profile_prerequisite/summary.md` | 三个 synthetic kernel case median 4.273x 到 5.327x，说明局部 Gaussian 权重循环值得进入 production probe（生产探针） |
| 阶段入口历史 production public board | `test-rvv/keypoints/sift_keypoint/log/board/repeated_phase010_public_compute/summary.md` 的早期覆盖版本 | `public_sift_keypoint_320x240` 曾有 3-run median 0.988x，提示旧 production patch 不支持采纳；当前摘要已被阶段结果刷新为 5-run positive |
| public output trace | `test-rvv/keypoints/sift_keypoint/log/board/public_trace_phase010_public_compute/public_trace_compare.md` | Std/RVV 输出 116 个 keypoint 完全一致，说明 public correctness（公开入口正确性）可作为继续 probe 的基础 |
| Evidence Doctor | `test-rvv/keypoints/sift_keypoint/log/board/repeated_phase010_public_compute/evidence_doctor.md` | 历史与当前报告均为 Errors=0，Warnings=0，Suggestions=2；环境和 binary identity metadata 缺失只影响复现实证强度 |

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | Phase 000 是 diagnostic；本阶段最终采纳只看 production-public repeated board（真实公开入口重复板卡测试） |
| A/B boundary | diagnostic 为 test helper；production 为 public overload（公开重载） |
| 当前决策问题 | RVV-vs-scalar：完整 Gaussian RVV 累加是否让 public `compute()` 快于当前标量路径 |
| diagnostic 是否可外推到 production | 只能作为 bounded production probe 的理由，不能直接采纳 |
| comparison-boundary / baseline mismatch 风险 | 有；diagnostic 排除了 `radiusSearch`、downsample、extrema 和 output assembly，public case 会包含这些成本 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前 diagnostic 强正向且 public correctness 已有 trace，对同一 `computeScaleSpace()` 热段允许本阶段 probe |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前没有已采纳 SIFT RVV family；若 public Std/RVV 为 positive，可按用户本轮授权采纳窄范围 production patch |

## 实现动作和完成判据

| 动作 | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| 实现完整 Gaussian RVV 累加 | `keypoints/include/pcl/keypoints/impl/sift_keypoint.hpp` | RVV 分支使用 `expf_RVV_f32m2`、`vfmul` 和 `vfredosum`，非 RVV 构建保留原标量循环 |
| public correctness | `make -C test-rvv/keypoints/sift_keypoint run_test_compare` | Std/RVV gtest 通过；public smoke 仍能构造输出 |
| output trace | `make -C test-rvv/keypoints/sift_keypoint board_public_trace compare_public_trace` | `PointWithScale` 输出计数、顺序和字段差异在容差内 |
| asm attribution（反汇编归属） | `make -C test-rvv/keypoints/sift_keypoint check_sift_keypoint_rvv_asm` | RVV binary 能看到 `expf_RVV_f32m2` / `vfredosum` / load / vsetvl 相关指令 |
| board production evidence | `make -C test-rvv/keypoints/sift_keypoint board_repeated_public record_evidence_state_public` | 目标硬件 repeated summary 进入稳定 decision bucket；Evidence Doctor 无 Error |

## 板卡复跑预算和决策桶

默认跑 `REPEATED_BOARD_RUNS=5`，`SIFT_PUBLIC_BENCH_ARGS="--mode public --case-filter public_sift_keypoint_320x240 --iterations 5 --warmup-iterations 1"`。
若 5-run median 明显 `>1.05x` 且 `B/A < 1` 不超过 1/5，判为 positive；`1.00x-1.05x`
且方向稳定判为 weak-positive；median 低于 1 或全部 run 低于 1 判为 negative。若和旧负向 run
方向冲突，可做最多一次同边界确认复跑；预算耗尽后仍跨 bucket 摇摆则标为 unstable。

## 继续 / 停止条件

若 production public 板卡为 positive 或用户可接受的 weak-positive，本轮按 prompt override
直接采纳窄范围 production patch，并创建 `doc-rvv/keypoints/sift_keypoint-RVV.zh.md`。若仍为
negative，本阶段不采纳该 production patch，进入 no-production / rollback 交接；回滚仍要按
当前用户授权和 dirty isolation（脏工作区隔离）处理。若板卡不可达，停止在 `blocked`，保留本地
correctness、asm 和恢复命令。

下一阶段候选：如果本阶段采纳后仍想继续优化，优先看 `findScaleSpaceExtrema()` 的 extrema scan
（局部极值扫描）独立 phase；如果本阶段不采纳，则不建议继续扩大 SIFT production 接入，除非先有
public profile（公开入口性能剖析）证明 scale-space 仍是主瓶颈。
