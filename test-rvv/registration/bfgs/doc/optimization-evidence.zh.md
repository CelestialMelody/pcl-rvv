# bfgs 优化证据索引

## 本文职责

本文把 `bfgs` topic 的候选优化方式映射到代码、测试、bench、asm、board 和 EvidenceDecision（证据决策）。Phase 020 已完成 test-only direction-update board repeated diagnostic，结果为 negative；production 仍未修改。

## 当前结论摘要

当前是 `diagnostic_stop_no_production`。production 源码未改，`doc-rvv` 不适用。Phase 020 的 board repeated 结果显示 direction-update RVV helper 在两个 case 上均慢于 Std，默认不继续 caller hotspot audit，也不进入 production integration loop。

## 优化方式总表

| 优化方式 | 当前状态 | 证据 | 下一步 |
| --- | --- | --- | --- |
| Eigen vector expression baseline | `partial` | `dump_bench_rvv` 生成 filtered asm，RVV bench binary 中有 RVV 指令；未闭合 production hot symbol | 若进入 production，再做更细符号归属 |
| Direction update fused diagnostic | `attempted / correctness passed / board negative` | Std/RVV gtest 通过；QEMU smoke 2 case；board 5-run median B/A 为 0.648x / 0.740x；board Evidence Doctor Errors=2 | 停止默认 production 推进 |
| Move-to + slope combined diagnostic | `correctness passed / bench deferred / no default expansion` | gtest 对拍通过；未作为当前 QEMU smoke 主证据 | direction-update 已 negative，默认不扩展 |
| Line-search scalar control | `not_applicable with evidence` | `lineSearch()` 以标量分支和 functor 回调为主 | 只做 boundary correctness |
| GICP caller hotspot audit | `not_unblocked` | GICP 是确认 caller，但 direction-update 局部 board 诊断已 negative | 默认不做；除非用户要求负向原因分析 |

## 标量路径与 RVV 路径差异

当前 RVV 路径只存在于 `include/impl/bfgs_candidates.hpp` 的 test-only helper。它用 `__RVV10__` 下的 RVV intrinsic（内建函数）处理连续 double 向量，非 RVV build 回退到标量 reference。任何改变 line-search cache 或 functor 调用次数的实现都不能进入 production。

## 代码级证据索引

| 对象 | 证据角色 | 路径 |
| --- | --- | --- |
| `BFGS::minimizeOneStep()` direction update | planned diagnostic candidate | `registration/include/pcl/registration/bfgs.h` |
| `direction_update_candidate()` | test-only diagnostic candidate | `test-rvv/registration/bfgs/include/impl/bfgs_candidates.hpp` |
| `direction_update_std()` | scalar reference | `test-rvv/registration/bfgs/include/impl/bfgs_references.hpp` |
| `BFGSDiagnostic.DirectionUpdate*` | correctness gate | `test-rvv/registration/bfgs/src/test_bfgs.cpp` |
| `bench_bfgs --case-filter direction-update` | QEMU / board diagnostic bench | `test-rvv/registration/bfgs/src/bench_bfgs.cpp` |
| `generate_bfgs_qemu_evidence_manifest.py` | manifest generator | `test-rvv/registration/bfgs/script/generate_bfgs_qemu_evidence_manifest.py` |
| `generate_bfgs_board_repeated_summary.py` | board summary / manifest generator | `test-rvv/registration/bfgs/script/generate_bfgs_board_repeated_summary.py` |
| `log/qemu/evidence_doctor.md` | QEMU doctor summary | `test-rvv/registration/bfgs/log/qemu/evidence_doctor.md` |
| `log/board/direction_update_repeated/summary.md` | board repeated summary | `test-rvv/registration/bfgs/log/board/direction_update_repeated/summary.md` |
| `log/board/direction_update_repeated/evidence_doctor.md` | board doctor summary | `test-rvv/registration/bfgs/log/board/direction_update_repeated/evidence_doctor.md` |
| `BFGS::moveTo()` / `slope()` | Eigen baseline asm probe | `registration/include/pcl/registration/bfgs.h` |
| `OptimizationFunctorWithIndices` | GICP caller boundary | `registration/include/pcl/registration/gicp.h` |
| `estimateRigidTransformationBFGS()` | production caller | `registration/include/pcl/registration/impl/gicp.hpp` |

## 细粒度 target 字典

| target | 角色 | 当前状态 |
| --- | --- | --- |
| `run_test_compare` | correctness aggregate | adopted |
| `run_test_candidate_direction` | direction-update correctness alias | adopted |
| `run_test_public_api_smoke` | public API smoke alias | adopted |
| `run_bench_direction_update_smoke` | QEMU log-shape smoke | adopted |
| `run_bench_move_to_slope_smoke` | optional QEMU smoke | available |
| `run_bench_gicp_shaped_vector6_smoke` | optional caller-shaped QEMU smoke | available |
| `dump_bench_rvv` | filtered RVV asm | adopted |
| `run_qemu_smoke_evidence_doctor` | manifest + doctor | adopted |
| `record_qemu_correctness_state`、`record_qemu_smoke_evidence_state`、`evidence_status` | registry / freshness | adopted |
| `collect_board_bfgs_direction_update_repeated` | board repeated diagnostic | adopted / negative |
| `run_board_bench_bfgs_direction_update_repeated` | board collect + doctor + registry | adopted / negative |

## 当前可提交证据

本文、evaluation、roadmap、matrix、phase result、`log/qemu/evidence_doctor.md`、`log/board/direction_update_repeated/summary.md` 和 `log/board/direction_update_repeated/evidence_doctor.md`。raw log、manifest、registry 和 build 输出默认不提交。

## 结论边界

`board negative` 不能写成 production-ready。当前结论是：test-only direction-update RVV helper 不支持继续默认生产接入路径；若用户后续要求继续，只能作为 bounded negative-analysis，而不是 production dispatch。
