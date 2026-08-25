# Phase 000 Result: current-state-and-label-equivalence

## 执行范围

实际执行范围与 `plan.zh.md` 一致：本阶段只创建 `test-rvv/features/organized_edge_detection`
下的 test-only diagnostic（测试专用诊断）资产，不修改 production。validated scope（已验证范围）为
`OrganizedEdgeBase<PointXYZ, Label>` depth label helper；unvalidated scope（未验证范围）包括 production
dispatch、泛型点类型、`PointLT` traits、RGB / normal 派生入口和上游完整测试。

## 计划动作回填

| action | status | evidence | conclusion |
| --- | --- | --- | --- |
| A1 scaffold | done | `Makefile`、`board.mk`、`src/test_organized_edge_detection.cpp`、`src/bench_organized_edge_detection.cpp`、`include/organized_edge_detection.h`、`include/impl/organized_edge_detection_depth_labels.hpp` | topic-local scaffold 已采用 `src/` + `include/impl/` 结构。 |
| A2 same-chain correctness | done | `make -C test-rvv/features/organized_edge_detection run_test_compare` | QEMU Std / RVV 构建各 3 个 gtest 全通过。 |
| A3 bench diagnostic | done | `log/board/repeated-summary.md` | 板卡 5-run 三组 case 均 strong positive，checksum match。 |
| A4 asm attribution | done | `build/asm/riscv/bench_organized_edge_detection_rvv.full.asm` | `computeDepthLabelsRVV` clone 附近有关键 RVV 指令。 |
| A5 Evidence Doctor / registry | done | `log/board/evidence_doctor.md`、`log/evidence_registry.json` | Errors=0 / Warnings=0 / Suggestions=3；registry 记录 summary、manifest、doctor 为 fresh。 |
| A6 docs | done | README、evaluation、roadmap、matrix、本 result | phase loop 可按短 prompt 恢复。 |

## Correctness / QEMU / asm / board 分层结论

| 证据层 | 结果 | 不能证明什么 |
| --- | --- | --- |
| correctness | `run_test_compare`：`MatchesScalarForDepthStepAndTailWidth`、`MatchesScalarForNanBoundarySearch`、`HonorsDisabledEdgeTypeBits` 在 Std/RVV 构建均通过。 | 不证明 production dispatch 或派生入口。 |
| QEMU path | QEMU 日志证明 RVV build 可执行并通过 correctness。 | 不证明真实性能。 |
| asm attribution | `computeDepthLabelsRVV` clone 附近出现 `vle32.v`、`vfsub.vv`、`vfmin.vv`、`vfmax.vv`、`vcpop.m`、`vse32.v`。 | 归属到 test-only helper，不是 production 符号。 |
| board performance | finite 320x240 mean `4.446x` / median `4.452x`；finite 641x481 tail mean `4.617x` / median `4.676x`；NaN boundary 320x240 mean `3.075x` / median `3.073x`。 | 不证明真实 `OrganizedEdgeBase::compute()` 入口收益，不证明 RGB / normal 路径。 |

## Diagnostic 到 production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | `diagnostic`。 |
| A/B boundary | `test helper`；Std build 的 `computeDepthLabelsRVV` 回退到标量 helper，RVV build 命中 RVV fast path。 |
| 当前决策问题 | `RVV-vs-scalar`，判断 depth label loop 是否值得 production probe。 |
| diagnostic 是否可外推到 production | 只能作为 bounded production probe 输入。production 容器、模板点型、真实 `compute()` 和派生入口仍未闭合。 |
| comparison-boundary / baseline mismatch 风险 | Evidence Doctor 修正后 wrapper metadata 同边界；仍需 production direct 同边界重跑。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前为 strong positive，可允许 PI1 计划；若后续 production direct 弱或负，不能沿用本诊断 speedup。 |
| clean adoption 是否需要 production boundary 内 RVV-vs-RVV detail A/B | 若后续有多个 RVV family，需要同一 production boundary 内比较；当前仅支持进入第一个 production probe。 |

## Evidence Doctor

`log/board/evidence_doctor.md` 结果为 Errors=0 / Warnings=0 / Suggestions=3。Suggestions 都是环境 metadata
缺少 taskset、governor、freq、temperature；当前板卡结果方向稳定且 checksum 一致，因此不阻塞 diagnostic
结论。若进入 production direct board bench，应补充或记录这些环境字段，或者在 Handoff 中继续说明边界。

## Evidence registry 和 freshness

`log/evidence_registry.json` 已记录：

- `log/board/repeated-summary.md`
- `log/board/evidence_manifest.json`
- `log/board/evidence_doctor.md`

三者 freshness state 为 `fresh`。raw repeated logs 位于 `log/board/repeated_*/`，默认 local-only，不作为 topic-only
提交边界的一部分，除非用户明确要求 evidence logs（证据日志）。

## Topic maturity / doc suite audit

| area | current shape scan | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- |
| test/bench source layout | `src/test_organized_edge_detection.cpp` 和 `src/bench_organized_edge_detection.cpp` 已存在。 | adopted | 当前 topic 无根目录长源码。 | none |
| aggregator and internal helpers | `include/organized_edge_detection.h` 聚合入口和 `include/impl/organized_edge_detection_depth_labels.hpp` 内部 helper 已存在。 | adopted | 职责为 reference、candidate、index helper、checksum；当前行数未超过 soft limit。 | production probe 时再按 traits / fallback 拆分。 |
| script and registry | topic-local manifest script、Evidence Doctor 和 registry 已存在。 | adopted | `record_board_evidence_state` 已执行。 | production direct 后扩展 manifest metadata。 |
| target granularity | correctness aggregate、bench compare、board repeated、doctor / registry target 已存在；QEMU bench compare guard 由共享 Makefile 保持。 | adopted | 本阶段没有 production direct target。 | PI1 / PI3 补 production direct target。 |
| topic-local docs | README、evaluation、roadmap、phase index、matrix、plan/result 已存在；细分 testing docs 合并进 evaluation。 | adopted for Phase 000 | 当前诊断规模较小，独立 role 文档可在 production probe 后补。 | PI1 后若进入 production，补完整 doc suite。 |
| production topic doc | 不适用。 | not_applicable with evidence | 没有 adopted production behavior。 | 用户确认采纳 production patch 后再创建。 |
| artifact tracking | topic 路径新增文件需进入 topic artifact boundary；build 和 raw repeated logs 默认 excluded。 | partial | 需提交前路径限定扫描。 | closeout / commit 前复查。 |

## EvidenceDecision

`partial-production-candidate`。Phase 000 证明 depth label test helper 值得进入 PI1 production integration plan，但不能直接接入或采纳 production。

## Continue / Stop Decision

本轮停止命中 `production integration requires user authorization`。继续到 PI1 会开始生产接入计划，并可能在 PI1 gate 闭合后修改 `features/include/pcl/features/impl/organized_edge_detection.hpp`；按仓库规则需要用户确认。默认下一 phase 是 `010-production-probe-plan`。
