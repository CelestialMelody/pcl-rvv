# organized_pointcloud_conversion 正确性测试

## TEST 字典

| TEST | 输入 | 被测路径 | 断言 | 证明范围 |
| --- | --- | --- | --- | --- |
| `PointXYZCloudToDisparityMatchesHandCheckedLiterals` | 4 点 literal cloud，包含 finite、NaN、infinity | test-only PointXYZ diagnostic | 手算 disparity 与 invalid 0 | disparity 公式、`pcl::isFinite` 语义和截断语义 |
| `PointXYZDiagnosticMatchesPclScalarPathForMixedCloud` | 257 点 mixed invalid `PointXYZ` | diagnostic vs PCL scalar public overload | vector 内容和 checksum 相等 | 非全 finite 输入与标量路径同构 |
| `PointXYZDiagnosticHandlesNonMultipleOfVectorLength` | 1027 点 finite `PointXYZ` | diagnostic vs PCL scalar public overload | vector 内容相等 | strip-mining tail 不丢元素 |
| `AnalyzeOrganizedCloudMatchesScalarFiniteCloud` | 32x24 organized finite `PointXYZ` | analyze diagnostic vs scalar same-chain | max depth / focal length checksum 相等 | 最大深度点选择和 focal length 公式一致 |
| `AnalyzeOrganizedCloudMatchesScalarMixedInvalidCloud` | 32x24 organized mixed-invalid `PointXYZ` | analyze diagnostic vs scalar same-chain | max depth / focal length checksum 相等 | invalid 点跳过语义一致 |
| `AnalyzeProductionDetailMatchesStdFiniteCloud` | 32x24 organized finite `PointXYZ` | production detail dispatch vs `analyzeOrganizedCloudStd` | max depth / focal length bit pattern 相等 | production helper 接入后保持有限点语义 |
| `AnalyzeProductionDetailMatchesStdPointXYZICloud` | 32x24 organized mixed-invalid `PointXYZI` | production detail dispatch vs `analyzeOrganizedCloudStd` | max depth / focal length bit pattern 相等 | representative uncolored 点型上的 detail helper fallback / RVV 语义 |
| `PointXYZRGBCloudToDisparityRgbMatchesHandCheckedLiterals` | 3 点 literal `PointXYZRGB` | colored diagnostic RGB | disparity 和 RGB bytes 手算 | invalid 点 RGB 置 0、RGB 顺序和 disparity 同步 |
| `PointXYZRGBCloudToDisparityMonoMatchesPclScalarPath` | 259 点 mixed invalid `PointXYZRGB` | colored diagnostic mono | disparity、mono 和 checksum 相等 | 灰度公式、invalid zero 与标量一致 |
| `PointXYZRGBFusedColorDiagnosticMatchesPclScalarPath` | 263 点 mixed invalid `PointXYZRGB` | fused colored diagnostic | disparity、RGB 和 checksum 相等 | fused color pack 不改变语义 |
| `DisparityToPointXYZDiagnosticMatchesPclScalarPath` | literal disparity buffer | decode diagnostic vs PCL scalar | cloud checksum / fields 相等 | decode v0 正确性；不证明性能 |
| `PointXYZIProductionDirectMatchesScalarHelper` | 307k representative `PointXYZI` | production public overload vs Std helper | disparity vector 相等 | traits-gated uncolored representative 点型语义 |
| `PointXYZRGBAProductionDirectMatchesScalarHelper` | 307k representative `PointXYZRGBA` | production public overload vs Std helper | disparity 和 RGB bytes 相等 | colored representative 点型语义 |

## 不能证明的范围

这些测试不覆盖用户自定义 xyz-like 点型、normal 复合点型、`Scalar=double`、非 AoS xyz layout、真实 `OrganizedPointCloudCompression` public class entry 或 decode production adoption。`PointXYZEncodePointCloudSmokeProducesStableBytes` 只证明 test-only shaped helper 的输出稳定；analyze production-detail TEST 证明 protected detail helper 语义，不证明 public class direct。Production path 的泛型 gate 虽然按 traits 判断，但板卡性能结论仍只按已验证代表点型报告。

## 运行方式

```bash
cd test-rvv/io/organized_pointcloud_conversion
make run_test_compare
```

Std build 和 RVV build 都应通过 14 个 TEST。若新增 point type expansion 或 public class direct probe，需要先补对应 correctness test，再进入 board 性能采集。
