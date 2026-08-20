# 正确性测试

## 测试族

| test | 作用 | 通过条件 |
| --- | --- | --- |
| `CandidateMatchesDenseSphereReference` | dense sphere 对拍 | reference、edge candidate、prepass candidate 统计一致 |
| `CandidateMatchesWaveReference` | wave surface 对拍 | 统计一致 |
| `CandidatePreservesNaNCellSkip` | sparse sphere 的 NaN skip | 统计一致，NaN cell 被跳过 |
| `ProductionDirectMatchesReference` | production direct 对拍 | synthetic public path 统计一致 |
| `GenericXYZPointTypesMatchReference` | generic point types 对拍 | `PointXYZ`、`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` 在 RVV build 下仍与 reference 一致 |
| `NonAoSXYZFallsBackToScalarReference` | generic fallback 对拍 | 不满足 AoS traits gate 的 xyz fallback 在 RVV build 下仍与标量 reference 一致 |

## 证明范围

- 证明测试 helper 的点数、三角形数和 checksum 对齐。
- 证明 production direct synthetic public path 已经命中当前 generic gate。
- 证明 generic xyz AoS point types 在 RVV build 下仍与标量 reference 一致。
- 证明不满足 AoS traits gate 的 xyz fallback 在 RVV build 下仍回退标量。
- 不证明真实 Hoppe / RBF 输入分布。
