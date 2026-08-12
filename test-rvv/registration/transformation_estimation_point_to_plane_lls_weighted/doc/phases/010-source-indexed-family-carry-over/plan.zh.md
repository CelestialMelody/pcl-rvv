# Phase 010: source-indexed 实现族迁移审计计划

## 阶段意图和边界

本阶段回答 `tmp/test-update/4.5.md` 提出的核心问题：full-cloud 已 adopted 的
block-reduction + A/B/C/N block groups + fused formula / ILP code shape，是否需要先在
source-indexed、dual-indices、correspondences 中逐条 row source policy 做同边界 test-rvv
candidate / test / bench，而不是因为旧 staged-gather / compressed-tail 路径已经正向就停下。

本阶段允许修改：

- `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/doc/phases/010-source-indexed-family-carry-over/`
- 只服务本 topic 的 test-rvv candidate、gtest、bench target、topic-local summary / manifest wrapper 和 topic docs。

本阶段先不直接修改：

- `registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls_weighted.hpp`
- dual-indices / correspondences production dispatch
- `doc-rvv` 中声明新的 production 行为。只有真实 production 接入后，长期生产文档才升级。

## S0 恢复与偏好冻结

| 项 | 当前冻结 |
| --- | --- |
| `preferences_loaded` | 已读取 `AGENTS.md`、`.agents/config/defaults.yaml`、`.agents/knowledge/pcl-rvv-knowledge-map.md`、`rvv-workflow`、`rvv-test`、`optimization-phase-loop`、`worker-quality-gates`、`handoff-packet`、`document-ownership-and-traceability`；`.agents/local/user-preferences.yaml` 不存在；当前 prompt 授权继续下一阶段。 |
| 注释策略 | test-rvv、diagnostic、prototype 使用详细中文；production 源码本阶段不改。 |
| 文档策略 | 阶段计划 / 结果 / 矩阵归属 `doc/phases/010-*`；topic docs 只写稳定索引和证据边界；长期 `doc-rvv` 不写新 production 行为。 |
| 证据策略 | `summary-only`；raw logs 不默认提交。EvidenceDecision 前必须有 manifest / Evidence Doctor 或明确 `not_run` 边界。 |
| dirty isolation | 工作树进入本阶段前已 dirty，包含 `.agents/**`、production 头文件、topic 文档、test-rvv 源码 / 日志。本阶段只审查本 topic 允许路径，不回退或归因其它既有 diff。 |

## 当前状态清单

| row source policy | 当前实现族 | 当前 production 状态 | 已有证据 | 当前缺口 |
| --- | --- | --- | --- | --- |
| full-cloud | block-reduction + A/B/C/N + fused-abcd-ilp | adopted | `run_test_candidates`、`run_test_production_direct`、`production_dispatch_fused_abcd_ilp/summary.md`、`asm_production_symbol_attribution.md`、doctor `0E / 0W / 3S` | 只维护 accepted-risk，不外推到 indexed。 |
| source-indexed | staged-gather + compressed-tail | adopted for current family | `run_test_source_indices_compare`、`run_test_production_direct`、`production_source_indices_staged_gather/summary.md`、doctor `0E / 7W / 6S` | 缺同边界 block-reduction / fused formula / ILP family candidate、bench、asm 和 doctor；不能写成全家族最优。 |
| dual-indices | staged-gather + compressed-tail diagnostic | production 保持标量 | `run_test_row_sources`、`run_bench_row_sources`、row-source doctor `0E / 0W / 0S`，单次板卡负向 | 缺同 family candidate / bench / board；不能直接 production。 |
| correspondences | staged-gather + compressed-tail diagnostic，含 query/match/weight 展开 | production 保持标量 | `run_test_row_sources`、`run_bench_row_sources`、row-source doctor `0E / 0W / 0S`，单次板卡负向 | 缺同 family candidate / component ablation；不能把负向单因归因为 gather。 |

## 假设与候选族

| candidate family | 审计假设 | 本阶段处理 |
| --- | --- | --- |
| source-indexed block-reduction baseline | source gather 后仍可把 lane contribution 放进 A/B/C/N vector partial sums，减少 `vcompress` + scalar tail 成本，但会增加 gather、index staging 和 m1/m2 形态差异。 | 先在 test-rvv 增加 source-indexed PointNormal same-boundary candidate、correctness test 和 bench case-filter。 |
| source-indexed fused-abcd-ilp | 复用 full-cloud 的 fused formula / ILP code shape 可能改善公式调度；也可能被 gather 与 index staging 成本淹没。 | 只做 test-rvv candidate，不替换 production。先闭合 correctness 和 QEMU bench shape；板卡 / asm 缺失时保持 `attempted / metadata_incomplete`。 |
| dual-indices block/fused family | 双侧 gather 可能让 block-reduction收益不足，但必须先有同族 candidate 才能证明。 | 本阶段先写规划和矩阵，若 source-indexed candidate 闭合且不需要 production 扩权，再进入下一阶段。 |
| correspondences block/fused family | query/match/weight 展开、容器访问、双侧 gather 和 solve 都计时；需要拆 component ablation。 | 本阶段先写规划和矩阵，不直接 production。 |
| production direct | diagnostic 正向不能自动升级成 production evidence。 | 本阶段不做 production C++；若 source-indexed new family 在板卡上稳定正向，再开 PI1。 |

## Optimization Matrix

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| block-reduction + A/B/C/N + fused-abcd-ilp | full-cloud | 三类代表点型 / `Scalar=float` / f32 AoS gated | production public full-cloud | `run_test_candidates`、`run_test_production_direct` | `run_bench_production_dispatch`、`run_bench_production_default_fused_abcd_ilp_rvv` | `production_dispatch_fused_abcd_ilp/summary.md` | `asm_production_symbol_attribution.md` | `0E / 0W / 3S` | adopted | 不外推；作为 carry-over audit 参照。 |
| staged-gather / compressed-tail | source-indexed | 三类代表点型 / `Scalar=float` / valid source index | production public source-indexed | `run_test_source_indices_compare`、`run_test_production_direct` | `run_bench_production_source_indices` | `production_source_indices_staged_gather/summary.md` | source-indexed-specific asm missing | `0E / 7W / 6S` | adopted for current family | 保留，但必须和新 family 比较。 |
| block-reduction baseline | source-indexed | `PointNormal` first / `Scalar=float` / source gather + target normal f32 AoS | test-rvv candidate | planned `run_test_source_indexed_family` | planned `run_bench_source_indexed_family` | missing | missing | planned / not_run | planned | 新增 candidate + gtest + bench case-filter。 |
| fused-abcd-ilp on block baseline | source-indexed | `PointNormal` first / `Scalar=float` / same boundary | test-rvv candidate | planned `run_test_source_indexed_family` | planned `run_bench_source_indexed_family` | missing | missing | planned / not_run | planned | 若 correctness 通过，跑 QEMU bench shape；板卡和 asm 作为后续。 |
| representative generic carry-over | source-indexed | `PointXYZ -> PointNormal`、`PointXYZ -> PointXYZINormal` / `Scalar=float` / layout-gated | test-rvv candidate | missing | missing | missing | missing | missing | deferred | PointNormal first 闭合后再泛化，避免一步扩大 production 语义。 |
| block/fused family | dual-indices | `PointNormal` first / `Scalar=float` /双 gather | test-rvv candidate | missing | missing | missing | missing | missing | planned / deferred | source-indexed family 审计后，新阶段补 dual 同族 candidate；不直接 production。 |
| block/fused family | correspondences | `PointNormal` first / `Scalar=float` / query/match/weight 展开 + 双 gather | test-rvv candidate + component ablation | missing | missing | missing | missing | missing | planned / deferred | 新阶段拆展开成本和 block/fused candidate；不直接 production。 |
| production replacement | source-indexed | production public source-indexed | PI1-PI5 only | missing production direct for new family | missing production board | missing | missing | missing | blocked by authorization | 只有 test-rvv + board + asm 正向后，另开 PI1。 |

## 实现和测试动作

| 动作 | 产物 | 依赖 | 完成判据 |
| --- | --- | --- | --- |
| B1 写 phase 010 计划 | 本文件 | Phase 000 result | `phase_plan_written_before_edits` 通过。 |
| B2 增加 source-indexed block/fused PointNormal candidate | `include/impl/teptplw_candidate_row_sources.hpp` 或窄 helper | B1 | 不改 production；保留 valid-index-only、finite point/normal mask、weight 不参与 finite mask。 |
| B3 增加同族 correctness target | `src/test_teptplw_row_sources.cpp`、`Makefile` filter / target | B2 | std/RVV QEMU gtest 都能运行；RVV 构建应命中新 candidate。 |
| B4 增加 same-boundary bench target | `include/impl/teptplw_bench_cases.hpp`、`Makefile` case-filter | B2 | bench label 能区分 staged-gather、block-baseline、block-fused-abcd-ilp；QEMU 只作 shape / checksum。 |
| B5 运行本地可得验证 | `run_test_source_indexed_family_compare`，必要时 `run_bench_source_indexed_family_compare` | B3/B4 | 记录命令、日志路径和未运行原因；QEMU 不写真实性能结论。 |
| B6 Evidence Doctor / manifest 边界 | 若有 board summary 则生成 manifest / doctor；否则在 result 中写 `not_run` | B5 | 不用缺 board/asm 的数据关闭 adopted；医生状态写入矩阵。 |
| B7 更新 phase result、README 和 topic docs | `result.zh.md`、phase README、topic docs / Handoff | B5/B6 | 每个动作有 done / partial / deferred / blocked；明确下一阶段默认入口。 |

## Evidence Doctor 规则

| 证据 | 输入 | 本阶段预期 | 异常处理 |
| --- | --- | --- | --- |
| source-indexed family QEMU correctness | `log/qemu/run_test_source_indexed_family_*.log` | correctness / path shape only；不跑 doctor。 | 若失败，B2/B3 标为 blocked，不能进入 bench。 |
| source-indexed family QEMU bench shape | `log/qemu/analyze_bench_compare_source_indexed_family.log` | 只检查 case label、checksum shape 和 std/RVV 均可运行。 | QEMU timing 不作性能结论；若 A/B 边界缺失，result 写成 `metadata_incomplete`。 |
| source-indexed family board repeated | planned future `log/board/source_indexed_family_carry_over/summary.md` | 需要 manifest + Evidence Doctor 后才可进入 EvidenceDecision。 | 若本阶段未跑板卡，decision 只能是 `attempted / needs board` 或 `planned`。 |
| asm attribution | planned future source-indexed family asm summary | 验证 gather、A/B/C/N reduction、fused formula 指令归属。 | 缺失时不能写 clean pass 或 production-ready。 |

## 阶段完成条件

- source-indexed staged-gather 和 block/fused family 不再混成同一 adopted 结论。
- source-indexed block/fused PointNormal candidate 至少有 test-rvv correctness 和 bench shape，或有明确实现阻塞。
- dual-indices / correspondences 的同族 candidate 进入矩阵，并写清为什么先不 production。
- result 写清 pre-production diagnostic 与 post-production direct 的分界。
- Handoff Packet 给出 dirty isolation、Evidence Doctor 状态、unblocked next action 和下一阶段默认入口。

## Continue / Stop Decision

默认继续。B1 不能作为停止理由；B2-B7 都是当前授权且未阻塞的动作。只有以下情况允许暂停：

- 新 candidate 无法在不触碰 production C++ 的条件下表达，或会扩大到泛型 public API。
- 本地工具链 / QEMU 缺失，导致 B3-B5 无法执行；必须写清命令和解除条件。
- correctness 失败且短时间内无法修复。
- 继续需要板卡、asm 归因或 production replacement 权限。
