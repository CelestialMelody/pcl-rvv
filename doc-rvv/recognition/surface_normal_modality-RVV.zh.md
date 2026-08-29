# surface_normal_modality RVV 生产说明

## 当前状态

`recognition/include/pcl/recognition/surface_normal_modality.h` 的 `processInputData()`
已经接入 production direct（真实生产路径）RVV 分流。当前采用的实现是：

- `computeAndQuantizeSurfaceNormals2()` 作为公开入口。
- `computeAndQuantizeSurfaceNormals2Std()` 保留原标量语义。
- `computeAndQuantizeSurfaceNormals2RVV()` 在 `__RVV10__` 下尝试 RVV 路径，失败时自然回落到 Std。

当前生产行为已经通过板卡重复测试采纳，文档以此作为长期维护事实，不再把它写成诊断原型。

## 函数入口作用

`SurfaceNormalModality<PointInT>::processInputData()` 是 LINEMOD RGB-D 模态的预处理入口。
它先生成 surface normal quantization map，再做 5x5 filter 和 spread，供后续 template matching
消费。当前 RVV 接管 `computeAndQuantizeSurfaceNormals2()`、`filterQuantizedSurfaceNormals()`
和默认 `spreading_size_=8` 下的 surface-normal 专用 spread 子链路；公共
`QuantizedMap::spreadQuantizedMap()` 仍保持原实现，供其它 modality 调用。

## 标量路径

标量路径把每个像素的 `z` 深度转换成毫米深度，使用 5 像素半径的 8 邻域做 bilateral
accumulation，算出 `det/ddx/ddy`，归一化法线，再用 `atan2` 量化方向桶。后续
`filterQuantizedSurfaceNormals()` 做 5x5 histogram filter（直方图滤波），把出现次数最多的
方向桶转换成 bit mask；surface-normal 专用 spread helper 按默认半径 8 把 filter 后的
bit mask 传播到邻域。

## 当前采用的优化方式

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| dispatch / fallback | adopted | `__RVV10__` 下先尝试 RVV helper，失败则回 Std helper | `run_test_compare`、production direct board | 非 RVV 构建和小输入回退标量 |
| 输入布局 | adopted | 当前生产直连证据覆盖 organized `PointXYZRGBA` public overload | `test_snm.cpp`、board summary | 其它模板实例仍需 point-type expansion phase |
| RVV 组织方式 | adopted | RVV 每个 VL chunk 做 8 邻域整数累加；`sqrt/atan2/angle` 仍由标量逐 lane 完成；5x5 filter 由 RVV byte load、bin compare 和 store 接管；默认 spread 8 用两遍 byte OR / store 接管 | asm、board repeated | 公共 `QuantizedMap::spreadQuantizedMap()` 未改 |
| 维护成本 | adopted | 边界窄、fallback 清晰、证据链完整 | Evidence Doctor 0/0/4 | 后续只继续独立 candidate |
| 替代方案 | deferred | 公共 `QuantizedMap::spreadQuantizedMap()` 可另开跨 modality topic | roadmap / matrix | 需要其它 modality 的独立证据 |

## 覆盖范围与 fallback

| 范围 | 当前状态 | 证据 / 原因 |
| --- | --- | --- |
| `PointXYZRGBA` organized public input | adopted | production direct board 使用该公开入口 |
| 非 RVV 构建 | scalar fallback | `__RVV10__` 未启用时只走 Std helper |
| 小规模输入 | scalar fallback | RVV helper 的 width/height gate 失败后回标量 |
| 非默认 spreading size | scalar fallback | 当前 spread RVV 只覆盖默认 `spreading_size_=8` |
| 其它模板实例 | conservative fallback | 当前生产 direct 证据仅覆盖 `PointXYZRGBA` |
| `extractFeatures()` | out of scope | 当前计时边界不包含它 |

## 详细设计

RVV helper 的基本组织方式是：

1. 读取 organized 深度云的 `z` 字段，转换成 `uint16` 毫米深度。
2. 按 VL chunk 处理内部像素，使用 8 个固定邻域偏移做整数累加。
3. 计算 `det/ddx/ddy`，把归一化后的方向写回 orientation map。
4. 5x5 filter 对每个内部像素读取 25 个邻域值，逐 bin 统计出现次数，并按和标量相同的
   strict-greater tie-break（严格大于才更新最大值，平票保留较早 bin）写出 `1u << (bin - 1)`。
5. 默认 spread 8 分两遍执行：横向把 8 个连续 byte 做 OR 写入临时 map，纵向再把 8 行临时值
   OR 到输出 map。非默认 spread size 回退公共标量 helper。

这不是对整个 `processInputData()` 的全面 RVV 化；它覆盖 compute-and-quantize 与 5x5 filter，
以及 surface-normal 默认 spread，但不覆盖公共 spread helper 的其它调用方和 feature extraction。

## Bench case 说明

| case | 入口 | 数据 | 计时边界 | 证明点 |
| --- | --- | --- | --- | --- |
| `production_process_320x240` | `SurfaceNormalModality<PointXYZRGBA>::processInputData()` | organized `PointXYZRGBA` 320x240 | depth-to-normal / quantize、5x5 filter、spread | 常规尺寸 public entry 收益 |
| `production_process_641x481_tail` | `SurfaceNormalModality<PointXYZRGBA>::processInputData()` | organized `PointXYZRGBA` 641x481 | 同上，覆盖 RVV tail lanes | 非整齐宽度收益是否保持 |

## 测试、QEMU、反汇编和板卡证据

| 证据层 | 路径 | 结果 | 能证明什么 |
| --- | --- | --- | --- |
| correctness | `test-rvv/recognition/surface_normal_modality/src/test_snm.cpp` | `run_test_compare` 4/4 | forced scalar / RVV 对拍、fallback 语义 |
| asm | `test-rvv/recognition/surface_normal_modality/Makefile` | `check_snm_rvv_asm`、`check_snm_production_rvv_asm` 通过 | 生产 helper 命中 RVV 指令与调用链 |
| board repeated | `test-rvv/recognition/surface_normal_modality/log/board/repeated_phase030_spread_rvv/summary.md` | median `1.620x` / `1.640x`，`0/5` 退化 | 当前 public RVV path 快于当前 public scalar path |
| Evidence Doctor | `test-rvv/recognition/surface_normal_modality/log/board/repeated_phase030_spread_rvv/evidence_doctor.md` | `Errors=0 / Warnings=0 / Suggestions=4` | 无阻塞异常，但环境 metadata 可补强 |

## 正确性与高效性证据链

当前证据链覆盖：

- correctness：公开入口命中、forced scalar / RVV 对拍、checksum 一致。
- performance：5-run repeated board 的 positive 结果。
- boundary：只覆盖 `PointXYZRGBA` public overload 和当前计时边界。
- risk：公共 spread helper 的其它调用方未覆盖，`extractFeatures()` 未纳入证据边界。

QEMU 只用于正确性和日志形状，不用于性能结论。

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `SurfaceNormalModality::processInputData` | production public entry | 模态预处理入口 | LINEMOD RGB-D 使用方 | quantize / filter / spread | production boundary | `recognition/include/pcl/recognition/surface_normal_modality.h` |
| `computeAndQuantizeSurfaceNormals2Std` | production Std helper | 标量事实来源 | public entry fallback | `quantized_surface_normals_`、orientation map | fallback source of truth | 同上 |
| `computeAndQuantizeSurfaceNormals2RVV` | production RVV helper | 8 邻域 RVV 累加与量化 | public entry RVV dispatch | `quantized_surface_normals_`、orientation map | adopted production RVV path | 同上 |
| `filterQuantizedSurfaceNormalsStd` | production Std helper | 5x5 histogram filter 标量事实来源 | filter fallback | `filtered_quantized_surface_normals_` | fallback source of truth | 同上 |
| `filterQuantizedSurfaceNormalsRVV` | production RVV helper | 5x5 邻域 byte bin 统计和 bit mask 写回 | filter RVV dispatch | spread | adopted production RVV path | 同上 |
| `spreadFilteredQuantizedSurfaceNormalsRVV` | production RVV helper | 默认 spread 8 的 surface-normal 专用两遍 byte OR | spread RVV dispatch | `spreaded_quantized_surface_normals_` | adopted production RVV path | 同上 |
| `test_snm.cpp` | correctness target | forced scalar / RVV 对拍 | `run_test_compare` | gtest assertions | correctness gate | `test-rvv/recognition/surface_normal_modality/src/test_snm.cpp` |
| `bench_snm.cpp` | bench wrapper | production direct board 入口 | repeated board target | summary / doctor / registry | performance input | `test-rvv/recognition/surface_normal_modality/src/bench_snm.cpp` |
| `generate_snm_evidence_manifest.py` | analysis script | manifest 和 summary 生成 | `record_evidence_state_repeated` | evidence registry | evidence metadata | `test-rvv/recognition/surface_normal_modality/script/generate_snm_evidence_manifest.py` |
| Phase 010 result | phase result | 生产接入闭环审计 | phase doc | evaluation / closeout | adoption audit | `test-rvv/recognition/surface_normal_modality/doc/phases/010-production-integration/result.zh.md` |
| Phase 020 result | phase result | filter RVV 生产接入闭环审计 | phase doc | evaluation / closeout | adoption audit | `test-rvv/recognition/surface_normal_modality/doc/phases/020-filter-5x5-rvv/result.zh.md` |
| Phase 030 result | phase result | spread RVV 生产接入闭环审计 | phase doc | evaluation / closeout | adoption audit | `test-rvv/recognition/surface_normal_modality/doc/phases/030-spread-quantized-map-rvv/result.zh.md` |
| production direct summary | evidence output summary | repeated board 摘要 | board repeated target | evaluation / long-term doc | board performance | `test-rvv/recognition/surface_normal_modality/log/board/repeated_phase030_spread_rvv/summary.md` |

## 后续方向

当前 adopted production behavior 已闭合。当前 topic 内不建议继续扩大：公共
`QuantizedMap::spreadQuantizedMap()` 会影响多个 modality，应另开跨 modality topic；`extractFeatures()`
只有在 profile 证明是热点后才值得启动。
