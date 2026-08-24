# Phase 060 Plan: normal-field diagnostic

## 阶段意图和边界

本阶段评估 `PointCloudImageExtractorFromNormalField<PointT>::extractImpl` 的 normal field
（法线字段）路径。标量 production 语义是读取 `normal_x`、`normal_y`、`normal_z` 三个
float 字段，分别计算 `(value + 1.0) * 127` 并转成 `unsigned char`，写入 `rgb8`。

本阶段只新增 test-only diagnostic（测试专用诊断）helper、correctness（正确性）测试和
bench label（性能测试标签）。它不修改 production header，也不改变 Phase 040/050 冻结的
PI2 production probe（生产探针）范围。

## 当前状态清单

| item | state | evidence |
| --- | --- | --- |
| production source shape | scalar only | `io/include/pcl/io/impl/point_cloud_image_extractors.hpp` 中 normal extractor 为逐点标量循环。 |
| topic harness | available | `src/test_pcie.cpp`、`src/bench_pcie.cpp` 和 `script/generate_pcie_evidence_manifest.py` 已覆盖 RGB/scaling 诊断。 |
| prior stop condition | still active for PI2 | production patch 仍需要用户明确确认；本阶段绕开 production，只推进 roadmap 中的 normal diagnostic。 |

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production-shaped diagnostic`，因为标量参考调用真实 normal extractor，但 RVV candidate 仍是 test helper。 |
| A/B boundary | `test helper`。Std build 走标量参考，RVV build 走测试专用 candidate。 |
| 当前决策问题 | `RVV-vs-scalar` 的诊断筛选，不是 production adoption。 |
| diagnostic 是否可外推到 production | unknown。它能证明 normal field 数据流有无候选价值，但不能证明真实 public dispatch、fallback 或 NaN post-pass。 |
| comparison-boundary / baseline mismatch 风险 | yes。candidate 直接写 `std::vector<uint8_t>`，不覆盖真实 `PCLImage` metadata 和 base `extract` post-pass。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no for current PI2 scope。若诊断不正向，normal path 不进入本 topic 已冻结的 PI2 scope；但该 diagnostic 不能全局拒绝未来单独授权的 bounded production probe。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes。即使诊断正向，也必须另开 PI1/PI2 范围冻结和 production direct 证据。 |

## 实现和测试动作

| action | artifact | expected evidence | completion |
| --- | --- | --- | --- |
| RED normal correctness test | `src/test_pcie.cpp` | `make run_test_rvv` 因缺少 normal helper 编译失败 | fail for expected missing helper |
| GREEN normal diagnostic helper | `include/impl/pcie_support.hpp` | Std/RVV 输出与真实 normal extractor 一致 | `make run_test_compare` passes |
| bench label and manifest metadata | `src/bench_pcie.cpp`, `script/generate_pcie_evidence_manifest.py` | label 可被 QEMU smoke、board repeated 和 Evidence Doctor 解析 | QEMU smoke compiles/runs for the label |
| docs and matrix update | topic-local docs | normal candidate 的证据边界可恢复 | phase result and roadmap/matrix updated |

## 优化矩阵

| candidate family | row source policy | point type / layout | correctness target | bench target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `normal_float_stride_v0` | organized cloud order | `PointNormal` normal_x/y/z float fields | `NormalFieldCandidateMatchesScalarReference` | `normal_field_pointnormal_640x480` | planned if correctness and QEMU smoke pass | `vlse32.v` in bench binary | planned with repeated board manifest | planned |

## 板卡复跑预算和决策桶

如果 correctness、QEMU smoke 和 manifest metadata 闭合，板卡可用时运行
`make collect_board_repeated PCIE_REPEATED_DIR=log/board/repeated_phase060 PCIE_REPEATED_RUNS=5`
并生成 Evidence Doctor。decision bucket 使用既有 topic 口径：median speedup 明显大于 1.0 且退化频率为 0
记为 positive；小幅正向记为 weak-positive；median 小于 1.0 或退化频率高记为 negative；方向摇摆记为 unstable。

## Continue / Stop 条件

本阶段可以关闭 `normal_float_stride_v0` 的诊断矩阵条目。若 normal 诊断 positive，只能进入
`partial-production-candidate within diagnostic boundary`，后续生产接入仍需新的 PI1 范围冻结和用户确认。
若 negative / weak / unstable，则记录不建议纳入当前 PI2 production patch，继续评估 roadmap 中的
`label_mono16` 或保持默认 PI2 等待确认。
