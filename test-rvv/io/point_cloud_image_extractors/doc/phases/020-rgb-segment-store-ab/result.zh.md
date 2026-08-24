# Phase 020 Result: rgb-segment-store-ab

## 实际执行范围

本阶段新增 `rgb_segment_store_v1`，只在测试专用 helper 中比较 RGB/RGBA 输出写回形态。
它复用 `vlse32` 读取 `rgb` / `rgba` 字段，用 RVV narrowing（向量收窄）得到 `uint8`
通道后，用 `vsseg3e8` segment store（三通道交错向量写回）生成 `rgb8` 输出。
production 源码未修改。

## TDD 回填

| step | status | 证据 |
| --- | --- | --- |
| RED | done | 新增 `RgbSegmentStoreCandidateMatchesScalarReference` 后，`make run_test_rvv` 编译失败于缺少 `extractRgbSegmentStoreCandidate`。 |
| GREEN | done | 实现 test-only helper 后，`make run_test_compare` 通过，Std/RVV 各 5 个 gtest pass。 |
| mutation check | done | 若 RGB 通道顺序、tail 或 `rgb`/`rgba` 字段 offset 错误，新测试会与真实标量 extractor 输出不一致。 |

## 执行命令和证据路径

| 证据层 | 命令 | 路径 / 结果 |
| --- | --- | --- |
| correctness | `make run_test_compare` | `log/qemu/run_test_std.log`, `log/qemu/run_test_rvv.log`; Std/RVV 各 5 tests pass |
| QEMU smoke | `make run_bench_rvv BENCH_ARGS='--iterations 1 --warmup-iterations 1 --case-filter rgb_segment_store_pointxyzrgb_640x480'` | 新 label 可运行，checksum 可输出；QEMU timing 不作性能结论 |
| asm | `make dump_bench_rvv && rg -n "vsseg3e8\|vsseg3" build/asm/riscv/bench_pcie_rvv.asm` | `build/asm/riscv/bench_pcie_rvv.asm` 可见 `vsseg3e8.v` |
| board repeated | `make collect_board_repeated BENCH_ARGS='--iterations 20 --warmup-iterations 3 --case-filter all' PCIE_REPEATED_RUNS=5 PCIE_REPEATED_DIR=log/board/repeated_phase020` | `log/board/repeated_phase020/summary.md` |
| Evidence Doctor | `make run_board_repeated_evidence_doctor PCIE_REPEATED_DIR=log/board/repeated_phase020` | `log/board/repeated_phase020/evidence_doctor.md`; `Errors=1, Warnings=0, Suggestions=0` |

## 板卡结果

| case | runs | median | min | max | decision bucket |
| --- | ---: | ---: | ---: | ---: | --- |
| `rgb_segment_store_pointxyzrgb_640x480` | 5 | 1.74x | 1.73x | 1.77x | positive |
| `rgb_segment_store_pointxyzrgba_640x480` | 5 | 1.75x | 1.74x | 1.78x | positive |
| `rgb_unpack_pointxyzrgb_640x480` | 5 | 1.29x | 1.27x | 1.30x | positive |
| `rgb_unpack_pointxyzrgba_640x480` | 5 | 1.31x | 1.30x | 1.35x | positive |
| `scaling_full_range_reduction_intensity_640x480` | 5 | 1.56x | 1.54x | 1.57x | positive |
| `scaling_full_range_intensity_640x480` | 5 | 0.92x | 0.92x | 0.94x | negative |

`rgb_segment_store_v1` 在同一批 repeated board 中明显强于 `rgb_u32_stride_unpack_v0`，因此 RGB
production probe（生产探针）优先选择 v1。这个结论只来自板卡 repeated，不使用 QEMU timing。

## Evidence Doctor 解释

Evidence Doctor 仍报告 `Errors=1`，唯一 Error 指向旧 label
`scaling_full_range_intensity_640x480`，也就是 Phase 000 的 `scaling_float_stride_v0`。
处理动作：

- 保留该 Error，用来证明 v0 full-range scaling 仍应拒绝。
- `rgb_segment_store_pointxyzrgb_640x480` 和 `rgb_segment_store_pointxyzrgba_640x480`
  没有退化 finding。
- 当前 evidence role 仍是 `production_shaped_diagnostic`，不能写成 production evidence。

## Diagnostic 到 production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | `production_shaped_diagnostic` |
| A/B boundary | `test_helper`；Std/RVV 都用 bench helper，不是真实 public extractor dispatch。 |
| 当前决策问题 | implementation-shape 和 RVV-vs-scalar 筛选。 |
| diagnostic 是否可外推到 production | 只能外推为 bounded production candidate。production 仍需 PI1-PI5。 |
| comparison-boundary / baseline mismatch 风险 | 存在。真实 production 有 extractor 对象、field lookup、`PCLImage` 赋值和 fallback gate。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | v1 为 positive，可进入 PI1；若生产接入成本过高，仍可回退到 v0 或不接 RGB。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 需要。PI2 后若接入 v1，需要 production boundary 内复核。 |

## Matrix 更新

- `rgb_segment_store_v1`：`planned -> partial-production-candidate within diagnostic boundary`。
- `rgb_u32_stride_unpack_v0`：保持 positive，但作为 v1 的 fallback implementation family。
- `scaling_reduction_v1`：保持 partial-production-candidate within diagnostic boundary。
- `scaling_float_stride_v0` full-range：保持 rejected。

## Continue / Stop Decision

`continue_stop_decision=turn_stop_deferred with stop_condition_hit`。

停止条件是：当前 roadmap 的下一高优先级动作是 PI1 production integration plan（生产接入计划）。
PI1 之后的 PI2 会修改 `io/include/pcl/io/impl/point_cloud_image_extractors.hpp`，需要用户确认生产接入范围。

`next_phase_default=PI1-production-integration-plan`，候选范围为：

- `rgb_segment_store_v1`：`PointXYZRGB` / `PointXYZRGBA` RGB/RGBA unpack 的优先 production probe。
- `scaling_reduction_v1`：`PointXYZI::intensity` full-range scaling 的优先 production probe。
