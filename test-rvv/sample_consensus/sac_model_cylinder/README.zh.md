# sac_model_cylinder RVV topic

本目录保存 `SampleConsensusModelCylinder<PointT, PointNT>` 的 topic-local（主题本地）RVV
评估、测试支撑、benchmark（性能测试）和证据摘要。当前 production（生产源码）已经接入三个公开距离入口：
`countWithinDistance`、`selectWithinDistance` 和 `getDistancesToModel`。

## 当前结论

当前 EvidenceDecision（证据决策）是
`production-adopted/count-select-getDistances-direct-indexed-representative-point-types`。采纳依据是接入后的
production direct（真实生产路径）5-run board repeated（重复板卡测试）。当前证据覆盖
`PointXYZ + Normal`、`PointXYZI + Normal`、`PointXYZRGB + Normal` 和 `PointXYZ + PointNormal`
四组代表点型组合：

| point type | count median / min / max | select median / min / max | getDistances median / min / max | checksum | decision |
| --- | ---: | ---: | ---: | --- | --- |
| `PointXYZ + Normal` | `5.3124x / 5.1606x / 5.6082x` | `4.6445x / 4.5348x / 4.6579x` | `6.9318x / 6.8120x / 7.0688x` | `41164` / `5168917350774937783` / `65740` matched | adopted |
| `PointXYZI + Normal` | `5.5145x / 5.3724x / 5.5245x` | `4.4819x / 4.3949x / 4.5514x` | `6.7851x / 6.5743x / 6.8318x` | same matched checksums | adopted |
| `PointXYZRGB + Normal` | `5.3986x / 5.2755x / 5.4519x` | `4.4373x / 4.3543x / 4.5229x` | `7.3186x / 7.1422x / 7.3352x` | same matched checksums | adopted |
| `PointXYZ + PointNormal` | `5.4105x / 5.3094x / 5.4691x` | `4.3314x / 4.0728x / 4.3635x` | `6.2453x / 6.0669x / 6.2818x` | same matched checksums | adopted |

Evidence Doctor（证据体检）结果为 Errors=0、Warnings=0、Suggestions=0。QEMU（仿真器）只作为 correctness
（正确性）和日志形状证据，不作为性能结论。

## 阅读路径

1. `doc-rvv/sample_consensus/sac_model_cylinder-RVV.zh.md`：长期 production RVV 行为、证据链和 fallback（回退路径）。
2. `doc/sac_model_cylinder-evaluation.zh.md`：函数级评估、Traceability Map（可追踪性地图）和生产接入判断。
3. `doc/testing-overview.zh.md`：test / bench / QEMU / board / doctor target 分类和覆盖矩阵。
4. `doc/correctness-tests.zh.md`：11 个 gtest（Google Test 单元测试）的输入、断言和证明边界。
5. `doc/benchmark-and-evidence.zh.md`：bench label、production repeated manifest、Doctor 和 registry。
6. `doc/optimization-evidence.zh.md`：已采纳、已尝试、暂缓和不适用的优化方式证据索引。
7. `doc/test-support-code-map.zh.md`：测试支撑代码、script、output 和 production helper 的定位地图。
8. `doc/phases/040-cylinder-point-type-expansion/result.zh.md`：代表点型扩展的接入后板卡证据和范围结论。
9. `doc/phases/optimization-matrix.zh.md`：跨 phase 证据矩阵和后续扩展状态。

## 常用命令

```bash
make -C test-rvv/sample_consensus/sac_model_cylinder run_test_compare
make -C test-rvv/sample_consensus/sac_model_cylinder clean_bench_rvv check_production_asm
make -C test-rvv/sample_consensus/sac_model_cylinder record_production_board_evidence_state
make -C test-rvv/sample_consensus/sac_model_cylinder production_evidence_status
```

`run_cylinder_count_select_tests` 仍保留为 Phase 000 diagnostic（诊断）局部回归入口；生产采纳判断使用
`run_test_compare`、`check_production_asm` 和 production repeated board manifest。

## 证据白名单和默认排除

可提交候选包括 topic-local 源码、文档、`doc/phases/*/*manifest.json`、`*doctor.md/json` 和
`log/evidence_registry.json`。raw board logs、QEMU logs、`build/`、`script/__pycache__/`、本机配置和私有板卡路径默认排除。
