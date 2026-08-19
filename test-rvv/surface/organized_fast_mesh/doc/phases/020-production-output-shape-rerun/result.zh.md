# Phase 020 Result

## 当前结论

本阶段完成 output-shape parity（输出形态同构）修正：production RVV helper 和 test-only candidate
都改为与原标量路径一致的预分配 `resize` + `idx` 写回模型。QEMU correctness 通过；QEMU bench smoke
仍显示 public adaptive-cut RVV 慢于标量。板卡复测因 SSH 超时未完成。

## 实现动作

| action | result |
| --- | --- |
| production helper 写回同构 | `surface/include/pcl/surface/impl/organized_fast_mesh.hpp` 中 helper 改为 `addTriangleAt(..., idx, polygons)`，最后 `polygons.resize(idx)` |
| test-only candidate 写回同构 | `include/impl/ofm_candidates.hpp` 中 candidate 改为 `addTriangleAt` / `addQuadAt` 和 `idx` 写回 |
| dispatch gate | 未扩大，仍只覆盖 `PointXYZ` / step 1 / `store_shadowed_faces_ == true` / adaptive-cut production path |

## 验证

| command | result |
| --- | --- |
| `git diff --check -- surface/include/pcl/surface/impl/organized_fast_mesh.hpp test-rvv/surface/organized_fast_mesh` | pass |
| `make run_test_compare` | QEMU Std/RVV 均 6/6 pass |
| `make run_bench_compare ALLOW_QEMU_BENCH_COMPARE=1 BENCH_ARGS='--iterations 3 --warmup-iterations 1 --case-filter ofm_public_adaptive_cut'` | checksum 一致；QEMU smoke 为 RVV `0.66x`，仅作日志形状和风险提示 |
| `make clean_bench_rvv dump_bench_rvv` | refreshed RVV asm dump generated |
| `make board_smoke BENCH_ARGS='--iterations 20 --warmup-iterations 5 --case-filter ofm_public_adaptive_cut'` | blocked：SSH connect timeout |

## EvidenceDecision

本阶段没有产生可采纳的 production performance evidence。organized_fast_mesh topic 的 RVV
生产接入探索已结束，生产补丁已按用户确认回滚。当前不建议继续扩大到 shadow edge、泛型点类型或
fixed cut / quad production RVV 接入。
