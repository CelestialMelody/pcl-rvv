# Phase 000 Result

本阶段已完成接入 production（生产源码）前的 GICP 组件诊断。production 目标文件 `registration/include/pcl/registration/impl/gicp.hpp` 未修改，当前证据不能作为 production direct evidence（生产边界直接证据）。

## 计划动作状态

| action | status | evidence | conclusion |
| --- | --- | --- | --- |
| 创建 topic scaffold | done / partial-order-deviation | `test-rvv/registration/gicp/` | scaffold 已建立；流程偏差是 scaffold 先于 phase plan 创建，本 result 和交接包必须暴露 |
| QEMU correctness | done / historical | `test-rvv/registration/gicp/log/qemu/run_test_std.log`、`test-rvv/registration/gicp/log/qemu/run_test_rvv.log` | Phase 000 当时 Std/RVV 各 7 个 GTest 全部通过；Phase 001 后当前日志为 10 个 GTest |
| QEMU smoke + asm | done | `test-rvv/registration/gicp/log/qemu/run_bench_all_rvv.log`、`test-rvv/registration/gicp/build/asm/riscv/bench_gicp_rvv.asm` | QEMU bench 仅证明 build/log shape；asm dump 中 RVV 指令存在，但不是 production hot-symbol attribution |
| QEMU Evidence Doctor | done / metadata-limited | `test-rvv/registration/gicp/log/qemu/evidence_doctor.md` | `missing_comparisons`，因为 QEMU smoke 只给 raw bench log；仅作为轻量 reviewer aid，不作为性能证据 |
| board smoke | done / refreshed | `test-rvv/registration/gicp/log/board/test_smoke/run_test.log` | Phase 000 当时板卡 RVV GTest 7 个通过；Phase 001 后当前板卡 smoke 为 10 个 GTest |
| board repeated | done | `test-rvv/registration/gicp/log/board/component_diagnostic_repeated/summary.md`、`test-rvv/registration/gicp/log/board/component_diagnostic_repeated/evidence_manifest.json` | 5-run component diagnostic 已完成 |
| implementation iteration | done | `test-rvv/registration/gicp/include/impl/gicp_candidates.hpp`、`test-rvv/registration/gicp/src/bench_gicp.cpp` | residual 改为 `f64m4` 宽分块，新增 indexed-gather residual，covariance 保留 `f64m1`；bench checksum 加 salt |
| board Evidence Doctor | done / diagnostic-warning-only | `test-rvv/registration/gicp/log/board/component_diagnostic_repeated/evidence_doctor.md` | Errors=0、Warnings=5；Warnings 是 reduction order 差异和长尾波动导致的 diagnostic 降级 |
| evidence registry | done | `test-rvv/registration/gicp/log/evidence_registry.json` | 已登记 QEMU correctness、QEMU smoke/asm、board summary/manifest/doctor |

## 板卡 repeated 结果

| case | runs | median speedup | min | max | B/A < 1 | bucket | decision |
| --- | ---: | ---: | ---: | ---: | ---: | --- | --- |
| `covariance-post-knn-default-k` | 5 | 1.151x | 1.027x | 1.259x | 0/5 | `weak_positive` | 支持进入 PI1 profile/probe 讨论；需解释长尾和 KdTree/SVD 稀释 |
| `residual-indexed-gather` | 5 | 1.255x | 1.140x | 1.322x | 0/5 | `positive` | 支持进入 PI1 profile/probe 讨论；索引访存未吞掉 residual 收益 |
| `residual-mahalanobis-dense` | 5 | 1.347x | 1.317x | 1.411x | 0/5 | `positive` | 支持进入 PI1 profile/probe 讨论；仍需验证 optimizer 调用频率 |

## Evidence Doctor 解释

`test-rvv/registration/gicp/log/board/component_diagnostic_repeated/evidence_doctor.md` 当前没有 Error。上一版 `f64m1` residual 曾出现 `ba_degradation_frequency`；本轮通过 residual `f64m4` 宽分块修正后，dense-row 和 indexed-gather residual 的 5/5 run 均高于 1。

三个 case 都有 `contract_mismatch` warning：baseline 是 `scalar order`，candidate 是 `RVV chunk reduction`。这是当前 diagnostic 的有意差异，用于评估 RVV chunk reduction（分块规约）本身；因此结论必须降级为 pre-production diagnostic，不能外推成 production A/B。`covariance-post-knn-default-k` 和 `residual-indexed-gather` 另有 long-tail warning，必须在 PI1 中记录 min/median/max，不能只引用 median。

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | `pre_production_diagnostic` |
| A/B boundary | `test helper`，不是 public GICP entry |
| 当前决策问题 | 是否值得进入 PI1 production integration plan（生产接入计划） |
| diagnostic 是否可外推到 production | 不能直接外推；当前排除了 KdTree search、correspondence search、Eigen SVD、Newton/BFGS solver 和 dispatch |
| residual 组件是否支持接入 | 不支持直接接入；dense-row 与 indexed-gather 都支持进入有界 PI1 profile/probe |
| covariance 组件是否支持接入 | 不支持直接接入；支持进入有界 PI1 profile/probe，但优先级低于 residual |
| clean adoption 是否需要 production direct evidence | yes，需要同一 production boundary 内的 fallback、asm、correctness、board repeated 和 public profile |

## 当前 EvidenceDecision

`diagnostic / recommend_PI1_before_source_edit`。

不建议直接将当前 test-only RVV 优化实现接入 `registration/include/pcl/registration/impl/gicp.hpp`。但与上一版不同，本轮板卡证据已足以建议进入 PI1：围绕 residual / Mahalanobis dense-row、residual indexed-gather 和 covariance post-KNN 三个组件做 public-entry profile prerequisite（公开入口性能占比前置证明）、production boundary audit（生产边界审计）和有界生产探针。

`doc-rvv/registration/gicp-RVV.zh.md` 仍不适用，因为 production 行为尚未采纳。

## Continue / Stop Decision

停止在用户确认点：建议继续到 PI1，但仍不要直接接 production。PI1 必须先验证 public GICP entry 中 residual / covariance 占比、optimizer 调用频率、fallback 形状、asm attribution 和 production direct correctness。
