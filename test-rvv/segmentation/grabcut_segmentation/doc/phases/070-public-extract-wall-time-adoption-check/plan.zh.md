# Phase 070: public extract wall-time adoption check plan

## 阶段意图和边界

本阶段发生在用户确认采纳 Phase 060 production patch（生产补丁）之后。目标是补一条更接近公开调用方式的
public wall-time（公开入口总耗时）证据：构造 organized `PointXYZRGB` 输入，调用真实
`GrabCut<PointXYZRGB>::setBackgroundPointsIndices()` 和 `extract()`，观察已采纳的 terminal weight
RVV helper 在完整 `refine()` 链路中是否仍有可见收益。

本阶段不修改 production 源码，不扩大 Phase 060 的 RVV 覆盖范围，不接入 organized n-link、non-organized
KNN、max-flow solver（最大流求解器）、color staging（颜色暂存）或公共 RVV API。若 public wall-time
结果为 neutral（中性）或 negative（负向），它只能说明完整 GrabCut pipeline 中其它阶段稀释了当前 helper
收益；不自动触发回滚，因为 Phase 060 已有同边界 production-detail 正向证据且用户已确认保留。

## 当前状态清单

| 项 | 当前状态 | 证据 |
| --- | --- | --- |
| production patch | 已采纳窄范围 `initGraph()` unknown terminal batch helper | Phase 060 result、用户确认 |
| production-detail correctness | Std 4/4、RVV 6/6 通过 | `make run_test_compare` |
| production-detail board | Milkv-Jupiter 5-run B/A median 约 `3.1280x` | `060-production-initgraph-terminal-evidence/repeated-evidence-manifest.json` |
| public wall-time bench | 尚未存在 | `src/bench_grabcut.cpp` 只有 `production_initgraph_terminal` detail case |
| Evidence Doctor / registry | Phase 030 和 Phase 060 summary fresh | `make evidence_status` |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| public `extract()` wall-time evidence | 真实 `setBackgroundPointsIndices()` 会在初始化时触发 `fitGMMs()` 和一次 `initGraph()`，`extract()` 后续 `refine()` 也会触发 `initGraph()`；因此 terminal helper 可能在完整公开流程中留下收益。 | `buildGMMs`、`learnGMMs`、organized n-link 和 max-flow 可能占主成本，稀释 terminal helper 收益。 |
| 后续 n-link / solver RVV | 若 public wall-time 显示收益被稀释，应先用 profile 或更窄消融确认瓶颈，再决定是否重开 n-link / solver。 | Phase 000 organized n-link 历史观察弱负向，max-flow 是状态机，不适合直接 RVV。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| public `extract()` wall-time evidence | organized image scan | `PointXYZRGB` / float color / organized cloud |真实 `GrabCut<PointXYZRGB>::extract()` | 复用 `make run_test_compare`；bench 输出 cluster checksum | 新增 `bench_grabcut --case public_extract` | 5-run board repeated，默认 `96x72`、`iterations=3`、`warmup=1`；`320x240` partial run 只作 too-heavy smoke | 复用 Phase 060 production symbol attribution | 新增 repeated manifest / Doctor | planned |

## 实现和测试动作

| 动作 | 产物 | 完成判据 |
| --- | --- | --- |
| 新增 public bench case | `src/bench_grabcut.cpp` | `--case public_extract` 可在 Std/RVV 构建运行，输出稳定 `BENCH` 行和 checksum。 |
| 更新 manifest case label | `script/generate_grabcut_board_evidence_manifest.py` | Evidence Doctor 可识别 `public_extract`，并标记为 `production-public`。 |
| 新增 repeated target 和 registry | `Makefile` | 可采集 5-run board summary，并用 `make evidence_status` 检查 freshness。 |
| 更新文档 | `doc-rvv`、evaluation、roadmap、matrix、README、phase result | 文档采用接入后的板卡测试数据，区分 production-detail 与 production-public。 |

## Evidence Doctor 和 registry 规则

本阶段新增 summary-only（只提交摘要）证据：

- `doc/phases/070-public-extract-wall-time-adoption-check/repeated-evidence-manifest.json`
- `doc/phases/070-public-extract-wall-time-adoption-check/repeated-evidence-doctor.md`
- `doc/phases/070-public-extract-wall-time-adoption-check/repeated-evidence-doctor.json`

raw board logs（原始板卡日志）继续保留在 repeated run directory，本轮默认不提交。

## 板卡复跑预算和决策桶

| 项 | 设置 |
| --- | --- |
| board availability | 当前会话已说明板卡可用；若部署或运行失败，记录为工具 / 环境阻塞。 |
| run count | 5-run repeated 起步。 |
| 参数 | 默认 `--width 96 --height 72 --iterations 3 --warmup 1 --case public_extract`。先前尝试的 `320x240` 完整公开入口每 iteration 约 28-34s，且中止后留下不完整日志，因此只作为 partial / too-heavy smoke，不作为本阶段 repeated performance closeout。 |
| positive | 5-run median B/A >= `1.05x` 且 Evidence Doctor 无 Error。 |
| weak / neutral | median 在 `[0.97x, 1.05x)`；说明完整 pipeline 稀释明显，不作为新 production 扩展依据。 |
| negative | median < `0.97x` 或 Error 无法修复；保留当前 patch，但不建议继续同 topic 追加新的 production RVV。 |
| unstable | run 方向摇摆且预算用完；降级为需要 profile 或人工判断。 |

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production-public`：真实公开入口 `extract()` wall-time。 |
| A/B boundary | public method boundary（公开方法边界）：Std 构建 vs RVV 构建。 |
| 当前决策问题 | 已采纳 terminal helper 在完整公开流程中是否仍有可见收益，以及是否值得继续寻找下一段优化。 |
| diagnostic 是否可外推到 production | 不依赖 diagnostic 外推；本阶段直接测 production public。 |
| comparison-boundary / baseline mismatch 风险 | Std/RVV 输入和 case 相同，但完整流程包含 GMM learn、n-link、graph solve 和输出聚类，因此不能把结果归因到单一 helper。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段不是 probe；若结果弱或负，后续只允许 profile / component ablation，不直接改 production。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前只有一个已采纳 RVV family；若新增 family，需要同边界 RVV-vs-RVV A/B。 |

## Continue / stop conditions

本阶段完成后：

- 若 public wall-time positive，当前 patch 的公开收益得到补充证据；下一步再审计是否有比 terminal helper 更值得推进的未阻塞 candidate。
- 若 public wall-time weak / neutral / negative，当前 topic 不继续直接接入 n-link 或 solver；需要 profile 或另开消融阶段作为恢复条件。
- 若 Evidence Doctor Error 或 board 工具失败无法修复，停止在 blocked handoff，保留已采纳 patch 和已有 Phase 060 证据。
