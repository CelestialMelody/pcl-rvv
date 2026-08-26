# Phase 000: current-state and moment accumulation diagnostic plan

## 阶段意图和边界

本阶段只评估 `features/include/pcl/features/impl/moment_invariants.hpp` 中 `computePointMomentInvariants` 的 centroid（质心）之后中心矩累加是否值得 RVV 化。范围是 `PointXYZ`、`Scalar=float`、AoS（结构数组）输入、full-cloud 和 indices（索引）两种 helper 形态。production（生产源码）不修改，`computeFeature` 的 searchForNeighbors（邻域搜索）成本只通过 public-search-shaped bench（公开入口形态性能测试）观察是否稀释收益。

## 当前状态清单

| 对象 | 当前状态 | 证据 |
| --- | --- | --- |
| production helper | 两个 `computePointMomentInvariants` 都先调用 `compute3DCentroid`，再用标量循环累加 `mu200/mu020/mu002/mu110/mu101/mu011` 并组合 `j1/j2/j3`。 | `features/include/pcl/features/impl/moment_invariants.hpp` |
| public entry | `computeFeature` 对每个 `indices_` 元素做邻域搜索，失败时输出 NaN，否则调用 indexed helper。 | `features/include/pcl/features/impl/moment_invariants.hpp` |
| upstream test | `test_invariants_estimation` 只用 `bun0.pcd` 验证 public helper 和 estimator 输出。 | `test/features/test_invariants_estimation.cpp` |
| 筛选状态 | 保留候选第 5 项，建议先做 `component ablation`，确认 helper 占比是否能穿透 search。 | `doc-rvv/library-screening/features/features-retained-candidate-rescreen.zh.md` |

## 假设与候选族

| candidate family | 假设 | 风险 | 本阶段动作 |
| --- | --- | --- | --- |
| indexed moment accumulation RVV | centroid 后 6 个中心矩可以用 RVV gather（离散加载）和 vector reduction（向量规约）缩短 helper-only 成本。 | j2/j3 对规约顺序敏感；indices gather 和输出仅三值可能让收益窄。 | 建立 Std/RVV 同构 helper、QEMU correctness、asm 和板卡 repeated bench。 |
| public-search-shaped baseline | public `computeFeature` 可能被邻域搜索稀释。 | test-only helper positive 不能外推 production。 | bench 同时打印 helper-only 和 public-search-shaped case，EvidenceDecision 分层。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| indexed moment accumulation RVV | indexed cloud | `PointXYZ / float / AoS` | test helper | `run_test_compare` | `mi_accumulation_indexed` | planned, 5 runs | `computeMomentSummaryRVV` inlined into bench | planned | planned |
| full-cloud moment accumulation RVV | full cloud | `PointXYZ / float / AoS` | test helper | `run_test_compare` | `mi_accumulation_full_cloud` | deferred unless indexed is positive | `computeMomentSummaryRVV` | deferred | planned |
| public-search-shaped baseline | k-neighbor public shape | `PointXYZ / float / AoS` | production-shaped diagnostic | same test fixture, no production dispatch | `mi_public_search_shape` | planned observation only | no RVV production helper | metadata-incomplete until bench exists | diagnostic only |

## 实现和测试动作

1. 创建 topic scaffold（脚手架）：`Makefile`、`board.mk`、`include/`、`src/`、evaluation、roadmap 和 matrix。
2. TDD 红灯：`src/test_moment_invariants.cpp` 先调用尚未实现的 `computeMomentSummaryRVV`，确认 RVV 构建失败。
3. 实现 test-only Std/RVV helper：不改 production，`__RVV10__` 关闭时 RVV helper 回到 Std。
4. 运行 `make run_test_compare`，确认 Std/RVV 构建下 helper 与 production helper 的 j1/j2/j3 容差通过。
5. 运行 `make dump_bench_rvv` 和必要的 `dump_test_rvv`，确认 RVV 指令归属到 candidate helper。
6. 在板卡上运行 `make run_board_mi_repeated`，预算 5 次；若 decision bucket 稳定则关闭本阶段性能证据，若摇摆则降级为 unstable。

## Evidence Doctor 和 registry 规则

board repeated summary 写入 `log/board/repeated_phase000_moment_accumulation_diagnostic/summary.md`，manifest 写入同目录 `evidence_manifest.json`，Evidence Doctor（证据体检）写入同目录 `evidence_doctor.md` / `.json`。registry 写入 `log/evidence_registry.json`，doc-ref 指向本 phase result 和 evaluation。QEMU 日志只用于 correctness 和日志形状，不写性能结论。

## 阶段完成条件

`run_test_compare` 通过、反汇编中出现 candidate RVV 指令、板卡 repeated summary 有稳定 decision bucket，且 Evidence Doctor 没有未解释 Error 时，本阶段可给出 `diagnostic` 或 `partial-production-candidate`。如果 helper-only 正向但 public-search-shaped 中性，本阶段只保留 diagnostic，不进入 production integration loop（生产接入闭环）。

## 板卡复跑预算和决策桶

预算为 5 次 repeated board compare，case 为 `mi_accumulation_indexed`，每次 `--points 262144 --iterations 30 --warmup-iterations 4`。median speedup >= 1.10 且 0/5 退化为 positive；median >= 1.03 且最多 1/5 退化为 weak-positive；0.97 到 1.03 为 neutral；median < 0.97 为 negative；其它情况为 unstable。

## 继续 / 停止条件

默认下一阶段是 `010-public-search-dilution-check`：如果 helper-only positive 或 weak-positive，继续确认 public `computeFeature` 是否被 search 稀释；如果 helper-only neutral/negative，则进入 no-production closeout 并更新筛选状态。只有板卡不可达、工具链失败、Evidence Doctor Error 未能解释、dirty isolation 不安全或 production 授权边界被触碰时，本轮允许停止。

## 文档更新清单

本阶段更新 topic-local README、evaluation、optimization roadmap、optimization matrix、phase result 和 Handoff。没有 adopted production behavior（已采用生产行为）前，不创建 `doc-rvv/features/moment_invariants-RVV.zh.md`。

## diagnostic-to-production mismatch audit

| 项 | 本阶段口径 |
| --- | --- |
| evidence role | diagnostic（诊断）和 production-shaped diagnostic（生产形态诊断）分层记录。 |
| A/B boundary | helper-only case 是 test helper；public-search-shaped case 是 production-shaped helper，不是 production direct。 |
| 当前决策问题 | 先回答 RVV-vs-scalar helper value，再判断是否值得 bounded production probe（有界生产探针）。 |
| 是否可外推到 production | helper-only 不能外推；public-search-shaped positive 也只能支持进入 PI1 计划。 |
| mismatch 风险 | centroid、search、输出三值和邻域规模都会稀释 helper 收益。 |
| weak/negative 时是否允许 probe | weak-positive 可继续 public dilution check；neutral/negative 默认 no-production，除非 profile 证明 helper 占比高。 |
| clean adoption 是否需要 detail A/B | 需要真实 production patch、production direct correctness、fallback、asm 和 board repeated 证据，且 PI5 后等待用户确认。 |
