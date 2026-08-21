# Phase 020 Plan: fused-colored-pack

## 阶段意图和边界

本阶段仍是 test-only diagnostic（测试专用诊断），不修改 production。Phase 010 的 `PointXYZRGB`
colored path 正确但只有 1.07x-1.10x，主要风险是当前 RVV build 先执行 disparity RVV candidate，再用
第二个 scalar loop（标量循环）重新做 `pcl::isFinite` 和 RGB / mono 写出。

本阶段验证一个更窄 candidate：在 RVV build 中复用同一批 `vfclass` finite mask（有限值掩码）和 lane
顺序，单次 organized cloud scan 同时写 disparity 和 RGB / mono color buffer。颜色字节本身可以继续用
lane scalar store（逐 lane 标量写），阶段目标是避免二次 scan 和重复 finite check，而不是承诺完整 byte
gather/store 向量化。

## Phase Scope 与扩展队列

`validated_scope`：`PointXYZRGB` / `float` / organized cloud order / cloud -> disparity + RGB 或 mono。

`unvalidated_scope`：`PointXYZRGBA`、decode path、generic PointT、production `encodePointCloud`、PNG/LZF 后端、
`Scalar=double`、production direct 和 repeated board。

`phase_closeout_boundary`：本阶段只比较 `rgb_or_mono_color_pack_rvv_v1_fused_pack` 相对 Phase 010 v0
scalar-pack 诊断形态是否改善 colored encode 局部路径。

## 当前状态清单

| item | 当前状态 |
| --- | --- |
| correctness | `make run_test_compare` 通过 std/RVV 各 5 个 gtest |
| board | `make board_smoke` 通过，colored v0 为 1.07x-1.10x |
| asm | `make dump_bench_rvv` 已生成 RVV disparity 指令归属 |
| Evidence Doctor | `Errors=0, Warnings=12`，全部为 low-run-count 或 mask contract mismatch |
| production | 未修改 |

## 优化矩阵

| candidate family | point type / layout | test | bench | board | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `rgb_or_mono_color_pack_rvv_v0_scalar_pack` | `PointXYZRGB` / AoS + RGB | pass | weak-positive | 1.07x-1.10x | disparity RVV only | warning-only | baseline diagnostic |
| `rgb_or_mono_color_pack_rvv_v1_fused_pack` | `PointXYZRGB` / AoS + RGB | planned | planned | planned | planned | planned | pending |

## 实现和测试动作

| action | 产物 / 命令 | 预期证据 | 完成判据 |
| --- | --- | --- | --- |
| 写 fused helper failing test | `src/test_organized_pointcloud_conversion.cpp` | helper 缺失导致 RED | 失败原因是新 helper 未定义 |
| 实现 fused diagnostic helper | `include/impl/opc_candidates.hpp` | RGB / mono 与 PCL scalar path 一致 | `make run_test_compare` 通过 |
| 让 RVV bench 使用 fused helper | `src/bench_organized_pointcloud_conversion.cpp` | colored label 复用原 label，当前 run 可直接对比 Phase 010 | checksum 一致 |
| QEMU correctness | `make run_test_compare` | correctness 证据 | std/RVV 全绿 |
| 反汇编归属 | `make dump_bench_rvv` | RVV disparity region 仍存在 | asm 文件含目标 RVV 指令 |
| 板卡 bench + Doctor | `make board_smoke` + Evidence Doctor | colored speedup 与 warning 数 | 更新 manifest / result |

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `diagnostic` |
| A/B boundary | `test helper` |
| 当前决策问题 | fused colored pack 是否比 v0 scalar-pack 更值得继续 |
| diagnostic 是否可外推到 production | no；production 仍需 public path direct、fallback 和 backend 稀释审计 |
| comparison-boundary / baseline mismatch 风险 | yes；本阶段是 topic helper A/B，不是 public overload |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不建议 colored production probe；优先转 decode/backprojection |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes；若要 clean adopt，需要 production boundary 内 detail A/B |

## 板卡复跑预算和决策桶

本阶段先使用单次 board diagnostic：30 iterations、5 warmup、run count 1。若 fused candidate 把 colored speedup
提升到 1.2x 以上且 checksum 一致，判为 `attempted_positive_diagnostic` 并考虑后续 production-shaped plan。
若仍在 1.0x-1.15x，判为 `weak_positive_diagnostic` 并转 decode phase。若低于 1.0x 或 checksum 不一致，
判为 rejected / correctness blocker。

## 继续 / 停止条件

不出现 checksum mismatch、板卡不可用或明确退化时继续推进。若 fused pack 仍弱正向，下一阶段默认转
`decode_backprojection_rvv`，而不是继续在 colored encode 上堆更多 candidate。

## 文档更新清单

完成后更新 `020-fused-colored-pack/result.zh.md`、`doc/phases/README.zh.md`、
`doc/phases/optimization-matrix.zh.md`、`doc/optimization-roadmap.zh.md`、evaluation 和 queue row。
