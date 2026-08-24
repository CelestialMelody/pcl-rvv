# pcd_io RVV 已采纳生产说明

## 当前状态

`io/src/pcd_io.cpp` 已接入一条窄范围 RVV production patch（生产补丁）：
`PCDWriter::writeBinaryCompressed(std::ostream&, const PCLPointCloud2&, ...)` 在写
binary_compressed PCD 时，如果有效字段都是 4 字节并且 field offset（字段偏移）与 `point_step`
按 4 字节对齐，就用 RVV stride load/store（跨步加载 / 连续写回）完成 AoS（结构数组）到
field-major（字段连续）payload buffer 的打包；不满足 gate（准入条件）时回到标量 helper。

本轮 PI5 production-public（真实公开入口）板卡结果为 positive：

| case | runs | median | min | max |
| --- | ---: | ---: | ---: | ---: |
| `production_writer_xyzi_307k` | 5 | `1.33x` | `1.32x` | `1.33x` |
| `production_writer_padded_xyzi_307k` | 5 | `1.25x` | `1.24x` | `1.28x` |

checksum（校验和）一致，Evidence Doctor（证据体检）为 `Errors=0, Warnings=0, Suggestions=4`。
用户已确认本 topic 的采纳偏好：接入后板卡测试如果显示有收益即可采纳。因此当前窄边界写成
`adopted production behavior`（已采纳生产行为），并进入 topic-only（仅当前主题产物）提交边界。

## 覆盖范围

| item | current boundary |
| --- | --- |
| production entry | `pcl::PCDWriter::writeBinaryCompressed(std::ostream&, const PCLPointCloud2&, ...)` |
| data path | `PCLPointCloud2::data` 从 AoS 打包到 binary_compressed payload 的 field-major buffer |
| RVV gate | `__RVV10__ && __riscv_vector`，point count 非零，过滤 `_` padding 后有效字段非空 |
| field layout | 所有有效字段 size 为 4，field offset 与 `cloud.point_step` 4 字节对齐 |
| compression | `pcl::lzfCompress` 保持原有标量实现 |
| stream behavior | header、locale、ostream write 和 flush 保持原有路径 |
| fallback | 非 RVV build、mixed field size、unaligned offset、unaligned point_step、empty data 或 LZF 失败回到 Std |

不覆盖 `PCDReader::readBodyBinary`、filename overload 的 mmap / file lock / `msync` 细节、
templated writer、ASCII writer、non-4-byte fields、泛型所有 PCD datatype 或其它 I/O 文件格式。

## 函数语义

`PCDWriter::writeBinaryCompressed(std::ostream&, ...)` 先写 PCD header，再过滤字段名为 `_` 的 padding field，
计算每个有效字段大小和 uncompressed data size（未压缩 payload 大小）。原标量路径按 point × field
双重循环，从 `cloud.data[i * point_step + field.offset]` 拷贝到 field-major buffer：

```text
AoS:          x0 y0 z0 i0 | x1 y1 z1 i1 | ...
field-major: x0 x1 ...    | y0 y1 ...    | z0 z1 ...    | i0 i1 ...
```

打包后，代码调用同一个 `pcl::lzfCompress`，再把 compressed size、uncompressed size 和压缩 payload
写到 `ostream`。RVV patch 只接管字段打包阶段，不改变 LZF、header 或 stream 语义。

## 当前采用的优化方式

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 / 边界 |
| --- | --- | --- | --- |
| dispatch（分流逻辑） | adopted | public overload 中先尝试 RVV payload helper，失败时自然调用 Std helper。 | production source diff、public ostream gtest。 |
| 4 字节 field gate | adopted | `vlse32.v` / `vse32.v` 与当前 PCD 常见 `x/y/z/intensity/rgb` 4 字节字段匹配；mixed size 风险由 fallback 管住。 | fallback gtest、board positive。 |
| AoS -> field-major pack | adopted | 每个字段独立按 `point_step` stride 读，多点连续写入 field-major buffer。 | asm attribution 和 production-public bench。 |
| LZF compression | scalar-only | 当前证据说明优化打包后能穿透 LZF；LZF 状态机本身不在本 topic。 | board case 计时边界包含 LZF。 |
| reader unpack / finite scan | deferred | Phase 040 reader shaped 只有 weak-positive，padding case median 1.03x，不建议生产探针。 | topic-local phase result。 |
| filename overload / templated writer | separate topic | 入口和对象语义不同，不能从本 public ostream 证据外推。 | fallback / scope boundary。 |

### VL chunk 内部流程

每个有效字段单独打包。对某个字段：

1. `src` 指向第一点的该字段，`dst` 指向 field-major buffer 中该字段的起点。
2. 每个 VL chunk（可变向量长度分块）用 `vlse32.v` 按 `cloud.point_step` 从 AoS 读取多个点的同一字段。
3. 用 `vse32.v` 把这些 32-bit 值连续写入 `dst + point`。
4. 处理完该字段后，`packed_offset += field_size * point_count`，进入下一个字段。

这个组织方式等价于原标量双重循环，只把内层小 `memcpy` 和指针推进换成按字段批量的 RVV load/store。

## Fallback 矩阵

| 条件 | 行为 | 语义保持证据 |
| --- | --- | --- |
| 非 `__RVV10__` 构建 | RVV helper 不编译，public overload 调用 Std helper。 | `run_test_compare` 的 Std build 通过。 |
| `point_count == 0` 或有效字段为空 | 回到 Std helper，写出原有 empty payload 语义。 | production helper 逻辑保持原标量路径。 |
| 任一有效字段 size 不是 4 | 回到 Std helper。 | `PublicOstreamFallsBackForMixedFieldSizes`。 |
| `cloud.point_step` 非 4 字节对齐 | 回到 Std helper。 | `PublicOstreamFallsBackForUnalignedPointStep`。 |
| 任一有效 field offset 非 4 字节对齐 | 回到 Std helper。 | `PublicOstreamFallsBackForUnalignedFieldOffset`。 |
| `_` padding field | padding field 被过滤；有效字段满足 gate 时仍可 RVV。 | `PublicOstreamIgnoresPaddingField`。 |
| `pcl::lzfCompress` 返回 0 | RVV helper 返回失败并回到 Std；Std 也失败时保留原错误返回。 | source fallback boundary。 |

## 范围决策表

| 范围 | 状态 | 当前证据 | 不能外推到哪里 |
| --- | --- | --- | --- |
| public ostream binary_compressed writer，4 字节对齐有效字段 | adopted | 5-run production-public median `1.33x` / `1.25x`，Doctor 0 Errors / 0 Warnings。 | 不能外推到 filename overload 的 file I/O 或 mmap 行为。 |
| `_` padding + 4 字节有效字段 | adopted | fallback / padding gtest；padded board case median `1.25x`。 | 不能外推到任意 padding / datatype 组合。 |
| mixed field size、unaligned layout | scalar-only fallback | production fallback gtest。 | 不提供 RVV 性能结论。 |
| reader compressed payload | deferred | Phase 040 shaped median `1.07x` / `1.03x`。 | 不进入 reader production probe。 |
| templated writer / ASCII writer | separate topic | 本 topic 未测试模板点型语义或文本格式化成本。 | 需要独立 production direct 证据。 |

## 标量路径与 RVV 路径差异

| 阶段 | 标量路径 | RVV 路径 | 保留边界 |
| --- | --- | --- | --- |
| header 生成 | 原有 stream 输出。 | 相同。 | 不优化。 |
| field 过滤与 data size | 过滤 `_` padding，计算有效字段 size。 | 相同。 | gate 只读取过滤后的有效字段。 |
| field-major pack | point × field 双重循环，小 `memcpy`。 | field × VL chunk，`vlse32.v` stride load + `vse32.v` contiguous store。 | 仅 4-byte aligned fields。 |
| LZF compression | `pcl::lzfCompress`。 | 相同。 | 不优化 LZF。 |
| payload write | 写 compressed / uncompressed size 和 payload。 | 相同。 | 不改变 ostream 语义。 |

## Traceability Map（可追踪性地图）

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `PCDWriter::writeBinaryCompressed(std::ostream&, ...)` | production public entry | 写 binary_compressed PCD header 和 payload。 | PCL PCD writer 使用者。 | Std/RVV payload helper、`pcl::lzfCompress`。 | adopted production behavior | `io/src/pcd_io.cpp` |
| `canUseRvvCompressedWriterPack` | production gate | 判断 4-byte field / alignment 是否可 RVV。 | RVV payload helper。 | dispatch / fallback。 | fallback boundary | `io/src/pcd_io.cpp` |
| `packCompressedWriterFieldsStd` | production Std helper | 保留原 AoS -> field-major 标量打包。 | Std payload helper。 | LZF input buffer。 | scalar reference | `io/src/pcd_io.cpp` |
| `packCompressedWriterFieldsRVV` | production RVV helper | RVV stride load / contiguous store 打包 4-byte fields。 | RVV payload helper。 | LZF input buffer。 | adopted RVV path | `io/src/pcd_io.cpp` |
| `writeBinaryCompressedPayloadStd` | production Std payload | 标量 pack + LZF + size header。 | public overload fallback。 | ostream payload write。 | fallback baseline | `io/src/pcd_io.cpp` |
| `tryWriteBinaryCompressedPayloadRVV` | production RVV payload | RVV pack + LZF + size header，失败时交回 Std。 | public overload dispatch。 | ostream payload write。 | production-public candidate | `io/src/pcd_io.cpp` |
| `test_pcd_io.cpp` | correctness gate | public ostream payload、path hit 和 fallback tests。 | `make run_test_compare`。 | gtest output。 | correctness / fallback evidence | `test-rvv/io/pcd_io/src/test_pcd_io.cpp` |
| `bench_pcd_io.cpp` | bench wrapper | `production_writer_*` public overload 计时。 | QEMU / board runner。 | repeated summary / manifest。 | production-public performance | `test-rvv/io/pcd_io/src/bench_pcd_io.cpp` |
| manifest generator | analysis script | 生成 production-public Evidence Doctor manifest。 | Make targets。 | JSON manifest。 | doctor input | `test-rvv/io/pcd_io/script/generate_pcd_io_evidence_manifest.py` |
| production repeated summary | evidence output summary | 5-run production-public 板卡结果。 | board logs。 | Evidence Doctor、本文。 | board performance | `test-rvv/io/pcd_io/log/board/production_writer_repeat_5/summary.md` |
| phase 050 result | documentation section | PI2-PI5 执行事实、EvidenceDecision 和停止条件。 | production integration loop。 | 本文、Handoff。 | recovery pointer | `test-rvv/io/pcd_io/doc/phases/050-production-writer-payload-integration/result.zh.md` |

## VL chunk 算例

以 3 个点、4 个字段 `x/y/z/intensity` 为例，`point_step = 16`。标量路径按点读取：

```text
point0: x0 y0 z0 i0
point1: x1 y1 z1 i1
point2: x2 y2 z2 i2
```

RVV pack 在 `x` 字段这一轮用 stride 16 读取 `x0, x1, x2`，连续写成 `x0 x1 x2`。随后对
`y/z/intensity` 重复同样过程，最终 buffer 为：

```text
x0 x1 x2 | y0 y1 y2 | z0 z1 z2 | i0 i1 i2
```

这正是 PCD binary_compressed 为提升 LZF 压缩率而需要的 field-major 布局。

## Bench 与证据

production-public benchmark 通过 `PCDWriter::writeBinaryCompressed(std::ostream&, ...)` 写到
`std::ostringstream`，计时边界包含 binary_compressed header、field layout conversion、LZF compression
和 ostream payload write；checksum 在计时后计算，speedup 口径是 `Std time / RVV time`。

| case | 数据 | 计时边界 | 结论 |
| --- | --- | --- | --- |
| `production_writer_xyzi_307k` | 307200 points，4 个连续 4-byte fields，`point_step=16` | public ostream header + pack + LZF + payload write | median `1.33x` positive |
| `production_writer_padded_xyzi_307k` | 307200 points，`_` padding 后 4 个有效 4-byte fields，`point_step=24` | public ostream header + pack + LZF + payload write | median `1.25x` positive |

QEMU smoke（仿真小型验证）只证明 case 可运行和日志形状，不作为性能结论。

## 正确性与高效性证据链

| evidence area | 当前证据 | 结论边界 |
| --- | --- | --- |
| correctness | `make -C test-rvv/io/pcd_io run_test_compare`：Std/RVV 各 10 个 gtest 通过。 | 证明 public ostream payload、path hit、fallback 和既有 diagnostic helper correctness。 |
| QEMU smoke | `make -C test-rvv/io/pcd_io run_bench_rvv BENCH_ARGS="--case-filter production_writer --iterations 1 --warmup-iterations 0"` 通过。 | 只证明 production writer bench label 可运行；不作为性能结论。 |
| asm attribution | `make -C test-rvv/io/pcd_io dump_bench_rvv` 后，`bench_pcd_io_rvv.full.asm` 在 public writer path 可见 `vlse32.v` / `vse32.v`。 | 证明 RVV 指令归属到当前 production writer pack path。 |
| board performance | `test-rvv/io/pcd_io/log/board/production_writer_repeat_5/summary.md`：5-run median `1.33x` / `1.25x`。 | 只覆盖 Milkv-Jupiter、synthetic PCLPointCloud2、public ostream binary_compressed writer。 |
| Evidence Doctor | `test-rvv/io/pcd_io/log/board/production_writer_repeat_5/evidence_doctor.md`：Errors=0，Warnings=0，Suggestions=4。 | Suggestions 为环境字段和 binary hash 缺失；不阻塞当前 positive adoption。 |
| evidence registry | `test-rvv/io/pcd_io/log/evidence_registry.json` freshness check 通过。 | 证明当前 summary / manifest / doctor 与 registry 一致。 |
| fallback | mixed field size、unaligned point_step、unaligned field offset、padding field 和非 RVV build 有语义证据。 | 不覆盖所有 PCD datatype 组合的 RVV 性能。 |

## Production Closeout

| 项 | 最终状态 | 证据 |
| --- | --- | --- |
| production file | 修改 `io/src/pcd_io.cpp`，新增 Std/RVV payload helper、4-byte layout gate 和 public overload dispatch。 | source diff |
| public API | 不改变 public API、函数签名、返回值或异常语义。 | source diff |
| compile gate | RVV helper 只在 `__RVV10__ && __riscv_vector` 下编译。 | Std/RVV correctness |
| adopted RVV path | public ostream binary_compressed writer 的 4-byte aligned effective fields。 | production-public board summary |
| scalar-only paths | reader、filename overload file I/O、templated writer、ASCII writer、non-4-byte fields、unaligned layouts。 | fallback matrix / scope audit |
| evidence basis | 接入后的 production-public board 数据。 | `test-rvv/io/pcd_io/log/board/production_writer_repeat_5/summary.md` |
| rollback boundary | 可移除 RVV payload helper 和 public overload 短路分流，Std helper 保留原标量主体。 | single production source diff |

## 后续方向

当前 topic 不建议继续自动扩大 production patch。理由是：

- 当前授权边界已经有 positive production-public evidence，且 fallback 明确。
- reader shaped unpack + scalar finite scan 只有 weak-positive，不建议直接接 reader production。
- `pcl::lzfCompress`、filename overload、templated writer、ASCII writer 和 non-4-byte fields 是不同成本中心或入口语义，
  需要独立 phase / topic 和新的 production direct 证据。
- Evidence Doctor 的 metadata suggestions 可在后续证据质量增强中补充，不改变当前 positive decision bucket。

若未来要扩展同一文件，优先另建 phase 评估 reader finite scan RVV mask 或 filename overload public boundary；
不能把当前 `std::ostream` 4-byte writer 结果外推为整个 PCD I/O family 的泛型结论。
