# organized_pointcloud_conversion 优化证据索引

| candidate family | production / test path | test target | board evidence | Evidence Doctor | decision | 边界 |
| --- | --- | --- | --- | --- | --- | --- |
| `pointxyz_z_strided_disparity_rvv_v0_full_temp` | historical test-only candidate | historical | negative | historical | rejected | full-size temp arrays 成本过高，不恢复 |
| `pointxyz_z_strided_disparity_rvv_v1_per_vl_scratch` | `include/impl/opc_candidates.hpp` | `run_test_compare` | diagnostic positive | shared manifest warnings explained | superseded by production | 只作升级理由 |
| `rgb_or_mono_color_pack_rvv_v0_scalar_pack` | `include/impl/opc_candidates.hpp` | `run_test_compare` | weak positive | historical | superseded | 二次标量 color scan 稀释收益 |
| `rgb_or_mono_color_pack_rvv_v1_fused_pack` | `include/impl/opc_candidates.hpp` | `run_test_compare` | diagnostic positive | shared manifest warnings explained | adopted via production helper shape | 仍有 scalar lane color writeback |
| `decode_disparity_backprojection_rvv_v0` | `include/impl/opc_candidates.hpp` | `run_test_compare` | 0.85x-0.91x negative diagnostic | shared manifest Errors=3 | rejected | 不接 decode production |
| `encode_production_probe_pointxyz_like` | `io/include/pcl/compression/organized_pointcloud_conversion.h` | `run_test_compare` | production direct repeated positive | production repeated `Errors=0, Warnings=1` | adopted | 只覆盖 cloud encode overloads 和代表点型证据 |
| full `encodePointCloud` shaped end-to-end | `include/impl/opc_candidates.hpp` + production `OrganizedConversion` | `run_test_compare` smoke | 1.147x-1.157x positive shaped diagnostic | full encode Doctor `Errors=0, Warnings=0` | attempted_positive | 不是真实 public class evidence |
| `analyze_cloud_max_depth_rvv` | `include/impl/opc_candidates.hpp` | `run_test_compare` | 3.483x-3.548x positive diagnostic | analyze Doctor `Errors=0, Warnings=0` | superseded_by_production_detail | 070 只作为 production probe 依据 |
| `analyze_production_detail_rvv` | `io/include/pcl/compression/impl/organized_pointcloud_compression_analysis.hpp` | `run_test_compare` | production-detail median 3.908x / 3.815x / 1.820x；after-patch full shaped median 1.119x / 1.147x / 1.174x | detail Doctor `Errors=0, Warnings=1`；shaped Doctor clean | adopted | 不是真实 `encodePointCloud` public class direct |
| custom / more generic xyz-like point types | production traits gate | planned | not covered | not covered | deferred | 需要 dedicated point-type expansion phase |

## 当前采用方式

Production 采用 `encode_production_probe_pointxyz_like` 和 `analyze_production_detail_rvv`。公开 conversion overload 先尝试 RVV helper；helper 要求 `__RVV10__`、cloud size 至少 64、点类型满足 xyz 单 float AoS traits gate。不满足时自然回到 Std helper。RVV helper 用 strided load 读取 x/y/z，使用 `vfclass` 判断有限值，计算 disparity 后逐 lane 写出输出；colored path 在同一个有限值判断内写 RGB 或 mono。

Analyze production detail helper 在 `__RVV10__` 下用同一 xyz AoS traits gate 做 x/y/z 分块读取和 finite mask，先找最大 finite z 的 index，再按原公式回填 focal length；无法走 RVV 时调用 scalar helper。当前采纳范围是 protected detail helper，不扩大到 public class direct。

## 不建议继续的方向

Decode v0 当前不建议继续推进到 production。它虽然正确，但 per-VL x/y staging 与 AoS 点写回成本已经在板卡上超过公式收益。只有出现能避免这类 staging / 写回成本的新 code shape，才值得重开。

## 值得继续的方向

当前没有必须继续推进的高优先级性能方向。若要继续扩展，优先级应是：先解除真实 `OrganizedPointCloudCompression` public class direct 的 OpenNI 构建限制；其次做更多 traits-compatible 点型的 dedicated expansion；最后在 profile 或同边界 A/B 证明 color byte pack 仍是瓶颈后再尝试颜色写回向量化。
