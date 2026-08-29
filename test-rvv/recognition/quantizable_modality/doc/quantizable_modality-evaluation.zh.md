# quantizable_modality 函数级评估

## S2 函数级评估

目标源码是 `recognition/src/quantizable_modality.cpp`。公开 helper `QuantizedMap::spreadQuantizedMap()` 读取一个 `QuantizedMap` byte map，先横向把连续 `spreading_size` 个 byte 做 bitwise OR（按位或），写入临时 map；再纵向跨 `width` 步长读取临时 map 的 `spreading_size` 行并做 OR，写出 spreaded map。

该 helper 被多个 recognition modality 调用：`ColorModality::processInputData()`、`ColorGradientModality::processInputData()` / `processInputDataFromFiltered()` 和 `SurfaceNormalModality::spreadFilteredQuantizedSurfaceNormalsStd()`。因此它是共享 production helper，不是某个 caller 的私有局部片段。

## 当前判断

当前判断为 adopted production behavior（已采纳生产行为）。`recognition/src/quantizable_modality.cpp` 已接入 RVV / scalar 分流：`__RVV10__` 构建中默认 spread 8 且尺寸足够时尝试 `spreadQuantizedMapRVV()`，否则走 `spreadQuantizedMapStd()`。

板卡 repeated summary 显示：

- `shared_spread_320x240`: median speedup `4.670x`，range `4.510x` - `5.070x`，`B/A < 1` 为 `0/5`，checksum 一致。
- `shared_spread_641x481_tail`: median speedup `4.770x`，range `4.340x` - `4.790x`，`B/A < 1` 为 `0/5`，checksum 一致。

Evidence Doctor（证据体检）结果为 `Errors=0 / Warnings=0 / Suggestions=4`。Suggestion 只要求后续补 taskset、governor、freq、temperature 和 binary hash，不阻塞当前采纳。

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `QuantizedMap::spreadQuantizedMap` | production helper | 公共 byte-map spread helper，负责两遍 OR window | color、color-gradient、surface-normal modality | `spreadQuantizedMapRVV()` 或 `spreadQuantizedMapStd()` | adopted production boundary | `recognition/src/quantizable_modality.cpp` |
| `spreadQuantizedMapStd` | production Std helper | 保留原标量两遍 OR 语义 | public helper fallback | output `QuantizedMap` | fallback source of truth | `recognition/src/quantizable_modality.cpp` |
| `spreadQuantizedMapRVV` | production RVV helper | 默认 spread 8 的横向 / 纵向 byte OR RVV 实现 | public helper RVV dispatch | output `QuantizedMap` | adopted production RVV path | `recognition/src/quantizable_modality.cpp` |
| test hook | test-only gate | 强制标量或记录 RVV/Scalar path | `test_qm.cpp` | gtest assertions | correctness gate（正确性验收） | `recognition/src/quantizable_modality.cpp` |
| `qm.h` | test support aggregator | 构造 byte map、checksum 和 path label | `test_qm.cpp`、`bench_qm.cpp` | correctness / bench | test support | `test-rvv/recognition/quantizable_modality/include/qm.h` |
| `test_qm.cpp` | correctness target | forced scalar / RVV 对拍和 fallback 验证 | `run_test_compare` | gtest assertions | correctness gate | `test-rvv/recognition/quantizable_modality/src/test_qm.cpp` |
| `bench_qm.cpp` | bench wrapper | 生成 `shared_spread_*` board 输出 | board repeated target | summary / manifest | performance input | `test-rvv/recognition/quantizable_modality/src/bench_qm.cpp` |
| `generate_qm_evidence_manifest.py` | analysis script | 生成 repeated board summary 和 manifest | `record_evidence_state_repeated` | Evidence Doctor / registry | evidence metadata | `test-rvv/recognition/quantizable_modality/script/generate_qm_evidence_manifest.py` |
| repeated board summary | evidence output summary | 当前板卡性能摘要 | board repeated target | evaluation / doc-rvv / Handoff | board performance | `test-rvv/recognition/quantizable_modality/log/board/repeated_phase000_shared_spread_rvv/summary.md` |
| Phase 000 result | phase result | 阶段计划、证据解释和停止决定 | phase loop | evaluation / Handoff | adoption audit | `test-rvv/recognition/quantizable_modality/doc/phases/000-shared-spread-rvv/result.zh.md` |

## Evidence Registry 证据登记

这些路径是 `log/evidence_registry.json` 登记的 summary artifact（摘要证据产物）。当前 production truth（生产事实）以 `qm_phase000_shared_spread_rvv_repeated` 为主证据。

| phase | evidence role | run label | registered artifacts |
| --- | --- | --- | --- |
| Phase 000 | production_detail | `qm_phase000_shared_spread_rvv_repeated` | `test-rvv/recognition/quantizable_modality/log/board/repeated_phase000_shared_spread_rvv/summary.md`<br>`test-rvv/recognition/quantizable_modality/log/board/repeated_phase000_shared_spread_rvv/evidence_manifest.json`<br>`test-rvv/recognition/quantizable_modality/log/board/repeated_phase000_shared_spread_rvv/evidence_doctor.md`<br>`test-rvv/recognition/quantizable_modality/log/board/repeated_phase000_shared_spread_rvv/evidence_doctor.json` |

Registry doc refs（登记文档引用）：`doc/phases/000-shared-spread-rvv/result.zh.md`、`doc/quantizable_modality-evaluation.zh.md`。

## 生产接入判断

当前已经接入 production。采纳边界是公共 helper 本身：

- RVV 构建：默认 `spreading_size == 8` 且 `width > 9`、`height > 9` 时走 RVV。
- fallback（回退路径）：非 RVV 构建、非默认 spread、小尺寸输入或 test hook 强制标量时走 `spreadQuantizedMapStd()`。
- public API（公开接口）：没有新增或修改公开 API。
- caller 边界：不把 helper 的 `4.670x` / `4.770x` 直接写成完整 caller 的端到端收益。

## Doc Suite Role Inventory

| role | current shape scan | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- |
| topic_navigation | `README.zh.md` 已给出状态、命令、文档入口和证据提交边界 | adopted | 当前 topic 较小，但有 production helper 和 board evidence，需要独立导航 | closeout |
| testing_overview | 测试入口、QEMU / board 边界已合并进 README、evaluation 和 phase docs | merged:`README.zh.md#常用命令` | target 数量少，独立 testing-overview 暂无必要 | closeout |
| correctness_tests | gtest 语义写在 test 文件注释和 phase result | merged:`doc/phases/000-shared-spread-rvv/result.zh.md#计划与实际` | `run_test_compare` 覆盖 path-hit、tail 和 fallback | closeout |
| benchmark_and_evidence | bench case、summary、manifest、doctor、registry 已在 evaluation / phase result 中闭合 | merged:`doc/phases/000-shared-spread-rvv/result.zh.md#证据解释` | board repeated 和 Evidence Doctor 已完成 | closeout |
| optimization_evidence | adopted / deferred / separate-topic 候选归属在 roadmap 和 matrix 中写明 | merged:`doc/phases/optimization-matrix.zh.md` | 当前 topic 内无未阻塞候选 | closeout |
| optimization_roadmap | 搜索空间与恢复条件已收束 | standalone:`doc/optimization-roadmap.zh.md` | 只保留 adopted、deferred 和 separate-topic 路线 | closeout |
| test_support_code_map | 代码、bench、script 与 production helper 由 Traceability Map 覆盖 | merged:`doc/quantizable_modality-evaluation.zh.md#Traceability Map` | 支撑文件规模小，无需拆分 `include/impl` | closeout |
| phase_index / phase_plan / phase_result / optimization_matrix | phase suite 已覆盖 Phase 000 与矩阵 | standalone:`doc/phases/README.zh.md` | phase loop 已闭合 | closeout |
| evaluation_production | 当前 production patch、fallback、board repeated、EvidenceDecision 已闭合 | standalone:`doc/quantizable_modality-evaluation.zh.md` | adopted production behavior 适用 | closeout |
| production_topic_doc | 当前生产事实同步到 `doc-rvv/recognition/quantizable_modality-RVV.zh.md` | standalone:`doc-rvv/recognition/quantizable_modality-RVV.zh.md` | adopted production behavior 适用 | closeout |

## 结论

`shared-spread-rvv-2pass` 已达到当前用户授权下的采纳门槛。当前 topic 内建议停止：非默认 spread 缺少热点证据，caller 端到端收益需要另开 public-entry topic，`getSubMap()` 属于另一个容器 helper。
