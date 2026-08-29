# surface_normal_modality 函数级评估

## S2 函数级评估

目标源码是 `recognition/include/pcl/recognition/surface_normal_modality.h`。公开入口
`SurfaceNormalModality<PointInT>::processInputData()` 先调用
`computeAndQuantizeSurfaceNormals2()` 从 organized depth cloud（有宽高的深度点云）生成
`quantized_surface_normals_` 和 orientation map（方向图），再调用
`filterQuantizedSurfaceNormals()` 做 5x5 histogram filter（直方图滤波），最后通过
`QuantizedMap::spreadQuantizedMap()` 生成 LINEMOD template matching（模板匹配）后续使用的
spread map。

首阶段可 RVV 化的主片段是 `computeAndQuantizeSurfaceNormals2()`：它把每个
`PointInT::z` 转换成毫米 `uint16_t` 深度，内区像素使用 5 像素半径的 8 邻域做
bilateral accumulation（双边累加），再计算 `det/ddx/ddy`、归一化法线，并用
`atan2` 把方向量化成 8 个 bin（方向桶）。

## 当前判断

当前判断已经从 `diagnostic / in_progress` 推进到 `adopted production direct`。
`surface_normal_modality.h` 已接入 RVV / scalar 分流，真实公开入口
`SurfaceNormalModality<PointXYZRGBA>::processInputData()` 的板卡重复测试显示正向收益，
且用户已授权接入后板卡有收益即可采纳。当前 adopted production behavior（已采纳生产行为）
覆盖 `computeAndQuantizeSurfaceNormals2()`、`filterQuantizedSurfaceNormals()` 和默认
`spreading_size_=8` 的 surface-normal 专用 spread；公共 `QuantizedMap::spreadQuantizedMap()`
仍保持原实现，供其它 modality 调用。

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `SurfaceNormalModality::processInputData` | production public entry | 完整 surface normal modality 预处理入口 | LINEMOD RGB-D template / detection 输入准备 | depth quantize、filter、spread | production boundary, adopted | `recognition/include/pcl/recognition/surface_normal_modality.h` |
| `computeAndQuantizeSurfaceNormals2Std` | production Std helper | 原标量事实来源 | `processInputData()` fallback | `quantized_surface_normals_`、orientation map | fallback source of truth | `recognition/include/pcl/recognition/surface_normal_modality.h` |
| `computeAndQuantizeSurfaceNormals2RVV` | production RVV helper | 深度转毫米、8 邻域累加和角度量化 | `processInputData()` RVV dispatch | `quantized_surface_normals_`、orientation map | adopted production RVV path | `recognition/include/pcl/recognition/surface_normal_modality.h` |
| `filterQuantizedSurfaceNormalsStd` | production Std helper | 5x5 histogram filter 的标量事实来源 | `filterQuantizedSurfaceNormals()` fallback | `filtered_quantized_surface_normals_` | fallback source of truth | `recognition/include/pcl/recognition/surface_normal_modality.h` |
| `filterQuantizedSurfaceNormalsRVV` | production RVV helper | 按 VL chunk 统计 5x5 邻域 bin 并写出 bit mask | `filterQuantizedSurfaceNormals()` RVV dispatch | `filtered_quantized_surface_normals_`、spread | adopted production RVV path | `recognition/include/pcl/recognition/surface_normal_modality.h` |
| `spreadFilteredQuantizedSurfaceNormalsRVV` | production RVV helper | surface-normal 专用两遍 spread，默认 spread 8 下用 byte OR 生成 spreaded map | `processInputData()` RVV dispatch | `spreaded_quantized_surface_normals_` | adopted production RVV path | `recognition/include/pcl/recognition/surface_normal_modality.h` |
| `test_snm.cpp` | correctness target | forced scalar / RVV 对拍、fallback 验证 | `run_test_compare` | gtest assertions | correctness gate | `test-rvv/recognition/surface_normal_modality/src/test_snm.cpp` |
| `bench_snm.cpp` | bench wrapper | 生产直连 case、checksum 和 board 输出 | board repeated target | summary / manifest | performance input | `test-rvv/recognition/surface_normal_modality/src/bench_snm.cpp` |
| `generate_snm_evidence_manifest.py` | analysis script | 生成 board summary、Evidence Doctor manifest | `record_evidence_state_repeated` | evidence registry | evidence metadata | `test-rvv/recognition/surface_normal_modality/script/generate_snm_evidence_manifest.py` |
| Phase 030 production direct summary | evidence output summary | 当前生产直连板卡摘要 | board repeated target | evaluation / long-term doc | board performance | `test-rvv/recognition/surface_normal_modality/log/board/repeated_phase030_spread_rvv/summary.md` |
| Phase 010 result | phase result | 生产接入闭环审计 | current phase | evaluation / long-term doc | adoption audit | `test-rvv/recognition/surface_normal_modality/doc/phases/010-production-integration/result.zh.md` |
| Phase 020 result | phase result | filter RVV 生产接入闭环审计 | current phase | evaluation / long-term doc | adoption audit | `test-rvv/recognition/surface_normal_modality/doc/phases/020-filter-5x5-rvv/result.zh.md` |
| Phase 030 result | phase result | spread RVV 生产接入闭环审计 | current phase | evaluation / long-term doc | adoption audit | `test-rvv/recognition/surface_normal_modality/doc/phases/030-spread-quantized-map-rvv/result.zh.md` |

## Evidence Registry 证据登记

这些路径是 `log/evidence_registry.json` 登记的 summary artifact（摘要证据产物）。Phase 030
是当前 production direct（真实生产路径证据）结论的主证据；Phase 000、Phase 010 和 Phase 020
保留为 historical evidence（历史证据），用于审计候选从诊断、初次生产接入、filter RVV 到当前
spread RVV 的证据变化。

| phase | evidence role | run label | registered artifacts |
| --- | --- | --- | --- |
| Phase 000 | production-shaped diagnostic（生产形态诊断） | `snm_phase000_depth_quantize_repeated` | `test-rvv/recognition/surface_normal_modality/log/board/repeated_phase000_depth_quantize/summary.md`<br>`test-rvv/recognition/surface_normal_modality/log/board/repeated_phase000_depth_quantize/evidence_manifest.json`<br>`test-rvv/recognition/surface_normal_modality/log/board/repeated_phase000_depth_quantize/evidence_doctor.md`<br>`test-rvv/recognition/surface_normal_modality/log/board/repeated_phase000_depth_quantize/evidence_doctor.json` |
| Phase 010 | production direct | `snm_phase010_production_direct_repeated` | `test-rvv/recognition/surface_normal_modality/log/board/repeated_phase010_production_direct/summary.md`<br>`test-rvv/recognition/surface_normal_modality/log/board/repeated_phase010_production_direct/evidence_manifest.json`<br>`test-rvv/recognition/surface_normal_modality/log/board/repeated_phase010_production_direct/evidence_doctor.md`<br>`test-rvv/recognition/surface_normal_modality/log/board/repeated_phase010_production_direct/evidence_doctor.json` |
| Phase 020 | production direct | `snm_phase020_filter_5x5_repeated` | `test-rvv/recognition/surface_normal_modality/log/board/repeated_phase020_filter_5x5/summary.md`<br>`test-rvv/recognition/surface_normal_modality/log/board/repeated_phase020_filter_5x5/evidence_manifest.json`<br>`test-rvv/recognition/surface_normal_modality/log/board/repeated_phase020_filter_5x5/evidence_doctor.md`<br>`test-rvv/recognition/surface_normal_modality/log/board/repeated_phase020_filter_5x5/evidence_doctor.json` |
| Phase 030 | production direct | `snm_phase030_spread_rvv_repeated` | `test-rvv/recognition/surface_normal_modality/log/board/repeated_phase030_spread_rvv/summary.md`<br>`test-rvv/recognition/surface_normal_modality/log/board/repeated_phase030_spread_rvv/evidence_manifest.json`<br>`test-rvv/recognition/surface_normal_modality/log/board/repeated_phase030_spread_rvv/evidence_doctor.md`<br>`test-rvv/recognition/surface_normal_modality/log/board/repeated_phase030_spread_rvv/evidence_doctor.json` |

## 生产接入判断

当前已经接入 production。当前公开入口的 Phase 030 production direct repeated board 结果为
positive，checksum 一致，Evidence Doctor 无 Error / Warning。两个 production case 的 median
speedup 是 `1.620x` / `1.640x`，`B/A < 1 = 0/5`。仍需保留的边界是：

- 公共 `QuantizedMap::spreadQuantizedMap()` 仍是既有实现，未对其它 modality 扩大生产行为；
- `extractFeatures()` 未纳入当前计时边界；
- 环境 metadata 和 binary identity 仍是 Suggestions，而不是阻塞项。

## 结论

`computeAndQuantizeSurfaceNormals2()`、`filterQuantizedSurfaceNormals()` 和默认 spread 的 RVV
接入已经达到当前用户授权下的采纳门槛。当前 topic 内建议停止：继续公共
`QuantizedMap::spreadQuantizedMap()` 会影响多个 modality，应另开跨 modality topic；`extractFeatures()`
需 profile 证明是热点后再启动。

## Doc Suite Role Inventory

| role | current shape scan | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- |
| topic_navigation | `README.zh.md` 已给出状态、常用命令、文档入口和 board summary | adopted | 入口和结论可直接定位 | closeout |
| testing_overview | 测试总览与证据边界已合并进 `README.zh.md`、evaluation 和 phase docs | merged:`README.zh.md#当前状态` | QEMU / board / production direct 边界已写明 | closeout |
| correctness_tests | 正确性对拍、fallback 和 test hook 说明已合并进 `doc/phases/*` 与 `README.zh.md` | merged:`doc/phases/030-spread-quantized-map-rvv/result.zh.md#计划与实际` | `run_test_compare`、`run_test_rvv` 证据已在 phase/result 中归档 | closeout |
| benchmark_and_evidence | board repeated、checksum、manifest、doctor、registry 已在 evaluation / phase docs 中闭合 | merged:`doc/phases/030-spread-quantized-map-rvv/result.zh.md#证据解释` | 当前证据路径已登记且 freshness 通过 | closeout |
| optimization_evidence | adopted / deferred / separate-topic 的候选归属已在 roadmap、matrix 和 phase result 中写明 | merged:`doc/phases/optimization-matrix.zh.md#optimization-matrix` | 当前 topic 内无未阻塞候选 | closeout |
| optimization_roadmap | 搜索空间与恢复条件已收束到 closeout / separate-topic | standalone:`doc/optimization-roadmap.zh.md` | 只保留 adopted 和 deferred 路线 | closeout |
| test_support_code_map | 测试支撑代码、bench harness 和 script 已由 README、evaluation 和 phase docs 交叉索引 | merged:`README.zh.md#文档入口` | `include/`、`src/`、`script/` 的职责可追踪 | closeout |
| phase_index / phase_plan / phase_result / optimization_matrix | phase suite 已完整覆盖 000/010/020/030 与矩阵 | standalone:`doc/phases/README.zh.md` | phase loop 已闭合 | closeout |
| evaluation_production | 当前生产 patch、fallback、board repeated、EvidenceDecision 已在本评估中闭合 | standalone:`doc/surface_normal_modality-evaluation.zh.md` | adopted production behavior 已确认 | closeout |
| production_topic_doc | 当前生产事实已同步到 `doc-rvv/recognition/surface_normal_modality-RVV.zh.md` | standalone:`doc-rvv/recognition/surface_normal_modality-RVV.zh.md` | adopted production behavior 适用 | closeout |
