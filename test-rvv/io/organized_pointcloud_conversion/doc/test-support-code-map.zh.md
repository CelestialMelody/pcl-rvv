# organized_pointcloud_conversion 测试支撑代码地图

## 文件职责

| 文件 | 职责 | 证据角色 |
| --- | --- | --- |
| `include/organized_pointcloud_conversion.h` | topic 测试支撑聚合入口 | 让 test / bench 只依赖稳定入口 |
| `include/impl/opc_types.hpp` | 共享枚举、checksum 和轻量类型 | checksum / input policy support |
| `include/impl/opc_fixtures.hpp` | 构造 `PointXYZ`、`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` 和 disparity 输入 | correctness / bench fixtures |
| `include/impl/opc_candidates.hpp` | test-only diagnostic candidates，包括 encode、fused colored、decode v0、encode-shaped helper、analyze component 和 production-detail bench wrapper | diagnostic / production-shaped correctness 和 bench |
| `src/test_organized_pointcloud_conversion.cpp` | gtest correctness aggregate | correctness gate |
| `src/bench_organized_pointcloud_conversion.cpp` | diagnostic 和 production direct bench wrapper | board performance evidence |
| `src/prod_asm_organized_pointcloud_conversion.cpp` | production RVV asm probe | asm attribution |
| `script/generate_opc_board_evidence_manifest.py` | topic-local manifest wrapper | Evidence Doctor input |
| `Makefile` / `board.mk` | 本地、QEMU 和板卡 target 接入 | reproducibility |

## Production 对照

| production 符号 | 层级 | 作用 | 测试侧对应 |
| --- | --- | --- | --- |
| `convertCloudToDisparityStd` | production Std helper | uncolored encode fallback | Std build 和 production direct 对拍 |
| `convertCloudToDisparityColorStd` | production Std helper | colored encode fallback | Std build 和 production direct 对拍 |
| `convertCloudToDisparityRVV` | production RVV helper | uncolored encode RVV path | production bench / asm probe |
| `convertCloudToDisparityColorRVV` | production RVV helper | colored encode RVV path | production bench / asm probe |
| `OrganizedConversion<PointT,false>::convert(cloud, ...)` | public overload | uncolored dispatch | production direct cases |
| `OrganizedConversion<PointT,true>::convert(cloud, ...)` | public overload | colored dispatch | production direct cases |
| decode overloads | production scalar-only | disparity/depth -> cloud | diagnostic v0 rejected，生产保持标量 |
| `OrganizedPointCloudCompression::encodePointCloud` | production caller | analyze + conversion + PNG + stream | encode-shaped helper；不是真实 public direct |
| `organized_compression_detail::analyzeOrganizedCloud` | production detail dispatch | max depth / focal length scan | production detail tests、bench、asm probe |
| `OrganizedPointCloudCompression::analyzeOrganizedCloud` | protected production helper | 委托到 production detail dispatch | production detail tests；真实 public class direct 仍未覆盖 |

## Layout 和命名审计

当前 topic 已采用配置解析出的 `include/` 聚合入口、`include/impl/` 内部 helper 和 `src/` 源文件布局。没有旧 `test_support/` 目录、compatibility alias 或 legacy evaluation pointer 需要删除。Bench label 字典在 `doc/benchmark-and-evidence.zh.md`，优化方式索引在 `doc/optimization-evidence.zh.md`。

## 证据输出边界

`log/board/production_direct_repeated/summary.md` 和 Evidence Doctor 是 conversion production adoption 的摘要证据。`log/board/analyze_production_detail_repeated/summary.md` 和 Evidence Doctor 是 analyze production-detail adoption 的摘要证据。`log/board/full_encode_after_analyze_detail_repeated/summary.md` 是接入 analyze detail 后的 production-shaped 端到端形态证据。Raw logs、build binaries 和 board deployment artifacts 不进入默认提交边界。
