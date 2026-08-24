# Phase 010 Plan: scaling-full-range-reduction-ab

## 阶段意图和边界

本阶段只尝试 `scaling_reduction_v1`：在测试专用 RVV candidate 中，用 vector reduction
（向量规约）替换 `scaling_float_stride_v0` 的 full-range 第一遍 chunk store + scalar lane scan。

边界：

- 入口：`test-rvv/io/point_cloud_image_extractors` 的 diagnostic helper 和 bench label。
- 点类型：`PointXYZI::intensity`，organized cloud order，AoS field offset。
- 层级：production-shaped diagnostic，不修改 `io/include/pcl/io/impl/point_cloud_image_extractors.hpp`。
- 不覆盖：真实 production dispatch、generic field extractor、NaN/Inf full-range 特殊语义扩展。

## 当前状态清单

| area | 当前状态 | 路径 / 证据 |
| --- | --- | --- |
| v0 full-range | repeated board negative，5/5 低于 1 | `log/board/repeated_phase000/summary.md` |
| Evidence Doctor | `Errors=1`，case 为 `scaling_full_range_intensity_640x480` | `log/board/repeated_phase000/evidence_doctor.md` |
| asm | v0 已有 `vlse32.v`、float arithmetic 和 narrow convert | `build/asm/riscv/bench_pcie_rvv.asm` |
| production | 未修改 | `io/include/pcl/io/impl/point_cloud_image_extractors.hpp` |

## 假设与候选族

`scaling_reduction_v1` 的假设是：full-range 的第一遍 min/max 若保留在 RVV 寄存器中并用
`vfredmin` / `vfredmax` 规约，可以减少 scratch store 和逐 lane 标量扫描成本。

风险：

- 规约顺序不同，但当前测试输入为有限正值，目标是与既有 float 标量结果逐元素一致。
- 如果 second pass（第二遍写回）仍由 scratch store + scalar cast 主导，v1 可能仍为 weak 或 negative。
- full-range v1 的诊断负向仍不能直接推出 production no-production；只能拒绝该 diagnostic boundary。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production_shaped_diagnostic` |
| A/B boundary | `test_helper`，新增 label 与 v0 并列 |
| 当前决策问题 | `RVV-vs-scalar` 和 implementation-shape（实现形态）筛选 |
| diagnostic 是否可外推到 production | unknown；positive 只能支持 bounded production probe，negative 只拒绝当前 test helper 形态 |
| comparison-boundary / baseline mismatch 风险 | yes，production 还有 extractor dispatch、field metadata 和 `PCLImage` 赋值边界 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no，除非后续 production direct profile 指向同一瓶颈且用户批准 PI1 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| RED test | `src/test_pcie.cpp` | 新测试先因缺少 `extractScalingFullRangeReductionCandidate` 编译失败。 |
| GREEN helper | `include/impl/pcie_support.hpp` | `make run_test_compare` 通过，v1 与标量 full-range 输出一致。 |
| Bench label | `src/bench_pcie.cpp`, `script/generate_pcie_evidence_manifest.py` | 新增 `scaling_full_range_reduction_intensity_640x480`，manifest 能识别 `scaling_reduction_v1`。 |
| ASM | `make dump_bench_rvv` | 反汇编可见 `vfredmin` / `vfredmax` 或等价规约指令。 |
| Board repeated | `make collect_board_repeated BENCH_ARGS='--iterations 20 --warmup-iterations 3 --case-filter all' PCIE_REPEATED_RUNS=5 PCIE_REPEATED_DIR=log/board/repeated_phase010` | 新旧 scaling label 与 RGB label 生成 repeated summary。 |
| Evidence Doctor | `make run_board_repeated_evidence_doctor PCIE_REPEATED_DIR=log/board/repeated_phase010` | Errors / Warnings / Suggestions 被解释；若 v1 仍负向，标为 rejected/deferred。 |

## 板卡复跑预算和决策桶

- 预算：5-run repeated board，一次完整 batch。若 v1 处于 0.98x-1.03x 且方向摇摆，最多再加 1 个 5-run batch。
- 桶：positive `>= 1.20x`，weak-positive `1.05x-1.20x`，neutral `0.95x-1.05x`，negative `< 0.95x`，unstable 为跨桶摇摆。
- checksum mismatch、Evidence Doctor Error 无法解释、board 工具失败或 dirty isolation 不安全时停止。

## 继续 / 停止条件

默认继续到 correctness、asm、board repeated、Evidence Doctor 和 result 回填。

`next_phase_default`：

- 若 v1 positive：进入 scaling production mismatch audit / PI1 前置计划。
- 若 v1 weak/neutral/negative：拒绝或暂缓 scaling family，转向 `rgb_segment_store_v1` 或 RGB production integration plan。
- 若 Evidence Doctor Error 无法修正：标为 blocked，不改 production。
