# Phase 070 Plan: label-mono16 diagnostic

## 阶段意图和边界

本阶段评估 `PointCloudImageExtractorFromLabelField<PointT>::extractImpl` 中
`COLORS_MONO` 分支的 label mono16（标签转 16 位灰度）路径。标量 production 语义是：
查找 `"label"` 字段，设置 `img.encoding = "mono16"`，逐点读取 `std::uint32_t` label，
再用 `static_cast<unsigned short>(val)` 写入输出。

本阶段只新增 test-only diagnostic（测试专用诊断）helper、correctness（正确性）测试、
bench label（性能测试标签）和证据 metadata。它不修改 production header，不覆盖
`COLORS_RGB_RANDOM` 或 `COLORS_RGB_GLASBEY`，也不改变 Phase 040/050 冻结的 PI2 production
probe（生产探针）范围。

## 当前状态清单

| item | state | evidence |
| --- | --- | --- |
| production source shape | scalar only for label extractor | `io/include/pcl/io/impl/point_cloud_image_extractors.hpp` 中 `COLORS_MONO` 分支为逐点标量循环。 |
| topic harness | available | `src/test_pcie.cpp`、`src/bench_pcie.cpp` 和 `script/generate_pcie_evidence_manifest.py` 已覆盖 RGB/scaling/normal 诊断。 |
| prior label scope | deferred | Roadmap 仅保留 label mono16，random / Glasbey 因 map/set 和 LUT 状态暂缓。 |
| prior stop condition | still active for PI2 | production patch 仍需要用户明确确认；本阶段绕开 production。 |

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production_shaped_diagnostic`，因为标量参考调用真实 label extractor 的 mono mode，但 RVV candidate 仍是 test helper。 |
| A/B boundary | `test_helper`。Std build 走标量参考，RVV build 走测试专用 candidate。 |
| 当前决策问题 | `RVV-vs-scalar` 的诊断筛选，不是 production adoption。 |
| diagnostic 是否可外推到 production | unknown。它能证明 mono16 label 数据流有无候选价值，但不能证明真实 public dispatch、fallback、RGB color modes 或 base `extract` post-pass。 |
| comparison-boundary / baseline mismatch 风险 | yes。candidate 直接写 `std::vector<uint16_t>`，不覆盖真实 `PCLImage` metadata、color-mode switch 和所有 fallback。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前负向只能关闭本 diagnostic family；不能全局拒绝未来有边界的 production probe。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes。即使诊断正向，也必须另开 PI1/PI2 范围冻结和 production direct 证据。 |

## 实现和测试动作

| action | artifact | expected evidence | completion |
| --- | --- | --- | --- |
| RED label correctness test | `src/test_pcie.cpp` | `make run_test_rvv` 因缺少 label helper 编译失败 | fail for expected missing helper |
| GREEN label diagnostic helper | `include/impl/pcie_support.hpp` | Std/RVV 输出与真实 label mono extractor 一致 | `make run_test_compare` passes |
| bench label and manifest metadata | `src/bench_pcie.cpp`, `script/generate_pcie_evidence_manifest.py` | label 可被 QEMU smoke、board repeated 和 Evidence Doctor 解析 | QEMU smoke compiles/runs for the label |
| docs and matrix update | topic-local docs | label candidate 的证据边界可恢复 | phase result and roadmap/matrix updated |

## 优化矩阵

| candidate family | row source policy | point type / layout | correctness target | bench target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `label_mono16_stride_v0` | organized cloud order | `PointXYZL::label` uint32 field | `LabelMono16CandidateMatchesScalarReference` | `label_mono16_pointxyzl_640x480` | planned if correctness and QEMU smoke pass | `vlse32.v` plus narrowing/store in bench binary | planned with repeated board manifest | planned |

## Phase Scope 与扩展队列

- `validated_scope`：exact `PointXYZL`、organized cloud order、`COLORS_MONO`、`uint32_t label`
  到 `uint16_t` 输出、synthetic 640x480 bench。
- `unvalidated_scope`：random RGB、Glasbey LUT、其它 label-like 点型、泛型字段 offset production gate、
  NaN post-pass 与真实 production dispatch。
- `point_type_expansion_queue`：若 label mono16 诊断正向且用户后续授权 production probe，PI1 必须重新
  冻结 exact point type 或 traits gate、fallback matrix、production direct correctness、asm、board
  repeated 和 Evidence Doctor。
- `phase_closeout_boundary`：本阶段只能关闭 `label_mono16_stride_v0` 的诊断矩阵条目。

## 板卡复跑预算和决策桶

如果 correctness、QEMU smoke 和 manifest metadata 闭合，板卡可用时运行：

```bash
make collect_board_repeated PCIE_REPEATED_DIR=log/board/repeated_phase070 PCIE_REPEATED_RUNS=5 BENCH_ARGS='--iterations 20 --warmup-iterations 3 --case-filter label_mono16_pointxyzl_640x480'
make run_board_repeated_evidence_doctor PCIE_REPEATED_DIR=log/board/repeated_phase070
```

decision bucket 使用既有 topic 口径：median speedup 明显大于 1.0 且退化频率为 0 记为 positive；
小幅正向记为 weak-positive；median 小于 1.0 或退化频率高记为 negative；方向摇摆记为 unstable。
预算为 5-run；若 bucket 稳定则关闭该证据动作，若仍摇摆则降级为 unstable 并记录风险。

## Continue / Stop 条件

本阶段可以关闭 `label_mono16_stride_v0` 的诊断矩阵条目。若 positive，只能进入
partial-production-candidate within diagnostic boundary；后续生产接入仍需要新的 PI1 范围冻结和
用户确认。若 negative / weak / unstable，则记录不建议纳入当前 PI2 production patch。

`next_phase_default` 暂定为：070 完成后若没有新的 topic-local 未阻塞候选，回到
`PI2-production-patch after explicit user confirmation` 或 `ready_for_review_validity_check`。
