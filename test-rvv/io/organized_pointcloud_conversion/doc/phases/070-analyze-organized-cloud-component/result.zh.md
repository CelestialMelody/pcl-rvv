# Phase 070: analyzeOrganizedCloud component result

## 当前结论

`analyzeOrganizedCloud` 组件消融完成。test-only RVV candidate（测试专用 RVV 候选）在板卡上稳定正向，dense median 3.548x，mixed-invalid median 3.483x，checksum 一致，Evidence Doctor `Errors=0, Warnings=0, Suggestions=0`。

该结果说明 `analyzeOrganizedCloud` 值得继续进入 bounded production probe（有界生产探针）。它仍是 diagnostic evidence（诊断证据），不能直接写成 production adopted。

## 计划回填

| action | 状态 | 证据 / 路径 | 结果 |
| --- | --- | --- | --- |
| analyze checksum | completed | `include/impl/opc_types.hpp` | max depth / focal length bit pattern 进入 checksum |
| analyze diagnostic helper | completed | `include/impl/opc_candidates.hpp` | RVV 找最大有限 z，下标对应点按原公式计算 focal length |
| correctness TEST | completed | `src/test_organized_pointcloud_conversion.cpp` | finite 与 mixed-invalid organized cloud 通过 |
| bench cases | completed | `src/bench_organized_pointcloud_conversion.cpp` | `analyze_*` labels 可用 |
| manifest metadata | completed | `script/generate_opc_board_evidence_manifest.py` | analyze labels 标为 diagnostic component |
| QEMU smoke | completed | `make run_bench_std/run_bench_rvv BENCH_ARGS='--iterations 1 --warmup-iterations 0 --case-filter analyze_*'` | labels 输出且 checksum 一致；不使用 QEMU timing |
| board repeated + Doctor | completed | `log/board/analyze_component_repeated/*` | 5-run positive，Doctor clean |

## 板卡 repeated 结果

| case | runs | min | median | max | checksum |
| --- | ---: | ---: | ---: | ---: | --- |
| `analyze_pointxyz_organized_dense_307k` | 5 | 3.512x | 3.548x | 3.579x | match |
| `analyze_pointxyz_organized_mixed_invalid_307k` | 5 | 3.410x | 3.483x | 3.496x | match |

Evidence Doctor 路径：

- `log/board/analyze_component_repeated/summary.md`
- `log/board/analyze_component_repeated/evidence_manifest.json`
- `log/board/analyze_component_repeated/evidence_doctor.md`

## Evidence Doctor 处理

Doctor 输出 `Errors=0, Warnings=0, Suggestions=0`。Manifest 使用同一 `xyz_finite_semantics` mask contract；Std 与 RVV 的实现方式不同，但 correctness tests 已证明有限性语义一致。

## EvidenceDecision

`analyze_cloud_max_depth_rvv` 判为 `positive_diagnostic / production_probe_worthwhile`。当前证据只能回答“组件是否值得继续”，不能回答“真实 `OrganizedPointCloudCompression::encodePointCloud` 是否已经接入 RVV analyze”。

## 继续 / 停止判断

仍有一个技术上值得继续的方向：为 `analyzeOrganizedCloud` 做 bounded production probe。当前停止边界不是板卡不可用，也不是不建议继续优化，而是 production boundary（生产边界）扩大：下一步会触碰 `io/include/pcl/compression/impl/organized_pointcloud_compression.hpp`，并且真实 public-entry direct evidence 仍受无 OpenNI cross build 限制。恢复条件是创建 `080-analyze-production-probe` plan，先解决可测 production detail boundary，再决定是否修改 production。
