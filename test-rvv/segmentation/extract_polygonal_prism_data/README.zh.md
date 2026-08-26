# extract_polygonal_prism_data RVV topic

## 当前结论

当前已完成 Phase 070 size threshold tuning（规模阈值调优），并作为 adopted production behavior（已采用生产行为）保留：`ExtractPolygonalPrismData<PointT>::segment(PointIndices&)` 在 `__RVV10__` 构建下优先尝试 full-scan RVV 路径，不满足 gate（会回到标量的准入条件）时回到 `segmentStd`。板卡 repeated bench（重复性能测试）显示 single polygon dense / indexed median 均为 1.75x；nested polygon dense median 2.18x、indexed median 2.13x；`PointXYZI` median 1.85x、`PointXYZRGB` median 1.83x、`PointXYZRGBA` median 1.84x；32 点 threshold confirm5（阈值 5-run 确认）median 1.19x、min 1.18x、Evidence Doctor（证据体检）0 / 0 / 0。`PointXYZINormal` 当前因 `sizeof(PointT) > 32` 走标量 fallback，post-gate median 1.00x，不作为 RVV 收益。

当前 EvidenceDecision（证据决策）是 `adopted_production_behavior`。正式 production 长期主题文档为 `doc-rvv/segmentation/extract_polygonal_prism_data-RVV.zh.md`；它只记录已经采纳的生产行为、fallback（回退路径）和接入后的板卡证据，不承担阶段流水。

## 已覆盖范围

| 维度 | 当前证据 | 边界 |
| --- | --- | --- |
| public entry | `segment(PointIndices&)` 真实入口，Std/RVV 两个 build 对比 | 不改 `isPointIn2DPolygon` / `isXYPointIn2DXYPolygon` |
| RVV stage | plane setup 和 `projectPoints` 后的逐点扫描：平面距离、高度 mask、polygon edge parity、多 polygon XOR、保序 output compress | plane fitting 和 `projectPoints` 自身仍是标量 |
| row source | dense ordered indices 和合法 indexed gather | invalid indices 或 32-bit byte offset 不满足时 fallback |
| point type | `RVVXYZAoSFloatLayout<PointT>` 且 `sizeof(PointT) <= 32`；板卡覆盖 `PointXYZ`、`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` | `PointXYZINormal` 和更宽 AoS stride 走标量；用户自定义点型需后续证据 |
| polygon | `polygons_.empty()`、single polygon，以及合法 multi polygon XOR | degenerate polygon 或非法 polygon 顶点索引走标量 |
| size threshold | `indices_->size() >= 32` 可尝试 RVV；32 点 confirm5 为 positive | `<32` 保持标量；低于 32 未采纳 |

## 先读路径

| 目的 | 路径 |
| --- | --- |
| Phase 040 PI5 结果 | `doc/phases/040-production-integration-loop/result.zh.md` |
| Phase 045 S11 收口 | `doc/phases/045-production-closeout-after-adoption/plan.zh.md`、`doc/phases/045-production-closeout-after-adoption/result.zh.md` |
| Phase 060 点类型扩展结果 | `doc/phases/060-point-type-expansion/plan.zh.md`、`doc/phases/060-point-type-expansion/result.zh.md` |
| Phase 070 阈值调优结果 | `doc/phases/070-size-threshold-tuning/plan.zh.md`、`doc/phases/070-size-threshold-tuning/result.zh.md` |
| Phase 080 收尾提交流程审计 | `doc/phases/080-closeout-submit-audit/plan.zh.md`、`doc/phases/080-closeout-submit-audit/result.zh.md` |
| 正式 production 长期主题文档 | `doc-rvv/segmentation/extract_polygonal_prism_data-RVV.zh.md` |
| 函数级评估与证据边界 | `doc/extract_polygonal_prism_data-evaluation.zh.md` |
| 测试入口总览 | `doc/testing-overview.zh.md` |
| correctness tests（正确性测试）字典 | `doc/correctness-tests.zh.md` |
| benchmark / evidence（性能测试 / 证据）字典 | `doc/benchmark-and-evidence.zh.md` |
| 优化证据索引 | `doc/optimization-evidence.zh.md` |
| 测试支撑代码地图 | `doc/test-support-code-map.zh.md` |
| 阶段索引 | `doc/phases/README.zh.md` |
| 跨阶段路线图 | `doc/optimization-roadmap.zh.md` |
| 优化矩阵 | `doc/phases/optimization-matrix.zh.md` |
| 测试与 bench 入口 | `Makefile`、`src/test_eppd.cpp`、`src/bench_eppd.cpp` |
| 测试支撑代码 | `include/eppd.h`、`include/impl/eppd_reference.hpp`、`include/impl/eppd_candidates.hpp` |

## 常用命令

```bash
cd test-rvv/segmentation/extract_polygonal_prism_data
make run_test_compare
make run_board_test
make dump_bench_rvv
make run_board_eppd_repeated_indexed
make run_board_eppd_repeated_production
make run_board_eppd_repeated_production_indexed
make run_board_eppd_repeated_production_nested
make run_board_eppd_repeated_production_nested_indexed
make run_board_eppd_repeated_production_pointtypes
```

QEMU bench smoke（小型验证）只用于 build / log-shape（构建和日志形状）以及 checksum 检查，不作为性能结论。性能结论只引用 Milkv-Jupiter 板卡 repeated summary。

## 证据白名单

| 证据 | 路径 | 提交边界 |
| --- | --- | --- |
| diagnostic dense board summary | `test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated/summary.md`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated/evidence_manifest.json`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated/evidence_doctor.md` | summary evidence，默认 review 后按需 `git add -f` |
| diagnostic indexed board summary | `test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-indexed/summary.md`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-indexed/evidence_manifest.json`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-indexed/evidence_doctor.md` | summary evidence，默认 review 后按需 `git add -f` |
| production dense board summary | `test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production/summary.md`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production/evidence_manifest.json`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production/evidence_doctor.md` | adopted production 主证据，默认 review 后按需 `git add -f` |
| production indexed board summary | `test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-indexed/summary.md`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-indexed/evidence_manifest.json`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-indexed/evidence_doctor.md` | adopted production 主证据，默认 review 后按需 `git add -f` |
| production nested dense board summary | `test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-nested/summary.md`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-nested/evidence_manifest.json`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-nested/evidence_doctor.md` | adopted production 主证据，默认 review 后按需 `git add -f` |
| production nested indexed board summary | `test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-nested-indexed/summary.md`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-nested-indexed/evidence_manifest.json`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-nested-indexed/evidence_doctor.md` | adopted production 主证据，默认 review 后按需 `git add -f` |
| production point-type post-gate summaries | `test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzi-postgate/summary.md`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzi-postgate/evidence_manifest.json`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzi-postgate/evidence_doctor.md`；`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzrgb-postgate/summary.md`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzrgb-postgate/evidence_manifest.json`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzrgb-postgate/evidence_doctor.md`；`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzrgba-postgate/summary.md`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzrgba-postgate/evidence_manifest.json`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzrgba-postgate/evidence_doctor.md`；`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzinormal-postgate/summary.md`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzinormal-postgate/evidence_manifest.json`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzinormal-postgate/evidence_doctor.md` | adopted point-type / fallback confirmation 主证据，默认 review 后按需 `git add -f` |
| production threshold 32 confirmation | `test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-size32-threshold32-confirm5/summary.md`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-size32-threshold32-confirm5/evidence_manifest.json`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-size32-threshold32-confirm5/evidence_doctor.md` | adopted threshold 主证据，默认 review 后按需 `git add -f` |
| production threshold sweep summaries | `test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-size{32,48,64,96,128,256}-threshold32/{summary.md,evidence_manifest.json,evidence_doctor.md}` | screening evidence（初筛证据）；3-run 有 low-run-count warning，不作为强采纳主证据 |
| historical point-type summaries | `test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzi/summary.md`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzi/evidence_manifest.json`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzi/evidence_doctor.md`；`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzrgb/summary.md`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzrgb/evidence_manifest.json`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzrgb/evidence_doctor.md`；`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzrgba/summary.md`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzrgba/evidence_manifest.json`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzrgba/evidence_doctor.md`；`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzinormal/summary.md`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzinormal/evidence_manifest.json`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzinormal/evidence_doctor.md`；`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzinormal-confirm20/summary.md`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzinormal-confirm20/evidence_manifest.json`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzinormal-confirm20/evidence_doctor.md` | historical evidence，用于解释 Phase 060 gate；当前数值以 post-gate summaries 为准 |
| evidence registry | `test-rvv/segmentation/extract_polygonal_prism_data/log/evidence_registry.json` | ignored-local，默认不提交 |
| QEMU correctness / smoke raw logs | `test-rvv/segmentation/extract_polygonal_prism_data/log/qemu/*.log` | raw log，默认不提交 |
| board raw run logs | `test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated*/run*/` | raw log，默认不提交 |
| 反汇编输出 | `build/asm/riscv/bench_eppd_rvv.full.asm` | build 输出，默认不提交 |

## Topic Token

topic 目录使用完整名称 `extract_polygonal_prism_data`；源码与聚合入口使用短 token `eppd`，对应 `ExtractPolygonalPrismData`。

## Doc Suite Role Inventory

| role | 主归属 |
| --- | --- |
| topic_navigation | `README.zh.md` |
| testing_overview | `doc/testing-overview.zh.md` |
| correctness_tests | `doc/correctness-tests.zh.md` |
| benchmark_and_evidence | `doc/benchmark-and-evidence.zh.md` |
| optimization_evidence | `doc/optimization-evidence.zh.md` |
| optimization_roadmap | `doc/optimization-roadmap.zh.md` |
| test_support_code_map | `doc/test-support-code-map.zh.md` |
| phase_index / phase_results | `doc/phases/README.zh.md` 和 `doc/phases/*/result.zh.md` |
| evaluation_production | `doc/extract_polygonal_prism_data-evaluation.zh.md` |
| production_topic_doc | `doc-rvv/segmentation/extract_polygonal_prism_data-RVV.zh.md` |
