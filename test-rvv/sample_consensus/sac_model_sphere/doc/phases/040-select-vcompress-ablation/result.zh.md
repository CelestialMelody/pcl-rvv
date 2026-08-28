# Phase 040: selectWithinDistance vcompress 消融结果

## 执行范围

本阶段只在 `test-rvv/sample_consensus/sac_model_sphere/` 内新增 `vcompress`（按 mask 压缩有效
lane 的 RVV 指令）候选、bench label、manifest 解析和 Evidence Doctor（证据体检）入口。生产源码在本阶段
开始时未替换；本阶段结果只回答“是否值得进入后续生产接入探针”。

## 动作回填

| 动作 | 状态 | 证据 |
| --- | --- | --- |
| 新增 test-only helper | done | `src/test_sac_model_sphere.cpp`、`src/bench_sac_model_sphere.cpp` 中新增 `selectWithinDistanceVCompressCandidate`。 |
| correctness（正确性） | done | `make -C test-rvv/sample_consensus/sac_model_sphere run_test_compare`；Std/RVV 构建各 5 个测试通过。 |
| 反汇编归属 | done | `make -C test-rvv/sample_consensus/sac_model_sphere clean_bench_rvv dump_bench_rvv`；`selectWithinDistanceVCompressCandidateRVV` 独立符号内 RVV 指令数为 28，包含 2 条 `vcompress.vm`。 |
| 板卡 repeated（重复板卡测试） | done | `SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_sphere collect_vcompress_repeated_board_evidence`；5-run 均完成。 |
| manifest / doctor / registry | done | `make -C test-rvv/sample_consensus/sac_model_sphere record_vcompress_board_evidence_state`。 |

## 板卡结果

板卡为 Milkv-Jupiter，case 为 `PointXYZ`、65536 点、direct indexed `indices_`、200 iterations、5 warmup。
`std_vs_rvv_repeated` 的 B/A 表示 Std 构建耗时除以 RVV 构建耗时；`rvv_vs_rvv_repeated` 的 B/A 表示
当前 production RVV helper 耗时除以 `vcompress` 候选耗时，`>1` 表示候选更快。

| case | B/A values | median | min / max | 结论 |
| --- | --- | ---: | ---: | --- |
| public `selectWithinDistance` | `1.4932, 1.4236, 1.5084, 1.4835, 1.5155` | `1.4932x` | `1.4236x / 1.5155x` | Phase 020 production baseline 仍正向。 |
| test-only `vcompress` select | `1.9189, 1.7970, 1.9242, 1.9078, 1.9300` | `1.9189x` | `1.7970x / 1.9300x` | 候选自身相对 Std 明显正向。 |
| `vcompress` vs current production select | `1.3067, 1.2808, 1.2977, 1.3099, 1.3020` | `1.3020x` | `1.2808x / 1.3099x` | 值得进入生产接入闭环。 |
| diagnostic `getDistancesToModel` candidate | `0.7871, 0.7865, 0.7997, 0.7672, 0.7797` | `0.7865x` | `0.7672x / 0.7997x` | 继续支持当前候选拒绝结论。 |

## Evidence Doctor 处理

`vcompress-repeated-evidence-doctor.md` 输出 `Errors=2, Warnings=4, Suggestions=0`。

两个 Error 都来自 `getDistancesToModel` 行：public entry 接近中性且 3/5 低于 1，测试专用 candidate 5/5 低于 1。
这些行不是本阶段 `selectWithinDistance` 实现族选择的采纳对象，继续按 out-of-scope 降级处理。

四个 Warning 都来自 `vcompress` vs current production select 的 mixed-boundary（混合边界）对比：
baseline 是真实 public overload，candidate 是 test-only helper，wrapper、timer boundary 和 reduction 字段不完全一致。
因此本阶段不能 clean-adopt 新实现族，只能把它判为 production probe candidate（生产探针候选）。

## EvidenceDecision

本阶段决策为 `production-ready for bounded production probe`：`vcompress` 候选在测试专用边界正确性、反汇编归属和
5-run 板卡 A/B 中稳定正向，值得进入 Phase 045，把候选接入真实 production `selectWithinDistanceRVV` 后重跑
production direct（真实生产入口直连）证据。

## 未覆盖范围

本阶段不证明生产 public entry 已经采用 `vcompress`，不证明其它点型、`Scalar=double`、其它 row source 或
`getDistancesToModel` 可以接入。Phase 045 必须在真实生产边界重跑 correctness、asm、board repeated 和
Evidence Doctor；只有接入后板卡仍显示收益，才可按用户偏好采纳并刷新正式 `doc-rvv`。
