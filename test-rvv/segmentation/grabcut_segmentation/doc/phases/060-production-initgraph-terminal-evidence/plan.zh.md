# Phase 060: production initGraph terminal evidence plan

## 阶段意图和边界

当前 worktree 已存在 `GrabCut<PointT>::initGraphTerminalWeightsRVV()` production patch（生产补丁）。
本阶段按 Phase 040 的 PI2-PI5 合同验证这份现有补丁：确认 production direct（真实生产路径证据）
correctness、QEMU log-shape（QEMU 日志形状）、production-detail bench（生产细节性能测试）、
反汇编归属、板卡 repeated（重复板卡测试）和 Evidence Doctor（证据体检）。

本阶段不扩大生产范围。仍只覆盖 `initGraph()` 中 `TrimapUnknown` terminal weight（端点权重）
batch，不覆盖 color staging（颜色暂存）、organized n-link、non-organized KNN、max-flow、
public API（公开接口）变更、公共 RVV API 变更或 `Scalar=double`。PI5 无论证据正负，都停在用户检查点；
不自动采纳，也不自动回滚现有 production patch。

## 当前状态清单

| area | 当前事实 | 路径 / 证据 |
| --- | --- | --- |
| production patch | 新增 `initGraphTerminalWeightsRVV()`，`initGraph()` 在 RVV 构建中先尝试 unknown terminal batch，失败时走原标量 loop。 | `segmentation/include/pcl/segmentation/grabcut_segmentation.h`、`segmentation/include/pcl/segmentation/impl/grabcut_segmentation.hpp` |
| test asset | RVV 构建已有 `GrabCutProductionDirect.RvvTerminalWeightsMatchScalarAndFixedLabelsFallback`。 | `src/test_grabcut.cpp` |
| bench asset | `bench_grabcut --case production_initgraph_terminal` 已存在。 | `src/bench_grabcut.cpp` |
| evidence wrapper | `production_initgraph_terminal` 已在 CASE_LABELS 中登记为 `production_direct / production-detail`。 | `script/generate_grabcut_board_evidence_manifest.py` |
| registry | Phase 050 已计划登记 Phase 030 repeated summary；Phase 060 需要新增 production summary 登记。 | `log/evidence_registry.json` |

## pi2_scope 冻结

| 项 | 本阶段范围 |
| --- | --- |
| public entry | `fitGMMs()` / `refineOnce()` 通过私有 `initGraph()` 间接进入；测试和 bench 使用测试专用子类暴露 `initGraph()`，不改 public API。 |
| production detail | 只批量计算 `TrimapUnknown` terminal cost，并通过真实 `setTerminalWeights()` 写入 graph。 |
| point type / layout | 生产 helper 读取 `Image<Color>`、`GMM` 和 `indices_`；不直接读取 `PointT` 字段。测试入口使用 `PointXYZRGB`。 |
| fallback | 非 RVV 构建、小规模 unknown count、fixed-label trimap、n-link 和 solver 均保持标量。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| production terminal-weight RVV helper | indices ordered unknown trimap | `PointXYZRGB` via `Image<Color>` / float GMM | `GrabCut<PointT>::initGraphTerminalWeightsRVV()` | `make run_test_compare`，含 production direct 和 fixed-label fallback | `bench_grabcut --case production_initgraph_terminal` | 5-run board repeated planned | production helper or inlined call chain | production manifest + Doctor planned | planned / PI5 pending |

## 实现和测试动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| 生产范围审计 | Phase 060 result | diff 只触碰 Phase 040 允许的 production header / impl 范围。 |
| production correctness | QEMU gtest log | Std 4/4、RVV 6/6 通过；RVV 侧 production direct test 通过。 |
| QEMU smoke | QEMU bench log | `production_initgraph_terminal` case 可运行并输出 checksum / error budget；QEMU timing 只作日志形状。 |
| production asm | asm dump | `initGraphTerminalWeightsRVV` 或内联链路中能归属 RVV load/store/FMA 和 `expf_RVV_f32m2` 调用。 |
| production board repeated | run-labelled repeated dir | 5-run board B/A 进入 positive / weak-positive / neutral / negative / unstable bucket。 |
| production Evidence Doctor | Phase 060 manifest / Doctor | `Errors=0`；Warnings 必须解释或降级。 |
| registry | `log/evidence_registry.json` | 登记 production manifest / Doctor 摘要，不登记 raw board logs 为默认提交候选。 |

## Evidence Doctor 和 registry 规则

Phase 060 使用 production-specific summary artifact：

- `doc/phases/060-production-initgraph-terminal-evidence/repeated-evidence-manifest.json`
- `doc/phases/060-production-initgraph-terminal-evidence/repeated-evidence-doctor.md`
- `doc/phases/060-production-initgraph-terminal-evidence/repeated-evidence-doctor.json`

raw repeated board logs 保存到 `doc/phases/060-production-initgraph-terminal-evidence/repeated-board-<run-id>/`，
仅作为 local-only 取证输入。`evidence_status` 应同时检查 Phase 030 diagnostic summary 和 Phase 060 production summary。

## 板卡复跑预算和决策桶

| 项 | 设置 |
| --- | --- |
| run count | 5 |
| iterations / warmup | `--iterations 8 --warmup 2` |
| case | `production_initgraph_terminal` |
| positive | 5-run B/A 全部 `> 1.20`，median `> 1.20`。 |
| weak-positive | median 在 `1.05..1.20`，且无明显反向。 |
| neutral / negative | median `<= 1.05` 或多 run 反向。 |
| unstable | B/A 方向摇摆或 Evidence Doctor 暴露未处理 Error。 |

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | Phase 060 目标是 `production-detail`；Phase 030 只作为 historical diagnostic input（历史诊断输入）。 |
| A/B boundary | `production_detail_helper`，通过测试专用子类调用真实 `GrabCut<PointXYZRGB>::initGraph()`。 |
| 当前决策问题 | 现有 production patch 是否通过 correctness、asm、board 和 Evidence Doctor，能否进入 PI5 用户检查点。 |
| diagnostic 是否可外推到 production | 不再外推。Phase 060 直接测 production detail，但仍不是完整 public `extract/refineOnce` wall time。 |
| comparison-boundary / baseline mismatch 风险 | 存在剩余风险：bench 使用测试专用 prepared state，n-link 数量为 0，不覆盖 max-flow。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 若 Phase 060 为弱 / 负 / 中性 / 不稳定，不能建议采纳；需停在 PI5 用户检查点或等待回滚授权。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前只有一个 RVV family；若后续新增 family，需要同 boundary RVV-vs-RVV A/B。 |

## 继续 / 停止条件

若 correctness、asm、board 和 Doctor 均闭合，本阶段进入
`pending_user_confirmation_adopt_production`。若任一生产证据失败或反向，进入
`pending_user_confirmation_rollback` 或 `production_evidence_blocked`，并保留当前 patch 等待用户决定。
