# point_cloud_image_extractors RVV

## 当前状态

`io/include/pcl/io/impl/point_cloud_image_extractors.hpp` 已采纳三个窄范围 RVV production patch（生产补丁）：

- `PointCloudImageExtractorFromRGBField<PointT>` 在 exact `PointXYZRGB` / `PointXYZRGBA`
  且字段 metadata 匹配时，用 RVV 跨步读取 RGB/RGBA packed word，并用 segment store（三通道分段写回）
  写 `rgb8`。
- `PointCloudImageExtractorWithScaling<PointT>` 经 `PointCloudImageExtractorFromIntensityField`
  进入 exact `PointXYZI::intensity` 且 `SCALING_FULL_RANGE` 时，用 RVV vector reduction（向量规约）
  计算 min/max，再写 `mono16`。
- `PointCloudImageExtractorFromLabelField<PointT>` 在 exact `PointXYZL` 且 `COLORS_MONO` 时，
  用 RVV 跨步读取 `label` 字段，收窄为低 16 位并写 `mono16`。

当前文档只记录已经采纳的 RGB/scaling/label mono16 生产行为。normal field、RGB random label、
Glasbey label、泛型点型扩展、PNG writer 和 `pcd2png` 端到端路径不在当前 adopted 范围内。

性能数据来自接入后的 production-public（公开入口生产证据）板卡 repeated：
`test-rvv/io/point_cloud_image_extractors/log/board/repeated_pi4/summary.md` 和
`test-rvv/io/point_cloud_image_extractors/log/board/repeated_phase090/summary.md`。

## 函数语义

`PointCloudImageExtractor<PointT>::extract()` 先检查输入 cloud 是否是 organized cloud（有组织点云），
再调用具体 extractor 的 `extractImpl()` 生成 `pcl::PCLImage`。如果 `paint_nans_with_black_` 启用，
base `extract()` 会在 `extractImpl()` 之后按 image encoding 把 NaN 点对应像素清零。

RGB field extractor 的标量语义是查找 `rgb` 字段；若不存在则查找 `rgba` 字段。找到字段后，每个点按
`uint32_t` 读取 packed color，并把高到低三个字节写为 `R/G/B`。输出 encoding 是 `rgb8`，step 是
`width * 3`。

scaling extractor 的标量语义是查找配置字段并写 `mono16`。`SCALING_FULL_RANGE` 先扫描所有点得到
min/max，再用 `(value - min) * 65535 / (max - min)` 写出 `uint16_t`。`SCALING_NO` 和
`SCALING_FIXED_FACTOR` 仍保持标量路径。

label extractor 的标量语义由 `color_mode_` 决定。`COLORS_MONO` 每点读取 `uint32_t label`，
按 `static_cast<unsigned short>` 写出 `mono16`；`COLORS_RGB_RANDOM` 和 `COLORS_RGB_GLASBEY`
使用 map/set 状态和 LUT，当前保持标量路径。

## 当前采用的优化方式

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 / 边界 |
| --- | --- | --- | --- |
| RGB/RGBA exact point types | adopted | packed color 字段是 32-bit AoS stride load（结构数组跨步加载），输出是连续 RGB 三字节，适合 `vsseg3e8`。 | production-public 5-run：`PointXYZRGB` median `1.54x`，`PointXYZRGBA` median `1.55x`。 |
| intensity full-range scaling | adopted | full-range 的首遍 min/max 可由 `vfredmin` / `vfredmax` 规约，减少标量逐点 min/max 开销。 | production-public 5-run：median `1.52x`。 |
| label mono16 | weak-positive adopted | `uint32_t label` 是 32-bit AoS stride load，输出是连续 `uint16_t`，实现和 fallback 都很小。 | production-public 5-run：median `1.08x`，min `1.05x`，max `1.09x`。 |
| normal field | rejected for current shape | Phase 060 diagnostic correctness 通过但板卡 median `0.61x`，当前 RVV shape 不值得接入。 | Evidence Doctor 对 normal label 报 Error/Warning。 |
| 泛型点型扩展 | deferred | 当前 gate 是 exact-type gate（具体点型门控），未证明 traits-gated RGB-like、intensity-like 或 label-like 泛型点型。 | 后续需要 point-type expansion phase。 |

### RGB VL Chunk 流程

每个 VL chunk（可变向量长度分块）执行：

1. 用 `vlse32` 从 `cloud.points.data() + field.offset` 按 `sizeof(PointT)` 跨步读取 packed RGB/RGBA。
2. 对 32-bit lane 做右移和 `0xff` mask，得到 R、G、B。
3. 两级 narrow（收窄转换）把 `uint32_t` lane 转成 `uint8_t` lane。
4. 用 `vsseg3e8` 写入连续 `img.data`，保持 `RGBRGB...` 布局。

### Scaling VL Chunk 流程

full-range intensity 分两遍：

1. 第一遍用 `vlse32` 跨步加载 `PointXYZI::intensity`，用 `vfredmin` / `vfredmax` 得到全局 min/max。
2. 第二遍再跨步加载 intensity，执行 `(value - min) * scaling_factor`。
3. 当前写回阶段为保持 `uint16_t` 转换语义，先把 chunk 结果存入临时 float buffer，再由短标量 lane loop
   写 `unsigned short`。这个 scalar tail（标量尾段）仍在 adopted 证据边界内。

### Label VL Chunk 流程

每个 VL chunk 执行：

1. 用 `vlse32` 从 `cloud.points.data() + label_field.offset` 按 `sizeof(PointXYZL)` 跨步读取 `uint32_t label`。
2. 用向量窄化保留每个 lane 的低 16 位，复刻标量 `static_cast<unsigned short>` 语义。
3. 用 `vse16` 写入连续 `img.data`，保持 `mono16` 布局。

## Fallback 矩阵

| 条件 | 行为 | 语义保持证据 |
| --- | --- | --- |
| 非 `__RVV10__` 构建 | 只编译 / 执行 Std helper。 | `make run_test_compare` 的 Std build 通过。 |
| RGB exact `PointXYZRGB` / `PointXYZRGBA` 且字段 metadata 匹配 | RVV build 走 `extractRgbFieldRVV()`。 | production direct path-hit tests、asm、board positive。 |
| RGB 非 exact 点型，例如 `PointXYZRGBL` | 回到 `extractRgbFieldStd()`。 | `RgbNonExactPointTypeFallsBackToScalar`。 |
| RGB/RGBA 字段缺失或 metadata 不匹配 | 原返回 false 或标量 fallback 行为保持。 | field lookup / gate 在 public entry 内。 |
| exact `PointXYZI::intensity` + full-range | RVV build 走 `extractScalingFullRangeIntensityRVV()`。 | production direct path-hit tests、asm、board positive。 |
| intensity fixed-factor、`z` full-range、非 exact intensity 点型 | 回到 `extractScalingFieldStd()`。 | `ScalingGateMissesFallBackToScalar`。 |
| exact `PointXYZL::label` + `COLORS_MONO` | RVV build 走 `extractLabelMono16FieldRVV()`。 | production direct path-hit tests、asm、board weak-positive。 |
| label RGB random / Glasbey、非 exact label-like 点型 | 保持既有标量分支。 | `LabelRgbModesStayOnExistingScalarBranches`。 |
| base NaN post-pass | RVV `extractImpl()` 后继续由 base `extract()` 处理。 | `RvvExtractImplStillUsesBaseNaNPostPass`。 |

## 范围决策表

| 范围 | 状态 | 当前证据 | 不能外推到哪里 |
| --- | --- | --- | --- |
| RGB `PointXYZRGB` | adopted | production-public median `1.54x` | `PointXYZRGBL`、其它 RGB-like 点型、端到端 PNG 写出。 |
| RGB `PointXYZRGBA` | adopted | production-public median `1.55x` | 其它 RGBA-like 点型。 |
| intensity full-range `PointXYZI` | adopted | production-public median `1.52x` | `PointXYZINormal`、其它 intensity-like 点型、fixed-factor scaling。 |
| label mono16 `PointXYZL` | weak-positive adopted | production-public median `1.08x` | RGB random / Glasbey、generic label-like 点型。 |
| normal field `PointNormal` | rejected for current candidate | Phase 060 diagnostic median `0.61x` | 新 normal shape 需另建 phase。 |

## 标量路径与 RVV 路径差异

| 阶段 | 标量路径 | RVV 路径 | 保留边界 |
| --- | --- | --- | --- |
| field lookup | 公开入口用 `getFieldIndex` 查字段。 | 沿用同一 field lookup，再把 `PCLPointField` 传入 RVV gate。 | 字段缺失行为不变。 |
| RGB unpack | 每点 `getFieldValue<uint32_t>`，标量写 3 字节。 | `vlse32` 批量读取 packed word，`vsseg3e8` 写 RGB。 | 只覆盖 exact RGB/RGBA。 |
| scaling min/max | 标量 first pass 扫描 min/max。 | `vfredmin` / `vfredmax` 做向量规约。 | 只覆盖 full-range intensity。 |
| scaling write | 每点读 float 后按 scaling mode 写 `uint16_t`。 | full-range 第二遍用 RVV 计算 float，再标量 lane tail 写 `uint16_t`。 | no/fixed-factor 保持标量。 |
| label mono16 | 每点 `getFieldValue<uint32_t>` 后截断写 `uint16_t`。 | `vlse32` 批量读取 label，向量窄化后 `vse16` 写回。 | RGB random / Glasbey 保持标量。 |
| NaN post-pass | base `extract()` 清零 NaN 像素。 | 不改 base post-pass。 | 所有 encoding 继续走原逻辑。 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- |
| `PointCloudImageExtractorFromRGBField::extractImpl` | production public entry | RGB/RGBA field 到 `rgb8`。 | `PointCloudImageExtractor::extract` | production boundary | `io/include/pcl/io/impl/point_cloud_image_extractors.hpp` |
| `extractRgbFieldStd` | production Std helper | RGB 标量 fallback。 | RGB public entry | fallback baseline | 同上 |
| `extractRgbFieldRVV` | production RVV helper | RGB/RGBA 跨步加载和 segment store。 | RGB public entry | adopted RVV path | 同上 |
| `PointCloudImageExtractorWithScaling::extractImpl` | production public entry | 任意字段 scaling 到 `mono16`。 | intensity / z / curvature wrappers | production boundary | 同上 |
| `extractScalingFieldStd` | production Std helper | scaling 标量 fallback。 | scaling public entry | fallback baseline | 同上 |
| `extractScalingFullRangeIntensityRVV` | production RVV helper | intensity full-range min/max 规约和写回。 | scaling public entry | adopted RVV path | 同上 |
| `PointCloudImageExtractorFromLabelField::extractImpl` | production public entry | label 到 `mono16` / RGB label colors。 | `PointCloudImageExtractor::extract` | production boundary | 同上 |
| `extractLabelMono16FieldStd` | production Std helper | label mono16 标量 fallback。 | label public entry | fallback baseline | 同上 |
| `extractLabelMono16FieldRVV` | production RVV helper | label 跨步读取、窄化和 `mono16` 写回。 | label public entry | weak-positive adopted RVV path | 同上 |
| `src/test_pcie.cpp` | correctness gate | diagnostic、production direct、fallback 和 NaN post-pass 测试。 | `make run_test_compare` | correctness / fallback evidence | `test-rvv/io/point_cloud_image_extractors/src/test_pcie.cpp` |
| `src/bench_pcie.cpp` | bench wrapper | production-public case timing 和 checksum。 | QEMU / board targets | performance wrapper | `test-rvv/io/point_cloud_image_extractors/src/bench_pcie.cpp` |
| manifest generator | analysis script | 生成 Evidence Doctor manifest。 | repeated board logs | doctor input | `test-rvv/io/point_cloud_image_extractors/script/generate_pcie_evidence_manifest.py` |
| Phase 080 result | phase result | PI2-PI5 生产接入事实和 adoption 边界。 | worker / reviewer | recovery pointer | `test-rvv/io/point_cloud_image_extractors/doc/phases/080-pi2-production-patch/result.zh.md` |
| Phase 090 result | phase result | label mono16 production 接入事实和 weak-positive 采纳边界。 | worker / reviewer | recovery pointer | `test-rvv/io/point_cloud_image_extractors/doc/phases/090-label-mono16-production-plan/result.zh.md` |
| production repeated summary | evidence output summary | 5-run production-public 板卡结果。 | board logs | board performance | `test-rvv/io/point_cloud_image_extractors/log/board/repeated_pi4/summary.md`, `test-rvv/io/point_cloud_image_extractors/log/board/repeated_phase090/summary.md` |
| production Evidence Doctor | evidence validation | 检查 production-public summary 异常信号。 | manifest | evidence validation | `test-rvv/io/point_cloud_image_extractors/log/board/repeated_pi4/evidence_doctor.md`, `test-rvv/io/point_cloud_image_extractors/log/board/repeated_phase090/evidence_doctor.md` |

## VL Chunk 算例

RGB 示例：packed value `0x00112233` 进入一个 lane 时，RVV 路径计算：

| 分量 | 计算 | 输出字节 |
| --- | --- | ---: |
| R | `(value >> 16) & 0xff` | `0x11` |
| G | `(value >> 8) & 0xff` | `0x22` |
| B | `value & 0xff` | `0x33` |

三个分量经 `vsseg3e8` 写成 `11 22 33`。这与标量路径逐点写 `img.data[i*3 + 0/1/2]`
一致。

scaling 示例：假设一批 intensity 的全局 `min=2.0`、`max=6.0`，则
`scaling_factor = 65535 / 4 = 16383.75`。某 lane 的 `value=4.0` 时输出为
`static_cast<unsigned short>((4.0 - 2.0) * 16383.75)`，即 `32767`。RVV 路径只改变
min/max 和乘法的批量执行方式，不改变最终 `unsigned short` 写回语义。

label 示例：某 lane 的 `label=0x00010013` 时，标量路径写
`static_cast<unsigned short>(0x00010013)`，即 `0x0013`。RVV 路径先跨步读取 32-bit label，
再窄化为 16-bit lane，写回结果同样是 `0x0013`。

## Bench 与证据

production-public benchmark 使用真实 extractor public entry。Std/RVV 两侧的 case、输入、checksum
policy 和 timer boundary 一致；区别来自 `__RVV10__` 是否启用。speedup 口径是 `Std time / RVV time`，
大于 1 表示 RVV 更快。

| case | runs | median | min | max | 结论 |
| --- | ---: | ---: | ---: | ---: | --- |
| `production_rgb_pointxyzrgb_640x480` | 5 | `1.54x` | `1.53x` | `1.57x` | adopted |
| `production_rgb_pointxyzrgba_640x480` | 5 | `1.55x` | `1.54x` | `1.57x` | adopted |
| `production_scaling_full_range_intensity_640x480` | 5 | `1.52x` | `1.44x` | `1.54x` | adopted |
| `production_label_mono16_pointxyzl_640x480` | 5 | `1.08x` | `1.05x` | `1.09x` | weak-positive adopted |

Evidence Doctor 结果为 `Errors=0, Warnings=0, Suggestions=0`。QEMU smoke 只证明 bench label 可运行
和日志形状，不作为性能结论。

## 正确性与高效性证据链

| evidence area | 当前证据 | 结论边界 |
| --- | --- | --- |
| correctness | `make -C test-rvv/io/point_cloud_image_extractors run_test_compare`：Std/RVV 各 15 个 TEST 通过。 | 证明 diagnostic、production direct、fallback、label RGB mode non-hit 和 NaN post-pass，不证明未冻结点型。 |
| QEMU smoke | `run_bench_rvv` 对 RGB/scaling 和 label production case 以 1 次 warmup / 1 次 iteration 可运行。 | 只证明路径和日志形状。 |
| asm attribution | `make -C test-rvv/io/point_cloud_image_extractors dump_bench_rvv` 可见 `vlse32.v`、`vsseg3e8.v`、`vfredmin.vs`、`vfredmax.vs`、label narrow 和 `vse16.v`。 | 证明生产 helper / public entry 内联边界命中 RVV 指令，不单独证明收益。 |
| board performance | production-public 5-run 支持 4 条 adopted RVV path。 | 只覆盖当前 case、Milkv-Jupiter 和 synthetic organized cloud input。 |
| Evidence Doctor | production-public `Errors=0, Warnings=0, Suggestions=0`。 | 支持当前 RVV-vs-scalar 决策；不做 RVV-family-selection。 |
| fallback | 非 RVV 构建、非 exact RGB、fixed-factor intensity、`z` full-range、label RGB modes 和 base NaN post-pass 均有测试。 | 不覆盖未来 normal / generic traits 扩展。 |

## Production closeout

| 项 | 最终状态 | 证据 |
| --- | --- | --- |
| production file | 修改 `io/include/pcl/io/impl/point_cloud_image_extractors.hpp`，新增 Std helper、RVV helper、dispatch / fallback。 | source diff |
| public API | 不改变 `io/include/pcl/io/point_cloud_image_extractors.h` 的 public / protected API。 | header unchanged |
| compile gate | `__RVV10__` 下编译 RVV helper；非 RVV 构建自然走 Std helper。 | Std/RVV correctness |
| adopted RVV paths | RGB `PointXYZRGB`、RGB `PointXYZRGBA`、intensity full-range `PointXYZI`、label mono16 `PointXYZL`。 | production-public board summary |
| scalar-only paths | label RGB modes、normal、fixed-factor scaling、non-exact point types、generic traits、PNG writer 和 `pcd2png`。 | source gate / fallback tests / phase docs |
| evidence basis | 接入后的 production-public board 数据。 | `test-rvv/io/point_cloud_image_extractors/log/board/repeated_pi4/summary.md`、`test-rvv/io/point_cloud_image_extractors/log/board/repeated_phase090/summary.md` |
| rollback boundary | 可通过移除 `__RVV10__` helper / dispatch 回到 Std helper。 | production file single-topic diff |

## 后续方向

当前授权范围内没有新的高优先级未阻塞生产优化方向。normal field 当前不建议继续沿 v0 形态推进；
若未来恢复，应先提出不依赖 scratch + per-lane byte 写回的新 code shape，并重新完成 correctness、
asm、board repeated 和 Evidence Doctor。generic RGB/intensity/label-like 点型、label RGB modes、
PNG writer 和 `pcd2png` 端到端路径都属于新的范围扩展，需要单独 phase 计划和证据。
