# Phase 050 Result: getDistances vfsqrt full-RVV diagnostic

## 执行范围

本阶段重开 `getDistancesToModel` 的新实现族，但只在 test-only diagnostic（仅测试使用的诊断候选）边界内验证，不修改 production（生产源码）。候选参考 `normal_plane` 的 dense double write-back（密集 double 写回）形态：RVV gather（离散加载）读取 x/y，RVV 计算平方距离，使用 `vfsqrt`（RVV 向量平方根）、abs（绝对值）、`vfwcvt`（加宽转换）和 `vse64`（64 位向量写回）直接写入 `std::vector<double>`。

Phase 020 的负向结论只拒绝 `RVV sqr + scalar sqrt/store` 当前实现族。本阶段验证的是新的 full-RVV（完整 RVV）实现族，因此 Phase 020 不能作为停止理由。

## 动作结果

| action | 状态 | 证据 / 命令 | 结论 |
| --- | --- | --- | --- |
| test support split | done | `include/sac_model_circle_test_support.h`、`include/impl/sac_model_circle_candidates.hpp` | test / bench 通过稳定聚合头复用 Phase 020 与 Phase 050 候选。 |
| correctness | done | `make -C test-rvv/sample_consensus/sac_model_circle run_test_compare` | Std/RVV correctness 通过；full-RVV 候选与 public scalar companion 在 `1e-6` 误差预算内一致。 |
| asm attribution | done | `make -C test-rvv/sample_consensus/sac_model_circle check_getdistances_full_rvv_asm` | 候选符号内确认 `vfsqrt.v`、`vfwcvt.f.f.v` 和 `vse64.v`。 |
| repeated board | done | `collect_getdistances_full_rvv_repeated_board_evidence` | 5-run B/A 为 `1.4700, 1.4740, 1.4745, 1.4827, 1.4905`，median `1.4745x`。 |
| Evidence Doctor（证据体检） | done | `getdistances-full-rvv-repeated-evidence-doctor.md` | Errors=0，Warnings=0，Suggestions=0。 |
| registry | done | `log/evidence_registry.json` | Phase 050 manifest / doctor / json 已登记。 |

## Evidence paths

- manifest：`test-rvv/sample_consensus/sac_model_circle/doc/phases/050-getdistances-vfsqrt-full-rvv/getdistances-full-rvv-repeated-evidence-manifest.json`
- doctor markdown：`test-rvv/sample_consensus/sac_model_circle/doc/phases/050-getdistances-vfsqrt-full-rvv/getdistances-full-rvv-repeated-evidence-doctor.md`
- doctor json：`test-rvv/sample_consensus/sac_model_circle/doc/phases/050-getdistances-vfsqrt-full-rvv/getdistances-full-rvv-repeated-evidence-doctor.json`
- run label：`circle-phase050-getdistances-full-rvv-repeated-board`

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic（生产形态诊断），不是 production direct（真实生产入口直连证据）。 |
| A/B boundary | 同一 RVV binary 内，baseline 是 public `getDistancesToModel` companion，candidate 是 test-only full-RVV helper。 |
| 当前决策问题 | full-RVV 实现族是否值得进入 bounded production probe（有界生产探针）。 |
| diagnostic 是否可外推到 production | 不能直接外推；只能支持进入生产接入闭环。 |
| comparison-boundary / baseline mismatch 风险 | 有。baseline 和 candidate 的 wrapper、timer boundary 和实现层级不同。 |
| clean adoption 是否需要生产边界证据 | 需要。必须另做 production patch、production direct correctness、asm、board repeated 和 Evidence Doctor。 |

## EvidenceDecision

当前决策为 `enter_production_integration_loop`。Phase 050 的 full-RVV diagnostic 为 positive-stable，但它还不能写成 adopted production behavior（已采用生产行为）。它只证明这个实现族值得在 Phase 060 接入真实公开入口后复测。

## 阶段反思和下一步

full-RVV 形态消除了 Phase 020 的主要嫌疑点：逐 lane 标量 `sqrt` 和临时 float buffer 后处理。下一阶段默认进入 `060-getdistances-production-probe`，接入后必须停在 PI5（生产接入闭环的用户检查点），等待用户确认采纳或回滚 / 调整。
