# Phase 020 Plan: rgb-segment-store-ab

## 阶段意图和边界

本阶段只做 `rgb_segment_store_v1` 的 test-only A/B。它复用 Phase 000 的
`vlse32` RGB/RGBA 字段加载和 bit unpack（位拆包），但把 RGB 三通道写回从逐 lane
标量 store 改成 `vsseg3e8` segment store（三通道交错向量写回）。

边界：

- 入口：`test-rvv/io/point_cloud_image_extractors` 的 diagnostic helper 和 bench label。
- 点类型：`PointXYZRGB`、`PointXYZRGBA`。
- 数据流：organized cloud order，AoS field offset，输出 `rgb8` 三字节连续图像。
- 层级：production-shaped diagnostic（生产形态诊断），不修改 production header。
- 不覆盖：真实 `PointCloudImageExtractorFromRGBField` dispatch、泛型字段 gate、alpha 通道、PNG writer 和 `pcd2png`。

## 当前状态清单

| area | 当前状态 | 路径 / 证据 |
| --- | --- | --- |
| v0 RGB | repeated board positive，`PointXYZRGB` median 1.28x、`PointXYZRGBA` median 1.29x | `log/board/repeated_phase010/summary.md` |
| v0 写回 | RVV unpack 后用 scratch vector + per-lane scalar RGB store | `include/impl/pcie_support.hpp` |
| production | 未修改 | `io/include/pcl/io/impl/point_cloud_image_extractors.hpp` |

## 假设与候选族

`rgb_segment_store_v1` 的假设是：三通道输出是紧凑 RGBRGB... 布局，`vsseg3e8` 能减少
v0 的逐 lane store 和 scratch 数组搬运。风险是 `uint32 -> uint8` 收窄和 segment store 的指令、
寄存器压力或 `vsetvli` 成本可能抵消收益。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production_shaped_diagnostic` |
| A/B boundary | `test_helper`，新 label 与 v0 并列 |
| 当前决策问题 | implementation-shape 和 RVV-vs-scalar 筛选 |
| diagnostic 是否可外推到 production | unknown；positive 只能支持 bounded production probe，negative 只拒绝当前 helper 形态 |
| comparison-boundary / baseline mismatch 风险 | yes，production 仍有 extractor 对象、field lookup、`PCLImage` 赋值和 fallback gate |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 若 v1 weak/negative，仍可保留 v0 作为 production probe 候选；不因 v1 失败拒绝 RGB family |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| RED test | `src/test_pcie.cpp` | 新测试先因缺少 `extractRgbSegmentStoreCandidate` 编译失败。 |
| GREEN helper | `include/impl/pcie_support.hpp` | `make run_test_compare` 通过，v1 与标量 RGB/RGBA 输出一致。 |
| Bench label | `src/bench_pcie.cpp`, `script/generate_pcie_evidence_manifest.py` | 新增 `rgb_segment_store_pointxyzrgb_640x480` 和 `rgb_segment_store_pointxyzrgba_640x480`。 |
| ASM | `make dump_bench_rvv` | 反汇编可见 `vsseg3e8` 或等价 segment store 指令。 |
| Board repeated | `make collect_board_repeated BENCH_ARGS='--iterations 20 --warmup-iterations 3 --case-filter all' PCIE_REPEATED_RUNS=5 PCIE_REPEATED_DIR=log/board/repeated_phase020` | v0/v1/RGB/scaling labels 生成 repeated summary。 |
| Evidence Doctor | `make run_board_repeated_evidence_doctor PCIE_REPEATED_DIR=log/board/repeated_phase020` | Errors / Warnings / Suggestions 被解释。 |

## 板卡复跑预算和决策桶

- 预算：5-run repeated board，一次完整 batch。
- 桶：positive `>= 1.20x`，weak-positive `1.05x-1.20x`，neutral `0.95x-1.05x`，negative `< 0.95x`。
- 若 v1 和 v0 同为 positive，但差异小于 3%，不额外复跑；标为 implementation-family comparison pending，
  production 前由 PI1/PI2 决定是否保留 v0 或 v1。

## 继续 / 停止条件

默认继续到 correctness、asm、board repeated、Evidence Doctor 和 result 回填。若 v1 编译失败于 RVV
intrinsic 不可用，标为 `blocked_by_toolchain_intrinsic`，保留 v0。

`next_phase_default`：

- 若 v1 明显优于 v0：PI1 优先选择 `rgb_segment_store_v1`。
- 若 v1 相当或更差：PI1 保留 `rgb_u32_stride_unpack_v0`，v1 rejected/deferred。
- 若 production 仍未授权：停在 PI1 检查点，不改 production。
