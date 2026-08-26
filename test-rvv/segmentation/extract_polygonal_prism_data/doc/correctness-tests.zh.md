# 正确性测试

本文记录 `src/test_eppd.cpp` 中 gtest 的证据角色。Correctness（正确性）结论只说明输出和 fallback 语义；性能结论必须引用板卡 repeated summary。

## 测试字典

| TEST | 输入 / 被测路径 | 断言 | 证明范围 |
| --- | --- | --- | --- |
| `KeepsScalarOrderForSquarePrism` | diagnostic reference（诊断参考链路），水平 square prism | 输出 indices 保持标量顺序 | 基础 polygon scan 语义 |
| `AppliesXorAcrossNestedPolygons` | diagnostic reference，多 polygon nested rings | inner hole 通过 XOR 排除 | concave hull（凹包）多 polygon XOR 语义 |
| `FullScanNeedsPlaneAndProjectedCoordinates` | full-scan diagnostic candidate，倾斜平面 | RVV candidate 与 reference 一致 | 任意平面 `k1/k2` 投影坐标选择 |
| `FullScanIndexedPathKeepsSourceIndices` | indexed diagnostic candidate | indexed 输出写回原 source index | indexed gather（离散加载）和保序压缩 |
| `SegmentRvvMatchesSegmentStdForDenseSinglePolygon` | 真实 public `segment`，dense single polygon | RVV 输出等于 `segmentStd` | production direct dense single polygon |
| `SegmentRvvMatchesSegmentStdForIndexedSinglePolygon` | 真实 public `segment`，indexed single polygon | RVV 输出等于 `segmentStd` | production direct indexed gather |
| `SegmentRvvMatchesSegmentStdForNestedPolygons` | 真实 public `segment`，nested polygons | RVV 输出等于 `segmentStd` | production direct multi polygon XOR |
| `SegmentRvvMatchesSegmentStdForIndexedNestedPolygons` | indexed + nested polygons | RVV 输出等于 `segmentStd` | gather 与 multi polygon XOR 组合 |
| `SegmentRvvDeclinesDegeneratePolygons` | 退化 polygon | RVV helper 返回 false，public entry 回到标量 | polygon gate 的 fallback |
| `SegmentRvvDeclinesSmallInputs` | 16 点输入 | RVV helper 返回 false，public entry 回到标量 | `<32` small-input fallback |
| `SegmentRvvMatchesSegmentStdForPointXYZI` | `PointXYZI` production direct | RVV 输出等于 `segmentStd` | <=32-byte PointXYZ-like layout |
| `SegmentRvvMatchesSegmentStdForPointXYZRGB` | `PointXYZRGB` production direct | RVV 输出等于 `segmentStd` | RGB layout 中 xyz 字段读取 |
| `SegmentRvvMatchesSegmentStdForPointXYZRGBA` | `PointXYZRGBA` production direct | RVV 输出等于 `segmentStd` | RGBA layout 中 xyz 字段读取 |
| `SegmentRvvDeclinesPointXYZINormal` | `PointXYZINormal` production direct | RVV helper 返回 false | wide-stride 点型走标量 fallback |
| `RvvCandidateMatchesReferenceForSquarePrism` | 早期 post-projection RVV candidate | candidate 与 reference 一致 | historical diagnostic；已被 full-scan / production 覆盖 |
| `RvvCandidateFallsBackForNestedPolygons` | 早期 single-polygon-only candidate | nested polygons fallback | historical diagnostic；当前 production 已支持 nested XOR |

## 不能外推的范围

这些测试不证明 `projectPoints` 自身 RVV 化，也不证明 `Scalar=double`、低于 32 点强行 RVV、用户自定义点型或 `sizeof(PointT) > 32` 宽 stride 点型的 RVV 收益。对应范围保持标量或需要单独 phase。
