# color_modality Testing Overview

## 测试层级

| 层级 | target / case | 作用 | 边界 |
| --- | --- | --- | --- |
| correctness aggregate | `run_test_compare` | 分别构建 Std 和 RVV 测试二进制，在 QEMU 上跑 5 个 gtest | QEMU 不提供性能结论 |
| production direct test | `ColorModalityProductionDirect.*` | 通过公开 `processInputData()` 验证路径命中和 forced scalar/RVV 输出一致 | 只覆盖 `PointXYZRGB` organized 输入 |
| diagnostic helper test | `ColorModalityDiagnostic.*` | 对 test-support reference 和 candidate 做 quantize/filter/spread 对拍 | 不替代 production direct |
| QEMU smoke | `run_bench_rvv BENCH_ARGS="--case-filter production_process_320x240 --iterations 1 --warmup-iterations 1"` | 验证 bench wrapper 可运行和日志形状 | 不写性能结论 |
| asm gate | `check_cm_rvv_asm` | 检查 bench RVV 二进制中出现预期 RVV 指令 | 不单独证明收益 |
| board repeated | `board_repeated record_evidence_state_repeated` | 在板卡上 5-run 采集 public Std/RVV 对比、summary、manifest、doctor、registry | 性能结论只来自这里 |

## Target 粒度审计

| target 类别 | 当前状态 | 说明 |
| --- | --- | --- |
| correctness aggregate | adopted | `run_test_compare` 覆盖 Std/RVV gtest。 |
| correctness aliases | not_applicable with evidence | 当前 gtest 数量少，生产直连和诊断 helper 已在同一 target 中按 test suite 分层。 |
| bench diagnostic aliases | adopted | `color_preprocess_*` 保留 production-shaped diagnostic；`production_process_*` 是 production direct。 |
| QEMU smoke aliases | adopted | `run_qemu_smoke` 指向 correctness；额外 QEMU bench smoke 手工用 case-filter 限定。 |
| board smoke aliases | adopted | 共享 `run_board_bench_compare` 可做单次 smoke，默认性能结论使用 repeated。 |
| board repeated aliases | adopted | `board_repeated` + `record_evidence_state_repeated` 生成 Phase 040 正式证据。 |
| doctor / registry aliases | adopted | `evidence_manifest_repeated`、`evidence_doctor_repeated`、`record_evidence_state_repeated`、`check_evidence_freshness` 已接入。 |
| historical probe guarded aliases | not_applicable with evidence | 没有需要保留的历史 guarded target；误命名 Phase 030 registry 已清理。 |

## 覆盖矩阵

| 范围 | 当前状态 | 证据 |
| --- | --- | --- |
| `PointXYZRGB` organized `processInputData()` | adopted | production direct tests、board repeated。 |
| `width >= 3 && height >= 3` | adopted | 320x240 和 641x481 tail case。 |
| 非 RVV 构建 | scalar fallback | Std test binary。 |
| 强制标量 hook | adopted | `RvvProcessInputDataMatchesForcedScalarPipeline`。 |
| 其它 `PointInT` | scalar fallback / unvalidated performance | exact-type gate 不命中 RVV quantize。 |
| `extractFeatures()` | out of scope | 当前 tests 不调用完整 feature selection。 |
