# Phase 000 计划：当前状态与诊断脚手架

## 阶段意图和边界

本阶段为 `io/src/image_depth.cpp` 建立 production-shaped diagnostic（生产形态诊断，测试专用但贴近真实公开入口）的最小闭环。范围限定在 `DepthImage::fillDepthImage()` 和 `DepthImage::fillDisparityImage()` 的 16-bit depth buffer 到 float output 转换：invalid pixel（无效像素）mask、毫米到米转换、`constant / pixel` 视差公式、nearest-neighbor downsample（最近邻下采样）和 `line_step` padding（行尾填充）。

本阶段不修改 production（生产源码），不声称真实 `DepthImage` public entry（公开入口）已经命中 RVV，也不覆盖 OpenNI legacy `openni_camera/openni_depth_image.cpp`。`fillDepthImageRaw()` 只作为同类入口记录，首轮不做 RVV 候选。

## 当前状态清单

| 项目 | 当前状态 | 路径 |
| --- | --- | --- |
| 队列入口 | 建议队列第 2 项，状态为未启动 | `doc-rvv/library-screening/io/io-function-evaluation-queue.zh.md` |
| production 源码 | 三个公开方法共享 wrapper-backed depth buffer，当前没有 `__RVV10__` 分流 | `io/src/image_depth.cpp` |
| topic 测试资产 | 本阶段新建 | `test-rvv/io/image_depth/` |
| phase 文档 | 本阶段新建 | `test-rvv/io/image_depth/doc/phases/000-current-state-and-diagnostic-scaffold/` |
| production 长期主题文档 | 不适用，尚无 adopted production behavior（已采用生产行为） | `doc-rvv/io/image_depth-RVV.zh.md` 不创建 |

## 假设与候选族

| candidate family | 假设 | 风险 | 本阶段动作 |
| --- | --- | --- | --- |
| contiguous depth meters RVV | `xStep == 1` 时可以连续加载 `uint16_t`，用 mask 选择 NaN 或 `pixel * 0.001f` | NaN bit pattern（位模式）与标量 `quiet_NaN` 可能不同；padding 不能覆盖 | 写 correctness test、RVV helper、bench 和反汇编检查 |
| contiguous disparity RVV | `xStep == 1` 时可以连续加载并执行 `constant / pixel` | 除法吞吐可能限制收益；invalid lane 必须写 0 | 写 correctness test、RVV helper、bench 和板卡 case |
| downsample scalar fallback | `xStep > 1` 首轮保持标量，避免 stride load（跨步加载）收益和风险混在一起 | downsample 仍可能是常见入口；首轮不能写成已优化 | correctness 覆盖 fallback；roadmap 加后续 stride-load phase |

## 优化矩阵

| candidate family | row source / layout | scope and entry | correctness / fallback target | bench / board target | asm boundary | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| contiguous depth meters RVV | contiguous `uint16_t` depth buffer，tight 或 padded output | `fillDepthImage` 形态诊断 | `run_test_compare` | `run_board_depth_full` | `dump_bench_rvv` | planned | planned |
| contiguous disparity RVV | contiguous `uint16_t` depth buffer，tight output | `fillDisparityImage` 形态诊断 | `run_test_compare` | `run_board_disparity_full` | `dump_bench_rvv` | planned | planned |
| downsample fallback | `xStep > 1` | `fillDepthImage` / `fillDisparityImage` 下采样 | `run_test_compare` | bench smoke only | not_applicable first phase | planned | deferred |

## 实现和测试动作

| 动作 | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| 写 RED 测试 | `src/test_image_depth.cpp`，先因缺少候选 helper 编译失败 | 失败原因指向缺少 `image_depth.h` 或候选 API |
| 实现候选 helper | `include/image_depth.h` | `run_test_compare` 在 QEMU 通过 |
| 建 bench 入口 | `src/bench_image_depth.cpp` | `run_bench_rvv` 可在 QEMU 做日志形状 smoke |
| 反汇编归属 | `make dump_bench_rvv` | asm 中能看到 depth/disparity helper 相关 RVV 指令 |
| 板卡性能 | `run_board_depth_full`、`run_board_disparity_full` | repeated 或至少单次 board summary 可解释；性能结论只来自板卡 |

## Evidence Doctor 和 registry 规则

本阶段先使用 `test-rvv/script/analyze_bench_compare.py` 生成板卡 summary。若形成 EvidenceDecision（证据决策）或 repeated board summary，再补 topic-local manifest 并运行 `test-rvv/script/evidence_doctor.py`。当前 registry（证据登记表）尚未接入，Handoff 写 `evidence_registry_status=not_available` 并列人工检查路径。

## 板卡复跑预算和决策桶

板卡当前可用。首轮预算为每个主 case 1 次 `run_board_bench_compare`，每次 `iterations=20`、`warmup=3`。如果 summary 明显 `std/RVV speedup > 1.10x` 且 checksum 匹配，桶为 `positive`；`1.02x-1.10x` 为 `weak_positive`；`0.98x-1.02x` 为 `neutral`；低于 `0.98x` 为 `negative`。若 depth 和 disparity 桶相反，不做 production 结论，进入消融或保留 diagnostic。

## 继续 / 停止条件

默认继续到 correctness、QEMU smoke、asm 和板卡首轮性能。只有编译工具不可用、板卡命令失败、checksum 不一致、Evidence Doctor Error 未能处理、或继续需要修改 production 时停止。若主 case 板卡正向，本阶段只能升级为 partial-production-candidate（局部生产候选），下一阶段应先写 PI1 production integration plan（生产接入计划）。

## 文档更新清单

- 新建 `doc/image_depth-evaluation.zh.md` 记录函数级评估和诊断证据链。
- 新建 `doc/optimization-roadmap.zh.md` 和 `doc/phases/optimization-matrix.zh.md`。
- 本阶段 result 回填实际命令、证据路径、Evidence Doctor 状态、continue / stop decision。
- 不创建 `doc-rvv/io/image_depth-RVV.zh.md`，除非后续 production integration loop 通过且用户确认采纳。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | test helper，baseline 是测试专用 scalar reference，candidate 是测试专用 RVV helper |
| 当前决策问题 | RVV-vs-scalar 是否值得进入 production probe |
| diagnostic 是否可外推到 production | 只能部分外推；它复刻像素公式、invalid mask、padding 和 downsample，但没有真实 `DepthImage` dispatch |
| comparison-boundary / baseline mismatch 风险 | 有；helper 不含 wrapper 虚调用和异常检查，也不证明 OpenNI legacy 入口 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 允许条件是 contiguous depth 或 disparity 至少 weak-positive 且 correctness / asm 无缺口；否则先做消融 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 首次候选无已有 adopted family；production adoption 仍必须补 production direct correctness、fallback、asm 和 board |
