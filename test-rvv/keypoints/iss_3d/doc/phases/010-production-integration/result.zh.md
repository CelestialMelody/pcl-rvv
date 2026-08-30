# Phase 010 Result: Production Integration

## 执行范围

本阶段把 Phase 000 的 f64 scatter candidate 接入 `keypoints/include/pcl/keypoints/impl/iss_3d.hpp` 作为 bounded production probe（有界生产探针）。真实公开入口是 `ISSKeypoint3D::compute()` 进入 `detectKeypoints`，再调用 `getScatterMatrix`。RVV 只覆盖邻域索引已经生成后的 scatter 累加，不覆盖 `searchForNeighbors`、Eigen EVD、NMS 或 output。

## Production Patch Scope

| 文件 / 符号 | 状态 | 说明 |
| --- | --- | --- |
| `pcl::detail::computeISSScatterMatrixRVV<PointT>` | historical probe removed | probe 期间 `__RVV10__` 下使用 xyz AoS traits gate、32-bit byte offset gate 和 `neighbor_count >= 16` gate。 |
| `ISSKeypoint3D::getScatterMatrix` | scalar-only current source | probe 期间 min-neighbor check 后先尝试 RVV helper，失败则执行原标量 loop；当前源码已移除该 dispatch。 |
| `reduceISSScatterF64` | historical probe removed | probe 微调后 6 个 covariance 槽位各规约一次，再填对称矩阵；当前源码不保留。 |
| public API | unchanged | 未改类接口、模板参数和输出类型。 |

## 计划动作回填

| action | status | evidence |
| --- | --- | --- |
| T1 production asm RED | done | probe 期间 production patch 前 `check_iss_3d_production_rvv_asm` 失败，因为 helper 不存在；当前 Makefile 已删除该 active target。 |
| T2 production correctness | done | `run_test_compare` 通过 Std/RVV 4 个测试。 |
| T3 production asm | done | probe 期间 `check_iss_3d_production_rvv_asm` 通过，重建 bench binary 后确认；当前生产源码已回到标量路径，active asm gate 只保留 diagnostic `check_iss_3d_rvv_asm`。 |
| T4 production public board | done | 原始 run `iss_3d_phase010_production_public_repeated` 为 neutral 且有退化频率 Error。 |
| T4b reduction shape micro-opt | done | probe 期间新增 `check_iss_3d_production_reduction_shape`，RED saw 10，修正后通过 saw 7；撤回 production patch 后该 target 不再作为当前 Makefile 入口保留。 |
| T4c micro-opt production repeated | done | `iss_3d_phase010_production_public_reduction_shape` 5-run 完成并登记。 |
| T5 Evidence Doctor / registry | done | 当前 production run Errors=0、Warnings=0、Suggestions=3；registry fresh for current summary files。 |

## Production Public Evidence

| run label | median | min | max | B/A < 1 | doctor | decision |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| `iss_3d_phase010_production_public_repeated` | 1.005x | 0.996x | 1.021x | 2/5 | Errors=1，Suggestions=3 | historical neutral，不能采纳。 |
| `iss_3d_phase010_production_public_reduction_shape` | 1.014x | 1.005x | 1.022x | 0/5 | Errors=0，Warnings=0，Suggestions=3 | current neutral，不建议采纳。 |

当前 run 的 summary 是 `test-rvv/keypoints/iss_3d/log/board/repeated_phase010_production_public_reduction_shape/summary.md`，doctor 是 `test-rvv/keypoints/iss_3d/log/board/repeated_phase010_production_public_reduction_shape/evidence_doctor.md`。

## Evidence Doctor 回填

当前 production public run 没有 Error 或 Warning，但有 3 个 Suggestion：环境 metadata 缺失、binary identity 缺失和 near-threshold B/A（接近阈值的 B/A）。这些 suggestion 不阻塞当前阶段 closeout，但要求把结论降级为 neutral / weak probe，而不是 adopted production。

## Diagnostic-To-Production Mismatch Audit 回填

| question | answer |
| --- | --- |
| evidence role | production_public。 |
| A/B boundary | public `compute()`；summary Markdown 已修正为 `public_compute`。 |
| 当前决策问题 | 当前 RVV scatter probe 是否让 public compute 足够快，值得采纳。 |
| diagnostic 是否可外推 | 不能。Phase 000 的 1.6x 局部收益在 public compute 中降到 1.014x。 |
| comparison-boundary / baseline mismatch | 已发生：public boundary 包含 search、EVD、NMS 和 output。 |
| 弱 / 中性时是否继续 probe | 已完成一次 bounded rerun budget；不继续无限复跑。 |
| clean adoption 是否成立 | 不成立。production bucket 为 neutral。 |

## EvidenceDecision

本阶段 EvidenceDecision 是 `rollback/no-production`。用户本轮允许“有收益即可采纳”，但当前证据的正式 decision bucket 是 `neutral`，收益接近 1.0 且 public checksum 为 0；这不满足“真的值得接入”的判断。为进入提交流程，当前 production 源码已回到原标量路径，历史 probe 只保留为 topic-local 证据。

## Doc Suite Parity Audit

| area | current shape scan | quality bar | decision | evidence | next action |
| --- | --- | --- | --- | --- | --- |
| README navigation | 已新增 topic README。 | README 应提供当前结论、阅读路径、命令和证据白名单。 | adopted | `README.zh.md` | none |
| testing overview | 已新增测试总览。 | 需说明 target 粒度、覆盖矩阵和证据边界。 | adopted | `doc/testing-overview.zh.md` | none |
| correctness tests | 已新增 gtest 字典。 | 需说明输入、路径、断言和不覆盖范围。 | adopted | `doc/correctness-tests.zh.md` | none |
| benchmark/evidence | 已新增 bench 和 evidence 文档。 | 需说明 case、run label、doctor、registry 和提交边界。 | adopted | `doc/benchmark-and-evidence.zh.md` | none |
| optimization evidence | 已新增候选取舍文档。 | 需区分 adopted / attempted / rejected / deferred。 | adopted | `doc/optimization-evidence.zh.md` | none |
| test support code map | 已新增代码地图。 | 需能定位 helper、src、script、production 和 output。 | adopted | `doc/test-support-code-map.zh.md` | none |
| evaluation | 已新增主评估文档。 | 需承载 EvidenceDecision、Traceability Map 和 production 判断。 | adopted | `doc/iss_3d-evaluation.zh.md` | none |
| production topic doc | 不创建。 | 只有 adopted production behavior 或用户确认保留 patch 后适用。 | not_applicable with evidence | production evidence neutral and current source scalar-only | 用户若决定重新引入并采纳 patch，再创建 `doc-rvv/keypoints/iss_3d-RVV.zh.md`。 |
| artifact tracking | topic 路径包含 untracked 新文档。 | 未提交前必须显式列入 topic artifact boundary。 | adopted | Handoff 和 final status 列出。 | none |

## Continue / Stop Decision

`stop_condition_hit`：bounded rerun budget 已用完，production public decision bucket 仍是 neutral；继续优化需要新的 profile、非零输出 public case 或用户明确决定重新引入 near-threshold patch。当前 topic 内没有仍值得自动推进的 high-priority unblocked candidate。下一默认动作是提交 no-production closeout；`doc-rvv` 不适用。
