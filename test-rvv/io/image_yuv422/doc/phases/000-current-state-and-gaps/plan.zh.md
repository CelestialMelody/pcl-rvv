# Phase 000: current-state-and-gaps

## S0 偏好与边界冻结

| 字段 | 本阶段冻结 |
| --- | --- |
| loaded_instruction_sources | `AGENTS.md`、`.agents/config/defaults.yaml`、`.agents/knowledge/pcl-rvv-knowledge-map.md`、`rvv-workflow`、`short-prompt-entry.zh.md`、`worker-quality-gates.zh.md`、`optimization-phase-loop.zh.md`、`rvv-test`、`doc-suite-quality-bar.zh.md`、`function-evaluation-and-closeout.zh.md`、`document-ownership-and-traceability.zh.md`、`makefile-env.md`、`writing-good-tests.md`。 |
| preferences_loaded | defaults loaded；local override absent；prompt override 指定当前 topic、worker 角色和板卡可用。 |
| work_preferences | 中文优先；英文术语首次出现补中文解释；test-rvv / diagnostic 注释详细中文；production 注释克制且本阶段不改 production；证据默认 summary-only。 |
| commit_preferences | 默认不提交；本阶段只建立 topic 测试资产、topic-local 文档和本地可再生成 evidence。 |
| dirty_isolation | 允许路径限定为 `test-rvv/io/image_yuv422/**`、必要时更新 `doc-rvv/library-screening/io/io-function-evaluation-queue.zh.md` 的本 topic 状态；其它现有 dirty paths 不触碰。 |

## 阶段意图和边界

本阶段要证明 `io/src/image_yuv422.cpp` 中 YUYV / YUV422（两像素共享 U/V 的图像格式）颜色转换是否值得继续推进 RVV（RISC-V Vector，可变向量长度向量扩展）优化。首轮只做 production-shaped diagnostic（生产形态诊断）：测试专用 helper 复刻 `ImageYUV422::fillRGB` 和 `fillGrayscale` 的内层语义，Std / RVV 两侧在同一 wrapper（包装层）和同一输入生成器下对拍。

本阶段不证明 production dispatch（生产分流）已经接入，也不修改 `io/src/image_yuv422.cpp`。如果诊断板卡结果稳定正向，后续阶段才进入 production integration plan（生产接入计划）。

## 标量路径重建

`ImageYUV422::fillRGB` 的公开入口由 `ImageYUV422` wrapper 提供数据指针、输入宽高和输出 line step（行跨度）。全尺寸路径每 4 个输入字节读取 `U Y1 V Y2`，两个像素共享 `U` / `V`，分别使用：

- `R = clip(Y + ((V - 128) * 18678 + 8192) >> 14)`
- `G = clip(Y + ((V - 128) * -9519 - (U - 128) * 6472 + 8192) >> 14)`
- `B = clip(Y + ((U - 128) * 33292 + 8192) >> 14)`

downsample RGB 路径按 `wrapper_width / output_width` 跨步取样，每次只输出一个 RGB 像素；`fillGrayscale` 只复制 Y 分量，并按输出行跨度保留 padding（行尾填充）。

## validated_scope / unvalidated_scope

| scope | 内容 |
| --- | --- |
| validated_scope | `fillRGB` 全尺寸 contiguous two-pixel path；`fillRGB` downsample 标量 fallback 对拍；`fillGrayscale` 全尺寸和 downsample 对拍；输入宽度使用偶数，覆盖 padding。 |
| unvalidated_scope | 真实 production dispatch；OpenNI legacy `openni_image_yuv_422.cpp`；奇数宽度的历史未定义边界；真实 `FrameWrapper` 对象状态；生产接入后的 fallback gate。 |
| phase_closeout_boundary | 只能关闭 test-only diagnostic helper 的 correctness / bench / asm / board 证据；不能关闭 production topic。 |

## 候选族与假设

| candidate family | 假设 | 风险 | 本阶段动作 |
| --- | --- | --- | --- |
| contiguous-rgb-strided-u8 | 使用 `vlse8` 跨步加载 U/Y/V，并用整数 RVV 公式计算 RGB，再 `vsse8` 写回 interleaved RGB。 | byte interleave 写回和 narrowing（窄化转换）可能吞掉收益；全尺寸宽度必须偶数。 | 实现 test-only RVV candidate，运行 correctness、asm 和板卡 bench。 |
| grayscale-strided-y-copy | 使用 `vlse8` 加 `vsse8` 复制全尺寸 Y 分量。 | 纯内存搬运收益可能弱；downsample 仍是标量。 | 作为轻量 candidate 与 RGB 同步验证。 |
| downsample-rvv | 对 downsample 采样点做 RVV gather / strided load。 | 采样跨度和每次只输出一个像素，首轮收益不确定。 | 本阶段 `deferred`，先用标量 fallback 对拍，不作为板卡主结论。 |

## TDD 和测试动作

写测试前的 break statement（失效点声明）：如果 `U/V` 共享、`CLIP_CHAR` 饱和、两像素输出顺序、downsample stride 或 line_step padding 任何一个和 production 语义不一致，新增 gtest 必须失败。

| 动作 | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| RED | `src/test_image_yuv422.cpp` 先调用尚不存在的 `image_yuv422.h` helper；运行 `make -C test-rvv/io/image_yuv422 run_test_compare`。 | 编译失败点来自缺少测试专用 helper，而不是语法错误。 |
| GREEN correctness | 新增 `include/image_yuv422.h`，实现标量 reference 和 RVV candidate；补 `Makefile` / `board.mk`。 | QEMU `run_test_compare` Std / RVV 均通过。 |
| bench smoke | 新增 `src/bench_image_yuv422.cpp`；QEMU 只运行 RVV bench 小型 smoke 或只编译，不运行 QEMU compare。 | bench 输出包含 case label、Iterations、Total Time、checksum 和 build path。 |
| asm | 运行 `make -C test-rvv/io/image_yuv422 dump_bench_rvv`。 | asm 摘要出现当前 candidate 相关 RVV 指令；若归属不清，降级为 `asm boundary partial`。 |
| board | 板卡可用时运行 `run_board_yuv422_rgb_full` 和 `run_board_yuv422_smoke`，预算为初始 1 组、必要时同边界最多补 1 组。 | checksum 一致，decision bucket 进入 positive / weak_positive / neutral / negative / unstable。 |

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic（生产形态诊断），不是 production direct。 |
| A/B boundary | test helper（测试专用 helper）Std/RVV。 |
| 当前决策问题 | RVV-vs-scalar 候选筛选；不做 RVV-family-selection。 |
| diagnostic 是否可外推到 production | unknown；源码公式同构，但尚未证明真实 `ImageYUV422` 公开入口 dispatch 和 fallback。 |
| comparison-boundary / baseline mismatch 风险 | low for diagnostic helper；production 取舍前仍需 public entry 直接证据。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 允许条件是实现小、fallback 明确、correctness / asm 成立且退化只出现在 test helper 写回形态；否则不进入 production。 |
| clean adoption 是否需要 production boundary 内 RVV-vs-RVV detail A/B | 本阶段不适用；若后续存在多个 production RVV family，则需要。 |

## Optimization Matrix

| candidate family | entry / layout | correctness | bench | board | asm | doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| contiguous-rgb-strided-u8 | `fillRGB` full-size, even width, RGB output stride | planned | planned | planned | planned | planned | planned | 建 test-only candidate。 |
| grayscale-strided-y-copy | `fillGrayscale` full-size | planned | planned | planned | planned | planned | planned | 与 RGB candidate 同步验证。 |
| downsample-rvv | downsample RGB / grayscale | fallback-only planned | not_applicable | not_applicable | not_applicable | not_applicable | deferred | 若 full-size 正向，再开窄 phase。 |

## Board budget 与决策桶

本阶段板卡可用。默认预算：每个主 case 运行一次 `run_board_bench_compare`，若结果接近 1.0、方向与 asm / QEMU smoke 冲突，或 Evidence Doctor（证据体检）提示长尾 / 元数据缺口，则最多补一次同边界复跑。决策桶口径：`positive` 明显大于 1.10 且方向稳定；`weak_positive` 为 1.03 到 1.10；`neutral` 接近 1；`negative` 稳定小于 1；`unstable` 为预算内跨桶摇摆。

## 继续 / 停止条件

默认继续到 correctness、asm、板卡 bench 和 Evidence Doctor 人工检查闭合。允许停止的条件只有：工具链或板卡不可达、编译器 intrinsic 不支持导致 candidate 无法实现、Evidence Doctor Error 无法修复、dirty isolation 不安全，或诊断结果明确不建议进入 production 且 evaluation / matrix / roadmap 已同步。
