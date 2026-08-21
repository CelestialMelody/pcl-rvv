# Phase 020 Result: fused-colored-pack

## 结论

本阶段完成 `rgb_or_mono_color_pack_rvv_v1_fused_pack` test-only diagnostic（测试专用诊断）。
相对 Phase 010 的 v0 scalar-pack（标量颜色写出）弱正向结果，fused candidate 明显改善 colored encode：
RGB dense 1.48x、mono dense 1.80x、RGB mixed-invalid 1.49x。checksum 与 PCL scalar baseline 一致。

这说明 Phase 010 的收益瓶颈主要来自二次 scan 和重复 `pcl::isFinite`。当前结论仍是 diagnostic evidence
（诊断证据），不能替代 production direct（真实生产路径直连）或 repeated board（重复板卡）证据。

## 计划动作回填

| action | 产物 / 命令 | 结果 | 状态 |
| --- | --- | --- | --- |
| 写 fused helper failing test | `src/test_organized_pointcloud_conversion.cpp` | `make run_test_rvv` 因 helper 未定义失败 | completed |
| 实现 fused diagnostic helper | `include/impl/opc_candidates.hpp` | RVV build 单次 strip-mining 同时写 disparity 和 color；非 RVV fallback 走 v0 scalar 等价路径 | completed |
| 让 RVV bench 使用 fused helper | `src/bench_organized_pointcloud_conversion.cpp` | RVV colored bench label 切到 fused helper，std 仍是 PCL scalar | completed |
| QEMU correctness | `make run_test_compare` | std/RVV 各 6 个 gtest 全部通过 | completed |
| 反汇编归属 | `make dump_bench_rvv` | `vlse32.v`、`vfclass.v`、`vfrdiv.vf`、`vse32.v` 仍出现在 bench RVV asm | completed |
| 板卡 bench + Doctor | `make board_smoke` + Evidence Doctor | checksum 一致；Doctor `Errors=0, Warnings=13, Suggestions=0` | completed |

## 板卡结果

| case | Std ms/iter | RVV ms/iter | speedup | checksum |
| --- | ---: | ---: | ---: | --- |
| `pointxyz_disparity_dense_307k` | 13.1186 | 6.6060 | 1.99x | match |
| `pointxyz_disparity_mixed_invalid_307k` | 13.0073 | 6.6703 | 1.95x | match |
| `pointxyz_disparity_dense_1m` | 44.7136 | 22.4992 | 1.99x | match |
| `pointxyzrgb_disparity_rgb_dense_307k` | 19.4317 | 13.1507 | 1.48x | match |
| `pointxyzrgb_disparity_mono_dense_307k` | 20.6583 | 11.4837 | 1.80x | match |
| `pointxyzrgb_disparity_rgb_mixed_invalid_307k` | 19.4895 | 13.1234 | 1.49x | match |

## Evidence Doctor 处理

`log/board/evidence_doctor.md` 报告 `Errors=0, Warnings=13, Suggestions=0`。

已知 warning 分三类：

- `low_run_count`：当前是单次 board diagnostic，只能支撑继续 / 停止诊断决策。
- `contract_mismatch`：baseline 用 `pcl::isFinite`，candidate 用 `vfclass finite mask`；当前 correctness 和 checksum 一致，但 production 需要同边界验证。
- `group_outlier`：mono fused 1.80x 明显高于 RGB 1.48x-1.49x。解释是 mono 只写 1 byte / point，RGB 写 3 bytes / point；该差异应按 case 单独报告，不能把 mono 收益外推到 RGB。

## EvidenceDecision

`rgb_or_mono_color_pack_rvv_v1_fused_pack` 判为 `attempted_positive_diagnostic`。它支持继续保留
colored encode 作为 production-shaped 候选，但当前 topic 仍未覆盖 decode/backprojection（反投影）
半边，因此默认下一阶段转 `030-decode-backprojection`，先补齐同一 header 的 decode family 诊断证据。

## 下一阶段

下一阶段默认入口：`030-decode-backprojection`。

若 decode diagnostic 也为正向，再进入 production integration plan（生产接入计划）并读取
`rvv-implementation`，把 encode/decode 的 fallback、point type gate、push_back/resize 边界和
PNG/LZF backend 稀释统一审计。
