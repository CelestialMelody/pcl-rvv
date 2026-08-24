# pcd_io_templated_writer RVV 已采纳生产说明

## 当前状态

`io/include/pcl/io/impl/pcd_io.hpp` 已接入两条窄范围 RVV production patch（生产补丁）：
`PCDWriter::writeBinaryCompressed<PointT>(const std::string&, const pcl::PointCloud<PointT>&)`
在写 templated `PointCloud<PointT>` 的 binary_compressed PCD 时，如果过滤 `_` padding 后的有效字段
全部是 4 字节，并且 source / destination 基址、field offset（字段偏移）和 `sizeof(PointT)` stride
都按 4 字节对齐，就用 RVV stride load/store（跨步加载 / 连续写回）把 point-major（按点连续）
布局打包成 field-major（按字段连续）buffer；不满足 gate（准入条件）时回到标量 helper。

`PCDWriter::writeBinary<PointT>(const std::string&, const pcl::PointCloud<PointT>&)` 也已接入一条
tuple / segment output path（按点 tuple 输出路径）：当过滤 padding 后恰好有 4 个连续 4-byte
effective fields（有效字段），offset 为 `0/4/8/12` 时，compact 16B 布局直接 `memcpy`，tail-padding
布局使用 `vlse32.v` + `vsseg4e32.v` 把四个字段按 packed binary payload 连续写出。其它字段数量、
字段大小、offset、stride 或 indices overload（索引入口）都保持标量 fallback（回退路径）。

Phase 060 的 PI5 production-public（真实公开入口）板卡结果为 positive，用户已确认本 topic 的采纳偏好：
接入后板卡测试如果显示有收益即可采纳。因此当前 compressed writer 补丁写成
`adopted production behavior`（已采纳生产行为）。Phase 100 对 binary writer tuple / segment path
完成接入后 production-public 板卡测试，大规模 compact / padding case 同样为 positive，因此当前
binary writer tuple / segment 补丁也写成 `adopted production behavior`。

| case | runs | mean | median | min | max | mean Std ms | mean RVV ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `production_compressed_pointxyzrgba_4f_compact_262k` | 5 | `1.3122x` | `1.2969x` | `1.2192x` | `1.4410x` | `59.7364` | `45.5381` |
| `production_compressed_pointxyzrgba_4f_padding_262k` | 5 | `1.2495x` | `1.2261x` | `1.1937x` | `1.3856x` | `58.1228` | `46.5272` |
| `production_compressed_pointxyzrgba_4f_compact_small_512` | 5 | `1.1239x` | `1.0915x` | `0.9463x` | `1.3146x` | `0.5015` | `0.4459` |
| `production_binary_tuple_pointxyzrgba_4f_compact_262k` | 5 | `1.3046x` | `1.3016x` | `1.2835x` | `1.3408x` | `32.0421` | `24.5642` |
| `production_binary_tuple_pointxyzrgba_4f_padding_262k` | 5 | `1.3240x` | `1.3168x` | `1.3068x` | `1.3494x` | `32.4192` | `24.4881` |
| `production_binary_tuple_pointxyzrgba_4f_compact_small_512` | 5 | `1.1082x` | `1.1036x` | `1.0209x` | `1.1963x` | `0.3924` | `0.3544` |

Compressed writer 的 Evidence Doctor（证据体检）为 `Errors=0, Warnings=4, Suggestions=0`。大规模 compact / padding case
的 warning 是 long-tail / variance（长尾 / 方差），但 min 仍分别为 `1.2192x` 和 `1.1937x`，
不改变 positive bucket。small 512 case 有 1/5 退化，本文只把它作为 smoke-only（冒烟覆盖），
不用于生产收益判断。

Binary writer tuple / segment path 的 Evidence Doctor 为 `Errors=0, Warnings=1, Suggestions=0`。
唯一 warning 来自 small 512 case 的 long-tail / variance；大规模 compact 和 padding case 的 min
分别为 `1.2835x` 和 `1.3068x`，不改变采纳判断。

## 覆盖范围

| item | current boundary |
| --- | --- |
| production entry | `pcl::PCDWriter::writeBinaryCompressed<PointT>(const std::string&, const pcl::PointCloud<PointT>&)`；`pcl::PCDWriter::writeBinary<PointT>(const std::string&, const pcl::PointCloud<PointT>&)` |
| compressed data path | `cloud.points.data()` 从 point-major `PointT` 数组打包到 binary_compressed 的 field-major buffer |
| binary data path | `writeBinary<PointT>` 在 4 个连续 4-byte effective fields 下生成 point-major packed binary payload；compact 16B 直接复制，padding 20B 用 segment store 写出 |
| RVV compile gate | `__RVV10__ && __riscv_vector` |
| compressed field layout gate | 点数非 0，有效字段非空；所有有效字段 size 为 4，field offset 为 4 字节对齐 |
| binary field layout gate | 点数非 0；有效字段数量恰好为 4；每个字段 size 为 4，offset 恰好为 `0/4/8/12` |
| memory layout gate | source / destination 基址 4 字节对齐，`sizeof(PointT)` 为 4 字节倍数 |
| compression / file path | `pcl::lzfCompress`、PCD header、file lock、mmap/write、`msync` 和错误返回保持原路径 |
| fallback | 非 RVV 构建、mixed field size、unaligned offset / stride / base address、empty cloud 或不支持布局都走标量 helper |

不覆盖 `writeASCII<PointT>`、binary writer indices overload、PCLPointCloud2 writer / reader、
`io/src/pcd_io.cpp`、非 4 字节字段、非对齐字段、字段数量不是 4 的 binary writer 布局、
所有可能的自定义 point type 或其它 I/O 格式。`writeBinary<PointT>` 的 Phase 080 field-outer
production probe（生产探针）已完成真实公开入口验证，但板卡 repeated summary 为负向；用户确认负收益
可回滚后，该历史 field-outer 补丁已移除，不影响 Phase 100 当前采用的 tuple / segment path。

## 函数语义

templated compressed writer 先通过 `pcl::getFields<PointT>()` 取得 `PointT` 的字段列表，跳过字段名为
`_` 的 padding field，计算每个有效字段的字节数和 uncompressed data size（未压缩 payload 大小）。
原标量路径把 `PointCloud<PointT>` 的 point-major 布局转换成 field-major buffer，再调用同一个
`pcl::lzfCompress`：

```text
input PointT array:
  point0: x0 y0 z0 rgba0
  point1: x1 y1 z1 rgba1
  point2: x2 y2 z2 rgba2

compressed payload before LZF:
  x0 x1 x2 | y0 y1 y2 | z0 z1 z2 | rgba0 rgba1 rgba2
```

这个 field-major 变换是 PCD binary_compressed 格式的既有语义，用来提升 LZF 压缩率。RVV patch
只接管这段 pack loop，不改变 header、LZF、文件映射、写入、flush 或错误处理。

## 当前采用的优化方式

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 / 边界 |
| --- | --- | --- | --- |
| dispatch（分流逻辑） | adopted | public filename overload 中只在 RVV 构建下尝试 RVV pack helper，失败后自然调用 Std helper。 | production diff、production-direct gtest。 |
| 4 字节字段 gate | adopted | `vlse32.v` / `vse32.v` 直接匹配常见 `x/y/z/rgb/rgba` 4 字节字段；非 4 字节字段由 fallback 管住。 | mixed-size fallback gtest。 |
| AoS -> field-major pack | adopted | 每个字段独立按 `sizeof(PointT)` stride 读，多点连续写入该字段 plane。 | asm attribution 和 production-public bench。 |
| LZF compression | scalar-only | Phase 060 证明优化 pack 后仍能穿透 LZF 和真实文件写边界；LZF 状态机本身不在本 topic。 | board 计时边界包含 LZF。 |
| `writeBinary<PointT>` tuple / segment packed output | adopted | Phase 100 production-public board mean `1.3046x / 1.3240x / 1.1082x`，Evidence Doctor Errors=0 Warnings=1。 | compact 16B 走 `memcpy`，padding 20B 走 `vlse32.v` + `vsseg4e32.v`；只覆盖 4 个连续 4-byte effective fields。 |
| `writeBinary<PointT>` field-outer packed output | rollback/no-production | Phase 080 production-public board mean `0.9842x / 0.9727x / 0.9810x`，Evidence Doctor Errors=3。 | 历史 field-outer RVV production patch 已因负收益回滚；不要复用该实现族。 |
| ASCII / indices / PCLPointCloud2 | scalar-only or separate topic | 文本格式化、indices 语义和 PCLPointCloud2 数据结构不同。 | 不能从当前 templated compressed writer 证据外推。 |

### VL chunk 内部流程

RVV helper 对每个有效字段逐一处理：

1. `src` 指向第一个点中该字段的 32-bit 值，`dst` 指向 output buffer 中该字段 plane 的起点。
2. 每个 VL chunk（可变向量长度分块）用 `vlse32.v` 按 `sizeof(PointT)` stride 从 point-major AoS 读取
   多个点的同一字段。
3. 用 `vse32.v` 连续写入 `dst + point`。
4. 当前字段完成后，`plane_offset += 4 * point_count`，进入下一个有效字段。

这等价于原标量双重循环：

```text
for point in cloud:
  for field in fields:
    memcpy(field_plane_ptr[field], point + field.offset, 4)
```

只是在满足 gate 时，把大量小 `memcpy` 和字段指针推进换成按字段批量的 RVV load/store。

## Fallback 矩阵

| 条件 | 行为 | 语义保持证据 |
| --- | --- | --- |
| 非 `__RVV10__` / 非 RVV 构建 | RVV helper 不编译，公开入口直接使用 Std helper。 | `run_test_compare` 的 Std build 通过。 |
| `cloud.size() == 0` 或有效字段为空 | 回到 Std helper，保持原 empty cloud / invalid state 行为。 | production helper gate。 |
| 任一有效字段 size 不是 4 | 回到 Std helper。 | `CompressedPublicWriterFallsBackForMixedFieldSizes`。 |
| 任一有效 field offset 非 4 字节对齐 | 回到 Std helper。 | `compressedFieldsSupportRvv4BytePack` gate；mixed layout 由 fallback 覆盖。 |
| `sizeof(PointT)` 非 4 字节倍数 | 回到 Std helper。 | production helper gate。 |
| source / destination 基址非 4 字节对齐 | 回到 Std helper。 | production helper gate。 |
| `_` padding field | padding field 被过滤；有效字段满足 gate 时仍可 RVV。 | compact / padding production bench 均 positive。 |
| LZF、mmap、write、sync 或 file lock 失败 | 保持原有错误路径。 | RVV patch 不改变这些代码段。 |

## 范围决策表

| 范围 | 状态 | 当前证据 | 不能外推到哪里 |
| --- | --- | --- | --- |
| templated `writeBinaryCompressed<PointT>`；有效字段均为 4 字节且 offset / stride 对齐 | adopted | public writer correctness、fallback、asm、5-run board mean `1.3122x` / `1.2495x`。 | 不能外推到所有自定义 point type。 |
| compact 16B 和 tail-padding 20B registered point types | adopted | Phase 060 compact / padding production-public case positive。 | 不能外推到 mixed-size 或非对齐字段。 |
| small 512 points | smoke-only | mean positive 但 min `0.9463x`，1/5 退化。 | 不作为生产收益主证据。 |
| non-4-byte / unaligned effective fields | scalar-only fallback | production fallback test 和 gate。 | 不提供 RVV 性能结论。 |
| `writeBinary<PointT>`；4 个连续 4-byte effective fields | adopted | Phase 100 Std/RVV 15/15 gtest 通过；production-public board mean `1.3046x` / `1.3240x`，Doctor Errors=0。 | 不能外推到 indices overload、字段数量不是 4、非 4 字节字段、非连续 offset 或所有自定义 point type。 |
| `writeBinary<PointT>` field-outer RVV family | rollback/no-production | Phase 080 已完成 production direct tests、asm、board repeated 和 Doctor；性能不能支撑采纳。 | 历史负向实现族已移除，不代表 tuple / segment path。 |
| `writeASCII<PointT>` | rejected for current topic | stream formatting / locale 成本和语义风险主导。 | 需要 profile 后重开。 |
| PCLPointCloud2 writer / reader | separate topic | 已由 `test-rvv/io/pcd_io` 承载。 | 不在本文档 scope。 |

## 标量路径与 RVV 路径差异

| 阶段 | 标量路径 | RVV 路径 | 保留边界 |
| --- | --- | --- | --- |
| header 生成 | 原有 `generateHeaderBinaryCompressed` / file header 路径。 | 相同。 | 不优化。 |
| field discovery | `pcl::getFields<PointT>()`，过滤 `_` padding。 | 相同。 | gate 使用过滤后的有效字段。 |
| field-major pack | point × field 双重循环，按 field size `memcpy`。 | field × VL chunk，`vlse32.v` stride load + `vse32.v` contiguous store。 | 仅 4-byte aligned fields。 |
| LZF compression | `pcl::lzfCompress`。 | 相同。 | 不优化 LZF。 |
| file output | file lock、mmap/write、`msync`、error handling。 | 相同。 | 不改变文件语义。 |

## Traceability Map（可追踪性地图）

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `PCDWriter::writeBinaryCompressed<PointT>` | production public entry | 写 templated `PointCloud<PointT>` 的 binary_compressed PCD 文件。 | PCL templated writer 使用者。 | Std/RVV pack helper、`pcl::lzfCompress`、mmap/write。 | adopted production behavior | `io/include/pcl/io/impl/pcd_io.hpp` |
| `pcl::io::detail::compressedFieldsSupportRvv4BytePack` | production gate | 判断 point count、field size、offset、stride 和基址对齐是否可 RVV。 | RVV helper。 | dispatch / fallback。 | fallback boundary | `io/include/pcl/io/impl/pcd_io.hpp` |
| `pcl::io::detail::packBinaryCompressedFieldsStd` | production Std helper | 保留原 AoS -> field-major 标量打包。 | public entry fallback。 | LZF input buffer。 | scalar reference | `io/include/pcl/io/impl/pcd_io.hpp` |
| `pcl::io::detail::packBinaryCompressedFieldsRVV` | production RVV helper | RVV stride load / contiguous store 打包 4-byte fields。 | public entry RVV dispatch。 | LZF input buffer。 | adopted RVV path | `io/include/pcl/io/impl/pcd_io.hpp` |
| `pcl::io::detail::packBinaryFieldsStd` | production Std helper | 保留 `writeBinary<PointT>` 原 point x field 标量 packed output。 | `writeBinary<PointT>` fallback。 | mmap/write output buffer。 | scalar reference | `io/include/pcl/io/impl/pcd_io.hpp` |
| `pcl::io::detail::binaryFieldsSupportTuple4BytePack` | production gate | 判断 binary writer 是否恰好为 4 个连续 4-byte effective fields，并区分 compact / padding。 | `packBinaryFieldsTupleRVV`。 | dispatch / fallback。 | fallback boundary | `io/include/pcl/io/impl/pcd_io.hpp` |
| `pcl::io::detail::packBinaryFieldsTupleRVV` | production RVV helper | compact 16B 直接 `memcpy`；padding 布局用 `vlse32.v` + `vsseg4e32.v` 写 packed payload。 | `writeBinary<PointT>` RVV dispatch。 | mmap/write output buffer。 | adopted production behavior | `io/include/pcl/io/impl/pcd_io.hpp` |
| Phase 080 binary writer RVV probe | historical production probe | 回滚前的 field-outer RVV packed binary output helper，真实公开入口已验证但负收益；当前生产源码中已移除。 | `writeBinary<PointT>` public entry。 | mmap/write output buffer。 | rollback/no-production evidence | `test-rvv/io/pcd_io_templated_writer/doc/phases/080-binary-writer-PI2-production-patch/result.zh.md` |
| `PCL_RVV_PCD_WRITER_TEST_HOOK` helpers | test-only hook | 记录 production path 命中 Rvv / Scalar。 | production-direct gtests。 | path-hit assertions。 | test instrumentation | `io/include/pcl/io/impl/pcd_io.hpp` |
| `test_pcdtw` | correctness gate | 写真实 compressed PCD 文件、解压 payload、对拍 scalar pack 和 fallback。 | `make run_test_compare`。 | gtest output。 | correctness / fallback evidence | `test-rvv/io/pcd_io_templated_writer/src/test_pcdtw.cpp` |
| `bench_pcdtw` | bench wrapper | `production_compressed_*` public writer 计时。 | QEMU / board runner。 | repeated summary / manifest。 | production-public performance | `test-rvv/io/pcd_io_templated_writer/src/bench_pcdtw.cpp` |
| manifest generator | analysis script | 为 production-public summary 生成 Evidence Doctor manifest。 | Make targets。 | JSON manifest、Doctor、registry。 | doctor input | `test-rvv/io/pcd_io_templated_writer/script/generate_pcdtw_evidence_manifest.py` |
| production repeated summary | evidence output summary | 5-run production-public 板卡结果。 | board logs。 | Evidence Doctor、本文。 | board performance | `test-rvv/io/pcd_io_templated_writer/log/board/production_compressed_repeat_5/summary.md` |
| binary tuple production repeated summary | evidence output summary | 5-run production-public binary writer tuple / segment 板卡结果。 | board logs。 | Evidence Doctor、本文。 | board performance | `test-rvv/io/pcd_io_templated_writer/log/board/production_binary_tuple_repeat_5/summary.md` |
| Phase 060 result | documentation section | PI2-PI5 执行事实、EvidenceDecision 和 warning 处理。 | production integration loop。 | 本文、Handoff。 | recovery pointer | `test-rvv/io/pcd_io_templated_writer/doc/phases/060-compressed-writer-PI2-production-patch/result.zh.md` |
| Phase 070 result | documentation section | 用户确认采纳后的 closeout 和下一阶段判断。 | production closeout。 | Handoff、roadmap。 | recovery pointer | `test-rvv/io/pcd_io_templated_writer/doc/phases/070-compressed-writer-production-closeout/result.zh.md` |
| Phase 080 result | documentation section | binary writer PI2-PI5 的 production direct、asm、board negative 和 rollback closeout。 | production integration loop。 | Handoff、roadmap、筛选表。 | rollback/no-production pointer | `test-rvv/io/pcd_io_templated_writer/doc/phases/080-binary-writer-PI2-production-patch/result.zh.md` |
| Phase 100 result | documentation section | binary tuple / segment production probe 的接入、测试、asm、board positive 和 adopted decision。 | production integration loop。 | 本文、Handoff、roadmap、筛选表。 | adopted production pointer | `test-rvv/io/pcd_io_templated_writer/doc/phases/100-binary-writer-tuple-segment-production-probe/result.zh.md` |

## VL chunk 算例

以 3 个点、4 个有效字段 `x/y/z/rgba` 为例，`sizeof(PointT) = 16`。输入内存按点连续：

```text
point0: x0 y0 z0 rgba0
point1: x1 y1 z1 rgba1
point2: x2 y2 z2 rgba2
```

RVV pack 在 `x` 字段这一轮按 16-byte stride 读取 `x0, x1, x2`，连续写成 `x0 x1 x2`。随后对
`y`、`z`、`rgba` 重复同样流程，得到 binary_compressed payload 的未压缩输入：

```text
x0 x1 x2 | y0 y1 y2 | z0 z1 z2 | rgba0 rgba1 rgba2
```

tail-padding 点型的 `sizeof(PointT) = 20` 时，RVV 只读取有效字段 offset，不读取 `_` padding 字段；
只要 offset 和 stride 对齐，语义仍与原标量循环一致。

## Bench 与证据

production-public benchmark 通过真实 `PCDWriter::writeBinaryCompressed<PointT>` 写临时文件并计算输出
checksum（校验和）。计时边界包含 header、field layout conversion、LZF compression、file lock、
mmap/write、`msync` 和文件 checksum 外的热路径；speedup 口径是 `Std time / RVV time`。

| case | 数据 | 计时边界 | 结论 |
| --- | --- | --- | --- |
| `production_compressed_pointxyzrgba_4f_compact_262k` | 262144 points，4 个有效 4-byte fields，compact registered point type | public filename overload + pack + LZF + mmap/write | mean `1.3122x` positive |
| `production_compressed_pointxyzrgba_4f_padding_262k` | 262144 points，4 个有效 4-byte fields，tail padding layout | public filename overload + pack + LZF + mmap/write | mean `1.2495x` positive |
| `production_compressed_pointxyzrgba_4f_compact_small_512` | 512 points，compact layout | public filename overload + pack + LZF + mmap/write | smoke-only；不作为收益主证据 |
| `production_binary_tuple_pointxyzrgba_4f_compact_262k` | 262144 points，4 个连续 4-byte fields，compact 16B registered point type | public filename overload + packed binary write + mmap/write | mean `1.3046x` positive |
| `production_binary_tuple_pointxyzrgba_4f_padding_262k` | 262144 points，4 个连续 4-byte fields，tail padding layout | public filename overload + segment packed binary write + mmap/write | mean `1.3240x` positive |
| `production_binary_tuple_pointxyzrgba_4f_compact_small_512` | 512 points，compact layout | public filename overload + packed binary write + mmap/write | smoke-only；不作为收益主证据 |

QEMU smoke（仿真小型验证）只证明 case 可运行和日志形状，不作为性能结论。

## 正确性与高效性证据链

| evidence area | 当前证据 | 结论边界 |
| --- | --- | --- |
| correctness | Phase 100 后 `make -C test-rvv/io/pcd_io_templated_writer run_test_compare`：Std/RVV 各 15 个 gtest 通过。 | 证明 helper-level、compressed production-direct payload 对拍、binary tuple compact / padding production-direct payload 对拍、fallback 和 indices unchanged。 |
| RED / GREEN | 生产补丁前 `run_test_rvv` 因缺少 production hook 编译失败；补丁后 `run_test_compare` 通过。 | 证明新增 test 真实约束 production 接入。 |
| QEMU smoke | `make -C test-rvv/io/pcd_io_templated_writer run_bench_rvv BENCH_ARGS="--case-filter production_compressed_* --iterations 2 --warmup-iterations 1"` 通过。 | 只证明 production writer bench label 可运行；不作为性能结论。 |
| asm attribution | `make -C test-rvv/io/pcd_io_templated_writer dump_bench_rvv` 后，public writer 实例化符号附近可见 `vlse32.v` / `vse32.v`。 | 证明 RVV 指令归属到当前 production compressed writer pack path。 |
| board performance | `test-rvv/io/pcd_io_templated_writer/log/board/production_compressed_repeat_5/summary.md`：5-run mean `1.3122x` / `1.2495x`，small smoke mean `1.1239x`。 | 只覆盖 Milkv-Jupiter、synthetic registered point types、templated filename binary_compressed writer。 |
| binary board performance | `test-rvv/io/pcd_io_templated_writer/log/board/production_binary_tuple_repeat_5/summary.md`：5-run mean `1.3046x` / `1.3240x`，small smoke mean `1.1082x`。 | 只覆盖 Milkv-Jupiter、4 个连续 4-byte effective fields、templated filename binary writer。 |
| Evidence Doctor | compressed summary Doctor 为 Errors=0，Warnings=4，Suggestions=0；binary tuple summary Doctor 为 Errors=0，Warnings=1，Suggestions=0。 | Warnings 已解释；large cases 保持 positive，small case 降级。 |
| evidence registry | `test-rvv/io/pcd_io_templated_writer/log/evidence_registry.json` freshness check 通过。 | 证明当前 summary / manifest / Doctor 与 registry 一致。 |
| fallback | mixed field size fallback public writer test；gate 覆盖 alignment / empty / non-RVV build。 | 不覆盖所有 PCL datatype 组合的 RVV 性能。 |

### Binary writer Phase 080 历史负向证据

`PCDWriter::writeBinary<PointT>(const std::string&, const pcl::PointCloud<PointT>&)` 的当前
field-outer RVV production patch 已完成同一真实公开入口的 PI2-PI5 验证。正确性、fallback 和
asm attribution（反汇编归属）闭合，但 Milkv-Jupiter 5-run production-public board summary 显示无收益：

| case | mean | median | min | max | decision |
| --- | ---: | ---: | ---: | ---: | --- |
| `production_binary_pointxyzrgb_4f_compact_262k` | `0.9842x` | `0.9827x` | `0.9552x` | `1.0136x` | negative |
| `production_binary_pointxyzrgb_4f_padding_262k` | `0.9727x` | `0.9759x` | `0.9540x` | `0.9819x` | negative |
| `production_binary_pointxyzrgb_4f_compact_small_512` | `0.9810x` | `0.9945x` | `0.9409x` | `1.0154x` | smoke-only negative |

Evidence Doctor 路径为
`test-rvv/io/pcd_io_templated_writer/log/board/production_binary_repeat_5/evidence_doctor.md`，
结果 `Errors=3, Warnings=0, Suggestions=0`，三个 Error 均为 `ba_degradation_frequency`。用户确认
负收益可回滚后，该 production patch、binary production-direct tests 和当前 `production_binary_*` bench /
board wrapper 已移除；本文只把它记录为 historical rollback/no-production evidence，不能写成 adopted
production behavior。

### Binary writer Phase 100 当前采纳证据

Phase 090 的 tuple / segment diagnostic（诊断）只作为前置信号；Phase 100 把新实现族接入真实
`PCDWriter::writeBinary<PointT>(const std::string&, const pcl::PointCloud<PointT>&)` public overload
后重新测试。当前生产路径对 compact 16B 布局直接 `memcpy`，对 tail-padding 20B 布局使用
`vlse32.v` + `vsseg4e32.v`，其余布局 fallback。

| case | mean | median | min | max | decision |
| --- | ---: | ---: | ---: | ---: | --- |
| `production_binary_tuple_pointxyzrgba_4f_compact_262k` | `1.3046x` | `1.3016x` | `1.2835x` | `1.3408x` | positive |
| `production_binary_tuple_pointxyzrgba_4f_padding_262k` | `1.3240x` | `1.3168x` | `1.3068x` | `1.3494x` | positive |
| `production_binary_tuple_pointxyzrgba_4f_compact_small_512` | `1.1082x` | `1.1036x` | `1.0209x` | `1.1963x` | smoke-only positive |

Evidence Doctor 路径为
`test-rvv/io/pcd_io_templated_writer/log/board/production_binary_tuple_repeat_5/evidence_doctor.md`，
结果 `Errors=0, Warnings=1, Suggestions=0`。warning 只影响 small 512 case 的稳定性解释，
不改变大规模 compact / padding 的 positive bucket。

## Production Closeout

| 项 | 最终状态 | 证据 |
| --- | --- | --- |
| production file | 修改 `io/include/pcl/io/impl/pcd_io.hpp`，新增 Std/RVV pack helper、4-byte layout gate 和 public overload dispatch。 | source diff |
| public API | 不改变 public API、函数签名、返回值或错误语义。 | source diff |
| compile gate | RVV helper 只在 `__RVV10__ && __riscv_vector` 下编译。 | Std/RVV correctness |
| adopted RVV path | templated filename binary_compressed writer 的 4-byte aligned effective fields。 | production-public board summary |
| scalar-only paths | ASCII writer、binary indices overload、PCLPointCloud2、non-4-byte fields、unaligned layouts、binary field count != 4；binary writer field-outer RVV probe 已回滚。 | fallback matrix / scope audit / Phase 080 negative / Phase 100 fallback |
| evidence basis | 接入后的 production-public board 数据。 | `test-rvv/io/pcd_io_templated_writer/log/board/production_compressed_repeat_5/summary.md` |
| rollback boundary | binary writer RVV helper 和 public overload 短路分流已移除；compressed writer adopted patch 保留。 | single production source diff |

## 后续方向

当前 compressed writer patch 和 binary writer tuple / segment patch 都已关闭 production closeout。
`writeBinary<PointT>` 的 Phase 080 field-outer RVV 补丁已回滚，不采纳；Phase 100 新实现族已用接入后
production-public 板卡数据证明正收益并采纳。

当前不建议默认继续扩大 production。若后续另开新 phase，优先只做有界范围：任意 compact payload
memcpy（非 RVV 主路径）、non-4-byte fields、更多 point type / layout 扩展或 ASCII writer profile。
这些方向都需要新的范围冻结、correctness、fallback、asm、board repeated 和 Evidence Doctor，不能从
Phase 100 直接外推。

暂不建议继续的方向：

- `writeASCII<PointT>`：stream formatting（流式格式化）和 locale（区域设置）风险主导，缺少非格式化主成本证据。
- PCLPointCloud2 writer / reader：由 `test-rvv/io/pcd_io` 独立 topic 承载。
- non-4-byte fields：需要新的 datatype packing 策略和更宽 fallback / correctness matrix。
