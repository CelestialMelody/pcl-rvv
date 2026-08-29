# Phase 000: current-state and CGDM scaffold

## 阶段意图和边界

本阶段启动 `recognition/include/pcl/recognition/color_gradient_dot_modality.h` 的 RVV 函数级评估。
阶段范围收窄为 `ColorGradientDOTModality<PointInT>::processInputData()` 里的两段预处理：
`computeMaxColorGradients()` 和 `computeDominantQuantizedGradients()`。输入是 organized
`PointXYZRGB` 图像，输出是 `dominant_quantized_color_gradients_`。

本阶段不修改 production（生产源码），不接入 `ColorGradientDOTModality` 的真实分流，不优化
`computeInvariantQuantizedMap()`。后者会临时把 `color_gradients_` 的 magnitude 改成 `-1` 再恢复，
属于状态恢复风险更高的后续 phase。

## 当前状态清单

| 项目 | 当前事实 | 路径 |
| --- | --- | --- |
| 队列入口 | 执行清单第 5 项，状态为未启动，建议单独记录 correctness、bin tie-break 和收益 | `doc-rvv/library-screening/recognition/recognition-function-evaluation-queue.zh.md` |
| production 源码 | `processInputData()` 先算 color gradients，再生成 bin-level dominant quantized map | `recognition/include/pcl/recognition/color_gradient_dot_modality.h` |
| 相邻成熟 topic | `color_gradient_modality` 已采用 `src/`、`include/`、`include/impl/`、topic token 和 repeated board 证据结构 | `test-rvv/recognition/color_gradient_modality/` |
| DOTMOD 相邻 topic | `dotmod_template_matching` 目前只有空目录骨架，不作为成熟结构或证据来源 | `test-rvv/recognition/dotmod_template_matching/` |
| production 状态 | 当前文件没有 RVV path，也没有 `__RVV10__` 分流 | production remains scalar |

## 假设与候选族

| candidate family | 假设 | 风险 | 本阶段状态 |
| --- | --- | --- | --- |
| `cgdm-gradient-dominant-rvv` | RGB 右 / 下两点差分、最大通道选择、`sqrt` / `atan2` 和 bin 内 dominant 选择可以批量化 | `atan2_RVV_f32m2` 是有限输入域快速近似；dominant 选择的严格 `>` tie-break 和 output bit 语义必须保持 | planned |
| `cgdm-gradient-only-rvv` | 只替换 `computeMaxColorGradients()`，dominant bin 仍保持标量 | 可能被 bin 内 scalar scan 稀释；只能作为消融 | deferred |
| `cgdm-invariant-map-rvv` | `computeInvariantQuantizedMap()` 的局部 bin 多方向搜索可尝试 RVV 化 | 会临时改写并恢复 `color_gradients_`，状态语义复杂 | deferred to later phase |

## 诊断到 production 错配审计

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic（生产形态诊断） |
| A/B boundary | test helper 里的 scalar reference vs RVV candidate |
| 当前决策问题 | RVV-vs-scalar diagnostic 和 implementation-shape |
| diagnostic 是否可外推到 production | unknown；它复刻 `processInputData()` 的预处理链，但不修改真实 production dispatch |
| comparison-boundary / baseline mismatch 风险 | yes；bench 只测 test helper 或 public entry shaped wrapper，不证明 production RVV 已接入 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | weak_positive 以上且 correctness / asm 闭合时允许 PI1；neutral / negative 先拒绝本候选族 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | no；当前没有已采纳的同文件 RVV family，接入后 public Std/RVV positive 足以回答保留当前 patch 是否有收益 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `cgdm-gradient-dominant-rvv` | organized RGB image -> bin map | `PointXYZRGB` input，float magnitude / angle，`uint8_t` output map | test helper `computeDominantMapCandidate` | `run_test_compare`，RVV build 必须命中 candidate path | `bench_cgdm` case `dominant_map_*` | planned 5-run board repeated if QEMU and asm pass | `check_cgdm_rvv_asm` | planned | planned | write RED test first |
| `cgdm-invariant-map-rvv` | region / mask driven bin neighborhood | `PointXYZRGB` input + `MaskMap` / `RegionXY` | future helper for `computeInvariantQuantizedMap()` | not run | not run | not run | not run | not run | deferred | only after Phase 000 decision |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| Phase plan | 本文件 | 写在测试和实现前 |
| RED gtest | `make -C test-rvv/recognition/color_gradient_dot_modality run_test_rvv` | RVV build 因 candidate path 仍是 scalar fallback 失败 |
| GREEN candidate | `include/impl/cgdm_color_gradient.hpp` | RVV build 命中 candidate path，Std/RVV gtest 均通过 |
| QEMU smoke | `make -C ... run_bench_rvv BENCH_ARGS="--case-filter dominant_map_320x240 --iterations 1 --warmup-iterations 1"` | 只证明日志形状和可运行，不作性能结论 |
| asm | `make -C ... check_cgdm_rvv_asm` | 能看到 RVV load / convert / sqrt / atan2 / store 指令 |
| board repeated | `make -C ... board_repeated record_evidence_state_repeated` | 5-run summary 有 decision bucket |

## 板卡复跑预算和决策桶

板卡由用户说明可用。本阶段默认先跑 5-run repeated board。若 summary 或 Evidence Doctor 显示方向接近阈值、
长尾或 `B/A < 1` 摇摆，最多同边界再跑 1 组确认。

- `positive`: median speedup >= 1.20 且 `B/A < 1` 为 0。
- `weak_positive`: median speedup 在 `[1.05, 1.20)` 且方向稳定。
- `neutral`: median 在 `[0.95, 1.05)`。
- `negative`: median < 0.95。
- `unstable`: 预算内跨桶摇摆或长尾无法解释。

## 继续 / 停止条件

若 Phase 000 correctness、asm 和板卡证据为 positive 或 weak_positive，默认进入 PI1 production
integration plan，并按用户本轮授权在接入后板卡有收益时视为可采纳。若证据为 neutral / negative，
只拒绝当前 `cgdm-gradient-dominant-rvv` 候选，不直接推出整个 DOTMOD 链路 no-production。

## 文档更新清单

本阶段更新 README、evaluation、phase result、optimization roadmap、optimization matrix 和 Handoff。
`doc-rvv/recognition/color_gradient_dot_modality-RVV.zh.md` 只有在 production direct 证据支持并采纳后才适用。
