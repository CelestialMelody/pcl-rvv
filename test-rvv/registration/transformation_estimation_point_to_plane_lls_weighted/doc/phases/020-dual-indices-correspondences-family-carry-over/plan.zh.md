# Phase 020: dual-indices / correspondences 实现族迁移审计计划

## 阶段意图和边界

本阶段继续 `tmp/test-update/4.5.md` 的 row source family audit（行来源实现族审计）：在不修改 production 的前提下，先为 dual-indices（双索引路径）和 correspondences（对应关系索引路径）补 full-cloud adopted family 的 test-rvv candidate / correctness / bench / board smoke 证据，再决定是否值得进入 production integration loop（生产接入闭环）。

本阶段不证明 production 接入，也不扩大当前 production 行为。full-cloud 和 source-indexed 当前 production 结论保持不变；dual-indices / correspondences 仍然保持标量。

## S0 恢复与偏好冻结

| 项 | 当前冻结 |
| --- | --- |
| `preferences_loaded` | 已读取 `AGENTS.md`、`.agents/config/defaults.yaml`、`.agents/knowledge/pcl-rvv-knowledge-map.md`、`rvv-workflow`、`rvv-test`、`optimization-phase-loop`、`worker-quality-gates`、`handoff-packet`、`document-ownership-and-traceability`；`.agents/local/user-preferences.yaml` 不存在。 |
| 注释策略 | test-rvv、diagnostic、prototype 继续详细中文；production 本阶段不改。 |
| 文档策略 | 阶段探索归属本目录；topic docs 只写稳定索引和证据边界；长期 `doc-rvv` 不写新 production 行为。 |
| 证据策略 | `summary-only`；raw board logs 不默认提交。诊断 summary 写入结论前必须有 manifest / Evidence Doctor 或 `not_run` 边界。 |
| dirty isolation | 工作树已经包含 `.agents/**`、production 头文件、topic docs、logs 和 test-rvv 修改。本阶段只触碰当前 topic test-rvv 资产、phase docs、topic docs 和必要 evidence boundary，不回退其它 dirty diff。 |

## 当前状态清单

| row source policy | 当前实现族 | 当前 production 状态 | 当前证据 | 当前缺口 |
| --- | --- | --- | --- | --- |
| full-cloud | block-reduction + A/B/C/N + fused-abcd-ilp | adopted | production direct、repeated board、asm、doctor 均有当前边界。 | 不外推。 |
| source-indexed | staged-gather / compressed-tail adopted；block-baseline / fused-abcd-ilp attempted | adopted for current family | Phase 010 新增 `run_test_source_indexed_family_compare`、`run_bench_source_indexed_family_compare`、`run_board_bench_source_indexed_family` 和 diagnostic doctor。 | block/fused family 不替换 production；缺 repeated board / asm / generic carry-over。 |
| dual-indices | staged-gather / compressed-tail diagnostic | production 保持标量 | `run_test_row_sources`、`run_bench_row_sources`、row-source diagnostic doctor。 | 缺 block-baseline / block-fused-abcd-ilp 同族 candidate、bench 和 doctor。 |
| correspondences | staged-gather / compressed-tail diagnostic，含 query/match/weight 展开 | production 保持标量 | `run_test_row_sources`、`run_bench_row_sources`、row-source diagnostic doctor。 | 缺 block/fused candidate 和 component ablation，不能单因归因为 gather。 |

## 假设与候选族

| candidate family | 审计假设 | 本阶段处理 |
| --- | --- | --- |
| dual-indices block-baseline | 双侧 gather 之后仍可用 A/B/C/N block groups 避免 `vcompress` + scalar tail，但 gather 与索引准备可能抵消收益。 | PointNormal-first test-rvv candidate，correctness 与 bench shape。 |
| dual-indices block-fused-abcd-ilp | fused formula / ILP code shape 可能改善公式调度，也可能因双 gather 和寄存器压力退化。 | 先做同边界 diagnostic，不接 production。 |
| correspondences block-baseline | query/match/weight 展开后，block-reduction 可能减少 tail 成本，但展开和容器访问可能成为主成本。 | candidate 必须把展开成本是否计时写清。 |
| correspondences block-fused-abcd-ilp | D 项 fused formula 在 correspondence weight 来源下可能退化；需要 component no-solve 和 full estimate 分开看。 | 先补 component ablation 和 full estimate bench。 |
| production direct | 只有 dedicated repeated board、fallback、asm 和 doctor 闭合后才考虑。 | 本阶段明确 blocked by evidence；不改 production。 |

## Optimization Matrix

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| staged-gather / compressed-tail | dual-indices | `PointNormal` / `Scalar=float` / 双 index stream | test-rvv row-source candidate | `run_test_row_sources` | `run_bench_row_sources` | `run_board_bench_row_sources` diagnostic | none | row-source doctor `0E / 0W / 0S` | attempted / diagnostic negative | 作为 baseline 参照。 |
| block-baseline carry-over | dual-indices | `PointNormal` / `Scalar=float` / 双 gather | test-rvv candidate | planned `run_test_dual_correspondence_family` | planned `run_bench_dual_correspondence_family` | planned board smoke | missing | planned | planned | C2-C6。 |
| block-fused-abcd-ilp carry-over | dual-indices | 同上 | test-rvv candidate | planned | planned | planned board smoke | missing | planned | planned | C2-C6。 |
| staged-gather / compressed-tail | correspondences | `PointNormal` / `Scalar=float` / query/match/weight 展开 | test-rvv row-source candidate | `run_test_row_sources` | `run_bench_row_sources` | `run_board_bench_row_sources` diagnostic | none | row-source doctor `0E / 0W / 0S` | attempted / diagnostic negative | 作为 baseline 参照，不能单因归因为 gather。 |
| block-baseline carry-over | correspondences | `PointNormal` / `Scalar=float` / 双 gather + correspondence weight | test-rvv candidate | planned | planned full estimate + no-solve | planned board smoke | missing | planned | planned | C2-C6。 |
| block-fused-abcd-ilp carry-over | correspondences | 同上 | test-rvv candidate | planned | planned full estimate + no-solve | planned board smoke | missing | planned | planned | C2-C6。 |
| production dispatch | dual-indices / correspondences | public overload | production direct | missing | missing | missing repeated board | missing | missing | blocked | 只有 diagnostic 稳定正向后才开 PI1。 |

## 实现和测试动作

| 动作 | 产物 | 依赖 | 完成判据 |
| --- | --- | --- | --- |
| C1 写 phase 020 计划 | 本文件 | Phase 010 result | `phase_plan_written_before_edits` 通过。 |
| C2 增加 dual / correspondence block/fused candidate | `include/impl/teptplw_candidate_row_sources.hpp`、`teptplw_candidate_estimates.hpp` | C1 | 不改 production；valid-index-only、finite point/normal mask、weight 来源和展开成本写清。 |
| C3 增加同族 correctness target | `src/test_teptplw_row_sources.cpp`、`Makefile` filter / target | C2 | std/RVV QEMU gtest 都能运行；RVV 构建命中新 candidate。 |
| C4 增加 same-boundary bench target | `include/impl/teptplw_bench_cases.hpp`、`Makefile` case-filter | C2 | label 能区分 staged、block-baseline、block-fused-abcd-ilp、full estimate 和 no-solve。 |
| C5 运行 QEMU correctness / bench shape | `log/qemu/run_test_*`、`analyze_bench_compare_*` | C3/C4 | 记录结果；QEMU timing 只作 shape。 |
| C6 运行 board smoke 并补 doctor | `log/board/run_board_bench_*`、`evidence_manifest.json`、`evidence_doctor.md` | C5 | doctor Errors / Warnings / Suggestions 写入 result；不能用 single-run 写 production。 |
| C7 更新 result、topic docs 和 Handoff | `result.zh.md`、topic docs、最终 Handoff | C6 | 每项动作有 done / partial / deferred / blocked，给出下一阶段默认入口。 |

## Evidence Doctor 规则

| 证据 | 输入 | 预期输出 | 异常处理 |
| --- | --- | --- | --- |
| dual / correspondence QEMU correctness | `log/qemu/run_test_dual_correspondence_family_*.log` | correctness / path shape only。 | 若失败，先修 candidate，不跑 bench。 |
| dual / correspondence QEMU bench shape | `log/qemu/analyze_bench_compare_dual_correspondence_family.log` | case label 和 checksum shape。 | QEMU timing 不作性能结论。 |
| board smoke diagnostic | `log/board/run_board_bench_dual_correspondence_family/analyze_bench_compare.log` | diagnostic manifest / doctor。 | Errors 或 Warnings 必须解释；single-run 只能作为初筛。 |
| future repeated board | planned future summary | 需要 manifest + doctor 才能进入 production integration 判断。 | 缺 repeated board 时，decision 只能是 diagnostic attempted。 |

## 阶段完成条件

- dual-indices 与 correspondences 的 block-baseline / block-fused-abcd-ilp candidate 不再是纯 `missing`。
- correctness、bench label、board smoke 和 doctor 至少闭合到 PointNormal-first diagnostic。
- 若结果负向，写清候选原因只能作为可验证假设，不单因归因为 gather。
- 若结果正向，也只升级为 production integration candidate；不能直接 production。

## Continue / Stop Decision

默认继续。C1 不能作为停止理由；C2-C7 都是当前授权且未阻塞的动作。只有以下情况允许暂停：

- 新 candidate 无法在不触碰 production 的条件下表达。
- correctness 失败且无法快速修复。
- 本地 QEMU / 板卡工具不可用。
- 继续需要 production patch、public API 扩权、repeated board 或 asm 归因授权。
