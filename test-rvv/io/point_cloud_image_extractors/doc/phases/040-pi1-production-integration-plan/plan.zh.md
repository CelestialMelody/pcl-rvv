# Phase 040 Plan: PI1 production-integration-plan

## 阶段意图和边界

本阶段只完成 PI1 production integration plan（生产接入计划）：把 Phase 020 / 030 已支持的
diagnostic candidate（诊断候选）冻结成可审查的 production patch（生产补丁）范围、fallback
（回退路径）矩阵、production direct（真实生产路径证据）计划和停止条件。本阶段不修改
`io/include/pcl/io/impl/point_cloud_image_extractors.hpp`，不新增 production RVV helper，也不运行
新的板卡 benchmark。

当前 production 源码是 header-only 模板实现；继续到 PI2 会修改
`io/include/pcl/io/impl/point_cloud_image_extractors.hpp`，因此 PI2 需要用户明确确认。

## 当前状态清单

| 项 | 当前证据 | PI1 含义 |
| --- | --- | --- |
| RGB candidate | `rgb_segment_store_v1` 在 `PointXYZRGB` / `PointXYZRGBA` 上 repeated board median 1.74x / 1.75x，asm 可见 `vsseg3e8.v` | 可作为 RGB/RGBA 优先 production probe。 |
| RGB fallback family | `rgb_u32_stride_unpack_v0` repeated board median 1.29x / 1.31x | 若 segment-store production 接入遇到布局或 asm 归属阻塞，可退回 v0 或不接 RGB。 |
| scaling candidate | `scaling_reduction_v1` 在 `PointXYZI::intensity` full-range 上 repeated board median 1.56x，asm 可见 `vfredmin.vs` / `vfredmax.vs` | 可作为 intensity full-range 优先 production probe。 |
| rejected candidate | `scaling_float_stride_v0` full-range repeated board median 0.92x，Evidence Doctor Error 指向该 label | 不进入 production。 |
| doc suite | Phase 030 已补齐 role 文档和 doc-suite parity audit（文档套件对齐审计） | PI2 前恢复路径已稳定。 |
| production status | `io/include/pcl/io/impl/point_cloud_image_extractors.hpp` 当前无 `__RVV10__` 分流 | PI2 需最小生产补丁。 |

## 计划采用的 production 实现形态

PI2 的实现应保持 public API（公开接口）不变，并避免修改
`io/include/pcl/io/point_cloud_image_extractors.h` 的类声明。首选在
`io/include/pcl/io/impl/point_cloud_image_extractors.hpp` 中新增邻近 internal helper（内部辅助函数）：

```text
pcl::io::detail::extractRgbFieldStd(...)
pcl::io::detail::extractRgbFieldRvv(...)
pcl::io::detail::extractScalingStd(...)
pcl::io::detail::extractScalingFullRangeIntensityRvv(...)

PointCloudImageExtractorFromRGBField<PointT>::extractImpl
  -> 找到 rgb / rgba field
  -> #if __RVV10__ 且 gate 命中: extractRgbFieldRvv
  -> else: extractRgbFieldStd

PointCloudImageExtractorWithScaling<PointT>::extractImpl
  -> 找到 field_name_
  -> #if __RVV10__ 且 gate 命中: extractScalingFullRangeIntensityRvv
  -> else: extractScalingStd
```

这样做能让原标量主体成为命名清楚的 Std 路径，也避免扩大 protected / public API。
production 注释只解释 gate、fallback、字段布局和数值边界，不复述每条 intrinsic。

## 候选范围和 fallback 矩阵

| production entry | RVV 候选 | gate-hit 范围 | fallback 条件 | 不覆盖范围 |
| --- | --- | --- | --- | --- |
| `PointCloudImageExtractorFromRGBField<PointT>::extractImpl` | `rgb_segment_store_v1` | `PointT` exact 为 `PointXYZRGB` 或 `PointXYZRGBA`；`getFieldIndex` 找到 `rgb` 或 `rgba`；字段 count 为 1；字段 datatype 为 `FLOAT32` 或 `UINT32`；`sizeof(PointT)` 和字段 offset 满足 32-bit load alignment；cloud size 达到 PI2 冻结的最小阈值。 | 非 RVV 构建、非 exact 点型、字段缺失、字段 metadata 不匹配、小规模输入或布局不满足时调用 `extractRgbFieldStd`。 | `PointXYZRGBL`、用户自定义 color 点型、PNG writer 端到端、`pcd2png` 工具链。 |
| `PointCloudImageExtractorWithScaling<PointT>::extractImpl` | `scaling_reduction_v1` | `PointT` exact 为 `PointXYZI`；`field_name_ == "intensity"`；`scaling_method_ == SCALING_FULL_RANGE`；字段为单个 `float`；offset 和 stride 满足 f32 strided load；cloud size 达到 PI2 冻结的最小阈值。 | 非 RVV 构建、非 `PointXYZI`、非 `intensity` 字段、非 full-range scaling、字段缺失、小规模输入或布局不满足时调用 `extractScalingStd`。 | `PointCloudImageExtractorFromZField`、curvature、fixed-factor、no-scaling、`PointXYZINormal` intensity 和任意 runtime field name。 |
| `PointCloudImageExtractor<PointT>::extract` NaN post-pass | 保持标量 | RVV `extractImpl` 成功后仍由 base class 的现有 post-pass 处理 | 不变 | 不把 NaN black post-pass 放进 PI2 RVV 补丁。 |

## Point type expansion queue

| queue item | 当前状态 | 恢复条件 | 所需证据 |
| --- | --- | --- | --- |
| RGB traits-gated generic color types | deferred | PI5 证明 exact `PointXYZRGB/RGBA` production direct 正向，且 reviewer 接受扩大点类型 gate | correctness fallback、field metadata compatibility、production bench、asm、Doctor。 |
| `PointXYZRGBL` RGB/RGBA color path | deferred | 明确其 `rgba` / `label` 语义和 image extractor 使用频率 | production direct test、field offset audit、board repeated。 |
| `PointXYZINormal` intensity full-range | deferred | exact `PointXYZI` production probe 成立后，审计 intensity offset 和 POD layout | correctness、fallback、production bench、asm、Doctor。 |
| Z / curvature full-range scaling | deferred | production profile 或用户需求证明这些入口是热点 | 独立 field-name gate、NaN/Inf 语义、board repeated。 |
| normal field RGB mapping | deferred | 单独创建 normal-field phase | 饱和 / 截断语义 correctness、bench、asm、board。 |

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | 当前 evidence 为 `production_shaped_diagnostic`；PI2-PI5 需要补 `production-public` 和 `production-detail`。 |
| A/B boundary | Phase 020 是 `test_helper` A/B；PI4 必须在真实 `extractImpl` production boundary 内重跑。 |
| 当前决策问题 | PI1 只回答 implementation-shape（实现形态）和 production probe scope（生产探针范围）是否可控。 |
| diagnostic 是否可外推到 production | 只能外推为 bounded production candidate（有界生产候选）；不能写成 adopted。 |
| comparison-boundary / baseline mismatch 风险 | 存在。真实 production 有 `PCLImage` resize、field lookup、base `extract` 的 organized check 和 NaN post-pass。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | `rgb_segment_store_v1` 和 `scaling_reduction_v1` 为 positive，允许进入 PI2；若 PI4 结果弱 / 负 / 不稳定，PI5 停在用户确认回滚或补充诊断。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 需要。若 PI2 接入 v1，还需同一 production helper 内证明 v1 相对 fallback family 或 Std 的边界。 |

## PI2-PI5 证据计划

| PI step | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| PI2 production patch | 修改 `io/include/pcl/io/impl/point_cloud_image_extractors.hpp`，新增 Std/RVV helper 和 `__RVV10__` 分流 | public API 不变；非 RVV 构建自然走 Std；不触碰 label、normal、PNG writer 或其它 topic。 |
| PI3 production direct tests | 在 `test-rvv/io/point_cloud_image_extractors/src/test_pcie.cpp` 增加真实 extractor gate-hit 和 gate-miss 对拍，必要时复用现有上游小样本 | RVV build 真实 `PointCloudImageExtractorFromRGBField` / `PointCloudImageExtractorFromIntensityField` 输出与 Std 相同；fallback 条件可被单独触发。 |
| PI4 production evidence rerun | 增加 production bench labels，并运行 `make run_test_compare`、`make dump_bench_rvv`、`make collect_board_repeated ... PCIE_REPEATED_DIR=log/board/repeated_pi4`、`make run_board_repeated_evidence_doctor ...` | QEMU correctness 通过；asm 可归属到 production helper；board repeated bucket 稳定；Doctor Error 不指向新 production labels。 |
| PI5 production evidence decision | 更新 phase result、matrix、roadmap、evaluation 和 Handoff | 无论证据正负，都保留 production diff 并等待用户确认采纳或回滚。 |

## 板卡复跑预算和决策桶

- repeated board budget：默认 5 runs，每 run `--iterations 20 --warmup-iterations 3`。
- positive：median >= 1.20x 且 min > 1.05x，Doctor 不指向新 production label。
- weak-positive：1.05x <= median < 1.20x，只有实现小、fallback 简单且 Doctor clean 时才可交给用户判断。
- neutral / negative：median < 1.05x 或 Doctor Error 指向新 production label，PI5 停在 rollback confirmation。
- unstable：5 runs 内方向摇摆或 min/max 跨越 positive / negative 桶，停止自动复跑并降级证据。

## Continue / Stop Criteria

本阶段完成条件：

- 生产候选范围、fallback、point type gate、unvalidated scope、PI2-PI5 命令和 Doctor 处理策略写清。
- phase index、roadmap、optimization matrix 和 evaluation 能恢复到 PI2 前检查点。
- production 源码未修改。

本阶段后的默认动作是 `PI2-production-patch after explicit user confirmation`。没有该确认时，停止条件命中：
继续会修改 production header。
