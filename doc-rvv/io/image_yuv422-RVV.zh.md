# ImageYUV422 YUV422 to RGB RVV

## 当前状态

`io/src/image_yuv422.cpp` 的 `pcl::io::ImageYUV422::fillRGB` 已采纳两条 RVV production
patch（生产补丁）：full-size even-width YUYV 到 RGB24 输出路径，以及 RGB downsample 偶数
ratio gate。production public（真实公开入口）板卡 5-run strict A/B 支持接入：full-size
mean `2.0297x`，full-size padded mean `2.1030x`，RGB downsample mean `3.8150x`。Evidence Doctor
（证据体检）分别为 `Errors=0, Warnings=0, Suggestions=0`、`Errors=0, Warnings=0, Suggestions=0`
和 `Errors=0, Warnings=1, Suggestions=0`。灰度、其它 resize ratio 和 OpenNI legacy 入口不覆盖。

## 函数语义

`ImageYUV422::fillRGB` 从 `FrameWrapper` 读取 YUV422 / YUYV 字节。每 4 个输入字节按
`U Y1 V Y2` 排列，两个相邻像素共享 `U` 和 `V`。标量路径对每个像素执行整数公式：

- `R = clip(Y + (((V - 128) * 18678 + 8192) >> 14))`
- `G = clip(Y + (((V - 128) * -9519 - (U - 128) * 6472 + 8192) >> 14))`
- `B = clip(Y + (((U - 128) * 33292 + 8192) >> 14))`

输出是 RGBRGB 交错布局。`rgb_line_step == 0` 时按 `width * 3` 连续输出；非零时保留行尾 padding。

## 当前采用的优化方式

| 维度 | 当前状态 | 证据 / 边界 |
| --- | --- | --- |
| dispatch（分流逻辑） | `__RVV10__` 构建下，full-size even-width RGB 走 `fillRGBFullSizeRVV`；RGB downsample 水平/垂直 ratio 均为偶数时走 `fillRGBDownsampleRVV`。 | `io/src/image_yuv422.cpp` public entry。 |
| full-size RVV 数据流 | 每个 vector lane（向量通道）处理一个 YUYV pair，`vlse8` 分别跨步读取 U/Y1/V/Y2。 | 反汇编在 `ImageYUV422::fillRGB` 符号内可见 `vlse8.v`。 |
| full-size RGB 写回 | 对 Y1 和 Y2 分别构造 R/G/B 向量，用 `vssseg3e8` 写回 RGB triplet（RGB 三元组）。 | 反汇编可见 `vssseg3e8.v`。 |
| downsample RVV 数据流 | 每个 vector lane 处理一个采样输出像素，用 `vlse8` 按 `yuv_x_step` 跨步读取 U/Y/V，再用 `vsseg3e8` 连续写回 RGB triplet。 | production direct gtest、asm 和 5-run board 覆盖 640x480 -> 320x240。 |
| 标量保留路径 | 奇数宽度、非 RVV 构建、未覆盖 resize 形态和灰度路径保持标量。 | production direct gtest 覆盖 full-size、downsample 和 grayscale fallback。 |
| 暂缓范围 | OpenNI legacy YUV422、灰度 RVV、RGB downsample 其它 ratio / resize 形态。 | legacy / 灰度 / 其它 ratio 需要另开 phase 或补同边界证据。 |

## Fallback 矩阵

| 条件 | 行为 |
| --- | --- |
| 非 `__RVV10__` 构建 | 只编译和执行标量 helper。 |
| full-size even-width RGB | RVV build 走 `fillRGBFullSizeRVV`。 |
| RGB downsample，水平/垂直 ratio 均为偶数 | RVV build 走 `fillRGBDownsampleRVV`。 |
| RGB downsample，其它 ratio / resize 形态 | 走 `fillRGBStd` 的原 downsample 逻辑。 |
| `fillGrayscale` | 未接 RVV，保持原标量实现。 |
| OpenNI legacy `openni_image_yuv_422.cpp` | 未修改。 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 证据角色 |
| --- | --- | --- | --- |
| `io/src/image_yuv422.cpp` | production | `ImageYUV422::fillRGB` 的 Std/RVV 分流和 helper。 | production boundary |
| `test-rvv/io/image_yuv422/src/test_image_yuv422.cpp` | test | diagnostic 和 production direct correctness。 | correctness / fallback |
| `test-rvv/io/image_yuv422/src/bench_image_yuv422.cpp` | bench | `prod_rgb_full_640x480` production public case。 | performance wrapper |
| `test-rvv/io/image_yuv422/script/generate_image_yuv422_evidence_manifest.py` | script | 生成 Evidence Doctor manifest，支持 5-run 目录。 | evidence manifest |
| `test-rvv/io/image_yuv422/doc/phases/020-production-integration-plan/result.zh.md` | phase result | PI1-PI5 生产接入证据和用户确认边界。 | recovery pointer |
| `test-rvv/io/image_yuv422/doc/phases/040-downsample-production-integration/result.zh.md` | phase result | RGB downsample 生产补丁证据和采纳边界。 | recovery pointer |
| `test-rvv/io/image_yuv422/log/board/prod_rgb_full_640x480_repeat_5/summary.md` | summary artifact | 生产 public 5-run 板卡摘要。 | board performance |
| `test-rvv/io/image_yuv422/log/board/prod_rgb_full_640x480_repeat_5/evidence_doctor.md` | summary artifact | full-size production repeated Evidence Doctor 检查结果。 | evidence validation |
| `test-rvv/io/image_yuv422/log/board/prod_rgb_full_padded_640x480_repeat_5/evidence_doctor.md` | summary artifact | full-size padded production repeated Evidence Doctor 检查结果。 | evidence validation |
| `test-rvv/io/image_yuv422/log/board/prod_rgb_downsample_640x480_to_320x240_repeat_5/evidence_doctor.md` | summary artifact | downsample production repeated Evidence Doctor 检查结果。 | evidence validation |

## 正确性与高效性证据链

| 证据 | 当前结果 | 边界 |
| --- | --- | --- |
| QEMU correctness | `make -C test-rvv/io/image_yuv422 run_test_compare`：Std/RVV 各 6 个 gtest 通过。 | 证明功能和 fallback，不证明性能。 |
| board correctness | `make -C test-rvv/io/image_yuv422 run_board_test fetch_board_logs`：RVV test 6 个 gtest 通过。 | 目标硬件 correctness smoke。 |
| QEMU bench smoke | `prod_rgb_full_640x480` 和 `prod_rgb_downsample_640x480_to_320x240` 各 1 次 warmup / 1 次 iteration 可运行。 | 只证明日志形状。 |
| asm attribution | `dump_bench_rvv` 中 `ImageYUV422::fillRGB` 符号内可见 `vlse8.v`、`vmul.vx`、`vsra.vi`、`vssseg3e8.v`。 | 证明生产符号命中 RVV 指令。 |
| full-size board performance | `prod_rgb_full_640x480` production public 5-run：mean `2.0297x`、median `2.0247x`、min `2.0102x`、max `2.0576x`；mean Std `3.2082 ms`，mean RVV `1.5807 ms`。 | 只覆盖 640x480 full-size RGB。 |
| full-size padded board performance | `prod_rgb_full_padded_640x480` production public 5-run：mean `2.1030x`、median `2.1012x`、min `2.0889x`、max `2.1169x`；mean Std `3.2876 ms`，mean RVV `1.5633 ms`。 | 覆盖 640x480 full-size RGB 带行尾 padding。 |
| downsample board performance | `prod_rgb_downsample_640x480_to_320x240` production public 5-run：mean `3.8150x`、median `3.9294x`、min `3.3750x`、max `3.9692x`；mean Std `2.2022 ms`，mean RVV `0.5794 ms`。 | 只覆盖 640x480 -> 320x240 RGB downsample 偶数 ratio gate。 |
| Evidence Doctor | full-size 和 full-size padded 为 `production_public`、`strict_ab=true`、`Errors=0, Warnings=0, Suggestions=0`；downsample 为 `production_public`、`strict_ab=true`、`Errors=0, Warnings=1, Suggestions=0`。 | 支持 adopted production behavior；downsample warning 为 long-tail / variance，文档保留 min/median/max。 |

## 生产接入评估

当前已采纳 `fillRGB` full-size even-width RVV path 和 RGB downsample 偶数 ratio gate：实现范围小，
fallback 明确，production public correctness 和板卡性能都成立。未覆盖范围仍保持独立边界：

- 灰度、其它 resize ratio 和 OpenNI legacy 当前不因本补丁获得 RVV 结论。
- 若继续扩展 OpenNI legacy，必须作为独立 phase 补 correctness、asm、board 和 Evidence Doctor。
- 当前文件内暂不建议继续扩大生产补丁；进一步 ILP / LMUL tuning 需要新的 profile 证明 `fillRGB`
  仍是值得投入的热点。
