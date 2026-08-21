# openni2_grabber RVV 已采纳生产说明

## 当前状态

`io/src/openni2_grabber.cpp` 已接入 depth-only `PointXYZ` RVV production patch（生产补丁），当前状态是 `adopted production behavior / production-detail positive`。补丁已经通过接入后板卡重测，且用户已确认“有收益即可采纳”；当前写成 adopted production behavior（已采纳生产行为）。本轮仍不自动提交。

本文件记录已采纳生产行为和接入后证据链。Phase 000 的 diagnostic（诊断）数据不作为本文件的性能数字来源。

## 覆盖范围

| item | current boundary |
| --- | --- |
| production entry | `pcl::io::OpenNI2Grabber::convertToXYZPointCloud(const DepthImage::Ptr&)` |
| helper family | `fillXYZPointCloudStd` / `fillXYZPointCloudRVV` / `fillXYZPointCloudCandidate` |
| point type | 具体 `pcl::PointXYZ` |
| input layout | 连续 `std::uint16_t` depth map；尺寸不一致时沿用原 `fillDepthImageRaw` resize buffer |
| output layout | AoS（结构数组）`pcl::PointXYZ`，stride 为 `sizeof(PointXYZ)` |
| invalid values | `0`、`getNoSampleValue()`、`getShadowValue()` 写 quiet NaN |
| fallback | 非 `__RVV10__` 构建自然走 Std helper |

不覆盖 RGB/RGBA、IR intensity、depth/image mismatch stride mapping、legacy `io/src/openni_grabber.cpp`、public API 变化或泛型点型扩展。

## 实现方式

生产入口仍负责创建 cloud、设置 header、选择 frame id、处理 depth resize buffer、计算焦距倒数和主点。逐像素 depth projection（深度反投影）被抽到内部 helper：

- Std helper 保留原标量语义和公式顺序。
- RVV helper 使用 VL chunk（可变向量长度分块）加载 depth pixel，生成 invalid mask（无效掩码），把 millimeter depth 转成 meter，再按 AoS stride 写回 `x/y/z`。
- 无效点先写 quiet NaN；有效 lane（向量通道）再 masked store（带掩码写入）真实 `x/y/z`。

公式保持与原标量路径一致：

```text
z = pixel * 0.001f
x = (u - centerX) * z * constant_x
y = (v - centerY) * z * constant_y
```

## 接入后证据链

| evidence | command / path | result |
| --- | --- | --- |
| correctness | `make run_test_compare` in `test-rvv/io/openni2_grabber` | Std/RVV 各 8 个 gtest 通过，production-detail hook bitwise 对拍通过；RGB/RGBA 模板诊断覆盖 `PointXYZRGB` 与 `PointXYZRGBA`。 |
| QEMU smoke | `make run_bench_rvv BENCH_ARGS="--case-filter prod_xyz_depth_full_640x480 --iterations 2 --warmup-iterations 1"` | 构建、运行和日志形状通过；不用于性能结论。 |
| asm | `make check_openni2_grabber_production_rvv_asm` | 看到 `vle16`、`vfcvt`、`vfmul`、`vmseq`、`vsse32`。 |
| board repeated | `test-rvv/io/openni2_grabber/log/board/repeated_production_depth/summary.md` | 5-run median 1.19x，min 1.15x，max 1.22x。 |
| Evidence Doctor | `test-rvv/io/openni2_grabber/log/board/repeated_production_depth/evidence_doctor.md` | Errors=0，Warnings=0，Suggestions=0。 |
| registry | `test-rvv/io/openni2_grabber/log/evidence_registry.json` | production-detail summary / manifest / Doctor 已登记。 |

板卡 repeated values：`1.15x, 1.22x, 1.19x, 1.21x, 1.18x`，checksum matched。

## 证据边界

当前板卡性能是 production-detail evidence（生产内部边界证据）：bench 通过 test hook 直接调用 `openni2_grabber.cpp` 中的生产 helper。当前交叉 PCL 安装未启用 `HAVE_OPENNI2`，RISC-V 依赖树中也没有 OpenNI2 头/库，因此本轮没有完成完整 production-public（真实公开入口）设备对象测试。

这不影响“接入后 helper 本身仍有收益”的采纳判断，但它是后续补强 public-entry（真实公开入口）证据时需要处理的剩余风险。若要补 public-entry 证据，需要 OpenNI2-enabled RISC-V 构建环境或等价 public-entry smoke。

## 泛型测试计划

`convertToXYZPointCloud` 返回具体 `pcl::PointCloud<pcl::PointXYZ>`，没有模板点型参数，因此本补丁没有泛型点型生产测试面。RGB/RGBA 模板入口 `convertToXYZRGBPointCloud<PointT>` 可以做泛型点型诊断测试；Phase 030 已在 test-rvv 层覆盖 `PointXYZRGB` 与 `PointXYZRGBA` 的 depth/RGB bitwise 对拍。该测试不扩大 adopted production scope，也不改变 RGB/RGBA 当前暂缓结论。

## 采纳决策

当前 depth-only production patch 已采纳，因为接入后 production-detail 板卡数据稳定 positive，且实现范围窄、fallback 简单。采纳确认来自用户要求“有收益即可采纳”。

后续只在用户明确要求时才进入提交或回滚流程；若补 public-entry 证据，需要先提供 OpenNI2-enabled RISC-V 构建环境。
