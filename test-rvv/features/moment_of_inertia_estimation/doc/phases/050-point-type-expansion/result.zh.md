# Phase 050 result: PointXYZ-like production evidence expansion

## 执行范围

本阶段不改 production 算法，只在已采纳的 projected covariance production path 上补 common PointXYZ-like typed scope（常见 PointXYZ-like 点型范围）的 production-public（真实公开入口）板卡证据，验证 `RVVXYZAoSFloatLayout<PointT>` 不会把 `PointXYZ` 的布局假设外推错到其它内建点型。

## 实现结果

| item | result |
| --- | --- |
| correctness | `make run_test_compare` 通过：Std 8/8，RVV 14/14 |
| fallback isolation | `MomentOfInertiaProductionFallback.StdBuildUsesScalarPublicCompute`、`MomentOfInertiaProductionRVVFallback.MissingInputOrIndicesReturnFalse`、`MomentOfInertiaProductionRVVFallback.NonFloatXYZUsesScalarFallback` 通过 |
| RVV asm | `make dump_bench_rvv` 命中 `vluxei32`、`vfmacc`、`vfnmsac`、`vfredusum` |
| registry | `make evidence_status_phase050` fresh |

## Board 结果

| case | runs | median speedup | min | max | B/A < 1 | decision bucket |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `moi_public_compute_pointxyzi,points=65536` | 5 | 2.132x | 2.094x | 2.186x | 0/5 | `positive` |
| `moi_public_compute_pointxyzrgb,points=65536` | 5 | 2.395x | 2.357x | 2.532x | 0/5 | `positive` |
| `moi_public_compute_pointxyzrgba,points=65536` | 5 | 2.487x | 2.388x | 2.616x | 0/5 | `positive` |
| `moi_public_compute_pointxyzrgbnormal,points=65536` | 5 | 2.784x | 2.679x | 2.923x | 0/5 | `positive` |

Evidence Doctor（证据体检）结果：四组都是 `Errors=0, Warnings=0, Suggestions=1`，建议项均为 `binary_identity_missing`，不阻塞当前结论。

## EvidenceDecision

`current_decision`：`adopted_common_typed_scope`

phase040 已把 projected covariance RVV helper 采纳进 public `compute()`；phase050 进一步证明这个 helper 在常见 `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA`、`PointXYZRGBNormal` 上同样稳定正收益。也就是说，当前 adopted production behavior 不只对 `PointXYZ` 成立，对常见 PointXYZ-like typed scope 也成立。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production-public` |
| A/B boundary | public `MomentOfInertiaEstimation<PointT>::compute()` |
| 当前决策问题 | 已采纳的 projected covariance RVV helper 是否可扩到常见 PointXYZ-like typed scope |
| diagnostic 是否可外推到 production | 不依赖 diagnostic 外推，直接看 public board 证据 |
| comparison-boundary / baseline mismatch 风险 | 低；Std/RVV 使用同一 synthetic indexed cloud、同一 public boundary、同一 timer boundary |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 允许，但本阶段四个 typed case 都是 positive，不需要降级 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 不需要；这里只是同一 helper 的 typed scope 验证，不是新 RVV family 选择 |

## 结论

phase050 完成了当前 topic 内“常见点型”这一条后续方向。现在没有新的、同样清楚且值得默认继续推进的 RVV 优化方向；自定义点型、`Scalar=double`、更宽布局或更大范围 workload 都需要新 scope 再评审，而不是顺手继续往下推。

这次 review follow-up 还补齐了 fallback isolation：std build 明确走标量 public compute，RVV helper 在空 input / 空 indices 和非 `float` xyz layout 上都会提前返回 `false`。bench 原始日志的 dataset label 也按 case-filter 改成 typed 名称，方便和 manifest / summary 对齐。

`indices_` 的非连续 / 重排路径仍由现有 helper 对拍覆盖；`rvvMaxU32ByteOffsetElements<PointT>()` 对应的 oversized input guard 属于源码级门禁，本地没有可行的、不会破坏工作区的动态构造方式，因此把它记为 `not_applicable with evidence`，不硬造超大点云。

## Doc Suite / Artifact Tracking Gate

| area | decision | evidence / note |
| --- | --- | --- |
| README navigation | adopted | `README.zh.md` 默认恢复入口已指向 phase050，并列出 phase040 / phase050 证据和常用 target |
| phase index / result | adopted | `doc/phases/README.zh.md` 新增 phase050；本文件记录 typed evidence、decision 和 stop reason |
| optimization evidence | adopted | `doc/phases/optimization-matrix.zh.md` 新增 `PointXYZ-like production evidence expansion` 行 |
| optimization roadmap | adopted | `doc/optimization-roadmap.zh.md` 把 point type expansion 从 unblocked 改为 completed_common_scope |
| evaluation | adopted | `doc/moment_of_inertia_estimation-evaluation.zh.md` 新增 phase050 typed evidence 和 Traceability Map 行 |
| long-term `doc-rvv` | adopted | `doc-rvv/features/moment_of_inertia_estimation-RVV.zh.md` 只记录 adopted production behavior 和 production evidence |
| test / bench source layout | adopted | `src/test_moi.cpp` / `src/bench_moi.cpp` 已在 `src/` 下，聚合头位于 `include/moi.h`，内部 helper 位于 `include/impl/moi_reductions.hpp` |
| target granularity | adopted | Makefile 已覆盖 correctness、asm、phase040 board、phase050 typed board 和 evidence status targets |
| artifact tracking | adopted | 当前新增 topic-local files 和长期 `doc-rvv` 已在 `git status --short --untracked-files=all -- <topic paths>` 中显式列出；默认 commit boundary 为 topic-only，不提交 raw logs |

### doc_suite_role_inventory

| role | current shape scan | quality bar / supplemental calibration | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| topic_navigation | `README.zh.md` 给出阅读路径、默认恢复入口、常用命令和 production doc 链接 | canonical navigation role | adopted | `README.zh.md` | none |
| testing_overview | `README.zh.md`、`doc/phases/README.zh.md`、本 result 的实现/board/证据段落共同说明测试入口、板卡入口和证据边界 | canonical quality bar，未单列文件但读者路径清楚 | merged: `README.zh.md#阅读路径` + `doc/phases/README.zh.md` + 本文件 | 无独立 testing-overview 文件；已由导航和 phase suite 覆盖 | none |
| correctness_tests | `doc/moment_of_inertia_estimation-evaluation.zh.md` 的当前诊断证据链与 `src/test_moi.cpp` 的 helper / fallback 对拍说明 | canonical quality bar，评估文档承担 correctness 字典和证据链 | merged: `doc/moment_of_inertia_estimation-evaluation.zh.md#当前诊断证据链` | 无独立 correctness-tests 文件；正确性说明已可从 evaluation + test 源码恢复 | none |
| benchmark_and_evidence | `doc-rvv/features/moment_of_inertia_estimation-RVV.zh.md` 的 Bench 与证据、以及本 result 的 Board 结果 | canonical quality bar，bench label / case-filter / repeated board / doctor 边界已分层 | merged: `doc-rvv/features/moment_of_inertia_estimation-RVV.zh.md#Bench 与证据` + 本文件 `## Board 结果` | 无独立 benchmark-and-evidence 文件；证据路径已可复核 | none |
| optimization_evidence | `doc/phases/optimization-matrix.zh.md` 和 `doc/optimization-roadmap.zh.md` 记录候选、状态和恢复条件 | canonical quality bar，优化前沿和证据状态分工清楚 | merged: `doc/phases/optimization-matrix.zh.md` + `doc/optimization-roadmap.zh.md` | 无独立 optimization-evidence 文件；当前采纳/暂缓状态已写入矩阵与 roadmap | none |
| optimization_roadmap | `doc/optimization-roadmap.zh.md` 保留搜索空间、阶段反思和恢复条件 | canonical quality bar | adopted | `doc/optimization-roadmap.zh.md` | none |
| test_support_code_map | `doc/moment_of_inertia_estimation-evaluation.zh.md` 的 Traceability Map 与 `doc-rvv/features/moment_of_inertia_estimation-RVV.zh.md` 的 Traceability Map 互相定位 | canonical quality bar，代码/测试/脚本/输出定位已覆盖 | merged: `doc/moment_of_inertia_estimation-evaluation.zh.md#Traceability Map` + `doc-rvv/features/moment_of_inertia_estimation-RVV.zh.md#Traceability Map` | 无独立 test-support-code-map 文件；读者仍可从文档跳到 `src/`、script 和 summary 路径 | none |
| phase_index / phase_plan / phase_result / optimization_matrix | `doc/phases/README.zh.md`、各 phase plan/result 和 optimization matrix 已覆盖 | canonical quality bar | adopted | `doc/phases/README.zh.md`、本文件、`doc/phases/optimization-matrix.zh.md` | none |
| evaluation_diagnostic / evaluation_production | `doc/moment_of_inertia_estimation-evaluation.zh.md` 负责函数级评估、诊断证据链和 production 接入判断 | canonical quality bar | adopted | `doc/moment_of_inertia_estimation-evaluation.zh.md` | none |
| production_topic_doc | `doc-rvv/features/moment_of_inertia_estimation-RVV.zh.md` 只保留 adopted production 行为、证据链和长期边界 | canonical quality bar | adopted | `doc-rvv/features/moment_of_inertia_estimation-RVV.zh.md` | none |

### target_granularity_audit

| target class | current coverage | decision | evidence |
| --- | --- | --- | --- |
| correctness aggregate | `make run_test_compare` 一条总入口覆盖 Std/RVV 对拍、helper 命中和 fallback isolation | adopted | `test-rvv/features/moment_of_inertia_estimation/README.zh.md`、`src/test_moi.cpp`、本 result 实现结果 |
| correctness aliases | 目前没有拆成更多 correctness 别名；fallback/typed/producer coverage 直接并入总入口与 helper 命中测试 | not_applicable with evidence | 只有一个稳定 aggregate compare，额外别名不会改变证据边界 |
| bench diagnostic aliases | `moi_reductions`、`moi_projected_covariance`、typed public compute case-filter | adopted | `src/bench_moi.cpp`、`doc-rvv/features/moment_of_inertia_estimation-RVV.zh.md#Bench 与证据` |
| QEMU smoke aliases | `run_test_compare` 作为 correctness / log-shape smoke，不把 QEMU timing 当性能结论 | adopted | README 常用命令 + evaluation / result 证据分层 |
| board smoke aliases | 当前没有独立单次 smoke alias；board 结果直接以 repeated target 和 summary 作为决策输入 | not_applicable with evidence | 该 topic 的 board 证据已由 repeated target 覆盖 |
| board repeated aliases | `run_board_moi_phase040_projected_covariance_repeated`、`run_board_moi_phase050_*_repeated` | adopted | phase040 / phase050 summary 与 Evidence Doctor |
| doctor / registry aliases | `evidence_status_phase040`、`evidence_status_phase050` | adopted | 本 result、phase040/050 summary、registry fresh |
| historical probe guarded aliases | phase030 mean/AABB-only probe 仅保留为历史拒绝证据 | adopted | `doc/phases/030-production-mean-aabb-pi2-pi5/result.zh.md`、`doc/optimization-roadmap.zh.md` |

### artifact_tracking_status

- `README.zh.md`、`doc/phases/README.zh.md`、`doc/phases/050-point-type-expansion/result.zh.md`、`doc/phases/optimization-matrix.zh.md`、`doc/optimization-roadmap.zh.md`、`doc/moment_of_inertia_estimation-evaluation.zh.md`、`doc-rvv/features/moment_of_inertia_estimation-RVV.zh.md` 和 `doc-rvv/library-screening/features/features-function-evaluation-queue.zh.md` 都是 tracked / to-be-staged 变更。
- 本 topic 目录下没有新增未跟踪的 doc-suite 文件。
- 这组路径已经足够让下一轮 worker 仅凭文档恢复当前证据边界，不需要再查聊天记录。

## 下一步

默认暂停，保留当前 production patch 和 phase040/050 证据链。若后续要继续，只建议用新 phase 单独打开 custom point type / layout expansion。
