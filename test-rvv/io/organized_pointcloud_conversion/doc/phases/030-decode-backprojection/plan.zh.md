# Phase 030 Plan: decode-backprojection

## 阶段意图和边界

本阶段覆盖同一 header 中的 decode/backprojection（反投影）方向：disparity image（视差图）或 depth image
（深度图）转 organized cloud。目标是判断 `width * height` 的规则图像扫描、中心坐标公式和 invalid point
写回是否存在 RVV diagnostic 价值。

本阶段仍不修改 production。只新增 test-only helper、gtest、bench 和证据文档。

## Phase Scope 与扩展队列

`validated_scope`：`PointXYZ` / `float` / image scan order / disparity image -> cloud，优先覆盖 0 和 `0x7FF`
invalid disparity。若时间允许，再扩展 depth image -> cloud。

`unvalidated_scope`：colored decode、`PointXYZRGB/RGBA` color image 回填、generic PointT、production
dispatch、PNG/LZF 后端、repeated board。

`phase_closeout_boundary`：本阶段只关闭 uncolored decode diagnostic 的矩阵条目。

## 当前状态清单

| item | 当前状态 |
| --- | --- |
| encode correctness | `make run_test_compare` 通过 std/RVV 各 6 个 gtest |
| encode board | PointXYZ 1.95x-1.99x；PointXYZRGB fused RGB 1.48x-1.49x、mono 1.80x |
| Evidence Doctor | `Errors=0, Warnings=13`，均为 diagnostic 边界 warning |
| production | 未修改 |

## 优化矩阵

| candidate family | row source | point type / layout | test | bench | board | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `decode_disparity_backprojection_rvv_v0` | image scan order | `PointXYZ` / AoS output | planned | planned | planned | planned | planned | pending |

## 实现和测试动作

| action | 产物 / 命令 | 预期证据 | 完成判据 |
| --- | --- | --- | --- |
| 写 decode helper failing test | `src/test_organized_pointcloud_conversion.cpp` | helper 缺失导致 RED | 失败原因是新 helper 未定义 |
| 实现 disparity decode diagnostic helper | `include/impl/opc_candidates.hpp` | 与 PCL scalar path 对拍 | `make run_test_compare` 通过 |
| 增加 decode bench case | `src/bench_organized_pointcloud_conversion.cpp` | decode label 输出 checksum 和时间 | std/RVV checksum 一致 |
| QEMU correctness | `make run_test_compare` | correctness 证据 | std/RVV 全绿 |
| 反汇编归属 | `make dump_bench_rvv` | backprojection math 出现目标 RVV 指令 | asm 可定位 |
| 板卡 bench + Doctor | `make board_smoke` + Evidence Doctor | speedup 与 warning 数 | 更新 manifest / result |

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `diagnostic` |
| A/B boundary | `test helper` |
| 当前决策问题 | uncolored disparity decode 是否有 RVV-vs-scalar 局部价值 |
| diagnostic 是否可外推到 production | no；production 使用 `push_back` 和 generic PointT，需要同边界 probe |
| comparison-boundary / baseline mismatch 风险 | yes；helper 可用 `resize` 连续写，不等于 public overload |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 只在 encode 证据足够强且 decode 不是明显负向时考虑 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes |

## 板卡复跑预算和决策桶

先跑单次 board diagnostic：30 iterations、5 warmup、run count 1。>1.2x 且 checksum 一致为 positive diagnostic；
1.0x-1.2x 为 weak-positive；<1.0x 为 negative / rejected diagnostic。production 结论需要 repeated board 和
production direct。

## 继续 / 停止条件

若 decode checksum mismatch，先停在 correctness blocker。若板卡不可用，转 blocked handoff。若 decode 明显负向，
production integration 只能围绕 encode 侧做更窄 probe；若正向，进入 production integration plan 前读取
`rvv-implementation` 并统一审计 encode/decode 边界。
