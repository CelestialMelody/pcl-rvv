# image_depth RVV

## 当前状态

`io/src/image_depth.cpp` 的 `pcl::io::DepthImage::fillDepthImage()` 和
`fillDisparityImage()` 已采纳缩窄后的 RVV production patch（生产补丁）。当前采用范围是：

- `fillDepthImage()` 的 contiguous depth（连续深度图）输出路径。
- `fillDisparityImage()` 的 contiguous disparity（连续视差图）输出路径。
- `fillDisparityImage()` 的 integer downsample disparity（整数倍下采样视差图）输出路径。

`fillDepthImage()` 的 downsample depth（下采样深度图）在接入后的 production-public
（真实公开入口）板卡证据中没有形成收益，当前保持标量 fallback（回退路径）。`fillDepthImageRaw()`
和 OpenNI legacy depth 文件没有接入本次 RVV。

本文件记录 adopted production behavior（已采用生产行为）。性能数据来自接入后的
`test-rvv/io/image_depth/log/board/repeated_production_public/summary.md`，不是早期
production-shaped diagnostic（生产形态诊断）数据。

## 函数语义

`DepthImage` 从 `FrameWrapper`（帧包装器，提供 depth buffer 和尺寸元数据）读取
`uint16_t` 深度像素。公开入口支持目标尺寸等于源尺寸，也支持整数倍 downsample（下采样）；
upsample（上采样）和非整数倍 downsample 会抛出异常。

`fillDepthImage()` 写出 `float` depth buffer：

- 像素为 `0`、`no_sample_value_` 或 `shadow_value_` 时输出 `NaN`。
- 其它像素按毫米到米转换，输出 `pixel * 0.001f`。
- `line_step == 0` 时输出行是 tight row（无行尾 padding）；非零 `line_step` 保留调用方给定的行尾 padding。

`fillDisparityImage()` 写出 `float` disparity buffer：

- invalid 像素输出 `0.0f`。
- 其它像素输出 `focal_length_ * baseline_ * 1000.0f / xStep / pixel`。
- `xStep` 是 `src_width / width`，因此 downsample disparity 的标量语义和 RVV 语义都要把常量除以 `xStep`。

## 标量路径

标量路径按行遍历目标输出。每个目标像素通过 `depthIdx` 从源 depth buffer 取样，`xStep`
控制列方向采样间隔，`ySkip` 控制行方向跳过源行。写完一行后，若 `line_step` 大于
`width * sizeof(float)`，标量路径会跳过输出 padding。

本次补丁把原循环抽成 `fillDepthImageStd()` 和 `fillDisparityImageStd()`。这些 helper
仍承担非 RVV 构建、非覆盖布局、depth downsample 和其它 fallback 情况的语义基线。

## 当前采用的优化方式

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 / 边界 |
| --- | --- | --- | --- |
| depth contiguous | adopted | 连续 `uint16_t` load、invalid mask（无效值掩码）、拓宽转换和乘法适合 RVV 分块。 | production-public 5-run：`prod_depth_full_640x480` median `1.42x`；padded median `1.20x`。 |
| disparity contiguous | adopted | 批量 `constant / pixel` 降低逐像素分支和除法循环开销。 | production-public 5-run：median `1.80x`，min `1.46x`，max `1.82x`；Evidence Doctor 有 variance warning。 |
| disparity downsample | adopted | `vlse16` 跨步加载能覆盖 `xStep > 1` 的固定步长取样，且接入后仍有收益。 | production-public 5-run：median `1.34x`，min `1.13x`，max `1.39x`；Evidence Doctor 有 variance warning。 |
| depth downsample | rejected / scalar fallback | 接入后的 public entry 结果为 context median `0.99x`，不足以承担生产 RVV 维护成本。 | manifest 标为 `production-fallback-coverage`，不作为 RVV speedup candidate（加速候选）。 |
| `fillDepthImageRaw()` | deferred | full-size tight row 常见入口已有 `memcpy` 快路径，缺少 profile（性能剖析）证明非 memcpy 分支是主成本。 | 不属于当前 adopted production behavior。 |
| OpenNI legacy depth | deferred | 另一个 production file，wrapper 和依赖边界不同。 | 通用 `image_depth.cpp` 采纳后可另开 parity phase。 |

RVV helper 只在 `__RVV10__` 构建中编译。`canUseFloatImageRVV()` gate（准入条件）要求输出
`line_step` 能按 `float` 对齐并容纳目标行；不满足时回到标量 helper。

### VL chunk 内部流程

contiguous path 每次用 `vle16` 读取一段 `uint16_t` pixels。downsample disparity path 用
`vlse16` 按 `xStep * sizeof(uint16_t)` 固定字节步长读取源像素。每个 VL chunk（可变向量长度分块）中：

1. `invalidDepthMask()` 批量比较 `0`、`no_sample_value_` 和 `shadow_value_`。
2. 有效像素经 `vwaddu` 拓宽为 `uint32_t`，再用 `vfcvt` 转成 `float`。
3. depth path 执行 `value * 0.001f`；disparity path 执行 `constant / value`。
4. 输出先写默认 invalid 值：depth 写 `NaN`，disparity 写 `0.0f`。
5. 再用 masked store（带掩码写回）覆盖有效 lane（向量通道）的结果。

这样保留了标量路径“invalid 分支先决定输出，再对有效像素套公式”的语义。

## Fallback 矩阵

| 条件 | 行为 | 语义保持证据 |
| --- | --- | --- |
| 非 `__RVV10__` 构建 | 只编译 / 执行 Std helper。 | `make run_test_compare` 的 Std build 通过。 |
| `line_step` 不能按 `float` 对齐或不足以容纳一行 | 回到 Std helper。 | `canUseFloatImageRVV()` gate。 |
| `fillDepthImage()` full-size contiguous | RVV build 走 `fillDepthImageContiguousRVV()`。 | production path hit test、asm、board positive。 |
| `fillDepthImage()` downsample | 回到 `fillDepthImageStd()`。 | production fallback test；board context median `0.99x`。 |
| `fillDisparityImage()` full-size contiguous | RVV build 走 `fillDisparityImageContiguousRVV()`。 | production path hit test、asm、board positive。 |
| `fillDisparityImage()` integer downsample 且 `xStep > 1` | RVV build 走 `fillDisparityImageDownsampleRVV()`。 | production path hit test、asm、board positive。 |
| upsample 或非整数倍 downsample | 保持原异常行为。 | correctness tests 覆盖异常边界。 |
| `fillDepthImageRaw()` | 保持原实现。 | 本次未修改。 |
| OpenNI legacy depth file | 保持原实现。 | 本次未修改。 |

## 范围决策表

| 范围 | 状态 | 当前证据 | 不能外推到哪里 |
| --- | --- | --- | --- |
| `fillDepthImage()` full-size tight row | adopted | `prod_depth_full_640x480` median `1.42x` | depth downsample、raw path、OpenNI legacy。 |
| `fillDepthImage()` full-size padded row | adopted | `prod_depth_full_padded_640x480` median `1.20x` | 任意 padding 形态；只证明当前 aligned float output。 |
| `fillDepthImage()` downsample | scalar-only / rejected RVV | fallback context median `0.99x` | 不能写成 RVV 收益；新实现需另开 phase。 |
| `fillDisparityImage()` full-size | adopted | `prod_disparity_full_640x480` median `1.80x` | 其它尺寸和真实相机分布；当前有 variance warning。 |
| `fillDisparityImage()` downsample | adopted | `prod_disparity_downsample_640x480_to_320x240` median `1.34x` | 非整数 downsample；其它比例需另测。 |
| `fillDepthImageRaw()` | deferred | source audit only | 当前没有 production RVV 结论。 |
| OpenNI legacy depth / disparity | deferred | source audit only | 需要另开 legacy parity 验证。 |

## 标量路径与 RVV 路径差异

| 阶段 | 标量路径 | RVV 路径 | 保留边界 |
| --- | --- | --- | --- |
| 输入检查 | public entry 检查尺寸和 `line_step`。 | 沿用同一 public entry 检查，再加 `canUseFloatImageRVV()`。 | upsample / 非整数 downsample 异常不变。 |
| 取样 | 逐像素 `inputBuffer[depthIdx]`。 | contiguous 用 `vle16`；disparity downsample 用 `vlse16`。 | depth downsample 不使用 RVV。 |
| invalid 判断 | 标量分支比较 `0` / no-sample / shadow。 | `invalidDepthMask()` 生成向量 mask。 | sentinel 超出 `uint16_t` 时不额外匹配，等价于无像素可等于该值。 |
| 公式 | depth 乘 `0.001f`；disparity 执行 `constant / pixel`。 | depth 用 `vfmul`；disparity 用 `vfrdiv`。 | `constant` 对 downsample disparity 仍除以 `xStep`。 |
| 输出 | invalid 和 valid 在同一标量分支写回。 | 先写 invalid 默认值，再 masked store 有效结果。 | 行尾 padding 不写。 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `DepthImage::fillDepthImage` | production public entry | depth mm 到 meter float 输出。 | depth image 使用者。 | Std 或 RVV helper。 | production boundary | `io/src/image_depth.cpp` |
| `DepthImage::fillDisparityImage` | production public entry | depth mm 到 disparity float 输出。 | disparity image 使用者。 | Std 或 RVV helper。 | production boundary | `io/src/image_depth.cpp` |
| `fillDepthImageStd` | production Std helper | depth 标量 fallback。 | `fillDepthImage()` | output buffer。 | fallback baseline | `io/src/image_depth.cpp` |
| `fillDisparityImageStd` | production Std helper | disparity 标量 fallback。 | `fillDisparityImage()` | output buffer。 | fallback baseline | `io/src/image_depth.cpp` |
| `fillDepthImageContiguousRVV` | production RVV helper | contiguous depth RVV 转换。 | `fillDepthImage()` | output buffer。 | adopted RVV path | `io/src/image_depth.cpp` |
| `fillDisparityImageContiguousRVV` | production RVV helper | contiguous disparity RVV 转换。 | `fillDisparityImage()` | output buffer。 | adopted RVV path | `io/src/image_depth.cpp` |
| `fillDisparityImageDownsampleRVV` | production RVV helper | downsample disparity RVV 转换。 | `fillDisparityImage()` | output buffer。 | adopted RVV path | `io/src/image_depth.cpp` |
| `test_image_depth.cpp` | correctness gate | Std/RVV semantic checks、production path hit 和 fallback。 | `make run_test_compare` | gtest output。 | correctness / fallback evidence | `test-rvv/io/image_depth/src/test_image_depth.cpp` |
| `bench_image_depth.cpp` | bench wrapper | production-shaped 和 production-public timing。 | QEMU / board runner | summary / manifest。 | performance wrapper | `test-rvv/io/image_depth/src/bench_image_depth.cpp` |
| manifest generator | analysis script | 生成 Evidence Doctor manifest。 | Make targets | JSON manifest。 | doctor input | `test-rvv/io/image_depth/script/generate_image_depth_evidence_manifest.py` |
| repeated summary generator | analysis script | 从 repeated compare logs 生成 summary。 | Make targets | summary Markdown。 | summary producer | `test-rvv/io/image_depth/script/generate_image_depth_repeated_summary.py` |
| production-public repeated summary | evidence output summary | 5-run production-public 板卡结果。 | board logs | Evidence Doctor、本文。 | board performance | `test-rvv/io/image_depth/log/board/repeated_production_public/summary.md` |
| production-public Evidence Doctor | evidence validation | 检查 production-public summary 的异常信号。 | manifest | phase result、本文。 | evidence validation | `test-rvv/io/image_depth/log/board/repeated_production_public/evidence_doctor.md` |
| topic evaluation | documentation section | 候选取舍、证据链和未覆盖范围。 | worker / reviewer | 本文、phase docs。 | decision audit | `test-rvv/io/image_depth/doc/image_depth-evaluation.zh.md` |
| production phase result | documentation section | PI2-PI5 执行事实和用户采纳边界。 | production integration loop | 本文、Handoff。 | recovery pointer | `test-rvv/io/image_depth/doc/phases/050-production-public-probe/result.zh.md` |

## VL chunk 算例

以 `no_sample_value=65535`、`shadow_value=65534`、`baseline=0.075`、`focal_length=525`
为例，full-size disparity 的 `constant = 525 * 0.075 * 1000 = 39375`。

| lane | pixel | invalid? | depth output | disparity output |
| --- | ---: | --- | ---: | ---: |
| 0 | 1000 | false | `1.000f` | `39.375f` |
| 1 | 0 | true | `NaN` | `0.0f` |
| 2 | 2000 | false | `2.000f` | `19.6875f` |
| 3 | 65535 | true | `NaN` | `0.0f` |

对于 `640x480 -> 320x240` 的 disparity downsample，`xStep=2`，所以
`constant = 39375 / 2 = 19687.5`。RVV downsample helper 使用 `vlse16` 读取源列
`0, 2, 4, ...`，输出仍写入连续目标 buffer。

## Bench 与证据

production-public benchmark（性能测试）使用真实 `DepthImage` public entry。Std/RVV 两侧链接同一份
`io/src/image_depth.cpp`，区别来自 `__RVV10__` 是否启用。speedup 口径是 `Std time / RVV time`，
大于 1 表示 RVV 更快。

| case | runs | median | min | max | 结论 |
| --- | ---: | ---: | ---: | ---: | --- |
| `prod_depth_full_640x480` | 5 | `1.42x` | `1.31x` | `1.43x` | adopted |
| `prod_depth_full_padded_640x480` | 5 | `1.20x` | `1.14x` | `1.25x` | adopted |
| `prod_depth_downsample_640x480_to_320x240` | 5 | `0.99x` | `0.98x` | `1.08x` | fallback coverage；不采纳 RVV |
| `prod_disparity_full_640x480` | 5 | `1.80x` | `1.46x` | `1.82x` | adopted with variance warning |
| `prod_disparity_downsample_640x480_to_320x240` | 5 | `1.34x` | `1.13x` | `1.39x` | adopted with variance warning |

Evidence Doctor（证据体检）结果为 `Errors=0, Warnings=2, Suggestions=0`。两个 Warning
分别来自 `prod_disparity_full_640x480` 和
`prod_disparity_downsample_640x480_to_320x240` 的 long-tail / variance（长尾 / 波动）。
当前处理策略是保留 min/median/max，不剔除异常；如果后续需要更高稳定性信心，可扩大这两个 case
的 runs，但当前 min 值仍支持 positive bucket（正向决策桶）。

QEMU smoke（仿真小型验证）只证明 bench label 可运行和日志形状，不作为性能结论。

## 正确性与高效性证据链

| evidence area | 当前证据 | 结论边界 |
| --- | --- | --- |
| correctness | `make -C test-rvv/io/image_depth run_test_compare`：Std/RVV 各 9 个 TEST 通过。 | 证明语义、path hit 和 fallback，不证明性能。 |
| board correctness | `make -C test-rvv/io/image_depth run_board_test fetch_board_logs`：板卡 RVV test 9 个 TEST 通过。 | 目标硬件 correctness smoke。 |
| QEMU smoke | `run_bench_rvv --case-filter prod_depth_full_640x480 --iterations 1 --warmup-iterations 0` 可运行。 | 只证明 production-public bench label 可运行。 |
| asm attribution | `make -C test-rvv/io/image_depth dump_bench_rvv` 可见 production-linked `vle16`、`vlse16`、`vfcvt`、`vfmul`、`vfrdiv`、masked `vse32`。 | 证明 RVV 指令归属到 production helper / public entry 内联边界，不单独证明收益。 |
| board performance | production-public 5-run 支持 3 条 adopted RVV path。 | 只覆盖当前 case、Milkv-Jupiter 和 synthetic wrapper-backed input。 |
| Evidence Doctor | production-public `Errors=0, Warnings=2, Suggestions=0`。 | Warning 已解释；不阻塞 adoption，但保留稳定性风险。 |
| evidence registry | `make -C test-rvv/io/image_depth evidence_status`：registry fresh。 | 证明当前 summary / manifest / doctor 与引用文档一致。 |
| fallback | depth downsample、raw path、OpenNI legacy、非 RVV 构建和非覆盖 layout 保持标量。 | 不覆盖未来替代 RVV 设计。 |

## Production closeout

| 项 | 最终状态 | 证据 |
| --- | --- | --- |
| production file | 修改 `io/src/image_depth.cpp`，新增 Std helper、RVV helper、dispatch / fallback。 | source diff |
| public API | 不改变 public API、参数或返回类型。 | header shape unchanged |
| compile gate | `__RVV10__` 下编译 RVV helper；非 RVV 构建自然走 Std helper。 | Std/RVV correctness |
| adopted RVV paths | depth contiguous、disparity contiguous、disparity downsample。 | production-public board summary |
| scalar-only paths | depth downsample、raw path、OpenNI legacy、非覆盖 layout。 | fallback tests / source audit |
| evidence basis | 接入后的 production-public board 数据。 | `test-rvv/io/image_depth/log/board/repeated_production_public/summary.md` |
| rollback boundary | 可通过移除 `__RVV10__` helper / dispatch 回到 Std helper。 | production file single-topic diff |

## 后续方向

当前 topic 不建议继续自动优化 depth downsample RVV。已接入后的 public entry 数据没有收益；如果未来要重开，
需要新 candidate family（候选实现族）和同边界 correctness、asm、board repeated、Evidence Doctor。

`fillDepthImageRaw()` 只有在 profile 指向非 memcpy conversion loop 时才值得恢复。OpenNI legacy depth
文件涉及另一个 production file，建议作为后续 parity topic 独立处理。disparity 两个 adopted case
若需要更强稳定性，可以扩大到 20-run；这是信心增强动作，不是当前采纳的前置条件。
