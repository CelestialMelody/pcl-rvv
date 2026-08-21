# openni2_grabber 函数级评估

## 范围和目标源码

目标源码是 `io/src/openni2_grabber.cpp`。本 topic 评估 OpenNI2 grabber 在公开 signal 前构造点云的三个转换入口：

| production entry | 输出点型 | 热点循环 |
| --- | --- | --- |
| `OpenNI2Grabber::convertToXYZPointCloud` | `pcl::PointXYZ` | depth map 到 xyz 的逐像素反投影。 |
| `OpenNI2Grabber::convertToXYZRGBPointCloud<PointT>` | `PointXYZRGB` / `PointXYZRGBA` | depth 到 xyz，再 RGB buffer 覆盖 `rgba`。 |
| `OpenNI2Grabber::convertToXYZIPointCloud` | `pcl::PointXYZI` | depth 到 xyz，再 IR buffer 写 intensity。 |

## 当前函数级结论

当前结论是 `adopted production behavior / production-detail positive`。Phase 020 已把 `PointXYZ` 同尺寸 depth projection（深度反投影）接入 `io/src/openni2_grabber.cpp` 的 production detail helper（生产内部 helper），接入后板卡重测 `prod_xyz_depth_full_640x480` 为 5-run median 1.19x。Phase 030 已补 `PointXYZRGB` / `PointXYZRGBA` 的泛型点型诊断 correctness，但 RGB overlay、depth/image mismatch 和 IR 入口仍为 neutral 或 unstable，不建议接入 production patch（生产补丁）。

## 标量路径重建

`convertToXYZPointCloud` 先创建 `PointCloud<PointXYZ>`，填 header、height、width、`is_dense=false`，再从 `device_` 和 `depth_parameters_` 生成 focal length reciprocal（焦距倒数）和 principal point（主点）。如果 `DepthImage` 尺寸与当前 depth mode 不一致，production 先通过 `fillDepthImageRaw` 写入 `depth_resize_buffer_`，再按 `depth_width_ * depth_height_` 顺序遍历。

标量主循环对每个 depth pixel 检查 `0`、`getNoSampleValue()` 和 `getShadowValue()`。无效点写 quiet NaN；有效点按 `z = pixel * 0.001f`、`x = (u - centerX) * z * constant_x`、`y = (v - centerY) * z * constant_y` 写入 AoS（结构数组）点云。

RGB 路径额外处理 depth/image 尺寸不一致：cloud 尺寸取 depth 和 image 的最大值，mismatch 时先把整张 cloud 初始化为 NaN + black + alpha 255，再分别用 `cloud->width / depth_width_` 与 `cloud->width / image_width_` 的整数 step 布点。IR 路径在写 xyz 后清零 `data_c[0..3]`，再写 `intensity`。

## RVV 诊断设计

Phase 000 在 `test-rvv/io/openni2_grabber/include/openni2_grabber.h` 中建立 test-only helper：

| helper family | 作用 | 当前判断 |
| --- | --- | --- |
| `fillXYZCloud*` | 复刻 `PointXYZ` depth projection | RVV candidate positive，进入 PI1。 |
| `fillXYZRGBAFromDepth*` / `fillXYZRGBFromDepth*` | 复刻 RGB/RGBA 路径中的 xyz 写入和 mismatch step | `PointXYZRGB` / `PointXYZRGBA` correctness 通过，board neutral / unstable，暂缓。 |
| `fillRGBOverlay*` | 复刻 RGB pack 和 alpha 语义 | `PointXYZRGB` / `PointXYZRGBA` correctness 通过，board neutral，暂缓。 |
| `fillXYZICloud*` | 复刻 IR + depth 语义 | 当前 candidate 为 scalar，board neutral / unstable，暂缓。 |

## 诊断证据链

| 证据 | 当前结果 | 能证明什么 | 不能证明什么 |
| --- | --- | --- | --- |
| correctness | `make run_test_compare` 通过，Std/RVV 各 8 个 gtest | test helper 与标量 reference 同构，关键语义受保护；Phase 030 覆盖 `PointXYZRGB` / `PointXYZRGBA` 模板诊断点型。 | 没证明真实 `OpenNI2Grabber` public entry 命中 RVV，也不证明 RGB/RGBA 有 production 收益。 |
| QEMU smoke | RVV bench 小迭代可运行 | RVV build 和日志形状可用。 | QEMU timing 不能写性能结论。 |
| asm | `vle16`、`vfcvt`、`vfmul`、`vmseq`、`vsse32`、`vlseg3e8` 可见 | RVV candidate 产生预期指令。 | 不等价于 production symbol attribution。 |
| board repeated | `xyz_depth_full_640x480` median 1.21x | `PointXYZ` depth diagnostic 有稳定收益。 | 不证明 RGB、IR、mismatch 或真实 callback 入口。 |
| Evidence Doctor | manifest-based：Errors=2，Warnings=1，Suggestions=2 | 暴露 IR / mismatch 退化频率、RGB 近阈值和方向摇摆。 | 不是 production direct Evidence Doctor pass。 |

## Phase 020 生产内部边界证据链

| 证据 | 当前结果 | 能证明什么 | 不能证明什么 |
| --- | --- | --- | --- |
| production-detail correctness | `make run_test_compare` 通过，Std/RVV 各 8 个 gtest | `openni2_grabber.cpp` 内部 helper 与标量链路 bitwise 对齐，RVV build 命中 RVV path；RGB/RGBA 模板诊断点型也对齐。 | 不能证明真实 OpenNI2 设备对象 public entry 已运行；不能把 RGB/RGBA 诊断覆盖写成 production 接入。 |
| QEMU smoke | `prod_xyz_depth_full_640x480` 小迭代可运行 | 构建、日志形状和 production hook 可用。 | QEMU timing 不能作为性能结论。 |
| asm | `check_openni2_grabber_production_rvv_asm` 通过 | RVV 指令归属到包含 production detail helper 的 bench binary。 | 不能替代板卡性能。 |
| board repeated | `test-rvv/io/openni2_grabber/log/board/repeated_production_depth/summary.md`：median 1.19x，min 1.15x，max 1.22x | 接入后的 production-detail 边界仍有稳定收益。 | 当前交叉环境无 OpenNI2 头/库，不能证明 production-public。 |
| Evidence Doctor | `test-rvv/io/openni2_grabber/log/board/repeated_production_depth/evidence_doctor.md`：Errors=0，Warnings=0，Suggestions=0；manifest 为 `test-rvv/io/openni2_grabber/log/board/repeated_production_depth/evidence_manifest.json` | 新生产内部边界证据无 Doctor 异常。 | 仍未覆盖真实 OpenNI2 设备对象 public entry。 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 证据角色 |
| --- | --- | --- | --- |
| `OpenNI2Grabber::convertToXYZPointCloud` | production public-adjacent entry | depth frame 到 `PointXYZ` cloud | Phase 020 已接入 helper dispatch，用户已确认采纳 |
| `fillXYZPointCloudStd` / `fillXYZPointCloudRVV` / `fillXYZPointCloudCandidate` | production detail helper | `PointXYZ` depth projection 标量/RVV 分流 | production-detail correctness / bench / asm |
| `OpenNI2Grabber::convertToXYZRGBPointCloud` | production public-adjacent template entry | depth + RGB 到 RGB/RGBA cloud | Phase 030 已补 `PointXYZRGB` / `PointXYZRGBA` 诊断 correctness，production 仍暂缓 |
| `OpenNI2Grabber::convertToXYZIPointCloud` | production public-adjacent entry | depth + IR 到 `PointXYZI` cloud | Phase 000 诊断，暂缓 |
| `include/openni2_grabber.h` | test support | scalar reference 和 RVV candidate | correctness / bench diagnostic |
| `src/test_openni2_grabber.cpp` | test | same-chain gtest 和 bitwise 对拍 | correctness |
| `src/bench_openni2_grabber.cpp` | bench | synthetic frame benchmark | QEMU smoke / board performance |
| `test-rvv/io/openni2_grabber/log/board/repeated_diagnostic/summary.md` | evidence output | 5-run repeated summary | diagnostic board evidence |
| `test-rvv/io/openni2_grabber/log/board/repeated_diagnostic/evidence_manifest.json` | evidence manifest | 记录 boundary、row source、checksum、run contract 和 repeated values | Evidence Doctor input |
| `test-rvv/io/openni2_grabber/log/board/repeated_diagnostic/evidence_doctor.md` | evidence validation | manifest-based Evidence Doctor | reviewer aid |

## Production 接入判断

当前 production patch 只覆盖 `convertToXYZPointCloud` 中同尺寸或 resize 后连续 depth buffer 的 `PointXYZ` RVV helper。补丁保留 `__RVV10__` 关闭时的标量路径，保留 resize buffer 语义，没有扩大到 RGB/RGBA、IR、legacy `openni_grabber.cpp` 或 public API 变化。接入后板卡证据为 positive，用户已确认有收益即可采纳，因此当前记录为 adopted production behavior。

## 文档归属

本 evaluation 保存诊断结论、Traceability Map、production-detail evidence、PI5 后采纳状态和 Phase 030 泛型诊断边界。长期 `doc-rvv/io/openni2_grabber-RVV.zh.md` 已按接入后的板卡数据创建为 adopted production 文档；当前已按用户确认采纳。
