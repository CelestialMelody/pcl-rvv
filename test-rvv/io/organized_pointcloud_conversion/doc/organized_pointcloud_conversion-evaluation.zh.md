# organized_pointcloud_conversion 函数级评估

## 当前 EvidenceDecision

`OrganizedConversion<PointT>::convert(cloud, ...)` 的 encode RVV production patch 已采用，`OrganizedPointCloudCompression<PointT>::analyzeOrganizedCloud` 的 production-detail RVV helper 也已在 090 closeout 后采用。当前结论覆盖 cloud -> disparity / RGB / mono public overload，以及 protected analyze detail helper；不覆盖 decode overload，也不等同于真实 `OrganizedPointCloudCompression::encodePointCloud` public class entry 证据。

Production direct 5-run 板卡证据显示 9 个 public-overload case 全部正向且 checksum 一致；Evidence Doctor 输出 `Errors=0, Warnings=1, Suggestions=0`。唯一 warning 是 `PointXYZRGB` mono case 组内离群，已按 RGB / mono 分开报告，不影响 adoption。

060 end-to-end-shaped 5-run 板卡证据显示三个 `production_full_*` case median 为 1.147x、1.148x、1.157x，Doctor `Errors=0, Warnings=0`。080 analyze production-detail 5-run 板卡证据显示 `production_analyze_detail_pointxyz_dense_307k` median 3.908x、`production_analyze_detail_pointxyz_mixed_invalid_307k` median 3.815x、`production_analyze_detail_pointxyzi_mixed_invalid_307k` median 1.820x，Doctor `Errors=0, Warnings=1`。接入 analyze detail 后的 full encode-shaped 证据显示 median 1.119x、1.147x、1.174x，Doctor clean。

## 范围和目标源码

| 文件 | 作用 | 当前状态 |
| --- | --- | --- |
| `io/include/pcl/compression/organized_pointcloud_conversion.h` | `OrganizedConversion` cloud encode / image decode helper | encode cloud overloads adopted；decode scalar-only |
| `io/include/pcl/compression/impl/organized_pointcloud_compression.hpp` | `encodePointCloud` 调用 analyze、conversion 后 PNG 编码 | analyze detail 已接入；真实 public entry 因无 OpenNI 构建边界未直测 |
| `io/include/pcl/compression/impl/organized_pointcloud_compression_analysis.hpp` | production detail Std/RVV analyze helper | adopted production-detail helper |
| `test-rvv/io/organized_pointcloud_conversion` | correctness、bench、asm、Evidence Doctor 和 phase 文档 | adopted evidence + 090 closeout |
| `doc-rvv/io/organized_pointcloud_conversion-RVV.zh.md` | production 长期维护文档 | 已创建 |

## 函数 / 函数族作用速览

| 函数 / 函数族 | 作用 | 输入 / 输出状态 | RVV 判断 |
| --- | --- | --- | --- |
| `OrganizedConversion<PointT, false>::convert(cloud, ...)` | 无颜色点云转 disparity image | 读 `x/y/z`，写 `std::vector<uint16_t>` | adopted |
| `OrganizedConversion<PointT, true>::convert(cloud, ...)` | 彩色点云转 disparity + RGB/mono | 读 `x/y/z/r/g/b`，写 disparity 与 color buffer | adopted |
| `OrganizedConversion<PointT, false>::convert(disparity/depth, ...)` | image 反投影为点云 | 读 image，写 `PointT::x/y/z` | rejected v0 / scalar-only |
| `OrganizedConversion<PointT, true>::convert(disparity/depth, ...)` | image + color 反投影为彩色点云 | 读 image buffers，写 colored `PointT` | not attempted / scalar-only |
| `OrganizedPointCloudCompression::encodePointCloud` | analyze cloud、conversion、PNG encode、stream write | public compression caller | production-shaped positive；public direct pending |
| `OrganizedPointCloudCompression::analyzeOrganizedCloud` | 扫描 organized cloud，计算 max depth 和 focal length | protected helper | adopted production-detail |

## 标量流程与 RVV 流程对照

| 阶段 | 标量路径 | RVV production path | 保留标量 / 回退原因 |
| --- | --- | --- | --- |
| layout gate | 模板字段访问直接编译 | `kRVVXYZAoSPointCompatible<PointT>` 证明 xyz 单 float AoS | 不满足 gate 回退 Std |
| finite mask | `pcl::isFinite(point)` | x/y/z `vfclass` 后逐 lane 判断 | 保持 invalid 语义 |
| disparity formula | 每点 `focal/(scale*z)+shift/scale` | VL chunk 批量浮点计算 | cast 写回逐 lane 保持 `uint16_t` 语义 |
| color pack | 每点 RGB 或 mono push | 同一 finite 判断内逐 lane 写 RGB/mono | color byte pack 未向量化，避免新增未验证布局 helper |
| output container | `clear/reserve/push_back` | `assign` 固定大小后按 index 写 | correctness / checksum 对拍已覆盖 |
| decode | 标量 push `PointT` | v0 diagnostic rejected | AoS 写回和 x/y staging 退化 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `OrganizedConversion<PointT,false>::convert(cloud, ...)` | production public entry | uncolored dispatch | `encodePointCloud` / public callers | Std 或 RVV helper | adopted production boundary | `io/include/pcl/compression/organized_pointcloud_conversion.h` |
| `OrganizedConversion<PointT,true>::convert(cloud, ...)` | production public entry | colored dispatch | `encodePointCloud` / public callers | Std 或 RVV helper | adopted production boundary | `io/include/pcl/compression/organized_pointcloud_conversion.h` |
| `convertCloudToDisparityRVV` | production RVV helper | uncolored RVV conversion | public overload | disparity vector | production direct | `io/include/pcl/compression/organized_pointcloud_conversion.h` |
| `convertCloudToDisparityColorRVV` | production RVV helper | colored RVV conversion | public overload | disparity + color vectors | production direct | `io/include/pcl/compression/organized_pointcloud_conversion.h` |
| `organized_compression_detail::analyzeOrganizedCloud` | production detail dispatch | protected analyze helper 的 Std/RVV 分流 | `OrganizedPointCloudCompression::analyzeOrganizedCloud` | max depth / focal length | adopted production-detail | `io/include/pcl/compression/impl/organized_pointcloud_compression_analysis.hpp` |
| `analyzeOrganizedCloudDiagnostic` | diagnostic candidate | max-depth scan + focal length | test / bench | checksum | component diagnostic | `test-rvv/io/organized_pointcloud_conversion/include/impl/opc_candidates.hpp` |
| `convertDisparityToPointXYZCloudDiagnostic` | diagnostic candidate | decode v0 | test / bench | PointXYZ cloud checksum | rejected diagnostic | `test-rvv/io/organized_pointcloud_conversion/include/impl/opc_candidates.hpp` |
| `test_organized_pointcloud_conversion` | correctness gate | Std/RVV 对拍 | `make run_test_compare` | gtest log | correctness evidence | `test-rvv/io/organized_pointcloud_conversion/src/test_organized_pointcloud_conversion.cpp` |
| `bench_organized_pointcloud_conversion` | bench wrapper | diagnostic + production direct timing | board runner | summary / doctor | board performance evidence | `test-rvv/io/organized_pointcloud_conversion/src/bench_organized_pointcloud_conversion.cpp` |
| `generate_opc_board_evidence_manifest.py` | analysis script | log -> manifest | Make targets | Evidence Doctor | evidence contract | `test-rvv/io/organized_pointcloud_conversion/script/generate_opc_board_evidence_manifest.py` |
| production topic doc | documentation section | 当前 adopted production 行为 | reviewer / maintainer | production source | long-term fact | `doc-rvv/io/organized_pointcloud_conversion-RVV.zh.md` |

## 当前证据链

| 证据 | 命令 / 路径 | 结果 | 结论边界 |
| --- | --- | --- | --- |
| correctness | `make run_test_compare` | Std/RVV 各 14 个 TEST pass | 输出语义一致 |
| production asm | `make check_production_rvv_asm` | pass | RVV 指令归属到 production probe |
| production board repeated | `log/board/production_direct_repeated/summary.md` | 9 cases positive | conversion public overload 性能 |
| Evidence Doctor | `log/board/production_direct_repeated/evidence_doctor.md` | `Errors=0, Warnings=1` | warning 已解释 |
| end-to-end shaped board repeated | `log/board/full_encode_repeated/summary.md` | 3 cases median 1.147x-1.157x | production-shaped，不是真实 public entry |
| analyze production-detail board repeated | `log/board/analyze_production_detail_repeated/summary.md` | 3 cases median 1.820x-3.908x | protected detail helper adopted；不证明 public class direct |
| after-patch full shaped board repeated | `log/board/full_encode_after_analyze_detail_repeated/summary.md` | 3 cases median 1.119x-1.174x | 接入 analyze detail 后的 production-shaped context |
| shared Doctor | `log/board/evidence_doctor.md` | decode diagnostic `Errors=3` | 支撑 decode v0 rejected，不参与 encode adoption |

## Doc suite role inventory

| role | 状态 | 路径 |
| --- | --- | --- |
| topic_navigation | standalone | `test-rvv/io/organized_pointcloud_conversion/README.zh.md` |
| testing_overview | standalone | `doc/testing-overview.zh.md` |
| correctness_tests | standalone | `doc/correctness-tests.zh.md` |
| benchmark_and_evidence | standalone | `doc/benchmark-and-evidence.zh.md` |
| optimization_evidence | standalone | `doc/optimization-evidence.zh.md` |
| optimization_roadmap | standalone | `doc/optimization-roadmap.zh.md` |
| test_support_code_map | standalone | `doc/test-support-code-map.zh.md` |
| phase_index | standalone | `doc/phases/README.zh.md` |
| evaluation_production | standalone | `doc/organized_pointcloud_conversion-evaluation.zh.md` |
| production_topic_doc | standalone | `doc-rvv/io/organized_pointcloud_conversion-RVV.zh.md` |

## 未覆盖范围和后续判断

1. 真实 `OrganizedPointCloudCompression::encodePointCloud` public entry 仍未直测。当前 encode-shaped helper 已覆盖 analyze + production conversion + PNG + stream write，但因为无 OpenNI cross build 无法实例化真实类，证据角色必须保持 production-shaped diagnostic。
2. 泛型点类型性能不能外推到所有 traits-compatible 点型。当前 board repeated 覆盖 `PointXYZ`、`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA`，更多 custom / normal 点型需要独立 expansion phase。
3. Decode v0 不建议继续 production。除非出现避免 per-VL x/y staging 或更优 AoS 写回的新候选，否则保持标量。
4. `analyzeOrganizedCloud` production-detail 已采纳。真实 public class direct 仍需要解决 OpenNI / public-entry test，不能由 detail helper 证据自动外推。

## 生产接入判断

当前 adoption 成立：conversion public overload 与 analyze production-detail 证据均满足“RVV build 快于 Std build、checksum 一致、asm 路径命中、Evidence Doctor 无 Error”的主门槛。当前没有必须继续推进的高优先级性能候选；真实 public class direct、更多泛型点型和 color byte pack 都需要新的 phase 与额外证据。
