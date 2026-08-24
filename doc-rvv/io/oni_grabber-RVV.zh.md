# oni_grabber RVV 已采纳生产说明

## 当前状态

`io/src/oni_grabber.cpp` 已接入 depth-only `PointXYZ` RVV production patch（生产补丁），当前状态是 `adopted production behavior / production-detail positive`。补丁已经通过接入后板卡重测，且用户已确认“接入后板卡测试有收益即可采纳”；当前写成 adopted production behavior（已采纳生产行为）。本轮不自动提交。

本文件记录已采纳生产行为和接入后证据链。Phase 000 的 diagnostic（诊断）数据不作为本文件的性能数字来源。

## 覆盖范围

| item | current boundary |
| --- | --- |
| production entry | `pcl::ONIGrabber::convertToXYZPointCloud(const openni_wrapper::DepthImage::Ptr&)` |
| upstream callback | `pcl::ONIGrabber::depthCallback` 在 `point_cloud_signal_` 有 listener 时调用 depth-only 点云转换 |
| helper family | `fillXYZPointCloudStd` / `fillXYZPointCloudRVV` / `fillXYZPointCloudCandidate` |
| point type | 具体 `pcl::PointXYZ` |
| scalar type | `float` |
| input layout | 连续 `std::uint16_t` depth map；尺寸不一致时沿用原 `fillDepthImageRaw` resize buffer |
| output layout | AoS（结构数组）`pcl::PointXYZ`，stride 为 `sizeof(PointXYZ)` |
| invalid values | `0`、`getNoSampleValue()`、`getShadowValue()` 写 quiet NaN |
| RVV gate | `__RVV10__` 构建且 `width == center_x * 2`、`height == center_y * 2` |
| fallback | 非 `__RVV10__` 构建或奇数尺寸语义边界走 Std helper |

不覆盖 RGB/RGBA、IR intensity、真实 ONI 文件读取、replay reader 调度、完整 callback/signal public-entry throughput（公开入口吞吐）、public API 变化或泛型点型扩展。

## 实现方式

生产入口仍负责创建 cloud、设置 width / height / frame id、处理 depth resize buffer、计算焦距常量和主点。逐像素 depth projection（深度反投影）被抽到内部 helper：

- Std helper 保留原标量语义和公式顺序。
- RVV helper 使用 VL chunk（可变向量长度分块）连续加载 depth pixel，生成 invalid mask（无效掩码），把 millimeter depth 转成 meter，再按 AoS stride 写回 `x/y/z`。
- 无效点先写 quiet NaN；有效 lane（向量通道）再 masked store（带掩码写入）真实 `x/y/z`。
- Candidate helper 只在 RVV 宏和偶数尺寸 gate 同时满足时进入 RVV；其它情况回退 Std，保留原 `centerX = width >> 1`、`centerY = height >> 1` 的循环语义。

公式保持与原标量路径一致：

```text
z = pixel * 0.001f
x = u * z * constant
y = v * z * constant
```

其中 `u` 和 `v` 分别来自 `[-centerX, centerX)` 与 `[-centerY, centerY)` 的 organized traversal（有组织网格遍历）。

## 接入后证据链

| evidence | command / path | result |
| --- | --- | --- |
| correctness（正确性） | `make run_test_compare` in `test-rvv/io/oni_grabber` | Std/RVV 各 5 个 gtest 通过，包含 production-detail hook bitwise 对拍和奇数尺寸 fallback-to-Scalar 测试。 |
| QEMU smoke（QEMU 冒烟） | `make run_bench_rvv BENCH_ARGS="--case-filter prod_xyz_depth_full_640x480 --iterations 2 --warmup-iterations 1"` | 构建、运行和日志形状通过；不用于性能结论。 |
| asm attribution（反汇编归属） | `make check_oni_grabber_production_rvv_asm` | 看到 hook 符号以及 `vle16.v`、`vfcvt.f.xu.v`、`vfmul`、`vmseq`、`vsse32.v`。 |
| board smoke（板卡冒烟） | `make run_board_oni_grabber_production_smoke` | 单次 `1.21x`，checksum 一致。 |
| board repeated（重复板卡测试） | `test-rvv/io/oni_grabber/log/board/repeated_production_depth/summary.md` | 5-run median `1.18x`，min `1.16x`，max `1.22x`。 |
| Evidence Doctor（证据体检） | `test-rvv/io/oni_grabber/log/board/repeated_production_depth/evidence_doctor.md` | Errors=0，Warnings=3，Suggestions=2。 |

板卡 repeated values：`1.22x, 1.16x, 1.19x, 1.18x, 1.17x`，checksum matched。

## 证据边界

当前板卡性能是 production-detail evidence（生产内部边界证据）：bench 通过 test hook 直接调用 `oni_grabber.cpp` 中的生产 helper。它证明接入后的 helper 本身在 Milkv-Jupiter 上保持正收益，但不证明完整 ONI replay public-entry throughput。

Evidence Doctor 的 3 个 Warning 来自 summary-only metadata、case role 和 run contract 不完整。处理方式是把本文件结论限制为 production-detail positive，不写成完整 production-public（真实公开入口）证据通过。若要补 public-entry 证据，需要 ONI replay 文件场景、可重复 profile，或能构造真实 `ONIGrabber` public entry 的 RISC-V 测试环境。

## 后续优化边界

`convertToXYZPointCloud` 返回具体 `pcl::PointCloud<pcl::PointXYZ>`，没有模板点型参数，因此本补丁没有泛型点型生产测试面。RGB/RGBA 和 IR 在 `oni_grabber.cpp` 中是独立入口，涉及 color buffer、packed RGB/RGBA 字段或 `PointXYZI::data_c` 语义，不能从 depth-only 结果外推。

同构 OpenNI2 topic 的板卡证据显示 RGB overlay median `1.02x`、IR median `1.01x` 且存在低于 1 的 run，mismatch stride mapping median `0.98x`。因此当前不建议继续把 RGB/RGBA/IR 接入生产。恢复条件是出现新的 pack/store 或 IR candidate，或 profile 证明这些路径是实际瓶颈并值得单独建立 phase。

## Traceability Map（可追踪性地图）

| 符号 / 文件 | 层级 | 作用 | 证据角色 |
| --- | --- | --- | --- |
| `ONIGrabber::depthCallback` | production public entry | 深度回调入口，决定是否生成 depth-only 点云 | production boundary |
| `ONIGrabber::convertToXYZPointCloud` | production entry | 创建 `PointCloud<PointXYZ>` 并调用 candidate helper | adopted production behavior |
| `fillXYZPointCloudStd` | production detail helper | 保留原标量公式和 invalid depth 语义 | scalar reference |
| `fillXYZPointCloudRVV` | production detail helper | RVV VL chunk、invalid mask、AoS strided stores | adopted RVV path |
| `fillXYZPointCloudCandidate` | production dispatch | RVV / Std 分流和 fallback gate | dispatch boundary |
| `test-rvv/io/oni_grabber/src/oni_grabber_production_detail_test.cpp` | correctness gate | 对拍 production Std 与 candidate，并验证奇数尺寸 fallback | correctness evidence |
| `test-rvv/io/oni_grabber/src/bench_oni_grabber.cpp` | bench wrapper | `prod_xyz_depth_full_640x480` production-detail 计时 | board evidence |

## 采纳决策

当前 depth-only production patch 已采纳，因为接入后 production-detail 板卡数据稳定 positive，且实现范围窄、fallback 简单。采纳确认来自用户要求：接入后板卡测试如果显示有收益即可采纳。

后续只在用户明确要求时才进入提交或回滚流程；若补 public-entry 证据，需要先提供 ONI replay 场景或 profile。
