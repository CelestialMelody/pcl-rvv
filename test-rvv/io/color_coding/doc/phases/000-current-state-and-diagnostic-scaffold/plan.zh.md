# Phase 000 Plan: current-state-and-diagnostic-scaffold

## 阶段意图和边界

本阶段为 `io/include/pcl/compression/color_coding.h` 建立 component ablation（组件消融）脚手架，先回答 `ColorCoding::encodeAverageOfPoints`、`encodePoints`、`decodePoints` 和 `setDefaultColor` 的 byte average（字节平均）、XOR diff（异或差分）和连续写回是否值得继续 RVV 化。

本阶段不修改 production（生产源码）文件，不接入 `OctreePointCloudCompression` 公开入口，不证明完整 octree compression（八叉树压缩）性能。测试资产只覆盖 synthetic leaf（人工构造叶节点）和 RGBA offset（颜色字段偏移）语义。

## 当前状态清单

| area | current state | path |
| --- | --- | --- |
| production source | 当前源码为标量实现：按 `indices` 两次 gather color，计算平均值，保存平均颜色和差分颜色；decode 按连续 `[begin,end)` 写回 | `io/include/pcl/compression/color_coding.h` |
| screening source | 复筛清单建议以 color coding component ablation 启动，避免和 point coder 混测 | `doc-rvv/library-screening/io/io-retained-candidate-rescreen.zh.md` |
| topic assets | 本 topic 没有既有 `test-rvv/io/color_coding/` 产物 | `test-rvv/io/color_coding/` |
| sibling calibration | `lzf_image_io`、`point_cloud_image_extractors` 使用 `src/`、`include/`、topic-local docs、Makefile/board.mk | `test-rvv/io/lzf_image_io/`、`test-rvv/io/point_cloud_image_extractors/` |

## Phase Scope 与扩展队列

| field | value |
| --- | --- |
| validated_scope | diagnostic helper（诊断 helper）层；synthetic `ColorPoint`；32-bit RGBA little-endian field；`Scalar` 不适用；leaf sizes `1/2/7/31/257` |
| unvalidated_scope | 真实 octree traversal、entropy coder、完整 public compression entry、其它 PointT 布局、point coder family、生产 dispatch |
| point_type_expansion_queue | 若 board component evidence 正向，下一 phase 才审计 `pcl::PointXYZRGBA` / packed RGB field 的 production-shaped diagnostic；生产模板泛型不在本阶段关闭 |
| phase_closeout_boundary | 只能关闭测试支撑 scaffold、same-chain correctness 和 component-level bench readiness；不能关闭 production adoption |

## 假设与候选族

| candidate family | idea source | hypothesis | risk |
| --- | --- | --- | --- |
| scalar same-chain reference | 当前 production 源码 | 用测试专用 reference 固定整数舍入和 bit reduction（位深压缩）语义 | 若 reference 和 production 不对拍，后续 RVV 证据无效 |
| RVV indexed average/provisional encode | 当前源码 + RVV indexed load | 对 indexed leaf 做 `vluxei32` gather，减少 RGB 分量拆取和求和成本 | leaf trip count 可能太短；indices gather 可能吞掉收益 |
| RVV decode/default contiguous store | 当前源码 | decode 和 setDefaultColor 是连续输出段，可能比 indexed encode 更适合 RVV | 差分输入仍是 byte stream，store offset 可能受结构布局影响 |
| no-production diagnostic | 复筛清单 | component 负向时不进入 production | diagnostic 负向不能直接拒绝 bounded production probe，需写 mismatch audit |

## 优化矩阵

| candidate family | row source policy | point type / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| scalar same-chain reference | indexed leaf + contiguous decode | `ColorPoint` with RGBA offset | test support | `run_test_compare` | not_applicable | not_applicable | not_applicable | not_applicable | planned | write RED tests |
| RVV indexed encode candidate | indexed leaf | `ColorPoint` with RGBA offset | component diagnostic | `run_test_compare` | `run_board_color_coding_repeated` | required if bench compares | `dump_bench_rvv` | required before EvidenceDecision | planned | implement after RED |
| RVV decode/default candidate | contiguous output segment | `ColorPoint` with RGBA offset | component diagnostic | `run_test_compare` | `run_board_color_coding_repeated` | required if bench compares | `dump_bench_rvv` | required before EvidenceDecision | planned | implement after RED |

## 实现和测试动作

| action | artifact | command | completion |
| --- | --- | --- | --- |
| 写 RED correctness tests（正确性测试） | `src/test_color_coding.cpp` | `make -C test-rvv/io/color_coding run_test_rvv` | candidate stub 失败在期望断言上 |
| 建立测试专用 helper 聚合入口 | `include/color_coding.h`、`include/impl/color_coding_support.hpp` | 同上 | reference 能表达 production 语义；candidate 初始失败 |
| 填入最小 candidate | `include/impl/color_coding_support.hpp` | `make -C test-rvv/io/color_coding run_test_compare` | Std/RVV tests pass |
| 建立 bench 和 board target | `src/bench_color_coding.cpp`、`Makefile`、`board.mk` | `make -C test-rvv/io/color_coding run_bench_rvv`；板卡 target 后续运行 | QEMU 只作为构建 / log-shape smoke；性能只看板卡 |
| 更新 phase result / roadmap / matrix | phase docs | n/a | 写清 evidence boundary 和下一 phase |

## Evidence Doctor 和 registry 规则

本阶段 correctness（正确性）不需要 Evidence Doctor。若运行 board repeated benchmark（重复板卡性能测试），必须生成 summary、manifest、Evidence Doctor 报告并写入 `log/evidence_registry.json`。QEMU bench timing（QEMU 性能计时）不能用于性能结论。

## 板卡复跑预算和决策桶

板卡当前可用。性能 phase 的默认预算为 5 次 repeated run，每次 `20` iterations、`3` warmup。decision bucket（决策桶）：`positive >= 1.08x`，`weak-positive 1.02x-1.08x`，`neutral 0.98x-1.02x`，`negative < 0.98x`，若方向摇摆则 `unstable`。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic（诊断） |
| A/B boundary | test helper |
| 当前决策问题 | implementation-shape（实现形态）和 RVV-vs-scalar component evidence |
| diagnostic 是否可外推到 production | no；它只隔离 color coding 组件，不包含 octree traversal、entropy coder 和 public compression entry |
| comparison-boundary / baseline mismatch 风险 | yes；component helper 的时间边界小于完整压缩入口 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 允许条件：component weak-positive 以上、正确性 / asm / Doctor 干净，且生产 probe 范围只限 color coder helper；本阶段不进入 probe |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes；若未来生产已存在另一个 RVV family，必须同边界比较 |

## 文档更新清单

- 新建 `test-rvv/io/color_coding/doc/color_coding-evaluation.zh.md`，记录 S2 函数级评估和 Traceability Map。
- 新建 `doc/phases/README.zh.md`、本 plan、result、optimization matrix、optimization roadmap。
- 不新建 `doc-rvv/io/color_coding-RVV.zh.md`，因为本阶段没有 adopted production behavior（已采用生产行为）。

## 继续 / 停止条件

默认继续到 RED、GREEN、QEMU correctness、asm smoke 和板卡 component bench。只有构建工具 / 板卡不可用、Evidence Doctor Error 无法处理、dirty isolation 不安全，或需要修改 production 时才停。
