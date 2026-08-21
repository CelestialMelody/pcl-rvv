# Phase 000: current-state-and-diagnostic-scaffold

## 阶段意图和边界

本阶段为 `io/src/openni2_grabber.cpp` 建立 OpenNI2 frame-to-cloud（帧到点云）RVV topic 的首个诊断闭环。目标是用 synthetic frame（合成帧）测试专用 helper 复刻 `OpenNI2Grabber::convertToXYZPointCloud`、`convertToXYZRGBPointCloud` 和 `convertToXYZIPointCloud` 的逐像素语义，先证明 depth projection（深度反投影）、invalid depth mask（无效深度掩码）、RGB/IR 字段合并和分辨率 mismatch（不一致）布点语义可被同构测试和 bench（性能测试）驱动。

本阶段不修改 production（生产源码）`io/src/openni2_grabber.cpp`，不要求真实 OpenNI2 设备，也不声明 production direct（真实生产路径）证据。当前证据角色是 production-shaped diagnostic（生产形态诊断）：测试输入和输出形态贴近公开 grabber 转换函数，但执行入口仍是 topic-local test helper。

## 当前状态清单

| area | current state |
| --- | --- |
| 队列表 | `doc-rvv/library-screening/io/io-function-evaluation-queue.zh.md` 将 `src/openni2_grabber.cpp` 列为第 5 个未启动 direct-main-path topic，建议 synthetic `DepthImage` / `Image` wrapper，真实设备不是首阶段前置条件。 |
| production 源码 | `io/src/openni2_grabber.cpp` 中三个热点循环分别写 `PointXYZ`、`PointXYZRGB/RGBA` 和 `PointXYZI`。RGB 路径在 depth/image 分辨率不一致时先把整张 cloud 初始化为 NaN + black + alpha 255，再用 depth 步长写 xyz、用 image 步长写 rgba。 |
| 现有 topic 资产 | `test-rvv/io/openni2_grabber` 尚未存在；`doc-rvv/io/openni2_grabber-RVV.zh.md` 尚未存在，当前阶段也不适用。 |
| 相邻成熟结构 | `test-rvv/io/image_depth` 和 `test-rvv/io/organized_pointcloud_conversion` 已使用 `src/`、`include/`、`doc/phases`、topic-local evaluation 和 board/doctor 输出结构。本 topic 采用同级结构，但不复制其算法结论或性能数字。 |
| dirty isolation | 当前工作区存在 registration 主题的大量未提交改动；本阶段允许触碰路径仅限 `test-rvv/io/openni2_grabber/**` 和必要的 io 队列表状态。 |

## 假设与候选族

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status |
| --- | --- | --- | --- | --- | --- | --- |
| `depth_xyz_contiguous_rvv` | 当前源码逐像素深度反投影循环 | depth/image 尺寸一致的 `PointXYZ` / xyz portion | 将 u/v 序列、mm->m 和 xyz 公式用 VL chunk（可变向量长度分块）处理 | AoS（结构数组）写 `PointXYZ` 可能需要 strided store（跨步存储）；NaN mask 语义要保持 | gtest same-chain、QEMU correctness、bench build/smoke、asm、board repeated、Evidence Doctor | planned |
| `rgb_overlay_contiguous_rvv` | 当前源码 RGB 独立覆盖循环 | image 尺寸一致或更大 cloud 中的 RGB/RGBA 字段写入 | RGB pack（颜色打包）可能适合字节 load + 32-bit store | PointXYZRGB/PointXYZRGBA `rgba` union 语义和 alpha 语义要对齐 | gtest alpha/rgba、bench、asm、board | planned |
| `ir_intensity_contiguous_rvv` | 当前源码 `PointXYZI` 合并循环 | depth/IR 同尺寸 | 同一循环内写 xyz 与 intensity | `PointXYZI::data_c` 颜色清零与 intensity 覆盖不可被打乱 | gtest same-chain、bench、asm、board | planned |
| `mismatch_stride_mapping` | 队列表风险项 | depth_width != image_width 的 organized stride 写点 | 能判断后续 production gate 是否必须限制同尺寸 | 分辨率 ratio 非整数时 production 当前用整数除法 step，需复刻而非修正 | boundary gtest 和 bench case | planned |

## 阶段范围

| item | value |
| --- | --- |
| validated_scope | 测试专用 helper；`PointXYZ`、`PointXYZRGBA`、`PointXYZI`；`Scalar=float`；连续 synthetic depth/RGB/IR buffer；同尺寸和整数比例 mismatch case。 |
| unvalidated_scope | 真实 `OpenNI2Grabber` 构造、设备 callback、resize buffer 调用真实 `DepthImage::fillDepthImageRaw` / `Image::fillRGB` / `IRImage::fillRaw`、legacy `openni_grabber.cpp`、ONI replay、非整数 ratio 行为、production dispatch。 |
| point_type_expansion_queue | `PointXYZRGB` 与 `PointXYZRGBA` union parity；后续若接 production，只能覆盖已用公开信号的具体点型，不能外推泛型点类型。 |
| phase_closeout_boundary | 本阶段最多关闭 diagnostic scaffold、same-chain correctness、首轮 bench/asm/board 诊断矩阵；不能关闭 production-ready。 |

## 实现和测试动作

| action | dependency | artifact / command | completion criterion |
| --- | --- | --- | --- |
| RED: 写最小 same-chain gtest | 本计划 | `test-rvv/io/openni2_grabber/src/test_openni2_grabber.cpp` | 首次运行因缺少 helper 或候选行为失败，失败原因与测试意图一致。 |
| GREEN: 增加 test support 标量 reference / candidate 聚合入口 | RED | `test-rvv/io/openni2_grabber/include/openni2_grabber.h` | `run_test_compare` 中 Std/RVV 两侧同一批 correctness 通过。 |
| 补 bench 入口 | correctness 通过 | `src/bench_openni2_grabber.cpp`、`Makefile` | QEMU 只运行 RVV bench smoke（日志形状），不做性能结论。 |
| 反汇编检查 | RVV bench 可构建 | `make dump_bench_rvv` + topic-local grep | 出现并能归因到本 topic helper 的 RVV 指令，或记录归因未闭合。 |
| 板卡单次与 repeated diagnostic | bench/asm 通过且板卡可用 | `make run_board_bench_compare` 或 topic alias；后续 summary/doctor | 板卡结果形成 positive/weak/neutral/negative/unstable 桶，Evidence Doctor Errors=0 或降级说明。 |

## Evidence Doctor 和 registry 规则

本阶段涉及 benchmark、board summary 和 EvidenceDecision，因此必须运行 Evidence Doctor（证据体检）或人工填写同等 Errors / Warnings / Suggestions。首阶段可先用 `summary_only_metadata_missing` warning 作为 reviewer aid；若要把 repeated board 写成 production 取舍依据，必须补 topic-local manifest wrapper 并登记 `log/evidence_registry.json`。

## 板卡复跑预算和决策桶

当前会话说明板卡可用。本阶段默认先做 1 次 board smoke，若同边界 Std/RVV 都可运行且 checksum 一致，再做 5-run repeated summary。若 median speedup >= 1.20 且没有反向 case，记为 `positive`；1.05 到 1.20 记为 `weak_positive`；0.95 到 1.05 记为 `neutral`；低于 0.95 记为 `negative`；若 5-run 中方向跨桶且一次追加同边界复跑后仍摇摆，记为 `unstable`。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic。 |
| A/B boundary | test helper；baseline 是 topic-local scalar reference，candidate 是 topic-local RVV candidate。 |
| 当前决策问题 | RVV-vs-scalar 可行性筛选，以及是否值得进入 bounded production probe。 |
| diagnostic 是否可外推到 production | unknown。它复刻生产公式和布局，但不经过真实 `OpenNI2Grabber` 对象、设备 resize buffer 和 signal callback。 |
| comparison-boundary / baseline mismatch 风险 | yes。生产入口还有设备 focal length、camera intrinsics、frame id、resize buffer 和 callback 状态，本阶段不会证明这些路径。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 可以，但仅在实现小、fallback 清楚、production direct test 能覆盖同尺寸路径时允许；弱/负诊断不能直接写 no-production。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前没有已采用 RVV family；若后续出现多个 RVV family，需要同边界 RVV-vs-RVV A/B。 |

## 文档更新清单

本阶段会创建 topic-local phase index、optimization roadmap、optimization matrix、evaluation、README 和必要测试说明。`doc-rvv/io/openni2_grabber-RVV.zh.md` 判为 `not_applicable`，直到 production patch 被用户确认保留或 PI5 生产证据闭环通过。

## 继续 / 停止条件

默认下一步是完成 RED/GREEN correctness，然后继续 bench、asm、board 和 Evidence Doctor。只有出现下列情况才停止：测试无法在当前工具链编译、板卡 SSH/rsync 不可达、证据矛盾需要人工判断、继续会修改 production 源码并需要进入 PI1 授权边界、或 dirty isolation 无法保证只触碰当前 topic。
