# color_modality RVV 生产说明

## 当前状态

`recognition/include/pcl/recognition/color_modality.h` 当前已采用窄范围 production RVV。`ColorModality<PointXYZRGB>::processInputData()` 在 `__RVV10__` 下先尝试 `quantizeColorsRVV()` 和 `filterQuantizedColorsRVV()`，失败后自然回退 `processInputDataStd()`；随后继续调用既有 `QuantizedMap::spreadQuantizedMap()`。`extractFeatures()` 和 `computeDistanceMap()` 仍保持标量，不在当前 adopted production behavior 范围内。

当前采用依据是 Phase 040 production direct（真实生产路径）板卡重复测试。两组 case 的 repeated board 结果均为正向，且 `B/A < 1` 都是 `0/5`：

| case | runs | median speedup | min | max | B/A < 1 | checksum |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `production_process_320x240` | 5 | `2.490x` | `2.430x` | `2.510x` | `0/5` | `4030162183548831619` |
| `production_process_641x481_tail` | 5 | `2.410x` | `2.380x` | `2.430x` | `0/5` | `5008288661140242712` |

证据路径：

- summary: `test-rvv/recognition/color_modality/log/board/repeated_phase040_quantize_filter_rvv/summary.md`
- manifest: `test-rvv/recognition/color_modality/log/board/repeated_phase040_quantize_filter_rvv/evidence_manifest.json`
- Evidence Doctor: `test-rvv/recognition/color_modality/log/board/repeated_phase040_quantize_filter_rvv/evidence_doctor.md`
- Phase 040 result: `test-rvv/recognition/color_modality/doc/phases/040-rgb-extrema-quantize-production-rvv/result.zh.md`
- 函数级评估: `test-rvv/recognition/color_modality/doc/color_modality-evaluation.zh.md`

Phase 030 的 public entry repeated baseline 只证明 filter-only family 的生产收益大约是 `1.380x` / `1.360x`。Phase 040 在同一 public entry boundary 内继续加上 quantize RVV 后，板卡结果明显更高，因此当前长期生产实现采用 Phase 040 family。

## 函数语义和标量路径

`processInputData()` 处理 organized RGB 点云。它先复制 `r/g/b` 字段到 `PointXYZRGB` 输入链路的内部 buffer，再运行 RGB extrema quantize、3x3 dominant filter，最后把 filtered map 传播到 `spreaded_filtered_quantized_colors_`。标量路径的关键事实是：

1. `quantizeColors()` 逐像素计算 8 个 bin 的 extrema 距离，选取最小距离的 bin。
2. `filterQuantizedColors()` 在 3x3 邻域做 histogram，严格 `>` 保持最早 bin tie-break。
3. `QuantizedMap::spreadQuantizedMap()` 传播 filtered map。

## 当前采用的优化方式

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| dispatch / fallback | adopted | `processInputData()` 先尝试 RVV helper，失败后回退标量 | `run_test_compare`、production direct board | 非 RVV 构建和不满足 gate 的输入回退标量 |
| 输入布局 / point type | adopted | 只对 exact `PointXYZRGB` organized 输入开放 RVV 分流 | phase result、board repeated | 其它 RGB-like 模板点型保持标量 |
| RVV quantize 机制 | adopted | `vlse8` 从 AoS 里读 `r/g/b`，按整数缩放比较复刻标量距离和 tie-break | asm、board、gtest 对拍 | 未把 generic RGB traits 作为当前生产门控 |
| RVV filter 机制 | adopted | byte histogram + strict-greater 选择最早 bin | asm、board repeated | 边框和小图回退标量 |
| spread | adopted via existing helper | 继续复用既有 `QuantizedMap::spreadQuantizedMap()`，不在本 topic 重写 | 已有长期 helper 文档 | 该 helper 属于另一个长期 topic |
| 后续优化 | deferred | `extractFeatures()` / `computeDistanceMap()` 没有 profile 证据，不值得本轮继续推进 | roadmap / matrix | 先做 profile 或 component ablation |

## 覆盖范围与 fallback

| 范围 | 当前状态 | 证据 / 原因 |
| --- | --- | --- |
| `PointXYZRGB` organized public input | adopted | production direct board 和 gtest 使用该入口 |
| `width >= 3 && height >= 3` | adopted | 320x240 和 641x481 tail case 覆盖 |
| 非 RVV 构建 | scalar fallback | `__RVV10__` 未启用时只编译 Std helper |
| 小尺寸输入 | scalar fallback | width / height gate 失败后回标量 |
| 其它 RGB-like 模板实例 | scalar fallback | 当前生产证据只批准 exact `PointXYZRGB` |
| `extractFeatures()` | out of scope | 当前计时边界不包含它 |

## 详细设计

1. RVV helper 在 organized `PointXYZRGB` buffer 上按 `vlse8` 读取 `r/g/b`。
2. 颜色 extrema 量化使用整数缩放比较，避免在 RVV 路径里引入额外浮点不一致。
3. 3x3 filter 直接在 quantized byte map 上做计数，用 `vmerge` 保持标量 tie-break。
4. public 入口保留 Std fallback，不在 RVV 失败后执行大段共享逻辑。

## 数值算例

以单个 `PointXYZRGB` 像素为例，RVV helper 会把 `r/g/b` 字节拓宽成向量 lane，计算 8 个 bin 的距离，然后选出最小距离的 bin。对应 3x3 filter 时，中心像素会把周围 9 个 quantized byte 计数，再写成 one-hot bit mask。

## Bench case 说明

| case | 入口 | 数据 | 计时边界 | 证明点 |
| --- | --- | --- | --- | --- |
| `production_process_320x240` | `ColorModality<PointXYZRGB>::processInputData()` | synthetic organized `PointXYZRGB` 320x240 | RGB quantize、3x3 filter 和 spread | 常规尺寸 public entry 生产收益 |
| `production_process_641x481_tail` | 同上 | synthetic organized `PointXYZRGB` 641x481 | 同上，覆盖 RVV tail lanes | 非整齐宽度收益是否保持 |

## 测试、QEMU、反汇编和板卡证据

- correctness: `make -C test-rvv/recognition/color_modality run_test_compare` 通过，Std/RVV 两侧 5/5 成功。
- QEMU smoke: `make -C test-rvv/recognition/color_modality run_bench_rvv BENCH_ARGS="--case-filter production_process_320x240 --iterations 1 --warmup-iterations 1"` 可运行，QEMU 只证明路径和日志形状。
- asm: `make -C test-rvv/recognition/color_modality check_cm_rvv_asm` 通过，匹配 byte load/store 和比较/merge 指令。
- board repeated: `repeated_phase040_quantize_filter_rvv/summary.md` 显示 `2.490x` / `2.410x`，`0/5` 退化。
- Evidence Doctor: `Errors=0, Warnings=0, Suggestions=4`，仅缺环境 metadata 和 binary hash。

## 正确性与高效性证据链

当前证据链覆盖：

- correctness: 公开入口命中 RVV path，forced scalar/RVV 对拍一致。
- performance: 5-run repeated board 正向。
- boundary: 只覆盖 exact `PointXYZRGB` organized 输入和当前计时边界。
- risk: `extractFeatures()` / `computeDistanceMap()` 仍保留标量，后续扩展应另开 phase 或另开 topic。

## 生产接入评估

当前 production patch 已采纳，且长期文档只记录当前 adopted production behavior。后续如果要扩成 generic RGB traits、其它 RGB-like 点型或 feature extraction，必须新建 phase，并重新补 correctness、fallback、asm、board 和 Evidence Doctor。
