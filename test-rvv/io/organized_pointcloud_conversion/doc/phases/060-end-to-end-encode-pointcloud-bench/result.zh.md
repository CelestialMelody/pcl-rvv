# Phase 060: end-to-end encodePointCloud-shaped bench result

## 当前结论

本阶段完成了 encodePointCloud-shaped（形态模拟 `encodePointCloud` 的测试专用 helper）端到端验证。5-run 板卡 repeated 显示 adopted conversion patch 的收益能够传递到包含 analyze scan、production `OrganizedConversion`、PNG encode 和 stream write 的完整形态中，但端到端收益降到约 1.15x。

这批证据是 production-shaped diagnostic（生产形态诊断），不是 `OrganizedPointCloudCompression::encodePointCloud` 真实 public entry（公开入口）证据。原因是当前无 OpenNI cross build 下，`organized_pointcloud_compression.h` 的 `openni_wrapper::ShiftToDepthConverter` 成员无法实例化真实类。

## 计划回填

| action | 状态 | 证据 / 路径 | 结果 |
| --- | --- | --- | --- |
| organized fixture | completed | `include/impl/opc_fixtures.hpp` | 新增 640x480 organized PointXYZ / PointXYZRGB 输入 |
| encode-shaped helper | completed | `include/impl/opc_candidates.hpp` | 复刻 analyze + production conversion + PNG + stream write |
| smoke TEST | completed | `src/test_organized_pointcloud_conversion.cpp` | compressed stream 非空且 checksum 稳定 |
| bench labels | completed | `src/bench_organized_pointcloud_conversion.cpp` | `production_full_*` case-filter 可隔离 |
| manifest metadata | completed | `script/generate_opc_board_evidence_manifest.py` | labels 标为 `production_shaped_diagnostic` |
| board repeated + Doctor | completed | `log/board/full_encode_repeated/*` | Doctor `Errors=0, Warnings=0, Suggestions=0` |

## 板卡 repeated 结果

| case | runs | min | median | max | checksum |
| --- | ---: | ---: | ---: | ---: | --- |
| `production_full_pointxyz_encode_dense_307k` | 5 | 1.146x | 1.147x | 1.153x | match |
| `production_full_pointxyzrgb_encode_rgb_dense_307k` | 5 | 1.142x | 1.148x | 1.154x | match |
| `production_full_pointxyzrgb_encode_mono_dense_307k` | 5 | 1.156x | 1.157x | 1.173x | match |

Evidence Doctor 路径：

- `log/board/full_encode_repeated/summary.md`
- `log/board/full_encode_repeated/evidence_manifest.json`
- `log/board/full_encode_repeated/evidence_doctor.md`

## Evidence Doctor 处理

Doctor 输出 `Errors=0, Warnings=0, Suggestions=0`。Manifest 的 summary role 是 `production_shaped_diagnostic`，A/B boundary 是 `production_shaped_encodePointCloud_helper`，因此本结果只能说明“形态接近真实 encode pipeline 的 helper 中仍有正向收益”，不能替代真实 public class 证据。

## EvidenceDecision

`end_to_end_encode_pointcloud_bench` 判为 `attempted_positive_production_shaped_diagnostic`。这支持保留当前 conversion adoption，也说明继续查找端到端稀释来源仍值得做。

完整链路从 conversion-only 的 1.35x-2.11x 降到约 1.15x，说明 `analyzeOrganizedCloud`、PNG encode 或 stream write 正在稀释局部收益。下一阶段应优先做 `analyzeOrganizedCloud` 组件消融，而不是立即扩大点类型或改 color byte pack。

## 继续 / 停止判断

本阶段不命中停止条件。默认下一阶段是 `070-analyze-organized-cloud-component`。若该组件正向，后续再判断是否能在不绕过 OpenNI 构建限制的前提下做 bounded production probe。
