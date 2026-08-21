# lzf_image_io Test Support Code Map

## 文件地图

| path | role | 说明 |
| --- | --- | --- |
| `include/lzf_image_io.h` | aggregator | 测试支撑聚合头，只暴露当前 topic 的 reference / candidate helper。 |
| `include/impl/lzf_image_io_support.hpp` | internal helper | 类型、fixture-facing reference、RVV candidate、fallback wrapper。 |
| `src/test_lzf_image_io.cpp` | correctness tests | 构造 synthetic buffer，比较 diagnostic reference / candidate 和 production helper 输出。 |
| `src/bench_lzf_image_io.cpp` | bench harness / cases | 解析 CLI，构造 640x480 diagnostic 和 production-detail case，计时并输出 checksum。 |
| `script/generate_lzf_image_io_repeat_summary.py` | analysis script | 汇总 5-run board logs 为 summary。 |
| `script/generate_lzf_image_io_evidence_manifest.py` | manifest wrapper | 把 topic-specific summary / run logs 转成 Evidence Doctor 输入。 |
| `Makefile` | local harness | 定义 QEMU、board、summary、Doctor、registry target。 |
| `board.mk` | board harness | 定义板卡远端二进制名和目录。 |

## Helper 职责

| helper family | role | production relation |
| --- | --- | --- |
| `convertDepthToCloudScalar/Candidate/RVV` | depth16 到 x/y/z diagnostic | 复刻 `LZFDepth16ImageReader` 解压后循环；当前不接 production。 |
| `convertPlanarYuv422ToRgbScalar/Candidate/RVV` | planar YUV422 到 RGB diagnostic | 与 `LZFYUV422ImageReader` 解压后循环局部匹配；phase 000 进入 PI1。 |
| `pcl::io::detail::convertPlanarYuv422ToPointCloudStd/RVV` | planar YUV422 production helper | Phase 020 已接入 `LZFYUV422ImageReader::read/readOMP`；当前为 adopted weak-positive production behavior，正式文档见 `doc-rvv/io/lzf_image_io-RVV.zh.md`。 |
| `copyRgbBufferToCloudScalar/Candidate/RVV` | RGB buffer 到 cloud diagnostic | 对应 Bayer 后 copy / RGB24-like copy；当前拒绝。 |
| `clipByte`、`u8ToI32`、`u8OffsetToI32`、`clipI32ToU8`、`rgbChannel`、`storeRgbFields` | shared math / store helpers | 仅服务 test support；生产接入时需要按 production 文件风格重建或局部迁移。 |

## 结构审计

`include/impl/lzf_image_io_support.hpp` 当前约 348 行，低于 hard line limit。它混合 core types、reference、candidate 和 shared RVV helper；当前仍可审查。若后续同时扩展 depth、YUV、RGB copy 或更多 production direct wrapper，应按职责拆分 internal helper，避免一个文件继续承载所有候选。

## 不映射到 production 的对象

当前 test support 的 `PointXYZRGB` 是测试 POD，不是 PCL production point type。Phase 020 已为 YUV production helper 补 `pcl::PointXYZRGB` 和非 byte RGB 字段 fallback 测试；其它 PCL RGB/RGBA 点型仍未扩展成完整泛型结论。
