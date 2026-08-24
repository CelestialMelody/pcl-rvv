# Phase 010 Result: scaling-full-range-reduction-ab

## 实际执行范围

本阶段在测试专用 helper 中新增 `scaling_reduction_v1`。它只改变 full-range scaling
诊断候选的第一遍 min/max：用 RVV vector reduction（向量规约）在寄存器中完成
`vfredmin` / `vfredmax`，第二遍写 `mono16` 仍沿用测试支撑中的向量 float 计算和逐 lane
`uint16_t` 写回口径。production 源码未修改。

## TDD 回填

| step | status | 证据 |
| --- | --- | --- |
| RED | done | 新增 `FullRangeReductionCandidateMatchesScalarReference` 后，`make run_test_rvv` 编译失败于缺少 `extractScalingFullRangeReductionCandidate`。 |
| GREEN | done | 实现 test-only RVV helper 后，`make run_test_compare` 通过，Std/RVV 各 4 个 gtest pass。 |
| mutation check | done | 若 min/max 不经过新 helper、尾段未参与规约，或输出与标量 full-range 不一致，新测试会失败。 |

## 执行命令和证据路径

| 证据层 | 命令 | 路径 / 结果 |
| --- | --- | --- |
| correctness | `make run_test_compare` | `log/qemu/run_test_std.log`, `log/qemu/run_test_rvv.log`; Std/RVV 各 4 tests pass |
| QEMU smoke | `make run_bench_rvv BENCH_ARGS='--iterations 1 --warmup-iterations 1 --case-filter scaling_full_range_reduction_intensity_640x480'` | 新 label 可运行，checksum 可输出；QEMU timing 不作性能结论 |
| asm | `make dump_bench_rvv && rg -n "vfred" build/asm/riscv/bench_pcie_rvv.asm` | `build/asm/riscv/bench_pcie_rvv.asm` 可见 `vfredmin.vs` / `vfredmax.vs` |
| board repeated | `make collect_board_repeated BENCH_ARGS='--iterations 20 --warmup-iterations 3 --case-filter all' PCIE_REPEATED_RUNS=5 PCIE_REPEATED_DIR=log/board/repeated_phase010` | `log/board/repeated_phase010/summary.md` |
| Evidence Doctor | `make run_board_repeated_evidence_doctor PCIE_REPEATED_DIR=log/board/repeated_phase010` | `log/board/repeated_phase010/evidence_doctor.md`; `Errors=1, Warnings=0, Suggestions=0` |

## 板卡结果

| case | runs | median | min | max | decision bucket |
| --- | ---: | ---: | ---: | ---: | --- |
| `rgb_unpack_pointxyzrgb_640x480` | 5 | 1.28x | 1.27x | 1.29x | positive |
| `rgb_unpack_pointxyzrgba_640x480` | 5 | 1.29x | 1.29x | 1.30x | positive |
| `scaling_fixed_factor_intensity_640x480` | 5 | 1.05x | 1.01x | 1.07x | weak-positive |
| `scaling_full_range_intensity_640x480` | 5 | 0.91x | 0.91x | 0.92x | negative |
| `scaling_full_range_reduction_intensity_640x480` | 5 | 1.54x | 1.47x | 1.56x | positive |

`scaling_reduction_v1` 把 full-range 诊断从 v0 的 negative 提升到 positive。这个结论只来自板卡 repeated，
不是 QEMU timing。

## Evidence Doctor 解释

Evidence Doctor 仍报告 `Errors=1`，但 Error 指向旧 label `scaling_full_range_intensity_640x480`，
也就是 Phase 000 的 `scaling_float_stride_v0`。处理动作：

- 保留该 Error，用来证明 v0 full-range 仍应拒绝。
- 新 label `scaling_full_range_reduction_intensity_640x480` 没有退化 Error。
- 当前 evidence role 仍是 `production_shaped_diagnostic`，不能写成 production evidence。

## Diagnostic 到 production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | `production_shaped_diagnostic` |
| A/B boundary | `test_helper`；Std/RVV 都用 bench helper，不是真实 public extractor dispatch。 |
| 当前决策问题 | `RVV-vs-scalar` 和 implementation-shape 筛选。 |
| diagnostic 是否可外推到 production | 只能外推为 bounded production candidate。production 仍需 PI1-PI5。 |
| comparison-boundary / baseline mismatch 风险 | 存在。真实 production 有 `PCLImage` 对象赋值、extractor 状态、field lookup 和模板边界。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | v1 为 positive，可进入 PI1；v0 full-range 不允许。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 需要，特别是 production 接入后要确认同一 public boundary 下不是 helper 边界收益。 |

## Matrix 更新

- `scaling_reduction_v1`：`planned -> partial-production-candidate within diagnostic boundary`。
- `scaling_float_stride_v0` full-range：保持 rejected。
- `rgb_u32_stride_unpack_v0`：保持 partial-production-candidate within diagnostic boundary。
- `scaling_fixed_factor_intensity_640x480`：保持 weak-positive / deferred。

## Continue / Stop Decision

`continue_stop_decision=turn_stop_deferred with stop_condition_hit`。

停止条件是：下一步若进入 `PointCloudImageExtractorWithScaling` 或 RGB extractor 的 production
integration loop（生产接入闭环），会修改 `io/include/pcl/io/impl/point_cloud_image_extractors.hpp`，
而短 prompt worker 在 PI1 前必须先向用户报告候选范围、production diff 计划、证据路径和暂停点。

`next_phase_default=PI1-production-integration-plan`，候选范围为：

- `scaling_reduction_v1`：`PointXYZI::intensity` full-range scaling 的窄范围 production probe。
- `rgb_u32_stride_unpack_v0`：`PointXYZRGB` / `PointXYZRGBA` RGB/RGBA unpack 的窄范围 production probe。

若用户暂不进入 production，当前 topic 内仍可另开 `020-rgb-segment-store-ab`，只做 test-only RGB store
shape A/B，不触碰 production。
