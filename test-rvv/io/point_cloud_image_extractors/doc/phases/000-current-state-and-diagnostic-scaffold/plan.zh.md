# Phase 000 Plan: current-state-and-diagnostic-scaffold

## 阶段意图和边界

本阶段建立 `PointCloudImageExtractor` 的首个 production-shaped diagnostic（生产形态诊断）：

- 入口：测试专用 helper，复刻 `PointCloudImageExtractorFromRGBField` 和 `PointCloudImageExtractorWithScaling`
  的主要热点。
- 点类型：`PointXYZRGB`、`PointXYZRGBA`、`PointXYZI`。
- 数据流：organized cloud order（点云顺序扫描），AoS field offset（结构数组字段偏移）跨步读取。
- 层级：diagnostic，不修改 `io/include/pcl/io/impl/point_cloud_image_extractors.hpp`。

本阶段不覆盖 label random / Glasbey、normal field、真实 production dispatch、`pcd2png` 工具端到端、
PNG writer 上游和泛型字段组合。

## 当前状态清单

| area | 当前状态 | 路径 / 证据 |
| --- | --- | --- |
| 队列来源 | 建议队列第 7 项，状态未启动 | `doc-rvv/library-screening/io/io-function-evaluation-queue.zh.md` |
| production 源码 | header-only 标量实现，无 `__RVV10__` | `io/include/pcl/io/impl/point_cloud_image_extractors.hpp` |
| 上游测试 | 小样本覆盖 RGB/RGBA/label/scaling/NaN | `test/io/test_point_cloud_image_extractors.cpp` |
| topic 测试资产 | 新建 scaffold | `test-rvv/io/point_cloud_image_extractors` |
| board availability | 当前会话说明板卡可用；本机 config 有 board 覆盖 | `test-rvv/config.mk` 私有值不写入文档 |

## 假设与候选族

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `rgb_u32_stride_unpack_v0` | 源码和队列建议 | RGB/RGBA unpack | `vlse32` 批量读取字段，减少 `getFieldValue` 开销 | 逐 lane RGB store 可能抵消收益 | correctness、asm、board、Doctor | planned | 当前 |
| `scaling_float_stride_v0` | 源码和队列建议 | intensity scaling | 批量加载、乘法和转换 | full-range min/max 仍有标量规约；转换语义需审计 | correctness、asm、board、Doctor | planned | 当前 |
| `rgb_segment_store_v1` | image_yuv422 sibling store 经验 | RGB/RGBA store | 可能减少逐 lane store | narrowing / interleave 更复杂 | v0 不佳时 A/B | deferred | 后续 |

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `diagnostic` / `production-shaped diagnostic`，测试专用 helper |
| A/B boundary | `test helper`，Std 侧对 RGB 调真实 PCL extractor，scaling 侧复刻标量语义 |
| 当前决策问题 | `RVV-vs-scalar` 是否值得继续 |
| diagnostic 是否可外推到 production | no；只能支持 bounded production probe |
| comparison-boundary / baseline mismatch 风险 | yes；production 通过 `PCLImage` 和 virtual extractor，bench helper 直接写 vector |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 只有实现很小且 board case 至少 weak-positive 时才考虑 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 若 v1 store family 出现，production 前需要同边界 A/B |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 | 状态 |
| --- | --- | --- | --- |
| RED test | `src/test_pcie.cpp`; `make run_test_rvv` | 首次失败于缺少 `pcie.h` | done |
| GREEN helper | `include/pcie.h`, `include/impl/pcie_support.hpp` | `make run_test_compare` pass | done |
| Bench smoke | `src/bench_pcie.cpp`; `make run_bench_{std,rvv}` | QEMU 日志和 checksum 可解析 | done |
| ASM | `make dump_bench_rvv` | RVV 指令出现 | done |
| Board | `make board_smoke` | 板卡 correctness + compare summary | planned |
| Evidence Doctor | manifest 或人工 doctor 表 | Errors / Warnings / Suggestions 被解释 | planned |

## 板卡复跑预算和决策桶

- 初始预算：`board_smoke` 1 次。
- 复跑预算：若 decision bucket 在 positive / weak-positive / neutral / negative 间摇摆，最多再跑 2 次。
- 桶：`positive >= 1.20x`，`weak-positive 1.05x-1.20x`，`neutral 0.95x-1.05x`，`negative < 0.95x`。
- checksum mismatch 或 board 工具失败为 blocked，不把 QEMU timing 当性能结论。

## Continue / Stop Criteria

默认继续到 board evidence 和 result。只有板卡不可达、构建工具失败、checksum 不一致、Evidence Doctor
Error 无法修复、dirty isolation 不安全，或继续需要修改 production，才停止。

`next_phase_default`：根据 board result 进入 `rgb_segment_store_v1`、`normal_xyz_to_rgb`、
`production_integration_plan` 或 no-production closeout。
