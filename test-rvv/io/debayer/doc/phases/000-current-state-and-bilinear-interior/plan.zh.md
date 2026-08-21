# Phase 000: current-state-and-bilinear-interior

## S0 偏好与边界冻结

| 字段 | 本阶段冻结 |
| --- | --- |
| loaded_instruction_sources | `AGENTS.md`、`.agents/config/defaults.yaml`、`.agents/knowledge/pcl-rvv-knowledge-map.md`、`rvv-workflow`、`short-prompt-entry.zh.md`、`s0-preferences-and-recovery.zh.md`、`worker-quality-gates.zh.md`、`topic-lifecycle.zh.md`、`handoff-packet.zh.md`、`reviewability-and-language.zh.md`、`rvv-test`、`optimization-phase-loop.zh.md`、`test-taxonomy.zh.md`、`entry-shapes-and-test-support.zh.md`、`rvv-implementation`、`writing-good-tests.md`。 |
| preferences_loaded | defaults loaded；local override absent；prompt override 指定当前 topic、worker 角色、持续推进和板卡可用。 |
| work_preferences | 中文优先；英文术语首次出现补中文解释；test-rvv / diagnostic 注释详细中文；production 注释克制且本 phase 不改 production；证据默认 summary-only。 |
| commit_preferences | 默认不提交；本阶段只建立 topic 测试资产、topic-local 文档和可再生成 evidence。 |
| dirty_isolation | 允许路径限定为 `test-rvv/io/debayer/**`、必要时更新 `doc-rvv/library-screening/io/io-function-evaluation-queue.zh.md` 的 debayer 状态；其它现有 dirty paths 不触碰。 |

## 阶段意图和边界

本阶段要证明 `io/src/debayer.cpp` 中 `DeBayer::debayerBilinear` 的 full-size 内区 2x2 Bayer stencil（邻域模板）是否值得继续推进 RVV（RISC-V Vector，可变向量长度向量扩展）优化。首轮只做 production-shaped diagnostic（生产形态诊断）：测试专用 helper 复刻 production 内区语义，Std / RVV 两侧在同一输入生成器和同一输出布局下对拍。

本阶段不证明 production dispatch（生产分流）已经接入，也不修改 `io/src/debayer.cpp`。如果诊断板卡结果稳定正向，后续阶段才进入 production integration plan（生产接入计划）。

## 标量路径重建

`DeBayer::debayerBilinear` 的公开入口接收 Bayer 输入指针、RGB 输出指针、宽高、Bayer 行跨度和 RGB 行跨度。默认 `bayer_line_step=width`、`bayer_line_step2=width*2`、`rgb_line_step=width*3`。函数先处理首两行，再按 `y += 2` 进入主体；每个主体 block 处理 `(x,y)`、`(x+1,y)`、`(x,y+1)`、`(x+1,y+1)` 四个输出像素。

内区主体的可向量化点是固定邻域 byte load（字节加载）、`AVG` / `AVG4` 整数平均和 RGB stride store（跨步写回）。边界行列依赖单独公式，保留标量。`debayerEdgeAware` 与 `debayerEdgeAwareWeighted` 共享外层结构，但绿色通道增加 `abs` 梯度比较、mask（掩码）选择或加权除法，不在本 phase 关闭。

## validated_scope / unvalidated_scope

| scope | 内容 |
| --- | --- |
| validated_scope | `debayerBilinear` full-size contiguous Bayer 输入内区，偶数 width/height，GRBG 2x2 block，RGB line step 可含 padding。 |
| unvalidated_scope | Bayer 输入 padding；首末两行 / 首末两列；`debayerEdgeAware`；`debayerEdgeAwareWeighted`；OpenNI Bayer wrapper；PCLZF Bayer reader；真实 production dispatch。 |
| phase_closeout_boundary | 只能关闭 test-only bilinear 内区 candidate 的 correctness / bench / asm / board 证据；不能关闭 production topic。 |

## 候选族与假设

| candidate family | 假设 | 风险 | 本阶段动作 |
| --- | --- | --- | --- |
| bilinear-inner-u8-stencil | 使用 `vlse8` 跨步加载多个 2x2 block 的邻域字节，`vzext` 到 u16 后做 `AVG` / `AVG4`，再 `vsse8` 写回交错 RGB。 | 多通道 stride store、窄化转换和 u8/u16 LMUL（向量寄存器组倍率）选择可能吞掉收益。 | 先实现 test-only RVV candidate，运行 correctness、asm 和板卡 bench。 |
| scalar-boundary-split | production 若接入，边界行列仍保持标量，只替换内区主体。 | production diff 需要清晰拆分，避免重复计算或覆盖边界。 | 本阶段只记录为后续 PI1 候选。 |
| edge-aware-mask | 用 mask 表达梯度方向选择。 | 分支分布未知，weighted 分支还有除法。 | deferred 到后续 phase。 |

## TDD 和测试动作

写测试前的 break statement（失效点声明）：如果 GRBG 内区四个像素的 R/G/B 公式、RGB line padding 或 RVV path gate（路径命中验收）任何一个和 production 语义不一致，新增 gtest 必须失败。Bayer 输入 padding 已因当前 production 指针推进风险移入未验证范围。

| 动作 | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| RED | 新增 `include/debayer.h` stub、`src/test_debayer.cpp`；运行 `make -C test-rvv/io/debayer run_test_rvv`。 | RVV build 因 candidate path 仍是 `ScalarFallback` 而失败，不是语法错误。 |
| GREEN correctness | 在 `include/debayer.h` 实现 `debayerBilinearInteriorRVV`，保留非 RVV 标量 fallback。 | QEMU `run_test_compare` Std / RVV 均通过。 |
| bench smoke | `src/bench_debayer.cpp` 输出 case label、Iterations、Total Time、checksum 和 build path。 | QEMU 只运行 `run_bench_rvv` 作为日志形状 smoke，不运行 QEMU compare。 |
| asm | 运行 `make -C test-rvv/io/debayer dump_bench_rvv`。 | asm 摘要出现当前 candidate 相关 RVV 指令；若归属不清，降级为 partial。 |
| board | 板卡可用时运行 `run_board_debayer_bilinear_inner`，预算为初始 1 组、必要时同边界最多补 1 组。 | checksum 一致，decision bucket 进入 positive / weak_positive / neutral / negative / unstable。 |

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic（生产形态诊断），不是 production direct。 |
| A/B boundary | test helper（测试专用 helper）Std/RVV。 |
| 当前决策问题 | RVV-vs-scalar 候选筛选；不做 RVV-family-selection。 |
| diagnostic 是否可外推到 production | unknown；内区公式来自当前 production 源码，但尚未证明真实 `DeBayer` 公开入口 dispatch、边界拆分和 fallback。 |
| comparison-boundary / baseline mismatch 风险 | medium；bench 只测内区 helper，不含 production 边界处理和调用方包装成本。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 只有 correctness / asm 成立、实现很小、退化可归因于 test helper 计时边界时才允许；否则不进入 production。 |
| clean adoption 是否需要 production boundary 内 RVV-vs-RVV detail A/B | 本阶段不适用；若后续存在多个 production RVV family，则需要。 |

## Optimization Matrix

| candidate family | entry / layout | correctness | bench | board | asm | doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| bilinear-inner-u8-stencil | `debayerBilinear` full-size 内区，RGB output stride | planned | planned | planned | planned | planned | planned | 实现 test-only RVV candidate。 |
| scalar-boundary-split | production 边界行列标量，内区 RVV | not_applicable | not_applicable | not_applicable | not_applicable | not_applicable | deferred | 等内区诊断证据后进入 PI1。 |
| edge-aware-mask | edge-aware 内区 | planned later | planned later | planned later | planned later | planned later | deferred | 等 bilinear 证据。 |

## Board budget 与决策桶

本阶段板卡可用。默认预算：主 case 运行一次 `run_board_bench_compare`，若结果接近 1.0、方向与 asm / QEMU smoke 冲突，或 Evidence Doctor（证据体检）提示长尾 / 元数据缺口，则最多补一次同边界复跑。决策桶口径：`positive` 明显大于 1.10 且方向稳定；`weak_positive` 为 1.03 到 1.10；`neutral` 接近 1；`negative` 稳定小于 1；`unstable` 为预算内跨桶摇摆。

## 继续 / 停止条件

默认继续到 correctness、asm、板卡 bench 和 Evidence Doctor 人工检查闭合。允许停止的条件只有：工具链或板卡不可达、编译器 intrinsic 不支持导致 candidate 无法实现、Evidence Doctor Error 无法修复、dirty isolation 不安全，或诊断结果明确不建议进入 production 且 evaluation / matrix / roadmap 已同步。
