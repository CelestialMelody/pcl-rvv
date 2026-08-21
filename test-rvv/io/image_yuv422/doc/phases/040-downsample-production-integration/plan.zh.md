# Phase 040: downsample-production-integration Plan

## 阶段意图和边界

本阶段把 Phase 030 的 `downsample-rgb-stride-vsseg3-u8` 候选作为有界 production probe（生产探针）接入 `io/src/image_yuv422.cpp`。范围只扩大 `pcl::io::ImageYUV422::fillRGB` 的 RGB downsample 路径；full-size RGB 已采纳并保持不变，`fillGrayscale` 和 `io/src/openni_camera/openni_image_yuv_422.cpp` 不修改。

## PI1 范围冻结

| 维度 | 本阶段范围 |
| --- | --- |
| public entry | `ImageYUV422::fillRGB` |
| 新 RVV gate | `__RVV10__`、`wrapper_->getWidth () != width`、`wrapper_->getHeight () != height`、ratio 可整除且水平/垂直 ratio 为偶数 |
| fallback | 非 RVV 构建、full-size odd width、灰度、OpenNI legacy、非本 gate 的 resize 形态继续标量 |
| data layout | YUYV 输入，RGB24 输出，支持 `rgb_line_step` padding |
| evidence role | production public；若证据正向，PI5 仍需用户确认采纳 |

## 实现动作

1. 在 production 文件内新增 `storeRGBTripletContiguous` 和 `fillRGBDownsampleRVV`，复用 full-size 的 `u8ToI32`、`u8OffsetToI32`、`rgbChannel`。
2. 在 `fillRGB` public entry 中，full-size RVV gate 优先；downsample gate 次之；其它路径调用 `fillRGBStd`。
3. 更新 production direct test，使 RGB downsample 在 RVV build 下仍与标量 reference 对拍，并保留 padding 检查。
4. 增加 `prod_rgb_downsample_640x480_to_320x240` bench label 和 repeated board target。

## 证据计划

| gate | 命令 | 通过条件 |
| --- | --- | --- |
| correctness | `make -C test-rvv/io/image_yuv422 run_test_compare` | Std/RVV 各 6 个 gtest 通过 |
| QEMU smoke | `make -C test-rvv/io/image_yuv422 run_bench_rvv BENCH_ARGS="--case-filter prod_rgb_downsample_640x480_to_320x240 --iterations 1 --warmup-iterations 1"` | 可运行并输出 checksum；不作为性能结论 |
| asm | `make -C test-rvv/io/image_yuv422 dump_bench_rvv` | `ImageYUV422::fillRGB` 或 production helper 符号内可见 `vlse8.v` / `vsseg3e8.v` |
| board repeated | `make -C test-rvv/io/image_yuv422 run_board_yuv422_prod_rgb_downsample_repeated` | 5-run summary，保留 mean/median/min/max |
| Evidence Doctor | 同上 target 内生成 | Error=0；Warning 需解释 |

## 板卡预算和决策桶

复跑预算为 5-run。沿用 Phase 030 桶：

- `positive`：mean / median 均大于 `1.10x`，min 大于 `1.00x`，Doctor 无 Error。
- `weak-positive`：mean 大于 `1.03x` 但 min 接近或低于 `1.00x`。
- `neutral`：`0.98x` 到 `1.03x`。
- `negative`：低于 `0.98x`。
- `unstable`：方向摇摆或 Doctor Error 无法解释。

## Continue / Stop Decision

若 production direct positive，则停在 PI5 `pending_user_confirmation_adopt_downsample_production`，等待用户确认是否采纳扩大后的 production patch。若 neutral / negative，则停在 PI5 `pending_user_confirmation_rollback_downsample_probe`，等待用户确认是否回滚 downsample production 增量，full-size 已采纳补丁保持不动。
