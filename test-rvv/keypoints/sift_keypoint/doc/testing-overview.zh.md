# SIFT Keypoint Testing Overview

## 测试层级

| 层级 | target / case | 作用 | 边界 |
| --- | --- | --- | --- |
| correctness aggregate | `run_test_compare` | 分别构建 Std / RVV gtest 二进制并运行 | QEMU 只证明正确性和日志形状 |
| diagnostic helper test | `SiftScaleSpace.*` | 对 synthetic neighborhood batch 的 scalar reference 和 candidate 对拍 | 不覆盖完整 public SIFT pipeline |
| public smoke | `PublicSiftKeypointAcceptsSyntheticOrganizedCloud` | 证明当前 production 源码还能处理测试用 organized cloud | 不是 RVV 采纳证据 |
| bench compare | `run_bench_compare` / `run_bench_rvv` | 只测 scale-space helper hot loop | 不测 radiusSearch / detector 全流程 |
| asm gate | `check_sift_keypoint_rvv_asm` | 检查 RVV build 里是否出现 exp / vector load / `vsetvl` 相关指令；历史 diagnostic helper 里可能仍有 reduction 指令 | 不单独证明收益，也不表示当前 production path 采用 vector reduction |
| board repeated | `board_repeated record_evidence_state_repeated` | 在板卡上采集 helper repeated summary / manifest / doctor | 性能结论只来自这里 |
| production-public repeated | `board_repeated_public record_evidence_state_public` | 真实 public `SIFTKeypoint::compute()` 的 Std/RVV repeated board 对比 | 只覆盖 `public_sift_keypoint_320x240` |
| public output trace | `board_public_trace compare_public_trace` | 对比 Std/RVV `PointWithScale` 输出计数、顺序和字段 | 只证明当前 case 的公开入口正确性 |

## Target 粒度审计

| target 类别 | 当前状态 | 说明 |
| --- | --- | --- |
| correctness aggregate | adopted | 一个总入口覆盖 helper 对拍和 public smoke。 |
| correctness aliases | not_applicable with evidence | 当前测试族较少，先不拆更细 alias。 |
| bench diagnostic aliases | adopted | `--case-filter` 区分 96 / 192 / tail 三个 case。 |
| QEMU smoke aliases | adopted | `run_qemu_smoke` 只做 correctness + bench 可运行性。 |
| board smoke aliases | adopted | 单次 board 运行默认由 `run_board_bench_compare` 承载。 |
| board repeated aliases | adopted | `board_repeated` + `record_evidence_state_repeated` 生成 summary。 |
| doctor / registry aliases | adopted | `record_evidence_state_repeated` 和 `record_evidence_state_public` 都生成 manifest / doctor / registry。 |
| historical probe guarded aliases | not_applicable with evidence | 当前 topic 的历史 full-vector-reduction 尝试没有保留可默认运行 target，只在 phase result 中作为 rejected 记录。 |

## 当前 adopted 范围

`production-public-gaussian-weight-rvv` 已采纳，但只覆盖 `PointXYZI -> PointWithScale` 的 organized dense
synthetic public case。QEMU（仿真器）仍只支持 correctness（正确性）和日志形状；性能结论来自
`test-rvv/keypoints/sift_keypoint/log/board/repeated_phase010_public_compute/summary.md`。
