# Normal-plane Evidence Registry 与 Target Alias Phase Plan

## 阶段意图和边界

本阶段只补 `test-rvv/sample_consensus/plane_models` 的证据自动化入口，不修改 production 源码，不新增 RVV 实现族，也不改变 phase 000 的性能结论。目标是把当前手工维护的 phase-local manifest（证据清单）和 Evidence Doctor（证据体检）报告，变成可由 topic-local script + Make target 重建、登记和检查的证据链。

本阶段要证明：

- topic-local wrapper 能从当前 board Std/RVV bench logs 生成 phase 000 manifest。
- Makefile 提供 `generate_board_evidence_manifest`、`run_board_evidence_doctor`、`record_board_evidence_state` 和 `evidence_status`。
- `log/evidence_registry.json` 能登记 manifest / doctor summary，并能用 doc refs 做 freshness check（新鲜度检查）。

本阶段不证明：

- board compare 变成 repeated summary；当前仍是 run_count=1 positive bucket。
- protected helper bench 等同完整公开入口计时；公开入口仍由 GTest 证明。
- raw logs 默认可提交；仍采用 summary-only 策略。

## 当前状态清单

| 对象 | 当前事实 | 路径 |
| --- | --- | --- |
| phase 000 manifest | 已存在，但此前通过手工刷新。 | `doc/phases/000-normal-plane-current-state-and-public-entry-boundary/evidence-manifest.json` |
| Evidence Doctor | 已存在，Errors=0、Warnings=0、Suggestions=0。 | `doc/phases/000-normal-plane-current-state-and-public-entry-boundary/evidence-doctor.md` |
| board summary | `run_board_bench_compare fetch_board_logs` 后生成易覆盖日志。 | `log/board/analyze_bench_compare.log`、`log/board/run_bench_std.log`、`log/board/run_bench_rvv.log` |
| registry | 当前不存在。 | `log/evidence_registry.json` |
| doc refs | README、benchmark/evidence、phase result、evaluation 和长期 `doc-rvv` 已引用 phase 000 evidence。 | topic-local docs |

## 假设与候选族

| candidate family | 假设 | 本阶段动作 |
| --- | --- | --- |
| topic-local manifest wrapper | 当前 board logs 格式稳定，能解析三条 helper item 的 Std/RVV avg、iterations、dataset 和 size。 | 新增 `script/generate_normal_plane_board_evidence_manifest.py`。 |
| Make target aliases | topic-local target 比手工命令更可恢复。 | 新增 manifest、doctor、record、status target。 |
| registry freshness check | 登记 phase 000 summary artifact 能发现后续覆盖或未登记改动。 | 用共享 `evidence_registry.py` record/check。 |

## 优化矩阵

| candidate family | scope | correctness target | bench / evidence target | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- |
| manifest wrapper | phase 000 board helper evidence | not_applicable | `generate_board_evidence_manifest` | `run_board_evidence_doctor` | planned | 写 topic-local script |
| registry alias | manifest / doctor summary artifacts | not_applicable | `record_board_evidence_state`、`evidence_status` | doctor output must remain 0/0/0 | planned | 更新 Makefile |
| repeated board summary | run_count=5 board evidence | not_applicable | not in this phase | not_applicable | deferred | 需要单独 phase 或用户要求 |

## 实现和测试动作

| action | 产物 | 命令 / 证据 | 完成判据 |
| --- | --- | --- | --- |
| A1 phase plan | 本文件 | path-limited `git status` | plan 先于 script / Makefile 修改存在。 |
| A2 manifest wrapper | `script/generate_normal_plane_board_evidence_manifest.py` | `make -C test-rvv/sample_consensus/plane_models generate_board_evidence_manifest` | 生成 manifest 且数值与 board summary 一致。 |
| A3 doctor alias | Makefile | `make -C test-rvv/sample_consensus/plane_models run_board_evidence_doctor` | Evidence Doctor 输出 Errors=0、Warnings=0、Suggestions=0。 |
| A4 registry alias | Makefile + `log/evidence_registry.json` | `make -C test-rvv/sample_consensus/plane_models record_board_evidence_state`、`make ... evidence_status` | registry check 输出 fresh。 |
| A5 docs sync | README、testing overview、benchmark/evidence、optimization evidence、code map、matrix、roadmap、evaluation、phase result | `rg` stale scan；`git diff --check` | 文档引用 target / registry 状态一致。 |

## Evidence Doctor 和 registry 规则

`run_board_evidence_doctor` 必须先生成 manifest，再调用 `test-rvv/script/evidence_doctor.py`。`record_board_evidence_state` 登记 manifest、doctor md、doctor json 和 board analyze summary，不登记 raw `run_bench_*.log`。`evidence_status` 扫描这些 summary artifacts，并要求 README、evaluation、phase result 或长期 `doc-rvv` 至少引用其中的路径或 run label。

## 板卡复跑预算和决策桶

本阶段不要求重新跑板卡；它消费 phase 010 后已通过的 `log/board` 当前 summary。若生成脚本发现日志缺失或格式不匹配，先标为 `blocked` 并给出需要重新运行的命令，而不是改写性能结论。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic；bench 调用 protected helper hot path。 |
| A/B boundary | Std/RVV protected helper bench wrapper。 |
| 当前决策问题 | evidence automation / freshness，不做新的 RVV-vs-scalar 取舍。 |
| diagnostic 是否可外推到 production | 不外推；公开入口 dispatch / fallback 仍由 GTest 证明。 |
| comparison-boundary / baseline mismatch 风险 | script 必须从同一批 Std/RVV board logs 生成 manifest；registry 记录 digest 防止后续覆盖。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不适用；本阶段不新增 production probe。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 不需要；未选择新 RVV family。 |

## 继续 / 停止条件

继续条件：manifest wrapper、doctor alias、registry record/check 或文档同步任一项仍未关闭且没有工具阻塞。停止条件：日志缺失且板卡不可用、registry 脚本异常无法定位、Evidence Doctor 出现未处理 Error、dirty isolation 不安全，或继续需要扩大到 production / public API / 其它 topic。

默认下一 phase：若本阶段关闭，当前 topic 的结构收敛可进入 `ready_for_review_validity_check`。泛型点类型扩展和 repeated board summary 作为用户可选后续路径，不作为本阶段自动继续项。
