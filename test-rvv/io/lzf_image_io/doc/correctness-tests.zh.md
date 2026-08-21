# lzf_image_io Correctness Tests

默认入口：`make run_test_compare`。

## TEST 字典

| TEST | 输入构造 | 被测路径 | 断言 | 证明范围 |
| --- | --- | --- | --- | --- |
| `LzfImageIoDiagnostic.DepthXyzMatchesScalarWithInvalidDepth` | `23x11` depth16，包含 0、普通深度和 65535 | `convertDepthToCloudScalar` vs `convertDepthToCloudCandidate` | x/y/z 按 float 相等；NaN lane 保持 NaN；`is_dense` 一致 | depth16 post-decompress conversion 与 reference 一致 |
| `LzfImageIoDiagnostic.PlanarYuv422ToRgbMatchesScalar` | `34x9` planar U/Y/V，覆盖 clip 前后不同取值 | `convertPlanarYuv422ToRgbScalar` vs `convertPlanarYuv422ToRgbCandidate` | 每个点 `r/g/b` 字节一致 | PCLZF planar YUV422 conversion 与 reference 一致 |
| `LzfImageIoProductionProbe.PlanarYuv422ProductionHelperMatchesScalarForPointXYZRGB` | `34x9` planar U/Y/V | production `convertPlanarYuv422ToPointCloudStd` vs RVV helper | `pcl::PointXYZRGB` 每个点 `r/g/b` 字节一致；RVV build 要求 helper 返回 true | 生产 helper 对已验证 RGB 点型正确，且 RVV gate 能命中 |
| `LzfImageIoProductionProbe.PlanarYuv422ProductionHelperRejectsNonByteRgbFields` | `34x9` planar U/Y/V | production Std helper 与 RVV gate | 宽 RGB 字段类型下 RVV helper 返回 false，标量 helper 仍能写出一致结果 | 非 `std::uint8_t r/g/b` 点型不会误走 RVV |
| `LzfImageIoDiagnostic.RgbBufferToCloudMatchesScalar` | `31x7` RGBRGB buffer | `copyRgbBufferToCloudScalar` vs `copyRgbBufferToCloudCandidate` | 每个点 `r/g/b` 字节一致 | Bayer 后 RGB copy / RGB24-like copy 与 reference 一致 |

## Fallback 语义

非 RVV build 中 `convert*Candidate` 直接调用对应 scalar reference。RVV build 中 candidate 使用 `__RVV10__` helper。Phase 020 新增的 production probe 测试覆盖本地 RGB member gate（成员字段准入条件）：`pcl::PointXYZRGB` 命中 RVV，宽 RGB 字段类型回退。

## 不覆盖范围

- `loadImageBlob()` 和真实 PCLZF 文件读取。
- `decompress()` / `pcl::lzfDecompress`。
- `LZF*ImageReader::read/readOMP` 的真实文件读取 public entry；phase 020 只覆盖其 post-decompress 生产 helper。
- Bayer `DeBayer::debayerEdgeAware()` stencil 本体。
- 更多 RGB/RGBA PCL 点类型和真实 PCLZF 文件样本。
