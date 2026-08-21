# Phase 030: downsample-rvv-probe Plan

## 阶段意图和边界

本阶段只评估 `ImageYUV422::fillRGB` 的 RGB downsample（下采样）路径是否值得继续进入生产接入闭环。当前 production 已采纳 full-size even-width RGB RVV；本阶段不扩大 production patch，不修改 `io/src/image_yuv422.cpp`，只在 `test-rvv/io/image_yuv422` 的测试专用 candidate 中实现 downsample RVV probe（探针）。

验证范围：

| 维度 | 本阶段覆盖 | 不覆盖 |
| --- | --- | --- |
| entry | 测试专用 `fillRgbCandidate` 的 downsample 分支 | 真实 production dispatch |
| row source | `src_width / dst_width` 和 `src_height / dst_height` 为偶数的 stride sampling | 任意比例、奇数采样、非幂次比例 |
| layout | YUYV 输入、RGB24 连续或 padded 输出 | OpenNI legacy wrapper |
| size | 640x480 -> 320x240 代表规模 | 其它分辨率只作为后续扩展 |

## 当前状态清单

- `rgb-segment-store-u8` full-size production patch 已采纳，production public 5-run mean `2.0297x`，Evidence Doctor `Errors=0, Warnings=0, Suggestions=0`。
- 当前 `fillRgbCandidate` 在 downsample 条件下回到 `fillRgbScalar`，所以现有 `rgb_downsample_640x480_to_320x240` bench 不是 RVV 优化证据。
- `fillGrayscaleCandidate` 已明确保持标量；灰度全尺寸历史诊断不支持接入。

## 假设与候选族

候选族：`downsample-rgb-stride-vsseg3-u8`。

核心假设：downsample RGB 每个输出像素只读取一个 YUYV pair 的 `U/Y1/V`，可以用 `vlse8` 按 `yuv_x_step` 跨步读取样本，再用 `vsseg3e8` 连续写回 RGB triplet。它比 full-size 少一半或更多输出像素，但仍要支付跨步读和整数乘加成本；收益必须由板卡证明，不能从 full-size positive 外推。

## 优化矩阵

| candidate family | scope and entry | correctness | bench | asm | board | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| downsample-rgb-stride-vsseg3-u8 | test helper `fillRgbCandidate` RGB downsample 640x480 -> 320x240 | `run_test_compare` | `rgb_downsample_640x480_to_320x240` | `dump_bench_rvv` 内可见 downsample helper 的 RVV 指令 | 5-run board repeated | production-shaped diagnostic Doctor | pending |
| grayscale-downsample-rvv | grayscale downsample | scalar fallback only | not_applicable | not_applicable | not_applicable | not_applicable | rejected / no active candidate |

## 实现和测试动作

1. 在 `include/image_yuv422.h` 增加测试专用 `fillRgbDownsampleRVV`，只在 `__RVV10__` 且 downsample ratio 为偶数时命中。
2. 用 `vsseg3e8` 连续写 RGB triplet，保留 line step padding。
3. 保持 production direct downsample 测试仍证明真实 `ImageYUV422::fillRGB` 当前走标量 fallback。
4. 运行 `make -C test-rvv/io/image_yuv422 run_test_compare`。
5. 运行 QEMU bench smoke：`make -C test-rvv/io/image_yuv422 run_bench_rvv BENCH_ARGS="--case-filter rgb_downsample_640x480_to_320x240 --iterations 1 --warmup-iterations 1"`。
6. 运行 `make -C test-rvv/io/image_yuv422 dump_bench_rvv` 并检查 RVV 指令形态。
7. 若本地证据闭合，运行板卡 5-run repeated summary 和 Evidence Doctor；若板卡或工具失败，记录真实 blocker。

## Evidence Doctor 和 registry

本阶段使用 topic-local manifest 生成脚本：

- repeated summary：`test-rvv/io/image_yuv422/log/board/rgb_downsample_640x480_to_320x240_repeat_5/summary.md`
- manifest：`test-rvv/io/image_yuv422/log/board/evidence_manifest.json`
- doctor：`test-rvv/io/image_yuv422/log/board/evidence_doctor.md`

预期 `strict_ab=true`；若出现 checksum mismatch 或 boundary mismatch，先修复或降级为 blocked。

## 板卡复跑预算和决策桶

本阶段预算为 5-run repeated board。decision bucket：

- `positive`：mean / median 均大于 `1.10x`，min 大于 `1.00x`，Doctor 无 Error。
- `weak-positive`：mean 大于 `1.03x` 但 min 接近或低于 `1.00x`。
- `neutral`：`0.98x` 到 `1.03x`。
- `negative`：低于 `0.98x`。
- `unstable`：方向摇摆或 checksum / doctor 异常无法解释。

## 继续 / 停止条件

若 downsample probe positive 且 correctness / asm / Doctor 闭合，下一步是新的 production integration plan，不能直接扩大当前已采纳补丁。若 weak / neutral / negative，则保持 production scalar fallback，把候选标为 rejected 或 retained diagnostic，不继续接入。

OpenNI legacy parity 会触碰 `io/src/openni_camera/openni_image_yuv_422.cpp`，本阶段不做；只有当前通用 downsample 方向关闭后再判断。
