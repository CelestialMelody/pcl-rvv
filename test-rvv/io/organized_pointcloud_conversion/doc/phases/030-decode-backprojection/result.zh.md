# Phase 030 Result: decode-backprojection

## 结论

本阶段完成 `decode_disparity_backprojection_rvv_v0` test-only diagnostic（测试专用诊断）。
正确性通过，但板卡性能负向：dense 307k 为 0.91x、mixed-invalid 307k 为 0.90x、dense 1m 为 0.85x。
checksum 与 PCL scalar baseline 一致。

该 decode v0 不建议进入 production。负向原因的当前解释是：为了批量计算 `x/y/z`，helper 需要 per-VL
staging（每个 VL 块的临时缓冲）准备 disparity、x 坐标和 y 坐标，再做 AoS point 写回；这些额外内存流量和
标量 lane 写回压过了公式向量化收益。

## 计划动作回填

| action | 产物 / 命令 | 结果 | 状态 |
| --- | --- | --- | --- |
| 写 decode helper failing test | `src/test_organized_pointcloud_conversion.cpp` | `make run_test_rvv` 因 `convertDisparityToPointXYZCloudDiagnostic` 未定义失败 | completed |
| 实现 disparity decode diagnostic helper | `include/impl/opc_candidates.hpp` | 与 PCL scalar path 对拍；对齐 production 当前 `push_back` 后 `width=size,height=1` 的 metadata 语义 | completed |
| 增加 decode bench case | `src/bench_organized_pointcloud_conversion.cpp`, fixtures/checksum | 新增 3 个 decode label 和 point cloud checksum | completed |
| QEMU correctness | `make run_test_compare` | std/RVV 各 7 个 gtest 全部通过 | completed |
| 反汇编归属 | `make dump_bench_rvv` | 生成 RVV bench asm | completed |
| 板卡 bench + Doctor | `make board_smoke` + Evidence Doctor | decode checksum 一致但性能退化；Doctor `Errors=3, Warnings=16` | completed |

## 板卡结果

| case | Std ms/iter | RVV ms/iter | speedup | checksum |
| --- | ---: | ---: | ---: | --- |
| `pointxyz_decode_disparity_dense_307k` | 14.5491 | 16.0664 | 0.91x | match |
| `pointxyz_decode_disparity_mixed_invalid_307k` | 14.3830 | 15.9997 | 0.90x | match |
| `pointxyz_decode_disparity_dense_1m` | 46.7131 | 54.7532 | 0.85x | match |

Encode 侧在同一 run 中仍为正向：PointXYZ 1.96x-2.00x；PointXYZRGB fused RGB 1.48x-1.49x、mono 1.81x。

## Evidence Doctor 处理

`log/board/evidence_doctor.md` 报告 `Errors=3, Warnings=16, Suggestions=0`。

Errors 全部来自 decode case 的 `ba_degradation_frequency`，即 3/3 decode case 的 B/A 都低于 1。处理动作：

- `decode_disparity_backprojection_rvv_v0` 判为 `rejected_negative_diagnostic`。
- 不把 decode v0 纳入 production integration loop（生产接入闭环）。
- 若未来重新尝试 decode，需要新 candidate，例如避免 per-VL x/y staging、使用更紧凑坐标生成，或先做
  store wrapper / output layout 消融；不能复用本 v0 作为生产候选。

Warnings 来自 encode mask contract mismatch、low run count、decode low run count 和 mono group outlier。
这些 warning 不影响“decode v0 不生产化”的负向决策；encode 侧仍只能作为单次 diagnostic positive。

## EvidenceDecision

`decode_disparity_backprojection_rvv_v0` 判为 `rejected_negative_diagnostic`。topic 不停止：encode 侧已有
足够强的 diagnostic positive，可以进入更窄的 encode-only production probe 计划。该计划不得覆盖 decode
overload，也不得声称整个 `OrganizedConversion` family 已接入 RVV。

## 下一阶段

下一阶段默认入口：`040-encode-production-integration-plan`。

边界：只考虑 cloud -> disparity / RGB / mono encode 侧；decode overloads 保持标量。
