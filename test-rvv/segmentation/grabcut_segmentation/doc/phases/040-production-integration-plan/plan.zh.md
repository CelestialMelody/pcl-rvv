# Phase 040: production integration plan

## 阶段意图和边界

本阶段是 PI1 production integration plan（生产接入计划）。目标是把 Phase 030 `initGraph` no-solve diagnostic（不含求解诊断）的 positive bucket（正向决策桶）转换为可审查的 PI2-PI5 生产接入范围；本阶段只写 topic-local 文档，不修改 production（生产源码）。

本阶段不证明 production-ready（可直接接入生产）。Phase 030 的 evidence role（证据角色）仍是 `production_shaped_diagnostic`，A/B boundary（对照边界）仍是 `test_support_helper`，不能替代 production direct（真实生产入口证据）、production asm attribution（生产符号反汇编归属）或 PI5 用户确认。

## 当前状态清单

| area | 当前事实 | 路径 / 证据 |
| --- | --- | --- |
| Phase 030 correctness | `InitGraphNoSolveMatchesScalarReference` 已随 `make run_test_compare` 通过。 | `src/test_grabcut.cpp`、`doc/phases/030-initgraph-no-solve-diagnostic/result.zh.md` |
| Phase 030 board | Milkv-Jupiter `640x480`、`iterations=8`、`warmup=2` 下 5-run B/A 为 `1.5486, 1.5416, 1.5418, 1.5396, 1.5389`。 | `doc/phases/030-initgraph-no-solve-diagnostic/repeated-evidence-manifest.json` |
| Evidence Doctor | repeated report 为 `Errors=0, Warnings=0, Suggestions=0`。 | `doc/phases/030-initgraph-no-solve-diagnostic/repeated-evidence-doctor.md` |
| 反汇编 | `computeTerminalWeightsCandidate` 调用 `computeGMMProbabilityCandidate`，GMM 符号范围含 RVV load/store/FMA。 | `build/asm/riscv/bench_grabcut_rvv.full.asm`，仅作为本地生成证据 |
| production 状态 | 没有 production patch；`doc-rvv/segmentation/grabcut_segmentation-RVV.zh.md` 仍不适用。 | `git status` 路径限定扫描和当前源码 |

## pi2_scope

| 项 | PI2 候选范围 |
| --- | --- |
| public entry | `GrabCut<PointT>::fitGMMs()` 和 `GrabCut<PointT>::refineOnce()` 通过私有 `initGraph()` 进入 terminal weight loop。 |
| production detail | 只尝试 `initGraph()` 中 `TrimapUnknown` 分支的 background / foreground GMM probability 和 terminal source/sink cost 计算。 |
| point type / layout | `initCompute()` 已把 `PointT` 转为 `Image<Color>`；terminal batch 只读取 `Color`、`GMM::operator[]` 和 `indices_`，不直接读取 `PointT` 字段。若 PI2 不碰 color staging，则点类型 traits（点类型字段特征）不是本 helper 的直接 load/store gate。 |
| `Scalar` | production `Color` 和 GMM 使用 `float`；不新增 `double` 路径。 |
| row source | `indices_` 的当前顺序仍决定 graph node 顺序。PI2 候选只允许对可按 `indices_` 顺序写回 terminal staging 的元素批量化，不改变 indices 语义。 |
| trimap | RVV 只覆盖连续批量的 `TrimapUnknown` terminal formula；`TrimapBackground`、`TrimapForeground` 和其它固定标签分支保持标量。 |
| n-link / solver | n-link `addEdge` 和 `BoykovKolmogorov::solve()` 保持标量。 |

## forbidden_expansion

PI2 不得扩大到这些范围：public API（公开接口）变更、color staging、organized n-link RVV、non-organized KNN path、max-flow solver、公共 RVV math API 变更、泛型点类型 field traits gate、`Scalar=double`、其它 segmentation topic 或 `doc-rvv` 长期生产文档创建。

## dispatch / fallback 策略

| gate（验收条件） | PI2 行为 |
| --- | --- |
| 非 `__RVV10__` 构建 | 不编译 RVV helper，`initGraph()` 使用原标量路径。 |
| RVV helper 不适用 | 直接进入 `initGraph` Std fallback（标量回退）。 |
| 小规模或 `vlmax` scratch 超界 | fallback；阈值应按 work item count（本阶段为 unknown terminal candidate count）定义。 |
| 非 unknown trimap | 保持原标量 fixed-label terminal weights，不进入 RVV batch。 |
| GMM 分量 `pi <= 0` 或 determinant 非正 | 保持 `GMM::probabilityDensity` 当前语义；PI2 不改变零概率导致 `-log(0)` 的行为。 |
| n-link edge mutation | 保持原标量 `addEdge` 循环。 |

## production direct test 计划

PI3 至少需要新增或复用下列真实生产入口测试，且要先有 RED（失败）或等价 gating 证据：

| 测试 | 必须证明什么 | 不能证明什么 |
| --- | --- | --- |
| `GrabCut<PointXYZRGB>` public-shaped smoke | `setBackgroundPointsIndices()` / `fitGMMs()` 或 `refineOnce()` 形态能触达生产 `initGraph()`，输出 segmentation 或 graph-visible 状态不回归。 | 不单独证明 RVV 路径命中。 |
| RVV hit instrumentation test | 在 RVV 构建中证明 terminal unknown batch helper 被调用，非 RVV 构建或 gate 失败时不调用。 | 不证明性能。 |
| fixed-label trimap fallback | `TrimapBackground` / `TrimapForeground` 保持原标量 terminal cost 语义。 | 不证明 unknown branch RVV 数值。 |
| non-RVV build fallback | `__RVV10__` 关闭时源码可编译并通过同一 public-shaped correctness。 | 不证明目标硬件性能。 |

## PI4 证据计划

| 证据 | 命令形态 | 完成判据 |
| --- | --- | --- |
| correctness | `make -C test-rvv/segmentation/grabcut_segmentation run_test_compare` 和新增 production direct 目标 | Std/RVV 都通过；测试输出区分 QEMU correctness（QEMU 正确性）和生产命中证据。 |
| QEMU smoke | 小规模、少 iteration、非 compare 的 `--case` smoke | 只记录日志形状和 parser 可用性，不写性能结论。 |
| asm attribution | `make -C test-rvv/segmentation/grabcut_segmentation dump_bench_rvv` 或 production direct dump | 关键 RVV 指令能归属到 production helper 或明确的内联调用链。 |
| board repeated | production direct 或 production-detail bench 5-run 起步，`iterations=8`、`warmup=2` 或 PI2 计划修订后的有界预算 | decision bucket 稳定；若反转或低于 weak-positive，PI5 不能建议采纳。 |
| Evidence Doctor | production manifest wrapper + `../../script/evidence_doctor.py` | Errors 必须为 0；Warnings 必须解释或降级证据边界。 |

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | Phase 030 是 `production_shaped_diagnostic`；PI2 后才可能产生 `production-detail` 或 `production-public`。 |
| A/B boundary | Phase 030 baseline/candidate 是 `test_support_helper`；PI4 必须换成 production helper 或 public entry。 |
| 当前决策问题 | 是否允许把 terminal unknown branch 的 GMM probability + terminal cost batch 作为窄范围 production probe（生产探针）。 |
| diagnostic 是否可外推到 production | 只能外推为 PI2 候选。真实 graph mutation、fixed-label branches、n-link edges 和 solver 可能稀释收益。 |
| comparison-boundary / baseline mismatch 风险 | 存在。Phase 030 是不同 build 的 helper-level Std/RVV；PI4 需要同 production boundary 重跑。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前诊断为 positive；若 PI4 production direct 反转，则以 PI4 为准并停在 PI5 用户检查点。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要。若未来存在多个 RVV family，必须做同一 production boundary 内的 RVV-vs-RVV detail A/B；当前 PI2 只允许单 family bounded probe。 |

## 阶段完成条件

本阶段完成条件是：写清 PI2 候选范围、fallback、production direct tests、PI4 证据计划、PI2-PI5 暂停条件和文档同步边界。完成本阶段不代表已经授权生产补丁，也不代表 production-ready。

## PI2-PI5 暂停条件

下一轮若用户授权进入 production integration loop，遇到以下任一条件必须暂停并输出 Handoff Packet（交接数据包）：需要改 public API；需要新增跨 topic 公共 RVV API；fallback 无法隔离；production direct correctness 失败；非 RVV 构建失败；asm 不能归属到 production helper；board repeated 不稳定或反向；Evidence Doctor 出现未处理 Error；证据支持采纳但用户尚未确认保留；证据不支持接入但用户尚未授权回滚。

## 文档更新清单

PI2-PI5 完成后才能更新或创建 `doc-rvv/segmentation/grabcut_segmentation-RVV.zh.md`。在 PI5 用户确认前，诊断证据链主归属仍是 `doc/grabcut_segmentation-evaluation.zh.md`、`doc/phases/*/result.zh.md`、`doc/optimization-roadmap.zh.md` 和 `doc/phases/optimization-matrix.zh.md`。

## next_phase_default

`PI2 production_patch`，但仅在用户明确授权修改 production 源码后执行。授权前默认停止在 `PI2-blocked-on-user-authorization`。
