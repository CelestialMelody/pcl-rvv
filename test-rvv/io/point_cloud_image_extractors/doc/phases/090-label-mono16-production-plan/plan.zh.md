# Phase 090 Plan: label mono16 production integration

## 阶段意图和边界

本阶段把 Phase 070 的 `label_mono16_stride_v0` 从 production-shaped diagnostic（生产形态诊断）
推进到有界 production integration loop（生产接入闭环）。允许修改的 production 文件仍只有
`io/include/pcl/io/impl/point_cloud_image_extractors.hpp`；不修改
`io/include/pcl/io/point_cloud_image_extractors.h` 的公开 / protected API。

本阶段只覆盖：

- exact `PointXYZL`。
- `PointCloudImageExtractorFromLabelField<PointT>::extractImpl` 的 `COLORS_MONO` 分支。
- `label` 字段为单个 `UINT32`，offset 和 stride 满足 32-bit 跨步加载。
- organized cloud order（有组织点云顺序）。

本阶段不覆盖 `COLORS_RGB_RANDOM`、`COLORS_RGB_GLASBEY`、泛型 label-like 点型、RGB/scaling 已采纳路径、
normal field、PNG writer 或 `pcd2png` 端到端路径。

## 当前状态清单

| item | state | evidence |
| --- | --- | --- |
| RGB/scaling production patch | adopted | Phase 080 production-public repeated board positive，用户确认有收益即可采纳。 |
| label diagnostic | positive | Phase 070 repeated board median `1.21x`，min `1.18x`，max `1.27x`，Doctor `Errors=0, Warnings=0, Suggestions=0`。 |
| production label path | scalar-only | `COLORS_MONO` 分支逐点 `getFieldValue<uint32_t>` 后写 `unsigned short`。 |
| production scope authorization | granted by current prompt | 用户要求如果还有可继续优化方向，按 phase loop 继续推进；label 是当前未阻塞正向候选。 |

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | Phase 070 为 `production_shaped_diagnostic`；本阶段必须补 `production-public`。 |
| A/B boundary | 诊断 A/B 是 `test_helper`；本阶段 bench 必须走真实 `PointCloudImageExtractorFromLabelField` public entry。 |
| 当前决策问题 | `RVV-vs-scalar` 的有界 label mono16 production probe。 |
| diagnostic 是否可外推到 production | 只支持进入 probe；最终以 production-public correctness、asm、board 和 Doctor 为准。 |
| comparison-boundary / baseline mismatch 风险 | 存在真实 `PCLImage` metadata、`color_mode_` switch、base `extract` organized check 和 NaN post-pass 差异。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前 diagnostic 为 positive，允许本阶段有界 probe；若 production-public 转弱或负向，按证据降级或回滚。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前不是实现族选择；production-public positive 足以支持有界采纳。 |

## 实现和测试动作

| action | artifact | expected evidence | completion |
| --- | --- | --- | --- |
| production gate / helper | production header | 新增 `extractLabelMono16FieldStd`、`extractLabelMono16FieldRVV` 和 label path hook。 | 非 RVV 构建自然走 Std。 |
| production direct correctness | `src/test_pcie.cpp` | exact `PointXYZL` `COLORS_MONO` 命中 RVV；RGB random / Glasbey 或非覆盖路径 fallback。 | `make run_test_compare` pass。 |
| production bench label | `src/bench_pcie.cpp` / manifest script | 新增 `production_label_mono16_pointxyzl_640x480`，通过真实 label extractor。 | QEMU smoke 和 board repeated 可解析。 |
| QEMU / ASM | Make targets | QEMU smoke 通过；asm 可见 label helper 中 `vlse32.v`、`vse16.v` 或窄化相关指令。 | dump pass。 |
| board repeated / Doctor | `log/board/repeated_phase090` | 5-run positive 且 Doctor 无 Error。 | 若不成立，停止并记录 rollback/no-production。 |
| docs | phase result、matrix、roadmap、evaluation、topic docs、doc-rvv | 只在 production-public positive 后把 label 写成 adopted。 | final closeout。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `production_label_mono16_stride_v0` | organized cloud order | exact `PointXYZL::label` / `UINT32` -> `mono16` | `PointCloudImageExtractorFromLabelField` public entry, `COLORS_MONO` only | planned production-direct output + path-hit / fallback tests | `production_label_mono16_pointxyzl_640x480` | planned 5-run repeated | `extractLabelMono16FieldRVV` | planned | planned |

## Point type expansion queue

| queue item | status | resume condition |
| --- | --- | --- |
| generic label-like point types | deferred | exact `PointXYZL` production-public 成立后，审计 traits、field offset、POD layout 和 fallback tests。 |
| RGB random / Glasbey label modes | deferred | profile 指向 label color map 分支，且能设计不破坏 `std::map` / LUT 语义的候选。 |
| label NaN post-pass | deferred | 当前 `PointXYZL` 没有 NaN 坐标语义需求；若 production direct test 暴露 post-pass 风险再补。 |

## 板卡复跑预算和决策桶

- repeated board budget：5 runs。
- bench 参数：`--iterations 20 --warmup-iterations 3`。
- production label：`production_label_mono16_pointxyzl_640x480`。
- positive：median >= 1.10x 且 min > 1.0x，Evidence Doctor 无 Error。
- weak-positive：1.05x <= median < 1.10x，若实现小、fallback 简单且 Doctor 无 Error，可保留但文档标弱收益。
- neutral / negative / unstable：停止，保留证据并回滚或不采纳 label production path。

## Continue / Stop 条件

若 production direct correctness、asm、board repeated 和 Evidence Doctor 都闭合且 positive，本轮直接进入
S11 closeout：更新 `doc-rvv`、topic-local docs、matrix、roadmap 和队列表。若任一证据不成立，停止在
label rollback/no-production 决策，不影响已采纳的 RGB/scaling patch。
