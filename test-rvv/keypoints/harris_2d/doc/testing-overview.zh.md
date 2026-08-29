# harris_2d Testing Overview

## 测试层级

| 层级 | target / case | 作用 | 边界 |
| --- | --- | --- | --- |
| correctness aggregate | `run_test_compare` | 分别构建 Std 和 RVV 测试二进制，在 QEMU 上跑 gtest | QEMU 不提供性能结论 |
| diagnostic helper test | `Harris2DDiagnostic.*` | 对 test-support reference 和 candidate 做 response map 对拍 | 不替代 production direct |
| production direct test | `Harris2DProductionDirect.*` | 调用真实 `HarrisKeypoint2D::compute()`，验证 public entry 输出和多尺寸状态 | 不覆盖 NMS enabled |
| QEMU smoke | `run_bench_* --public-entry` 小规模运行 | 验证 bench wrapper 可运行和 correctness 行 | 不写性能结论 |
| asm gate | `check_harris_2d_rvv_asm` | 检查 bench RVV 二进制中出现预期 RVV float 指令 | 不单独证明收益 |
| board repeated | `board_repeated record_evidence_state_repeated` | 在板卡上 5-run 采集 public-entry Std/RVV 对比、summary、manifest、doctor、registry | 性能结论只来自这里 |

## Target 粒度审计

| target 类别 | 当前状态 | 说明 |
| --- | --- | --- |
| correctness aggregate | adopted | `run_test_compare` 覆盖 Std/RVV gtest。 |
| correctness aliases | not_applicable with evidence | 当前 gtest 数量少，四类 response method 在同一 target 中覆盖。 |
| bench diagnostic aliases | adopted | `bench_harris_2d` 通过 case-filter 区分 Harris、Tomasi 和 tail Noble case，`--public-entry` 切到真实 public compute。 |
| QEMU smoke aliases | adopted | `run_qemu_smoke` 可做 correctness + RVV bench 可运行性。 |
| board smoke aliases | adopted | 共享 `run_board_bench_compare` 可做单次 smoke，默认性能结论使用 repeated。 |
| board repeated aliases | adopted | `board_repeated` + `record_evidence_state_repeated` 生成 Phase 020 正式证据。 |
| doctor / registry aliases | adopted | `evidence_manifest_repeated`、`evidence_doctor_repeated`、`record_evidence_state_repeated`、`check_evidence_freshness` 已接入。 |
| historical probe guarded aliases | not_applicable with evidence | 当前 topic 没有历史 guarded target。 |

## 覆盖矩阵

| 范围 | 当前状态 | 证据 |
| --- | --- | --- |
| synthetic organized full-image response map | adopted | `Harris2DDiagnostic.CandidateMatchesScalarReference` |
| Harris/Noble/Lowe/Tomasi response formulas | adopted | scalar reference、candidate 和 public direct 对拍 |
| finite point gate | adopted | `NonFiniteInputPointKeepsZeroResponse` |
| RVV tail | adopted | `CandidateHandlesTailWidth`、`harris2d_noble_tail_641x481` |
| public `HarrisKeypoint2D` dispatch | adopted narrow | `Harris2DProductionDirect.*`、phase020 board repeated |
| multi-size same-process public compute | adopted | `PublicComputeDoesNotReusePreviousImageSize` |
| NMS sort / occupancy / output push | not_applicable with evidence | 保持标量，不纳入当前采纳范围。 |
