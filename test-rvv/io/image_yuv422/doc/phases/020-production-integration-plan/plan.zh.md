# Phase 020: production-integration-plan

## 阶段意图和边界

本阶段进入 production integration loop（生产接入闭环），把 Phase 010 的
`rgb-segment-store-u8` 候选从测试专用 helper 推进到真实
`pcl::io::ImageYUV422::fillRGB` public entry（公开入口）。本阶段默认连续推进
PI1-PI5：生产计划、生产补丁、production direct（真实生产路径）测试、生产证据重跑和
PI5 用户检查点。

本阶段只覆盖 `io/src/image_yuv422.cpp` 中 `fillRGB` 的 full-size RGB24 输出路径。
覆盖条件是输入 `FrameWrapper` 的宽高等于请求输出宽高、宽度为偶数、输出仍为 YUYV
两像素一组到 RGBRGB 交错布局。`fillRGB` downsample（下采样）保持原标量路径；
`fillGrayscale` 保持原标量路径；OpenNI legacy
`io/src/openni_camera/openni_image_yuv_422.cpp` 不在本阶段修改。

## PI1 候选范围和 fallback 矩阵

| 维度 | 本阶段冻结 |
| --- | --- |
| production entry | `pcl::io::ImageYUV422::fillRGB` |
| RVV gate | `__RVV10__` 已启用；`wrapper_->getWidth () == width`；`wrapper_->getHeight () == height`；`width % 2 == 0` |
| fallback | 非 RVV 构建、downsample、奇数宽度、灰度、OpenNI legacy 均走原标量语义 |
| public API | 不改 header，不新增公开 API |
| helper layout | 在 `io/src/image_yuv422.cpp` 内抽出文件局部 `fillRGBStd` 和 `fillRGBFullSizeRVV`；public entry 只保留既有参数检查、line step 准备和短路分流 |
| production 注释 | 只解释 fallback / dispatch / YUYV 数据布局边界，不逐行讲 intrinsic |

## 当前状态清单

| 项目 | 当前状态 |
| --- | --- |
| diagnostic correctness | `run_test_compare` Std / RVV 各 3 个 gtest 通过 |
| diagnostic asm | `dump_bench_rvv` 可见 `vssseg3e8.v`、`vlse8.v`、整数乘法和饱和指令 |
| diagnostic board | `run_board_yuv422_rgb_full`：`Std 2.3034 ms`、`RVV 1.5785 ms`，约 `1.46x` |
| diagnostic doctor | `Errors=0, Warnings=1, Suggestions=0`；warning 为 `low_run_count` |
| production source | 尚未修改 `io/src/image_yuv422.cpp` |
| production direct gap | 缺真实 `ImageYUV422` public entry 测试、bench、asm 归属和 repeated board |

## TDD 和实现动作

写测试前的失效点声明：如果生产直连测试没有真实构造 `ImageYUV422`，没有通过 public
`fillRGB` / `fillGrayscale` 入口，或者 bench label 仍只测 test-only helper，则本阶段测试必须失败或被标成缺口。

| PI | 动作 | 产物 / 命令 | 完成判据 |
| --- | --- | --- | --- |
| PI1 | 冻结计划 | 本文件 | 候选范围、fallback、bench、board budget 和停止条件清楚 |
| PI3-RED | 先补 production direct test / bench 入口 | `src/test_image_yuv422.cpp`、`src/bench_image_yuv422.cpp`、`Makefile` | 第一次运行暴露当前测试资产还未链接真实 production entry 或缺 production bench label |
| PI2 | 生产补丁 | `io/src/image_yuv422.cpp` | 非 RVV 构建仍使用原标量 helper；RVV 构建只在 full-size even-width RGB 命中 RVV |
| PI3-GREEN | production direct correctness | `make -C test-rvv/io/image_yuv422 run_test_compare` | Std / RVV 的 helper diagnostic 与 production direct 测试全部通过；downsample 和 grayscale fallback 保持一致 |
| PI4-QEMU | QEMU smoke | `make -C test-rvv/io/image_yuv422 run_bench_rvv BENCH_ARGS="--case-filter prod_rgb_full_640x480 --iterations 1 --warmup-iterations 1"` | 只验证可运行、checksum 和日志形状，不写性能结论 |
| PI4-asm | 生产反汇编 | `make -C test-rvv/io/image_yuv422 dump_bench_rvv` | RVV bench 二进制内可见生产路径相关 `vssseg3e8.v` / `vlse8.v` / 整数公式指令 |
| PI4-board | 生产板卡 bench | `make -C test-rvv/io/image_yuv422 run_board_yuv422_prod_rgb_full fetch_board_logs` | production public Std/RVV 同边界 checksum 一致，进入 positive / weak_positive / neutral / negative / unstable 决策桶 |
| PI4-doctor | Evidence Doctor | `make -C test-rvv/io/image_yuv422 run_evidence_doctor` | Errors 为 0；Warnings 必须解释并限制结论 |
| PI5 | 用户检查点 | phase result / Handoff | 保留当前 patch，报告 production diff、命令、板卡和 doctor 结果，等待用户确认采纳或回滚 |

## Diagnostic 到 Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | Phase 010 是 production-shaped diagnostic；Phase 020 要生成 production-public evidence。 |
| A/B boundary | Phase 020 的主 A/B 是真实 public overload：Std build 的 `ImageYUV422::fillRGB` 对 RVV build 的同一 public entry。 |
| 当前决策问题 | RVV-vs-scalar，判断当前 public RVV path 是否值得保留为 bounded production candidate。 |
| diagnostic 是否可外推到 production | 只能作为进入 PI 的理由；最终结论以 production direct correctness、asm 和 board 为准。 |
| comparison-boundary / baseline mismatch 风险 | 低到中等。测试 helper 与生产源码公式同构，但真实 `FrameWrapper`、line_step 和 public validation 必须通过 production direct 补证。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 已有 positive diagnostic 且实现范围小，允许本阶段有界生产探针；若 production direct 为 neutral/negative，则停在 PI5 等用户确认是否回滚。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前 production 无既有 RVV family，本阶段不做 family selection；PI5 positive 也只进入待用户确认采纳。 |

## Board budget 与决策桶

本阶段板卡已确认可用。默认先跑一次 production focused board compare，case-filter 为
`prod_rgb_full_640x480`，`iterations=20`、`warmup=3`。若 Evidence Doctor 报告方向接近阈值、
checksum / metadata 异常或结果与 Phase 010 方向矛盾，最多补一次同边界确认复跑。

决策桶：`positive` 为 speedup 明显大于 `1.10x` 且 checksum 一致；`weak_positive` 为
`1.03x` 到 `1.10x`；`neutral` 接近 1；`negative` 稳定小于 1；`unstable` 为预算内跨桶摇摆。
QEMU timing 不进入性能桶。

## 继续 / 停止条件

默认继续到 PI5 用户检查点。只有以下条件允许提前停止：生产 gate 无法隔离、测试无法真实构造
`FrameWrapper`、非 RVV 构建被破坏、asm 无法证明 RVV 指令命中生产路径、板卡访问失败、
Evidence Doctor Error 无法修复，或实现需要扩大到本阶段未授权的 OpenNI legacy / downsample /
灰度 / public API 变更。
