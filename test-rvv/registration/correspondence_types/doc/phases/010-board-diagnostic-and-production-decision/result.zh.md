# Phase 010 结果：board-diagnostic-and-production-decision

> 后续更新：Phase 011 已按用户反馈补真实 production helper（生产源码 helper）边界的
> production probe（生产路径探针）。该 probe 仍为 `negative` bucket，并已回退临时 production patch。
> 因此 Phase 010 的 no-production 方向没有反转；Phase 011 是当前恢复入口。

## 实际执行范围

本阶段在板卡可用后补齐 `registration/correspondence_types` 的目标硬件证据：先确认 board correctness（板卡正确性），再对 `strided-index-extract` candidate family（跨步索引抽取候选族）做 5 轮 repeated board benchmark（重复板卡性能测试），最后用 topic-local manifest（主题本地证据清单）和 Evidence Doctor（证据体检）解释结果。

本阶段没有修改 production（生产源码）。`registration/include/pcl/registration/impl/correspondence_types.hpp` 继续保持标量实现。

`distance-stats-reduction` 没有继续上板。原因是 index extraction 是本 topic 最低风险、最接近 production 的候选族；它在板卡上已经稳定落入 `negative` decision bucket（负向决策桶）。distance stats 还带有 reduction tree（规约树）和数值预算风险，即使补跑也不能改变当前不接 production 的主结论。

## 计划动作回填

| action | status | 命令 / 证据路径 | 结论 |
| --- | --- | --- | --- |
| B1 board 连接和部署 | done | `make check_board_ssh` | 板卡 SSH 可用，后续 board target 能执行。 |
| B2 board correctness | done | `make run_board_test fetch_board_logs`；`log/board/run_test.log` | RVV test 在板卡上 6/6 通过。 |
| B3 board index bench | done with bounded rerun | `make run_board_bench_compare fetch_board_logs BENCH_ARGS="--case-filter index-extract --iterations 20 --warmup-iterations 5"`，5 个 run-labelled 目录 | Std/RVV checksum 全部一致，但 3 个 case 的 median 都没有达到 `weak_positive`。 |
| B4 manifest / doctor | done with downgrade | `script/generate_board_repeated_summary.py`；`make run_evidence_doctor_board_index_extract`；`log/board/010-board-diagnostic/index-extract/evidence_manifest.json`、`evidence_doctor.md` | Evidence Doctor 输出 Errors=3、Warnings=1、Suggestions=0；这些 finding 作为 no-production 降级信号处理。 |
| B5 文档回填 | done with publication-boundary correction | 本文件、`doc/phases/optimization-matrix.zh.md`、`doc/optimization-roadmap.zh.md`、`doc/correspondence_types-evaluation.zh.md` | 当前证据已刷新为 board-negative / no-production；不发布 `doc-rvv` 长期主题文档。 |

## 板卡正确性

板卡 correctness 使用 RVV test binary，结果为 6/6 passed：

- 证据路径：`test-rvv/registration/correspondence_types/log/board/run_test.log`
- 覆盖：query index 顺序、match index sentinel（哨兵值）和顺序、空输入 fallback、distance stats 一般样本、高动态范围样本、`n==1` 既有 NaN 边界。
- 证据角色：correctness（正确性）和板卡可运行性，不是性能证据。

## Board Repeated Summary

本阶段正式采用 5 个 run-labelled 批次作为 bounded rerun budget（有界复跑预算）。每个 run 使用 `--case-filter index-extract --iterations 20 --warmup-iterations 5`。`B/A = Std ms / RVV ms`，大于 1 表示 RVV 更快。

| case | speedup values | median | min | max | checksum | bucket |
| --- | --- | ---: | ---: | ---: | --- | --- |
| `match-index candidate 64K` | `0.983, 0.989, 0.934, 0.933, 0.959` | 0.959 | 0.933 | 0.989 | match | `negative` |
| `query+match candidate 256K` | `0.966, 0.967, 0.953, 0.960, 0.948` | 0.960 | 0.948 | 0.967 | match | `negative` |
| `query-index candidate 4K` | `0.966, 0.987, 1.048, 0.987, 0.872` | 0.987 | 0.872 | 1.048 | match | `negative` |

证据主归属：

- summary artifact（摘要证据产物）：`log/board/010-board-diagnostic/index-extract/summary.md`
- manifest（本地机器输入，默认不提交）：`log/board/010-board-diagnostic/index-extract/evidence_manifest.json`
- Evidence Doctor：`log/board/010-board-diagnostic/index-extract/evidence_doctor.md`
- raw run logs（原始运行日志）：`log/board/010-board-diagnostic/index-extract/run-001` 到 `run-005`，默认只作为本机证据，不进入提交边界。

## Evidence Doctor 结果

Evidence Doctor 输入为 `log/board/010-board-diagnostic/index-extract/evidence_manifest.json`，输出为 `log/board/010-board-diagnostic/index-extract/evidence_doctor.md`。

| severity | signal | case | 处理动作 |
| --- | --- | --- | --- |
| Error | `ba_degradation_frequency` | `match-index candidate 64K` | 5/5 低于 1，作为稳定负向证据；不允许写 production speedup。 |
| Error | `ba_degradation_frequency` | `query+match candidate 256K` | 5/5 低于 1，作为稳定负向证据；不进入 PI1。 |
| Error | `ba_degradation_frequency` | `query-index candidate 4K` | 4/5 低于 1，且 median 未达 `weak_positive`；只保留为负向诊断。 |
| Warning | `long_tail_or_variance` | `query-index candidate 4K` | 保留 min/median/max，不剔除异常值；主 case 已稳定负向，因此不追加 20-run / 50-run。 |

这些 Error 不表示 correctness bug（正确性缺陷）。它们表示当前 board repeated evidence 不能支撑 production evidence（生产证据）或严格性能结论，结论必须降级为 `rollback/no-production`。

## 优化矩阵更新

| candidate family | 状态 | 当前证据 | 结论 |
| --- | --- | --- | --- |
| `strided-index-extract` | `rejected_by_board_negative` | QEMU correctness 通过，asm smoke 出现目标指令，5-run board median 分别为 0.959、0.960、0.987 | 当前不接 production。负向来源可能包括内存带宽、跨步加载开销、`vsetvli` / 寄存器压力、短 inline helper 的标量 baseline 已足够短；没有 profile 或消融时不做单因归因。 |
| `distance-stats-reduction` | `not_run_after_index_negative` | Phase 000 correctness 通过，QEMU smoke 与 asm smoke 只支撑诊断层 | 不作为当前 topic 的 production 候选。继续它需要单独数值预算、反汇编归属和板卡 A/B，且不会改变 index extraction no-production 结论。 |
| `production-dispatch` | `rejected_no_production` | 无 production patch；无 production direct / fallback / production asm attribution；主候选板卡负向 | 不进入 production integration loop。 |
| `compiler-auto-vectorization-report` | `not_required` | 方向已由 board repeated 证据闭合为负向 | 不运行 `generate_vec_report`；它不能改变目标硬件负向结论。 |

## 分层证据结论

| 证据层 | 状态 | 说明 |
| --- | --- | --- |
| correctness | pass | QEMU `make run_test_compare` 已通过；板卡 `run_board_test` 也 6/6 通过。 |
| QEMU path / log shape | pass as smoke | QEMU 只证明构建、路径、checksum 和日志形状；不使用 QEMU timing 做性能结论。 |
| asm smoke | partial pass | bench 二进制含 `vlse32.v`、`vse32.v`、`vfmul.vv`；不证明 production helper 符号命中。 |
| board performance | negative | 5-run index extraction repeated summary 稳定低于 `weak_positive` 门槛。 |
| production direct | not_started / no longer recommended | 板卡负向后不写 production dispatch、fallback test 或 production bench。 |

## Evidence Freshness 和 Registry

本阶段复跑改变了 Phase 000 中的 `board performance=missing / blocked` 状态。当前文档已刷新到 board-negative / no-production：

- `doc/phases/010-board-diagnostic-and-production-decision/result.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/correspondence_types-evaluation.zh.md`

当前结论没有 adopted production behavior（已采用的生产行为）或 production patch（生产补丁），因此
`artifact_layout.topic_doc_template` 对本 no-production closeout 判为 `not_applicable`。诊断证据链的主归属是
本阶段结果、optimization matrix、roadmap 和 evaluation。

`log/evidence_registry.json` 尚未接入，`evidence_registry_status=not_available`。人工 freshness（证据新鲜度）检查路径为：

- QEMU summary：`log/qemu/evidence_manifest.json`、`log/qemu/evidence_doctor.md`
- board summary：`log/board/010-board-diagnostic/index-extract/summary.md`
- board manifest（本地机器输入，默认不提交）：`log/board/010-board-diagnostic/index-extract/evidence_manifest.json`
- board doctor：`log/board/010-board-diagnostic/index-extract/evidence_doctor.md`
- board correctness：`log/board/run_test.log`

## 测试支撑结构审计

当前 topic 已采用配置解析出的轻量结构：

- test / bench 源码位于 `src/`。
- 聚合入口位于 `include/correspondence_types.h`。
- 内部 helper 位于 `include/impl/correspondence_types_candidates.hpp`，当前 271 行，没有超过 helper split 阈值。
- topic-local manifest / summary 脚本位于 `script/`。
- 不存在旧 `test_support/` 目录、legacy pointer（旧路径指针）或 compatibility alias（兼容别名）。

与同模块成熟 topic 相比，本 topic 是小型 no-production diagnostic：evaluation、phase 文档、optimization roadmap 和 optimization matrix 已经能定位 production helper、test support、bench wrapper、script 和 output summary。完整 README + testing overview + correctness-tests + benchmark/evidence + code-map 文档套件判为 `not_required with evidence`，不是 `phase_deferred + unblocked`。

## 继续 / 停止决策

当前 phase 完成，EvidenceDecision 为 `rollback/no-production`。停止命中条件：

- 主候选 `strided-index-extract` 的 5-run board repeated bucket 为 `negative`。
- Evidence Doctor 的退化频率 Error 已解释为 production 降级信号。
- 继续写 production 需要正向板卡证据和用户授权；当前二者不同时满足。
- 当前 topic 测试资产、summary、manifest、doctor、phase result、matrix、roadmap 和 evaluation 已经同步到当前证据状态。
- 没有仍在当前授权范围内、能改变 production decision 的 high-priority unblocked action。

`next_phase_default`：`ready_for_review_no_production`。若用户仍想继续当前 topic，推荐另开一个显式 profiling / ablation（性能剖析 / 消融）小阶段，只用于解释负向原因，不作为默认生产接入路径。
