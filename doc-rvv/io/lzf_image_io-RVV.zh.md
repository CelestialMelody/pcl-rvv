# lzf_image_io RVV

## 当前状态

`io/include/pcl/io/impl/lzf_image_io.hpp` 已采纳一条 RVV production patch（生产补丁）：
`LZFYUV422ImageReader::read()` 和 `readOMP()` 在解压后把 PCLZF planar YUV422
转换为点云 RGB 字段时，优先尝试 RVV helper；不满足 gate（准入条件）时回到标量 helper。

本次采纳范围很窄：只覆盖 PCLZF YUV422 的 post-decompress conversion（解压后转换），不优化
`pcl::lzfDecompress`、文件读取、PCLZF header 解析、depth16 转 xyz、Bayer debayer 本体、
RGB24 reader 或 ImageGrabber 调度。

接入后的 Milkv-Jupiter production-detail（生产 detail 边界）5-run 板卡结果为 mean `1.0864x`、
median `1.0832x`、min `1.0732x`、max `1.1135x`；Std mean `11.9608 ms`，RVV mean
`11.0093 ms`。Evidence Doctor（证据体检）为 `Errors=0, Warnings=0, Suggestions=0`。
该结果是 weak-positive（弱正向收益），用户已确认“有收益即可采纳”，因此保留当前 production patch。

## 函数语义

`LZFYUV422ImageReader` 读取 PCLZF image blob（图像压缩块）后，先通过 `decompress()` 得到
uncompressed buffer（解压后的缓冲区），再把 YUV422 planar 数据转换到
`pcl::PointCloud<PointT>` 的 RGB 字段。

PCLZF YUV422 的解压后布局不是 `ImageYUV422::fillRGB` 使用的 interleaved YUYV（交错 YUYV）。
这里的 buffer 按 plane（平面）排列：

- 前半段是 U plane，每个 U 样本对应两个 Y 样本。
- 中间是 Y plane，每个像素一个 Y 样本。
- 最后一段是 V plane，每个 V 样本对应两个 Y 样本。

标量路径每次读取一个 U/V pair，生成两个相邻点的 RGB：

- `R = clip(Y + (((V - 128) * 18678 + 8192) >> 14))`
- `G = clip(Y + (((V - 128) * -9519 - (U - 128) * 6472 + 8192) >> 14))`
- `B = clip(Y + (((U - 128) * 33292 + 8192) >> 14))`

`readOMP()` 原本只并行化这个 pair loop。接入后，RVV 构建下先尝试同一个 RVV helper；
如果 helper 因点类型或规模 gate 返回 false，再调用 OMP 标量 helper。

## 当前采用的优化方式

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 / 边界 |
| --- | --- | --- | --- |
| dispatch（分流逻辑） | adopted | `read/readOMP` 在解压和 `cloud.resize()` 后，`__RVV10__` 构建先调用 `convertPlanarYuv422ToPointCloudRVV()`；失败则调用 Std / StdOMP helper。 | production source diff、Std/RVV correctness。 |
| 点类型 gate | adopted narrow gate | RVV helper 要求 `PointT` 是 standard-layout，且 `r/g/b` 字段表达式类型均为 `std::uint8_t`。这比完整 PCL RGB traits 泛型更窄，但 fallback 明确。 | production helper direct gtest 覆盖 `PointXYZRGB` 命中和非 byte RGB 字段拒绝。 |
| YUV RVV 数据流 | adopted | 每个 vector lane（向量通道）处理一个 U/V pair，并生成两个 RGB 点。U/V 用连续 load，Y1/Y2 用 stride load（跨步加载）。 | 反汇编可见 `vlse8.v`、`vsra.vi`、`vsse8.v`。 |
| RGB 写回 | adopted | 每个 lane 的第一像素和第二像素分别通过 strided store（跨步写回）写入 AoS 点云的 `r/g/b` 字段。 | `pcl::PointXYZRGB` production helper correctness 和 board repeated。 |
| `readOMP()` | adopted with fallback | RVV 成功时直接返回；RVV gate 失败时保留原 OpenMP 标量 pair loop 语义。 | Std/RVV test 比较和源码 fallback。 |
| depth16 转 xyz | deferred | phase 000 diagnostic 只有 near-threshold weak-positive，median `1.0417x`，并且 invalid depth 与 `is_dense` 语义更复杂。 | 不接 production。 |
| RGB buffer copy | rejected | phase 000 diagnostic median `0.9894x`，5/5 低于 1，Evidence Doctor 报 degradation Error。 | 不接 production。 |
| Bayer edge-aware debayer | deferred / separate topic | 本 topic 只诊断 debayer 后 RGB copy；debayer stencil 本体属于 debayer family。 | 另按 debayer topic 或 profile 恢复。 |

### VL chunk 内部流程

每个 VL chunk（可变向量长度分块）覆盖若干个 U/V pair：

1. 从 U plane 和 V plane 连续读取 `u8` / `v8`，并减去 128 后拓宽到 `int32`。
2. 从 Y plane 用 stride 为 2 的 `vlse8` 分别读取 pair 内第一个像素的 Y1 和第二个像素的 Y2。
3. 对 U/V 计算三个整数 delta：`v * 18678`、`v * -9519 - u * 6472`、`u * 33292`。
4. 每个 delta 加 8192 后算术右移 14，再加到 Y，最后裁剪到 `[0, 255]`。
5. 第一个 RGB 向量按 `sizeof(PointT) * 2` 的 stride 写到偶数像素；第二个 RGB 向量按同一 stride 写到奇数像素。

该组织方式保留了标量路径“一组 U/V 共享两个 Y 样本”的语义；它不改变 `cloud.width`、`cloud.height`、
`cloud.resize()` 或 `decompress()` 的行为。

## Fallback 矩阵

| 条件 | 行为 | 语义保持证据 |
| --- | --- | --- |
| 非 `__RVV10__` 构建 | RVV helper 不编译；public entry 只调用 Std / StdOMP helper。 | `make run_test_compare` 的 Std build 通过。 |
| `PointT` 不是 standard-layout | `convertPlanarYuv422ToPointCloudRVV()` 返回 false，回到标量 helper。 | `HasByteRgbFields` gate。 |
| `PointT::r/g/b` 字段表达式不是 `std::uint8_t` | 回到标量 helper。 | production helper rejection gtest。 |
| 像素数为奇数 | 回到标量 helper。 | RVV helper runtime gate；PCLZF YUV422 正常应为偶数像素，本 gate 保守处理异常输入。 |
| `LZFYUV422ImageReader::read()` 覆盖范围 | RVV build 且 gate 通过时使用 RVV helper。 | production helper correctness、asm、board。 |
| `LZFYUV422ImageReader::readOMP()` 覆盖范围 | RVV build 且 gate 通过时使用 RVV helper；否则走 StdOMP helper。 | Std/RVV correctness 和源码 fallback。 |
| `LZFDepth16ImageReader` | 保持标量。 | 本次不修改该 reader。 |
| `LZFBayer8ImageReader` / `LZFRGB24ImageReader` | 保持标量或既有 debayer 后 copy 逻辑。 | phase 000 RGB copy diagnostic 负向，不接 production。 |
| `pcl::lzfDecompress` | 保持标量串行解压状态机。 | 本 topic 明确不覆盖解压。 |

## 范围决策表

| 范围 | 状态 | 当前证据 | 不能外推到哪里 |
| --- | --- | --- | --- |
| `LZFYUV422ImageReader::read/readOMP` post-decompress planar YUV422 -> byte RGB fields | adopted | production-detail 5-run mean `1.0864x`、median `1.0832x`，Doctor 0/0/0。 | 不能外推到 interleaved YUYV、RGB24 reader、Bayer debayer、depth reader 或完整文件读取成本。 |
| `PointXYZRGB` / byte RGB field 点型 | adopted narrow gate | production helper direct gtest 和 board case 使用 `pcl::PointXYZRGB`。 | 不能外推到任意自定义 RGB 点类型；非 byte fields fallback。 |
| 非 RVV build | scalar-only | Std build correctness 通过。 | 不提供 RVV 性能结论。 |
| depth16 -> xyz | deferred | diagnostic median `1.0417x` 且 near-threshold。 | 当前没有 production RVV 结论。 |
| RGB buffer -> cloud fields | rejected | diagnostic median `0.9894x` 且 5/5 退化。 | 当前候选不进入 production；新策略需另开 phase。 |
| Bayer edge-aware debayer 本体 | deferred / separate | 本 topic 未尝试 stencil 本体。 | 应按 debayer family 或 profile 独立处理。 |

## 标量路径与 RVV 路径差异

| 阶段 | 标量路径 | RVV 路径 | 保留边界 |
| --- | --- | --- | --- |
| 文件读取和解压 | `loadImageBlob()` 后调用 `decompress()`。 | 完全沿用标量路径。 | 解压状态机不优化。 |
| cloud 形状 | 设置 `width` / `height` 并 `resize()`。 | 完全沿用同一 public entry 行为。 | 不改变 public API。 |
| U/V 取样 | 每个 pair 标量读取一个 U 和一个 V。 | 每个 VL chunk 连续读取多组 U/V。 | 数据布局仍按 PCLZF planar。 |
| Y 取样 | 对 pair 内两个像素分别读取 `Y[y_idx]` 和 `Y[y_idx + 1]`。 | 用两个 stride load 分别读取 Y1 / Y2。 | 奇数像素数回退。 |
| RGB 公式 | 标量 int 乘加、右移和 clip。 | 向量 int32 乘加、右移和 clip。 | 常量、舍入偏置和 clip 语义保持一致。 |
| 写回 | 逐点写 `pt.r/g/b`。 | 对偶数/奇数像素分别 strided store 到 `r/g/b` 字段。 | 非 byte RGB fields fallback。 |
| OMP | 标量 pair loop 可并行。 | RVV 命中时不再进入 OMP loop；fallback 时保留 OMP。 | 线程数只影响 fallback。 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `LZFYUV422ImageReader::read` | production public entry | 读取 PCLZF YUV422 并生成 RGB point cloud。 | PCLZF image reader 使用者 / ImageGrabber PCLZF 路径。 | `decompress()`、Std/RVV YUV helper。 | production boundary | `io/include/pcl/io/impl/lzf_image_io.hpp` |
| `LZFYUV422ImageReader::readOMP` | production public entry | OMP 入口；RVV 命中时走同一 RVV helper，否则保留 OMP 标量转换。 | PCLZF image reader 使用者。 | `convertPlanarYuv422ToPointCloudRVV` 或 `StdOMP`。 | production boundary / fallback | `io/include/pcl/io/impl/lzf_image_io.hpp` |
| `convertPlanarYuv422ToPointCloudStd` | production Std helper | 标量 YUV planar 转 RGB 字段。 | `read()` fallback、test reference。 | point cloud RGB fields。 | fallback baseline | `io/include/pcl/io/impl/lzf_image_io.hpp` |
| `convertPlanarYuv422ToPointCloudStdOMP` | production Std helper | OpenMP 标量 fallback。 | `readOMP()` fallback。 | point cloud RGB fields。 | fallback baseline | `io/include/pcl/io/impl/lzf_image_io.hpp` |
| `convertPlanarYuv422ToPointCloudRVV` | production RVV helper | RVV YUV planar 转 RGB 字段。 | `read/readOMP`。 | point cloud RGB fields。 | adopted RVV path | `io/include/pcl/io/impl/lzf_image_io.hpp` |
| `HasByteRgbFields` | production gate | 限定 RVV 写回只覆盖 standard-layout byte RGB fields。 | RVV helper compile-time branch。 | dispatch / fallback。 | production boundary | `io/include/pcl/io/impl/lzf_image_io.hpp` |
| `test_lzf_image_io.cpp` | correctness gate | diagnostic 和 production helper direct gtest。 | `make run_test_compare`、board test。 | gtest output。 | correctness / fallback evidence | `test-rvv/io/lzf_image_io/src/test_lzf_image_io.cpp` |
| `bench_lzf_image_io.cpp` | bench wrapper | diagnostic bench 和 production-detail bench case。 | QEMU / board runner。 | summary / manifest。 | performance wrapper | `test-rvv/io/lzf_image_io/src/bench_lzf_image_io.cpp` |
| manifest generator | analysis script | 生成 Evidence Doctor manifest。 | Make targets。 | JSON manifest。 | doctor input | `test-rvv/io/lzf_image_io/script/generate_lzf_image_io_evidence_manifest.py` |
| repeated summary generator | analysis script | 从 repeated board logs 生成 summary。 | Make targets。 | summary Markdown。 | summary producer | `test-rvv/io/lzf_image_io/script/generate_lzf_image_io_repeat_summary.py` |
| production repeated summary | evidence output summary | YUV production-detail 5-run 板卡结果。 | board logs。 | Evidence Doctor、本文。 | board performance | `test-rvv/io/lzf_image_io/log/board/production_yuv422_repeat_5/summary.md` |
| production Evidence Doctor | evidence validation | 检查 production-detail summary 的异常信号。 | manifest。 | phase result、本文。 | evidence validation | `test-rvv/io/lzf_image_io/log/board/production_yuv422_repeat_5/evidence_doctor.md` |
| topic evaluation | documentation section | 候选取舍、证据链和未覆盖范围。 | worker / reviewer。 | 本文、phase docs。 | decision audit | `test-rvv/io/lzf_image_io/doc/lzf_image_io-evaluation.zh.md` |
| phase 020 result | documentation section | PI2-PI5 执行事实、生产证据和用户采纳边界。 | production integration loop。 | 本文、Handoff。 | recovery pointer | `test-rvv/io/lzf_image_io/doc/phases/020-yuv-planar-production-probe/result.zh.md` |

## VL chunk 算例

以一个 U/V pair 为例，`U=130`、`V=140`，所以 `u = 2`、`v = 12`。
若两个像素的 Y 分别是 `Y1=100`、`Y2=120`：

| channel | delta 计算 | delta after shift | pixel 1 | pixel 2 |
| --- | ---: | ---: | ---: | ---: |
| R | `12 * 18678 + 8192` | `14` | `clip(100 + 14) = 114` | `clip(120 + 14) = 134` |
| G | `12 * -9519 - 2 * 6472 + 8192` | `-7` | `clip(100 - 7) = 93` | `clip(120 - 7) = 113` |
| B | `2 * 33292 + 8192` | `4` | `clip(100 + 4) = 104` | `clip(120 + 4) = 124` |

RVV helper 在一个 VL chunk 中并行处理多组这样的 U/V pair。Y1 和 Y2 来自同一个 Y plane 的交错位置，
因此使用两个 stride load；RGB 写回到 AoS 点云字段时也按两个像素间隔写回。

## Bench 与证据

production-detail benchmark（性能测试）直接调用 production helper，输入为 640x480 的 PCLZF planar
YUV422 形态，输出点类型为 `pcl::PointXYZRGB`。speedup 口径是 `Std time / RVV time`，大于 1 表示
RVV 更快。QEMU smoke（仿真小型验证）只证明 bench label 可运行和日志形状，不作为性能结论。

| case | runs | mean | median | min | max | mean Std ms | mean RVV ms | 结论 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `yuv422_planar_rgb_production_640x480` | 5 | `1.0864x` | `1.0832x` | `1.0732x` | `1.1135x` | `11.9608` | `11.0093` | adopted weak-positive |

Evidence Doctor 结果为 `Errors=0, Warnings=0, Suggestions=0`。该收益低于 phase 000 diagnostic
的 median `1.1490x`，因此最终文档以接入后的 production-detail 数据为准。

## 正确性与高效性证据链

| evidence area | 当前证据 | 结论边界 |
| --- | --- | --- |
| correctness | `make -C test-rvv/io/lzf_image_io run_test_compare`：Std/RVV 各 5 个 gtest 通过。 | 证明 diagnostic helper、production helper direct correctness 和 fallback gate；不证明性能。 |
| board correctness | `make -C test-rvv/io/lzf_image_io run_board_test fetch_board_logs`：板卡 RVV gtest 5/5 通过。 | 目标硬件 correctness smoke。 |
| QEMU smoke | `make -C test-rvv/io/lzf_image_io run_bench_rvv BENCH_ARGS="--case-filter yuv422_planar_rgb_production_640x480 --iterations 2 --warmup-iterations 1"` 通过。 | 只证明 production-detail bench label 可运行。 |
| asm attribution | `make -C test-rvv/io/lzf_image_io dump_bench_rvv` 可见 YUV path 的 `vlse8.v`、`vsra.vi`、`vsse8.v`。 | 证明 RVV 指令存在并归属到 YUV conversion 路径；不单独证明收益。 |
| board performance | `test-rvv/io/lzf_image_io/log/board/production_yuv422_repeat_5/summary.md`：5-run mean `1.0864x`、median `1.0832x`。 | 只覆盖 Milkv-Jupiter、640x480 production-detail helper、`pcl::PointXYZRGB`。 |
| Evidence Doctor | `test-rvv/io/lzf_image_io/log/board/production_yuv422_repeat_5/evidence_doctor.md`：`Errors=0, Warnings=0, Suggestions=0`。 | 支持当前 weak-positive adoption；仍不扩大范围。 |
| evidence registry | `make -C test-rvv/io/lzf_image_io check_production_evidence_freshness`：fresh。 | 证明当前 summary / manifest / doctor 与 registry 一致。 |
| fallback | 非 RVV 构建、非 byte RGB fields、非 standard-layout、奇数像素数和未覆盖 readers 回到标量。 | 不覆盖未来泛型 RGB traits 扩展。 |

## Production closeout

| 项 | 最终状态 | 证据 |
| --- | --- | --- |
| production file | 修改 `io/include/pcl/io/impl/lzf_image_io.hpp`，新增 Std helper、StdOMP helper、RVV helper、byte RGB field gate 和 public entry dispatch。 | source diff |
| public API | 不改变 public API、模板参数、返回值或异常语义。 | header declarations unchanged |
| compile gate | `__RVV10__` 下编译 RVV helper；非 RVV 构建自然走 Std helper。 | Std/RVV correctness |
| adopted RVV path | `LZFYUV422ImageReader::read/readOMP` post-decompress planar YUV422 -> RGB byte fields。 | production-detail board summary |
| scalar-only paths | depth16 reader、Bayer reader、RGB24 reader、decompress、文件读取、ImageGrabber 调度、非覆盖点类型。 | source audit / phase result |
| evidence basis | 接入后的 production-detail board 数据。 | `test-rvv/io/lzf_image_io/log/board/production_yuv422_repeat_5/summary.md` |
| rollback boundary | 可通过移除 YUV helper dispatch 和 `__RVV10__` RVV helper 回到 Std / StdOMP helper。 | single production header diff |

## 后续方向

当前 topic 不建议继续自动扩大 production patch。理由是：

- 已采纳的 YUV path 是 weak-positive，但实现小、fallback 清晰、Evidence Doctor 无 finding，符合用户确认的采纳标准。
- `depth_xyz_rvv` 的诊断收益更弱，且涉及 `is_dense` 和 invalid depth 语义，不应用当前弱信号直接接生产。
- `rgb_buffer_to_cloud_rvv` 在板卡上 5/5 退化，当前候选应保持 rejected。
- Bayer edge-aware debayer 本体、RGB24 reader、ImageGrabber PCLZF 调度和 `lzfDecompress` 都是不同成本中心，应按 profile 或独立 topic 恢复。

如果未来要扩展泛型 RGB 点类型，下一 phase 需要先设计 PCL RGB/RGBA traits gate，补 fallback tests、
production-detail correctness、asm、board repeated 和 Evidence Doctor；不能把当前 `PointXYZRGB`
byte field gate 外推为完整泛型点类型策略。
