# Phase 046 结果：row-source shuffle mitigation detail A/B

## 执行摘要

本阶段完成 `row-source-shuffle-sorted-copy-detail-ab`（row-source shuffle 排序副本细节 A/B）。它不是 Std/RVV public adoption evidence（公开入口标量 / RVV 采纳证据），而是在已采纳 row-source production boundary（生产边界）内比较：

```text
current RVV shuffled gather vs sorted-copy + same RVV public row-source path
```

结论需要按 row source（行来源）和 size（规模）拆开：

| 子边界 | decision | 证据摘要 |
| --- | --- | --- |
| correspondence shuffle 64K / 256K | `positive_production_probe_candidate` | board B/A median `2.344x` / `2.130x`，min `2.003x` / `2.026x`；sorted-copy 计时包含 correspondence copy + sort。 |
| dual-indexed shuffle 256K | `weak_positive_detail_candidate` | board B/A median `1.185x`，min `1.050x`；可作为后续 probe 的次优候选，但不足以单独 clean-adopt。 |
| dual-indexed shuffle 64K | `unstable_or_negative_detail_candidate` | median `1.146x`，但 min `0.789x`，Doctor 给出 degradation 和 long-tail warning。 |
| dual-indexed / correspondence shuffle 4K | `rejected_for_small_size` | board B/A median `0.464x` / `0.574x`，5/5 runs 退化。 |

QEMU smoke 全部 negative，但 QEMU 只证明 build、label、误差和 manifest shape，不作为性能结论。板卡结果说明 sorted-copy 对中大规模 correspondence 有明确收益，但不能作为全 row-source、全规模 production patch 直接接入。

## 源码 / 测试 / bench / script 变化

| area | change | evidence role |
| --- | --- | --- |
| fixture helper | `include/impl/tesvd_scale_support.hpp` 新增 `makeSortedIndices`、`makeSortedIndexPairsBySource`、`makeSortedCorrespondencesByQueryIndex`。 | 构造 sorted-copy 输入，不改变 production。 |
| correctness guard | `src/test_tesvd_scale.cpp` 新增 `RowSourceSortedCopyMatchesShuffledPublicPath`。 | 证明 current path 和 sorted-copy path 都与同一个 selected-cloud reference 对齐。 |
| bench wrapper | `src/bench_tesvd_scale.cpp` 新增 `row-source-shuffle-sorted-copy-detail-ab` case-filter。 | 输出 current / sorted-copy paired labels，并把 copy + sort 计入 sorted-copy 计时。 |
| summary script | `script/generate_tesvd_scale_detail_ab_summary.py` 新增 RVV-only detail A/B summary / manifest。 | B/A 定义为 current RVV ms / sorted-copy RVV ms；manifest 记录预期 wrapper / timer boundary 差异。 |
| Makefile target | 新增 QEMU smoke、board repeated、Evidence Doctor 和 registry target；并修正 board 远端参数为 `REMOTE_BENCH_ARGS` / `REMOTE_BENCH_RVV_OUTPUT_FILE`。 | 确保板卡只运行 Phase 046 case-filter，不混入全矩阵。 |

## 证据结果

| evidence | result | path / notes |
| --- | --- | --- |
| correctness rerun | Std/RVV 各 13 个 gtest passed。 | `make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare_recorded` |
| QEMU detail smoke | 6 个 paired case 全部 negative；Doctor `Errors=6`、`Warnings=1`，全部是 QEMU 退化/组内离群信号。 | `log/qemu/row_source_shuffle_sorted_copy_detail_ab/summary.md`、`evidence_doctor.md`；不作为性能结论。 |
| board detail A/B | 6 个 paired case：correspondence 64K/256K positive，dual-indexed 256K weak-positive，4K negative，dual-indexed 64K 因 min 退化降级。 | `log/board/row_source_shuffle_sorted_copy_detail_ab_repeated/summary.md` |
| board Evidence Doctor | `Errors=2`、`Warnings=8`、`Suggestions=0`。 | Errors 只来自 4K 两个子边界 5/5 退化；warnings 包含 dual-indexed 64K 的 1/5 退化和 long-tail。 |
| registry | QEMU correctness、QEMU detail smoke 和 board detail A/B 已登记，当前 `evidence_status` fresh。 | `log/evidence_registry.json` |

## Board Detail A/B 摘要

| case | B/A values | median | min | bucket | interpretation |
| --- | --- | ---: | ---: | --- | --- |
| correspondence 64K | `2.003, 2.348, 2.322, 2.344, 2.355` | `2.344x` | `2.003x` | positive | sorted-copy 在中规模 correspondence shuffle 上明显改善 locality，且 copy + sort 成本没有吃掉收益。 |
| correspondence 256K | `2.026, 2.156, 2.098, 2.169, 2.130` | `2.130x` | `2.026x` | positive | 大规模 correspondence 仍强正向，是本阶段最清晰的生产探针候选。 |
| dual-indexed 256K | `1.050, 1.270, 1.185, 1.130, 1.270` | `1.185x` | `1.050x` | weak_positive | 有收益但边际较小，需要更窄 production probe 或更稳定 detail A/B。 |
| dual-indexed 64K | `1.072, 1.154, 1.146, 0.789, 1.205` | `1.146x` | `0.789x` | negative by current bucket | median 正向但有一次明显退化，不能作为 clean production signal。 |
| correspondence 4K | `0.574, 0.577, 0.572, 0.567, 0.580` | `0.574x` | `0.567x` | negative | 小规模 copy + sort 成本过高，应明确保持 current gather。 |
| dual-indexed 4K | `0.437, 0.494, 0.466, 0.464, 0.453` | `0.464x` | `0.437x` | negative | 小规模 sorted-copy 明确退化。 |

## Evidence Doctor 解释

Board Doctor 的 2 个 Errors 都来自 4K：dual-indexed 和 correspondence 均 5/5 B/A < 1。这是 expected negative（预期负向）信号，不表示 production 当前路径错误；它说明任何 sorted-copy production probe 都必须有 size gate（规模门控），不能覆盖小规模。

8 个 Warnings 主要分三类：

- dual-indexed 64K 有一次 `0.789x` 退化，且 max/min 为 `1.53`。该子边界不能进入 clean production probe。
- dual-indexed 256K 虽然 min 仍为 `1.050x`，但 max/min 为 `1.21`，只能写成 weak-positive。
- correspondence 64K/256K 与 4K 的组内差异很大，必须按 row source + size 分开报告，不能把 positive 外推到 4K 或 dual-indexed 64K。

QEMU Doctor 的退化信号与板卡不一致，处理方式是保留为 log-shape / correctness smoke，不参与性能 EvidenceDecision。

## Diagnostic-to-Production Mismatch Audit 回填

| question | result |
| --- | --- |
| evidence role | `production_detail_rvv_vs_rvv`。同一 row-source public production boundary 内的 detail A/B，但 sorted-copy wrapper 在 bench 内构造临时输入。 |
| A/B boundary | current RVV public shuffled row-source path vs copy/sort + same RVV public row-source path。 |
| 当前决策问题 | sorted-copy 是否值得进入 production integration probe。 |
| 是否可外推到 production | 只可外推为 `correspondence + shuffle-like disorder + size >= 64K` 的有界 probe 信号；不能直接外推到 all row source、4K、小规模、泛型点型或非法 correspondence。 |
| comparison-boundary / baseline mismatch 风险 | 有。candidate 计时包含 copy + sort，这是有意的 production-realistic 成本模型；manifest 已把 wrapper/timer boundary 差异标成预期变量。 |
| weak / negative / unstable 时是否允许 bounded production probe | 4K 不允许；dual-indexed 64K 不允许 clean probe；dual-indexed 256K 只能作为 secondary weak candidate；correspondence 64K/256K 允许进入有界 production probe。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 本阶段已提供 diagnostic detail A/B；production 接入仍需 PI1-PI5、生产源码 diff、公开入口 correctness / fallback / board repeated 和用户确认。 |

## 文档同步

已同步或需要同步的当前状态：

- `doc/phases/README.zh.md`：Phase 046 从 pending 改为 complete conditional-positive，并把下一默认动作指向 correspondence sorted-copy production probe 或 staged-selected-cloud detail A/B 的选择。
- `doc/phases/optimization-matrix.zh.md`：把 `row-source-shuffle-mitigation-detail-ab` 拆成 correspondence positive、dual-indexed weak/unstable 和 4K rejected。
- `doc/optimization-roadmap.zh.md`：新增 `correspondence-sorted-copy-production-probe` 和 `staged-selected-cloud-detail-ab` 的恢复条件。
- `README.zh.md`、`doc/testing-overview.zh.md`、`doc/benchmark-and-evidence.zh.md`、`doc/optimization-evidence.zh.md`、`doc/transformation_estimation_svd_scale-evaluation.zh.md`、`doc/test-support-code-map.zh.md`、`doc/correctness-tests.zh.md`：补充 Phase 046 target、证据路径和分规模结论。
- `doc-rvv/registration/transformation_estimation_svd_scale-RVV.zh.md`：本阶段不更新为 adopted production behavior；如后续 production probe 进入 PI5 并由用户确认采纳，再同步长期文档。

## 下一步

Phase 046 自身已 closed as conditional-positive。默认下一步有两个可执行方向：

| candidate | why now | state | next action |
| --- | --- | --- | --- |
| `correspondence-sorted-copy-production-probe` | correspondence shuffle 64K/256K board B/A median `2.344x` / `2.130x`，min 均 > `2.0x`。 | `phase_deferred + unblocked` | 若用户授权生产接入探针，开 PI1 plan，只覆盖 correspondence、合法 dense、`Scalar=float`、size >= 64K、shuffle-like disorder heuristic 或 explicit sorted-copy path。 |
| `staged-selected-cloud-detail-ab` | sorted-copy 证明排序改善 correspondence locality，但 dual-indexed 64K 不稳定；更重的 xyz staging 可能把双 gather 变成 strided loads。 | `phase_deferred + unblocked` | 若继续优化矩阵而暂不进 production，开下一 diagnostic detail A/B，仍需 copy/staging 成本计入计时。 |

由于当前 sorted-copy positive 只覆盖 correspondence 中大规模，不建议直接修改 production 源码并宣称 adopted。继续推进时应先由用户选择：做有界 production probe，还是继续 diagnostic 探索 staged-selected-cloud。
