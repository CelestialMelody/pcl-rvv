# Phase 030 Result: score-generation component split

## 当前结论

EvidenceDecision：`attempted-neutral / stop-current-topic-no-production`。

Phase 030 把 Phase 020 的真实 `RangeImage` score generation 边界拆成两个 component ablation（组件消融）case：`extractLocalSurfaceStructure()` 以及 local surface 已经预先生成后的 `extractBorderScoreImages()`。板卡 5-run repeated 结果显示两个 case median 均为 `1.000x`，且各有 1/5 低于 1。按 Phase 030 plan 的 decision bucket，二者都属于 `neutral`。

这说明 Phase 000 / 010 的 score-update 局部 `2.560x` / `2.210x` 无法穿透到真实 `RangeImage` score-generation 边界；继续把 `getNeighborDistanceChangeScore()` 或当前 score-update helper 做生产接入，不满足“有收益即可采纳”的前提。当前 topic 不建议继续 production RVV 接入，也不创建 `doc-rvv/features/range_image_border_extractor-RVV.zh.md`。

## 计划回填

| action | result | evidence |
| --- | --- | --- |
| component bench cases | done | `src/bench_range_image_border_extractor.cpp` 新增 `range_image_local_surface_160x120` 和 `range_image_border_scores_after_surface_160x120` |
| QEMU smoke | done | `make run_bench_compare ALLOW_QEMU_BENCH_COMPARE=1 BENCH_ARGS='--case-filter range_image_local_surface_160x120,range_image_border_scores_after_surface_160x120 --repeat 1 --iterations 1 --warmup 0'`；只验证输出形状和 checksum，不作性能结论 |
| board repeated + Doctor | done | `log/board/repeated_phase030_score_generation_component_split/{summary.md,evidence_manifest.json,evidence_doctor.md}` |
| EvidenceDecision | done | 当前 score-generation split 为 neutral；不进入 production integration loop |

## Board repeated summary

板卡命令：

```bash
make board_repeated \
  REPEATED_BOARD_TAG=phase030_score_generation_component_split \
  REPEATED_BOARD_REMOTE_TAG=phase030_score_generation_component_split \
  REPEATED_BOARD_RUN_LABEL=range_image_border_extractor_phase030_score_generation_component_split_repeated \
  REPEATED_BOARD_CASE_NAME= \
  RIBE_REPEATED_BENCH_ARGS='--case-filter range_image_local_surface_160x120,range_image_border_scores_after_surface_160x120 --repeat 1 --iterations 10 --warmup 2'
```

| case | runs | median speedup | values | checksum | decision |
| --- | ---: | ---: | --- | --- | --- |
| `range_image_local_surface_160x120` | 5 | `1.000x` | `0.990x, 1.000x, 1.010x, 1.000x, 1.000x` | `-26817.5` both sides | neutral |
| `range_image_border_scores_after_surface_160x120` | 5 | `1.000x` | `1.000x, 1.000x, 1.010x, 0.990x, 1.000x` | `24039.1` both sides | neutral |

Evidence Doctor：`Errors=0, Warnings=2, Suggestions=4`。Warnings 均为 `ba_degradation_frequency`，两个 case 各有 1/5 低于 1；Suggestions 为缺少 taskset / governor / freq / temperature 环境字段，以及 median 贴近 1.0 阈值。处理方式：结果降级为 neutral，不作为 production adoption 证据；不因单轮 `1.010x` 噪声继续扩大 runs。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | test helper / production-shaped helper；两侧都链接 production `range_image_border_extractor.cpp`，但 RVV 仍只存在于 test-only score-update helper |
| 当前决策问题 | implementation-shape；判断 score-update / neighbor-score 是否值得进入 production integration loop |
| diagnostic 是否可外推到 production | 可外推到全有限 160x120 `RangeImage` fixture 的 score-generation 子边界；不能外推到完整 `computeFeature()` 的 shadow/veil 和输出构造 |
| comparison-boundary / baseline mismatch 风险 | medium；未覆盖 inf / max range / unobserved 分支、真实 PCD 输入、并行 OpenMP 配置和完整 public output |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段不允许；after-surface 和 local-surface 都是 neutral，没有形成值得生产探针的收益信号 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes；当前未接 production，没有 clean adoption 条件 |

## Doc-suite closeout audit

| area | current shape scan | quality bar / supplemental calibration | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| README navigation | `README.zh.md` 给出当前结论、阅读路径、常用命令和 production topic doc 适用性 | `doc-suite-quality-bar.zh.md` 要求能从 topic navigation 找到证据白名单和默认恢复入口 | adopted | 当前 README 已标明 `attempted-neutral / no-production`，并说明 `build/`、raw log 和 registry 默认不提交 | none |
| testing-overview | `doc/testing-overview.zh.md` 记录 correctness、bench、QEMU、board 和 Doctor target 分类 | 需要按当前 `Makefile`、`board.mk`、test / bench / script 抽取真实 target | adopted | target 粒度审计已落到 testing overview 和 benchmark/evidence；不存在未记录的 production-direct target | none |
| target granularity audit | `Makefile` / `board.mk` 提供 QEMU correctness、board smoke、board repeated、Doctor 和 registry 入口 | 复杂 topic 需要区分 aggregate、alias、board repeated、doctor / registry 与历史 guarded probe | adopted | 当前 topic 没有 production patch，也没有 historical production probe；Phase 000-030 的 repeated target 都有 run label 和路径 | none |
| correctness-tests | `doc/correctness-tests.zh.md` 覆盖 score-update、four-image pipeline 和 RangeImage fixture gtest | 需要说明每类 TEST 的输入、被测路径、断言和不能证明的范围 | adopted | QEMU Std/RVV correctness 3/3 通过；public `computeFeature()` output oracle 未覆盖，已作为 no-production 边界记录 | none |
| benchmark-and-evidence | `doc/benchmark-and-evidence.zh.md` 记录 case-filter、checksum、board summary、manifest、Doctor 和提交边界 | 性能结论必须来自板卡；QEMU 只作 smoke / checksum / log-shape | adopted | Phase 000-030 summary / manifest / Doctor 路径齐全；raw logs、`build/` 和 `log/evidence_registry.json` 不提交 | none |
| optimization-evidence | `doc/optimization-evidence.zh.md` 把 candidate family 映射到 board、asm、Doctor 和 decision | attempted / rejected / not_applicable 结论必须能回到证据路径 | adopted | score-update / four-image 为 diagnostic-only positive；RangeImage / component split 为 neutral；production 接入被证据化拒绝 | none |
| test-support-code-map | `doc/test-support-code-map.zh.md` 能定位聚合头、internal helper、test / bench 源码、script 和 evidence output | 复杂 topic 需要 helper 职责和 output contract 可追踪 | adopted | `include/impl` 已按测试支撑职责拆分；production helper 仅作为标量语义来源被引用 | none |
| evaluation | `doc/range_image_border_extractor-evaluation.zh.md` 承载 S2、Traceability Map、诊断证据链和 no-production 判断 | diagnostic topic 的 evaluation 负责取舍审计，不创建长期 production 文档 | adopted | evaluation 已写明当前结论不证明完整 public path 永远不可优化，只关闭当前 score-update / neighbor-score 路线 | none |
| long-term `doc-rvv` | `doc-rvv/features/range_image_border_extractor-RVV.zh.md` 不存在 | 只有 adopted production behavior、production patch 或 PI5 证据闭环后才适用 | not_applicable with evidence | 本 topic 未修改 production，也未进入 PI1-PI5；当前证据不支持生产接入 | none |
| phase index / result | `doc/phases/README.zh.md`、本 result 和 `optimization-matrix.zh.md` 记录 Phase 000-030 | phase result 必须记录继续 / 停止判断和默认恢复入口 | adopted | matrix 无当前授权范围内的 recommended unblocked next action；shadow/veil 状态机需另建 profile / oracle phase | none |
| artifact tracking | topic docs / tests 为 to-be-staged；summary / manifest / Doctor 作为 doc-referenced summary evidence 精确提交；raw logs local-only | README、evaluation、roadmap、phase result 引用的新增文件必须存在并进入提交边界或明确排除 | adopted | 路径限定扫描发现新增 topic assets；提交策略为 topic-only + doc-referenced summary artifacts，排除 raw logs、`build/`、registry 和 worklog | none |
| production dispatch / fallback | production source diff 为空；没有新增 RVV dispatch、fallback gate 或 public API | production 接入后才需要 fallback matrix 和 dispatch 审计 | not_applicable with evidence | `features/include/pcl/features/impl/range_image_border_extractor.hpp` 与 `features/src/range_image_border_extractor.cpp` 未修改 | none |

commit_preferences：`topic-only + doc-referenced summary artifacts`。提交范围只包括当前 topic 的测试资产、topic-local 文档、筛选状态表中 range image border 行，以及 Phase 000-030 的 `summary.md` / `evidence_manifest.json` / `evidence_doctor.md`。不提交 raw run logs、QEMU logs、`build/`、`log/evidence_registry.json`、worklog 或其它 topic / `.agents` 脏变更。

## 继续 / 停止决策

`continue_stop_decision`：暂停当前 topic。停止条件是当前 phase 矩阵和 roadmap 中已授权、低风险、可审查的 score-update / score-generation 路线均已完成或被 neutral 证据降级；继续需要转向 shadow/veil 状态机或完整 public-output oracle，这会引入较高语义风险和新的 oracle/profile 前置工作，不建议作为本 topic 的自动继续优化。

`next_phase_default`：无当前 topic 内推荐继续 phase。若未来要重开，应先另建 profile-driven public workload / shadow-veil oracle phase，证明 `classifyBorders()` 或 `findAndEvaluateShadowBorders()` 是主成本，再考虑有界状态机 RVV 探针。
