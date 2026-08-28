# Phase 070: getDistances target 粒度补齐计划

## 阶段意图和边界

本阶段只补齐 `getDistancesToModel` correctness alias（正确性细分入口）。目标是让 reviewer 不必手写 `TEST_ARGS=--gtest_filter=*GetDistances*`，即可单独复核 Phase 040 的两个 getDistances correctness test（正确性测试）。

本阶段不修改 production（生产源码），不改变 count/select/getDistances 的 diagnostic evidence（诊断证据）结论，不新增 board（板卡）性能结论，也不创建 `doc-rvv/sample_consensus/sac_model_stick-RVV.zh.md`。

## 当前状态清单

| 项目 | 当前状态 |
| --- | --- |
| production | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_stick.hpp` 当前无本 topic diff；三入口仍无 production RVV dispatch（生产 RVV 分流）。 |
| correctness aggregate | `make -C test-rvv/sample_consensus/sac_model_stick run_test_compare` 已覆盖 Std/RVV 两个构建的 6 个 gtest。 |
| correctness aliases | 已有 `run_stick_count_tests` 和 `run_stick_select_tests`；缺少 `run_stick_getdistances_tests`。 |
| Phase 040 | `doc/phases/040-stick-getdistances-diagnostic/result.zh.md` 已记录 getDistances diagnostic candidate 为 `partial-production-candidate`。 |
| Phase 060 | `doc/phases/060-stick-doc-suite-structure/result.zh.md` 已记录 doc suite 结构闭合，但 target 粒度还有 getDistances alias 微缺口。 |

## RED 检查

新增 alias 前运行：

```bash
make -C test-rvv/sample_consensus/sac_model_stick run_stick_getdistances_tests
```

预期失败为 `No rule to make target 'run_stick_getdistances_tests'`。这个失败证明本阶段要补的是可运行入口行为，而不是只改文档文字。

## 实现和测试动作

| 动作 | 产物 | 完成判据 |
| --- | --- | --- |
| 新增 `STICK_GETDISTANCES_FILTER` | `Makefile` | filter 覆盖 `GetDistancesCandidateMatchesPublicDirectionCoefficientSemantics` 和 `GetDistancesCandidatePreservesPenaltyAndDenseIndexedOrder`。 |
| 新增 `run_stick_getdistances_tests` | `Makefile` | target 调用 RVV build 的 `run_test_rvv`，只跑 getDistances 两个 case。 |
| 同步文档入口 | README、testing overview、correctness tests、roadmap、matrix、phase index | 所有“当前没有 getDistances alias”的旧说法改为 Phase 070 已补齐。 |
| 写阶段结果 | `result.zh.md` | 记录 RED/GREEN、验证命令、证据边界和继续 / 停止判断。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| getDistances-target-granularity | not_applicable | topic-local Makefile target | `run_stick_getdistances_tests` alias | planned: RVV build gtest filter for two getDistances cases | not_applicable | not_applicable | not_applicable | registry freshness check after doc sync | planned | finish Phase 070 docs and verification |

## Evidence Doctor 和 registry 规则

本阶段不生成新的 benchmark summary（性能摘要）或 board evidence（板卡证据），因此不运行 Evidence Doctor 生成新报告。阶段结束前运行：

```bash
make -C test-rvv/sample_consensus/sac_model_stick repeated_evidence_status
```

它检查既有 Phase 000 / 020 / 040 summary evidence（摘要证据）仍被文档引用，且 registry（证据登记表）为 fresh。

## 阶段完成条件

- `run_stick_getdistances_tests` 从 RED 的无规则失败变为 GREEN，通过两个 getDistances case。
- `run_test_compare` 仍通过 Std/RVV 6 个 gtest。
- topic-local 文档不再声称 getDistances alias 缺失。
- `git diff --check` 对 topic 和队列表路径通过。

## 继续 / 停止条件

本阶段完成后，target 粒度缺口关闭。剩余高优先级动作仍是三条 production probe（生产探针）进入 PI2，但这需要用户明确授权修改 production；未授权前合法停止在 `authorization-gated`。
