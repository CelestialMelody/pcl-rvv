# 010 diagnostic-repeated-board 计划

## 阶段意图和边界

本阶段只增强 DON helper-only diagnostic（诊断）性能证据：把 phase 000 的 1-run board smoke 扩展成 5-run repeated board（重复板卡测试）summary，并用 Evidence Doctor（证据体检）检查稳定性。阶段仍不修改 production（生产源码），不把 test-only helper 证据写成 production direct（真实生产路径证据）。

验证范围保持为 ordered normal cloud（顺序 normal 点云）、`pcl::Normal` / `float` / AoS（结构数组）布局，输入规模 `262144`，bench case 为 `don_normal_pair`。

## 当前状态清单

| 项目 | 当前状态 |
| --- | --- |
| phase 000 | `partial-production-candidate`；correctness / asm / board smoke 已闭合 |
| 当前性能证据 | `log/board/analyze_bench_compare.log` 为 `1.10x`，但 Evidence Doctor 报 `low_run_count` |
| registry | `log/evidence_registry.json` 已登记 phase 000 board smoke |
| 板卡状态 | 当前会话说明板卡可用；本阶段继续有界板卡复跑 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| strided AoS normal diff RVV | ordered normal cloud | `pcl::Normal` / float / AoS | test-only `computeDoNRVV()` | phase 000 QEMU Std/RVV 3/3 | `don_normal_pair` helper-only | planned: 5-run repeated board | phase 000 asm dump | planned on repeated manifest | planned | run repeated board, summarize, doctor, registry |

## 实现和测试动作

| 动作 | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| repeated collect target | `Makefile` 的 `collect_board_don_repeated` | 生成 `log/board/repeated_phase010_diagnostic/run-01..run-05/` |
| repeated summary wrapper | `script/generate_don_repeated_summary.py` | 生成 `summary.md` 和 `evidence_manifest.json`，manifest 的 `run_count=5` 且含 5 个 `ba_values` |
| Evidence Doctor | `make -C test-rvv/features/don run_board_don_repeated_evidence_doctor` | `evidence_doctor.md` 有 Errors / Warnings / Suggestions 摘要 |
| registry | `make -C test-rvv/features/don record_board_don_repeated_state` | `log/evidence_registry.json` 登记 repeated summary / manifest / doctor |
| 文档回填 | `result.zh.md`、`optimization-matrix.zh.md`、`optimization-roadmap.zh.md`、`doc/don-evaluation.zh.md` | 写清 decision bucket 和 production 边界 |

## Evidence Doctor 和 registry 规则

本阶段使用 `test-rvv/script/evidence_doctor.py --manifest log/board/repeated_phase010_diagnostic/evidence_manifest.json`。Error 阻塞生产候选升级；Warning 必须解释并降级证据边界或列入 PI1 gate。registry 记录 run label `board-don-diagnostic-repeated-phase010`。

## 板卡复跑预算和决策桶

预算为 5-run repeated board，一轮完成后不自动扩大。decision bucket：

- `positive`：median >= `1.20x` 且没有 run 低于 `1.0x`。
- `weak_positive`：median >= `1.05x` 且 `B/A < 1` 不超过 1/5。
- `neutral`：median 位于 `[0.95x, 1.05x)`。
- `negative`：median < `0.95x`。
- `unstable`：不满足上述稳定条件或 Evidence Doctor 暴露无法解释的方向摇摆。

## 继续 / 停止条件

若 repeated 结果为 `weak_positive` 或 `positive` 且无 Evidence Doctor Error，默认下一阶段是 PI1 production integration plan，先冻结 `don.hpp` 的生产接入范围、fallback、点类型和测试计划。若结果为 `neutral`、`negative`、`unstable` 或板卡 / 工具失败，则停在 diagnostic closeout 或 blocked handoff，不改 production。
