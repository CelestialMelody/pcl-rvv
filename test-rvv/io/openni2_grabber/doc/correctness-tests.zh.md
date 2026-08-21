# OpenNI2 Grabber Correctness Tests

默认入口是 `make run_test_compare`，它分别构建 Std 和 RVV 版本并运行同一批 gtest。

| TEST | 输入 | 被测路径 | 断言 | 不能证明 |
| --- | --- | --- | --- | --- |
| `DepthXYZMatchesProductionFormulaAndInvalidMask` | 4x2 depth，含 `0` / no-sample / shadow | `fillXYZCloudCandidate` | 有效点公式与 invalid NaN 语义 | production public entry |
| `RGBOverlayUsesProductionAlphaAndPackedOrder` | 3 个 RGB 像素 | `fillRGBOverlayCandidate` | `r/g/b/a` 字段和 `rgba` packed order | RGB production 收益 |
| `MismatchedDepthWidthKeepsUnmappedRGBSlotsTransparent` | depth_width=2，cloud_width=4 | `fillXYZRGBAFromDepthCandidate` | 整数 step 布点和未映射 slot 保持初始化值 | 非整数 ratio 或真实 resize |
| `IRPointCloudClearsColorStorageAndCopiesIntensity` | 3 个 depth/IR 像素 | `fillXYZICloudCandidate` | invalid depth 仍写 intensity；`data_c` padding 语义 | RVV IR 优化收益 |
| `CandidateMatchesScalarBitwiseOnLargeFrame` | 37x11 synthetic depth/RGB | scalar vs candidate | xyz float bit pattern 和 rgba bitwise 对齐 | 所有点类型泛化 |
| `RGBTemplatePointTypesMatchScalarBitwise` | 17x5 depth + 34x5 RGB | `fillXYZRGBFromDepthCandidate` / `fillXYZRGBAFromDepthCandidate` + `fillRGBOverlayCandidate` | `PointXYZRGB` 与 `PointXYZRGBA` 的 xyz 和 packed color bitwise 对齐 | 当前 depth-only production patch 泛型化或 RGB/RGBA production 收益 |
| `OpenNI2GrabberProductionDetail.CandidateMatchesStdBitwiseOnDepthFrame` | 53x17 synthetic depth | `openni2_grabber.cpp` production detail hook | production Std/RVV helper bitwise 对齐 | OpenNI2 设备对象 public entry |
| `OpenNI2GrabberProductionDetail.CandidateRecordsScalarOrRvvPath` | 31x7 synthetic depth | production detail candidate hook | Std build 记录 Scalar，RVV build 记录 Rvv | 板卡性能 |

测试文件包含中文说明，目的是让 reviewer 能直接看出每个 TEST 的 production 语义来源。Phase 020 已新增 production-detail（生产内部边界）测试；Phase 030 已新增 RGB/RGBA 模板点型诊断测试。当前交叉环境缺少 OpenNI2 依赖，因此仍没有完整 production-public（真实公开入口）设备对象测试。
