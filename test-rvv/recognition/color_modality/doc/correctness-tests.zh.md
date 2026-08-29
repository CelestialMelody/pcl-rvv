# color_modality Correctness Tests

## gtest 字典

| TEST | 被测路径 | 输入 | 断言 | 证明范围 |
| --- | --- | --- | --- | --- |
| `ColorModalityDiagnostic.ScalarReferenceKeepsExpectedMapSizes` | test-support scalar reference | 23x17 synthetic `PointXYZRGB` | quantized / filtered / spreaded map 大小等于输入像素数 | reference fixture 和 map 输出尺寸有效 |
| `ColorModalityDiagnostic.RvvBuildHitsCandidatePathAndMatchesScalarReference` | test-support candidate | 65x37 synthetic `PointXYZRGB` | RVV build 命中 candidate；quantized / filtered / spreaded 与 scalar reference 一致 | production-shaped diagnostic 子链路正确 |
| `ColorModalityProductionDirect.ProcessInputDataProducesUsableMaps` | production public entry | 64x48 synthetic organized cloud | quantized / spreaded map 尺寸正确且非空 | 公开入口能生成可用状态 |
| `ColorModalityProductionDirect.RvvBuildHitsProcessInputDataFilterPipeline` | production dispatch hook | 65x49 synthetic organized cloud | RVV build hook 返回 RVV | `processInputData()` 真实命中 RVV path |
| `ColorModalityProductionDirect.RvvProcessInputDataMatchesForcedScalarPipeline` | production public entry Std/RVV 对拍 | 129x97 synthetic organized cloud | forced scalar 与 RVV 的 quantized / spreaded map 一致 | 当前 production RVV 路径保持标量语义 |

## 不覆盖范围

这些测试不证明目标板卡性能，不覆盖 `extractFeatures()` 的 list/sort feature 选择，也不证明其它 RGB-like 模板点型的 RVV 生产性能。泛型点型扩展需要新的 traits / fallback / board matrix。
