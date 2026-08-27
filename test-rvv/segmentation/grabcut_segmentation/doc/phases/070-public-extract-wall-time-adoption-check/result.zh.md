# Phase 070: public extract wall-time adoption check result

## 阶段结论

本阶段完成接入后的 production-public（真实公开入口）补证据。当前 production patch 已由用户确认保留 / 采纳；
`public_extract` case 通过真实 `GrabCut<PointXYZRGB>::setBackgroundPointsIndices()` 和 `extract()` 入口测量
完整 refine loop（迭代细化循环）wall time（总耗时）。Milkv-Jupiter clean 5-run repeated board 结果为 positive：
B/A 为 `1.124687, 1.110452, 1.112645, 1.113482, 1.112866`，median 为 `1.112866x`，Std/RVV checksum 一致，
Evidence Doctor（证据体检）为 `Errors=0, Warnings=0, Suggestions=0`。

该结果说明已采纳的 `initGraph()` unknown trimap terminal weight（未知 trimap 端点权重）RVV helper
在完整公开入口中仍有可见收益。它不能把收益单独归因到 n-link（邻接边权重）、GMM learn（高斯混合模型学习）
或 max-flow solver（最大流求解器），也不能证明新增 RVV family（实现族）优于当前已采纳 helper。

## 计划动作回填

| 计划动作 | 状态 | 证据 / 路径 | 结论 |
| --- | --- | --- | --- |
| 新增 public bench case | done | `src/bench_grabcut.cpp`，`--case public_extract` | Std/RVV 构建都能运行真实公开入口并输出稳定 `BENCH grabcut_component` 行。bench 入口把 PCL 日志级别调到 `L_ERROR`，避免 `PCL_INFO` refine 日志污染计时。 |
| 更新 manifest case label | done | `script/generate_grabcut_board_evidence_manifest.py` | `public_extract` 标记为 `production-public`，计时边界为 `setBackgroundPointsIndices_plus_extract_full_refine_loop`。 |
| 新增 repeated target 和 registry | done | `Makefile`、`log/evidence_registry.json` | `make run_public_extract_repeated_evidence_doctor` 已登记 Phase 070 manifest / Doctor summary。 |
| QEMU log-shape smoke | done | `make run_bench_std run_bench_rvv analyze_bench_compare BENCH_ARGS="--width 32 --height 24 --iterations 1 --warmup 0 --case public_extract" ALLOW_QEMU_BENCH_COMPARE=1` | Std checksum = RVV checksum = `13677801866028019573`；QEMU timing 不作为性能结论。 |
| production-public board repeated | done | `doc/phases/070-public-extract-wall-time-adoption-check/repeated-board-20260827-clean-96x72/` | 5-run repeated 完成，bucket 为 positive。 |
| Evidence Doctor / registry | done | `doc/phases/070-public-extract-wall-time-adoption-check/repeated-evidence-manifest.json`、`doc/phases/070-public-extract-wall-time-adoption-check/repeated-evidence-doctor.md`、`doc/phases/070-public-extract-wall-time-adoption-check/repeated-evidence-doctor.json` | `Errors=0, Warnings=0, Suggestions=0`；summary-only 证据已登记。 |

## 板卡 repeated summary

| run label | Std avg ms | RVV avg ms | B/A |
| --- | ---: | ---: | ---: |
| run01 | 1601.091959 | 1423.589112 | 1.124687 |
| run02 | 1598.154334 | 1439.192626 | 1.110452 |
| run03 | 1583.399265 | 1423.095251 | 1.112645 |
| run04 | 1601.263209 | 1438.067778 | 1.113482 |
| run05 | 1587.352542 | 1426.363959 | 1.112866 |

统计口径：5-run、`--width 96 --height 72 --iterations 3 --warmup 1 --case public_extract`，
设备为 Milkv-Jupiter。Std median 为 `1598.154334 ms`，RVV median 为 `1426.363959 ms`，
B/A median 为 `1.112866x`。所有 run 的 Std/RVV checksum 均为 `9089139176994405943`。

板卡输出仍有 `script/rvv-board-run.mk` clock skew（时钟偏移）warning。该 warning 来自板卡文件时间戳，
没有改变 `BENCH` 行、checksum、manifest 或 Evidence Doctor 结果，因此记录为环境噪声，不降级当前
production-public performance conclusion（真实公开入口性能结论）。

## 320x240 partial / too-heavy smoke

先前按计划初值尝试 `--width 320 --height 240 --iterations 3 --warmup 1 --case public_extract`。该规模每
iteration 约 28-34s，并且中止后只留下部分完整 run：run01/run02 有 Std/RVV 和 analyze summary，
run03/run04 缺 Std/analyze 或为空文件。因此该目录
`doc/phases/070-public-extract-wall-time-adoption-check/repeated-board-20260827-162304/`
只作为 too-heavy smoke（过重冒烟）和工具预算背景，不作为 repeated performance closeout。另一个
`repeated-board-20260827-96x72/` 历史目录为早期手工 run，当前 canonical repeated evidence（规范重复证据）
使用 `repeated-board-20260827-clean-96x72/`。

完整的 run01/run02 仍显示正向趋势：B/A 约 `1.18x` 与 `1.17x`，checksum 一致。这一趋势与 96x72 的
5-run positive 一致，但由于 repeated run 不完整，不进入当前 Evidence Doctor 结论。

## diagnostic-to-production mismatch audit

| question | result |
| --- | --- |
| evidence role | `production-public`：真实公开入口 `setBackgroundPointsIndices()` + `extract()`。 |
| A/B boundary | public method boundary（公开方法边界）：Std 构建 vs RVV 构建，同输入、同 case、同 checksum policy。 |
| 当前决策问题 | 已采纳 terminal helper 在完整公开流程中是否仍有可见收益，以及该收益是否支持继续保留 production patch。 |
| diagnostic 是否可外推到 production | 不依赖 diagnostic 外推；本阶段直接测 production public。 |
| comparison-boundary / baseline mismatch 风险 | 完整流程包含 `fitGMMs`、`learnGMMs`、n-link graph edge、max-flow 和输出聚类，因此不能把 `1.112866x` 逐项归因到单个 helper。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不适用；当前 production-public result 为 positive，且 production patch 已由用户确认保留。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前只有一个已采纳 RVV family；若后续新增 color staging、n-link 或 solver family，必须补同边界 RVV-vs-RVV A/B。 |

## Evidence Doctor 和 registry

| 文件 | 角色 |
| --- | --- |
| `doc/phases/070-public-extract-wall-time-adoption-check/repeated-evidence-manifest.json` | production-public repeated manifest；记录 run_count、warmup、B/A、checksum、timer boundary 和 repeated dir。 |
| `doc/phases/070-public-extract-wall-time-adoption-check/repeated-evidence-doctor.md` | Evidence Doctor summary；结果为 `Errors=0, Warnings=0, Suggestions=0`。 |
| `doc/phases/070-public-extract-wall-time-adoption-check/repeated-evidence-doctor.json` | 机器可读 Doctor 摘要。 |
| `log/evidence_registry.json` | summary artifact registry；登记 Phase 030 / 060 / 070 的 manifest / Doctor 文件。 |

raw board logs 默认 local-only。summary-only（只提交摘要）策略下，只有上述 manifest / Doctor summary 和明确引用的
phase result / evaluation / `doc-rvv` 进入可审查提交候选。

## 后续优化判断

当前 patch 值得保留：Phase 060 production-detail median `3.1280x` 证明 terminal helper 同边界收益，
Phase 070 production-public median `1.112866x` 证明该收益没有被完整公开入口完全稀释。

继续直接改 production 的高价值方向当前不足：

| 方向 | 当前判断 | 原因 / 恢复条件 |
| --- | --- | --- |
| organized n-link RVV | 不建议继续直接接 production | 历史 organized n-link component 观察约 `0.98x`，且 graph edge mutation / memory traffic 可能抵消收益；只有 profile 指向 n-link 成为公开入口主瓶颈时再重开。 |
| max-flow solver RVV | rejected with evidence | `BoykovKolmogorov::solve` 是 map/deque/parent/orphan 状态机，当前 topic 不适合用 RVV 直接改写。 |
| non-organized KNN n-link | deferred | KNN search 和不规则 neighbor list 主导风险高；需 profile 证明公式段占比高。 |
| color staging | not_now | `initCompute` RGB/RGBA 到 `Color` 的转换可能可向量化，但公开入口已 positive；其价值低于 profiling 后的主瓶颈，不建议无 profile 继续生产补丁。 |
| 新 RVV family comparison | deferred | 只有提出替代 family 后才需要 RVV-vs-RVV detail A/B；当前没有比已采纳 helper 更强的未阻塞候选。 |

## Continue / stop decision

`continue_stop_decision=turn_stop_deferred with stop_condition_hit=no_high_value_unblocked_production_candidate`。
停止原因不是证据失败：当前 production patch 已采纳且公开入口补证据为 positive。停止原因是 roadmap 和 optimization matrix 中
没有当前授权范围内、无需 profile / 新用户决策 / 新 production family 的高价值未阻塞优化方向。默认恢复动作是
文档 closeout、freshness check 和 final verification；若未来要继续优化，先新增 profile 或 component A/B phase，
不要直接追加 production RVV 补丁。
