# Phase 000: current-state and CGM scaffold

## 阶段意图和边界

本阶段启动 `recognition/include/pcl/recognition/color_gradient_modality.h` 的 RVV
函数级评估。阶段范围收窄为 organized RGB（有宽高的二维 RGB 输入）下的
`computeMaxColorGradientsSobel()` 与 `quantizeColorGradients()` 子链路，先验证
Sobel 选出的 dx/dy/magnitude 是否能通过 RVV `atan2_RVV_f32m2`（RVV 近似 atan2）
和向量量化保持 same-chain correctness（同构链路正确性）。

本阶段不修改 production（生产源码），不接入 `ColorGradientModality::processInputData()`，
不关闭 Gaussian smoothing、3x3 dominant filter、spread、feature extraction 或
LINEMOD end-to-end（端到端）收益问题。

## 当前状态清单

| 项目 | 当前事实 | 路径 |
| --- | --- | --- |
| 队列入口 | 执行清单第 2 项，状态为未启动，建议先建立 organized RGB same-chain correctness | `doc-rvv/library-screening/recognition/recognition-function-evaluation-queue.zh.md` |
| production 源码 | `processInputData()` 先 RGB 拷贝、Gaussian smoothing，再 Sobel、quantize、filter、spread | `recognition/include/pcl/recognition/color_gradient_modality.h` |
| 既有 topic 资产 | 启动前没有 `test-rvv/recognition` 或 `doc-rvv/recognition` 已完成主题 | `test-rvv/recognition/color_gradient_modality` |
| 相邻结构样板 | 采用 `src/` + `include/` + `include/impl/` 结构，topic token 为 `cgm` | `test-rvv/io/debayer`, `test-rvv/features/organized_edge_detection` |
| 工作流偏差 | RED 测试资产先于本 plan 写入；本 plan 记录该偏差，本阶段不把 `phase_plan_written_before_edits` 写成 pass | 本文件 |

## 假设与候选族

| candidate family | 假设 | 风险 | 本阶段状态 |
| --- | --- | --- | --- |
| `sobel-selected-angle-quantize-rvv` | Sobel max-channel 选出的 dx/dy 可以批量进入 RVV `atan2`、`sqrt` 和 angle quantize，减少 libm 热点 | `atan2_RVV_f32m2` 是 finite-domain fast approximation（有限输入域快速近似），角度靠近 22.5 度 bin 边界时可能跨 bin | planned |
| `full-sobel-rgb-stencil-rvv` | RGB 3 通道 3x3 Sobel stencil 可用 RVV stride load 批量计算 | 多通道 byte load / widening / channel max 可能让实现复杂，且 `atan2` 仍是风险中心 | deferred to roadmap |
| `dominant-3x3-filter-rvv` | 3x3 量化 map histogram 可用 byte load 和 mask 处理 | histogram tie-break 和阈值 `>=5` 需要逐点复刻 | deferred to Phase 010 |

## 诊断到 production 错配审计

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic（生产形态诊断） |
| A/B boundary | test helper（测试 helper）里的 scalar reference vs RVV candidate |
| 当前决策问题 | implementation-shape 和 RVV-vs-scalar diagnostic，不做 production adoption |
| diagnostic 是否可外推到 production | unknown；它复刻 Sobel+quantize 子链路，但不包含 Gaussian smoothing、filter、spread、feature extraction 和真实 public entry |
| comparison-boundary / baseline mismatch 风险 | yes；bench 只测 test helper 子链路，不代表完整 `processInputData()` |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no for this phase；必须先完成 filter / full processInputData production-shaped diagnostic |
| clean adoption 是否需要 production boundary A/B | yes；需要 production direct test、fallback、asm 和 board evidence |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `sobel-selected-angle-quantize-rvv` | organized RGB image | `pcl::RGB`, float magnitude / angle, AoS RGB | test helper `computeSobelQuantizedCandidate` | `run_test_compare` | `bench_cgm` case `sobel_quantize_*` | planned 5-run board repeated if QEMU and asm pass | `bench_cgm_rvv` filtered asm should contain vector load/convert/math | planned | planned | implement GREEN after RED |
| `dominant-3x3-filter-rvv` | quantized map | `uint8_t` map | production-shaped filter helper | future phase | future phase | not run | not run | not run | deferred | Phase 010 |
| `full-processInputData-production-shaped` | organized RGB image | `pcl::RGB` public-like input | full preprocessing diagnostic | future phase | future phase | not run | not run | not run | deferred | Phase 020 or later |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| RED gtest | `make -C test-rvv/recognition/color_gradient_modality run_test_rvv` | RVV build 因未命中 `RvvSobelQuantize` 失败 |
| GREEN candidate | `include/impl/cgm_sobel_quantize.hpp` | RVV build 命中 candidate path，Std/RVV gtest 均通过 |
| bench smoke | `make -C ... run_bench_rvv BENCH_ARGS="--case-filter sobel_quantize_320x240 --iterations 1 --warmup-iterations 1"` | QEMU 仅证明可运行和日志形状，不作为性能结论 |
| asm | `make -C ... check_cgm_rvv_asm` | 能看到本候选需要的 RVV 指令或明确降级 |
| board repeated | `make -C ... run_board_bench_compare ...` 或后续 topic-local repeated target | 板卡可用时做 5-run，有 decision bucket |
| Evidence Doctor | `test-rvv/script/evidence_doctor.py` 或人工检查 | Errors / Warnings / Suggestions 被写入 result |

## 板卡复跑预算和决策桶

板卡当前由用户说明可用。本阶段默认先跑 5-run repeated board（重复板卡性能测试）。
如果 summary 和 Evidence Doctor 显示方向接近阈值、长尾或 B/A 方向摇摆，最多同边界
再跑 1 组确认。decision bucket（决策桶）暂定：

- `positive`: median speedup >= 1.20 且 `B/A < 1` 为 0。
- `weak_positive`: median speedup 在 `[1.05, 1.20)` 且方向稳定。
- `neutral`: median 在 `[0.95, 1.05)`。
- `negative`: median < 0.95。
- `unstable`: 预算内跨桶摇摆或长尾无法解释。

## 继续 / 停止条件

本阶段只完成 Sobel 后 angle/quantize candidate 不能收口 topic。若 correctness、asm 和板卡
证据正向，下一 phase 默认进入 `dominant-3x3-filter-rvv` 或 full Sobel stencil RVV；若证据
弱 / 负，也只能拒绝当前候选族，不能直接推出 no-production。只有板卡不可达、工具失败、
证据矛盾、dirty isolation 不安全或需要扩大到 production 才允许停止。

## 文档更新清单

本阶段更新 README、evaluation、phase result、optimization roadmap、optimization matrix 和
Handoff。`doc-rvv/recognition/color_gradient_modality-RVV.zh.md` 当前不适用。
