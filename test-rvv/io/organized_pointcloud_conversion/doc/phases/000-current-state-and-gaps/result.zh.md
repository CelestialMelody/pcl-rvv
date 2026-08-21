# Phase 000 Result: current-state-and-gaps

## 当前阶段结论

Phase 000 完成 `PointXYZ` cloud -> disparity 的 test-only diagnostic（测试专用诊断）闭环。当前
RVV candidate（RVV 候选）使用 per-VL scratch buffer（每个 VL 的临时缓冲）后，QEMU correctness
（QEMU 正确性验证）和板卡 correctness 均通过，板卡单次 diagnostic bench 显示 1.72x-1.74x。

该结论只证明 `PointXYZ` / `float` / organized cloud order / cloud -> disparity 的诊断 helper 有继续价值。
它不是 production evidence（生产证据），不能直接改 `io/include/pcl/compression/organized_pointcloud_conversion.h`。

## 计划动作回填

| action | status | evidence | result / missing_items |
| --- | --- | --- | --- |
| 写 failing test | done | `make run_test_rvv` 首次失败于缺少 `organized_pointcloud_conversion.h` 聚合入口 | RED 成立，测试捕获缺失诊断层 |
| 实现 test-only diagnostic helper | done | `include/organized_pointcloud_conversion.h`, `include/impl/opc_candidates.hpp` | per-VL scratch 版本通过 correctness；早期 full-size temp 版本为 historical negative |
| 建立 bench | done | `src/bench_organized_pointcloud_conversion.cpp` | 输出 `Dataset:`、`Iterations:`、case avg、`Total Time` 和 checksum |
| QEMU correctness | done | `log/qemu/run_test_std.log`, `log/qemu/run_test_rvv.log` | std/RVV 各 3 个 gtest 通过 |
| 反汇编归属 | done | `build/asm/riscv/bench_organized_pointcloud_conversion_rvv.asm` | 可见 `vlse32.v`、`vfmul.vf`、`vfrdiv.vf`、`vfclass.v`、`vse32.v` |
| 板卡 bench | done | `log/board/run_test.log`, `log/board/run_bench_std.log`, `log/board/run_bench_rvv.log`, `log/board/analyze_bench_compare.log` | checksum 一致；dense 307k 1.74x，mixed-invalid 307k 1.72x，dense 1M 1.74x |
| Evidence Doctor | partial | `log/board/evidence_manifest.json`, `log/board/evidence_doctor.md` | Errors=0, Warnings=6；低 run count 和 mask boundary mismatch 使其只能作为 diagnostic |
| 文档和队列同步 | partial | 本 result、roadmap、matrix、evaluation | queue 标为 in_progress；完整 doc suite 和 registry closeout 后续补齐 |

## 当前优化矩阵更新

| candidate family | row source policy | point type / Scalar / layout | correctness / fallback | bench / board | asm | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `pointxyz_z_strided_disparity_rvv_v0_full_temp` | organized cloud order | `PointXYZ` / `float` / AoS stride | pass | negative: 0.64x / 0.94x / 0.69x | RVV instructions present | historical manifest superseded | rejected | none |
| `pointxyz_z_strided_disparity_rvv_v1_per_vl_scratch` | organized cloud order | `PointXYZ` / `float` / AoS stride | pass | positive diagnostic: 1.74x / 1.72x / 1.74x | RVV instructions present | Errors=0, Warnings=6 | attempted_positive_diagnostic | colored-cloud-to-disparity phase |

## Diagnostic 到 production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | `diagnostic`；真实 production 入口未改 |
| A/B boundary | `test helper`；Std 侧是 PCL scalar path，RVV 侧是测试专用 candidate |
| 当前决策问题 | `RVV-vs-scalar` 局部诊断 |
| diagnostic 是否可外推到 production | no；只能说明该局部公式值得继续扩展。production 仍需 PI1-PI5、fallback 和 PNG/LZF 稀释审计 |
| comparison-boundary / baseline mismatch 风险 | yes；candidate 使用 `resize` 和 per-VL scratch，production 当前 `reserve + push_back` |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前结果正向；若后续 colored/decode 负向，仍可只保留 PointXYZ diagnostic，不拒绝其它有界探针 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | production 前至少需要 production direct Std/RVV；如果多个 RVV family 同时存在，需要同边界 RVV-vs-RVV |

## Evidence Doctor 解释

Evidence Doctor 对结构化 manifest 输出 `Errors=0, Warnings=6`。

- `low_run_count`：当前是单次 board diagnostic。它足以决定继续扩展同 topic diagnostic，但不能支撑强 production performance 结论。
- `contract_mismatch`：baseline 使用 `pcl::isFinite`，candidate 使用 `vfclass` finite mask。correctness 对拍说明当前输入语义一致，但 A/B boundary 仍是 diagnostic，不是 strict production A/B。

## Continue / Stop Decision

`stop_condition_hit=none`。当前 roadmap 和 matrix 仍有授权范围内的 unblocked action：colored specialization
的 cloud -> disparity + RGB/mono 仍未验证。因此不能写 `ready_for_review` 或 closeout。

`next_phase_default=010-colored-cloud-to-disparity`。
