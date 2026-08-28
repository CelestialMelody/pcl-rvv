# sac_model_line RVV topic 导航

## 当前结论

Phase 000 为 `SampleConsensusModelLine<PointT>::countWithinDistance` 建立了 production-shaped diagnostic（生产形态诊断，测试专用代码模拟真实公开入口的数据形态）。Phase 010 为 `selectWithinDistance` 建立了同边界 select diagnostic。Phase 020 尝试 `getDistancesToModel` 的 RVV 平方距离 + 标量 sqrt / dense store 形状，结果为负向诊断。Phase 030 改为在 RVV chunk（可变向量长度分块）内执行 `vfsqrt`（RVV 向量平方根），得到同边界正向诊断。当前只覆盖 direct indexed `indices_`、`PointXYZ`、float xyz AoS（结构数组布局）和 65536 点 synthetic line-distance cloud（合成直线距离点云）。

`countWithinDistance` 测试专用 candidate（候选实现）在 5-run board repeated（板卡重复性能测试）中得到 positive-stable：`Std avg 1.770477 ms`，`RVV avg 0.396724 ms`，B/A values 为 `4.4920x, 4.4350x, 4.4569x, 4.4390x, 4.4913x`，median/min/max 为 `4.4569x / 4.4350x / 4.4920x`。

`selectWithinDistance` 测试专用 candidate 在 5-run board repeated 中也得到 positive-stable：`Std avg 2.466660 ms`，`RVV avg 0.795046 ms`，B/A values 为 `3.0647x, 3.1409x, 3.0622x, 3.1010x, 3.1434x`，median/min/max 为 `3.1010x / 3.0622x / 3.1434x`。公开 `countWithinDistance` / `selectWithinDistance` 行仍约 `1.00x`，只说明 production 源码尚未接入 line RVV 分流。

`getDistancesToModel` 的 Phase 020 scalar-sqrt 候选在 5-run board repeated 中退化，median/min/max 为 `0.9340x / 0.9289x / 0.9732x`，Evidence Doctor（证据体检）为 Errors=1 / Warnings=0 / Suggestions=0。Phase 030 vfsqrt 候选在 5-run board repeated 中得到 positive-stable：`Std avg 2.266758 ms`，`RVV avg 0.653100 ms`，B/A values 为 `3.3746x, 3.3924x, 3.5378x, 3.5283x, 3.5208x`，median/min/max 为 `3.5208x / 3.3746x / 3.5378x`，Evidence Doctor 为 0/0/0。

Phase 040 已把这三个入口接入 production（生产源码）并完成接入后的 production direct（真实生产入口直连）板卡重复性能测试。Phase 050 进一步把 `getDistancesToModelRVV` 的写回形态从 `float scratch + 标量 lane 转 double` 改成 `vfwcvt + vse64` 直接写 `std::vector<double>`，接入后 public board repeated 仍为 positive-stable，并把 `getDistancesToModel` median 提升到 `4.2237x`。Phase 060 又把 `selectWithinDistanceRVV` 的压缩平方误差写回从 `float scratch + 标量 lane 转 double` 改成 `vfwcvt + vse64` 直接写 `error_sqr_dists_`，接入后 public select median 为 `3.2460x`，Evidence Doctor 为 0/0/0。Phase 070 尝试 identity-index strided load（恒等索引跨步加载）后，同一生产边界内 RVV-vs-RVV A/B（RVV 实现族对比）显示 select 退化且 shuffled 控制组不稳，候选已拒绝并回退到 Phase 060 gather-only load family（只使用离散加载的实现族）。PI5 evidence（生产证据决策）为 positive-stable，当前范围已按本轮采纳条件写成 adopted production behavior（已采纳生产行为）。正式长期文档是 `doc-rvv/sample_consensus/sac_model_line-RVV.zh.md`，其中生产性能数据来自 Phase 060 接入后的板卡测试。

## 先读哪份文档

| 目的 | 入口 |
| --- | --- |
| 恢复当前阶段 | `doc/phases/README.zh.md` |
| 看函数入口、Traceability Map（可追踪性地图）和 EvidenceDecision（证据决策） | `doc/sac_model_line-evaluation.zh.md` |
| 看测试 target（目标）分类 | `doc/testing-overview.zh.md` |
| 看 gtest（GoogleTest 单元测试）覆盖 | `doc/correctness-tests.zh.md` |
| 看 bench（性能测试）、board、manifest（证据清单）和 Evidence Doctor（证据体检） | `doc/benchmark-and-evidence.zh.md` |
| 看候选取舍 | `doc/optimization-evidence.zh.md` |
| 看测试支撑代码定位 | `doc/test-support-code-map.zh.md` |
| 看后续 candidate 搜索空间 | `doc/optimization-roadmap.zh.md` |
| 看阶段矩阵 | `doc/phases/optimization-matrix.zh.md` |

## 常用命令

```bash
make -C test-rvv/sample_consensus/sac_model_line run_test_compare
make -C test-rvv/sample_consensus/sac_model_line dump_bench_rvv
make -C test-rvv/sample_consensus/sac_model_line check_production_asm
SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_line check_board_ssh
SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_line board_smoke
SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_line collect_repeated_board_evidence
SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_line collect_repeated_board_select_evidence
SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_line collect_repeated_board_get_distances_evidence
SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_line collect_repeated_board_get_distances_vfsqrt_evidence
SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_line collect_repeated_board_production_evidence
SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_line collect_repeated_board_production_vse64_evidence
SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_line collect_repeated_board_production_select_vse64_evidence
make -C test-rvv/sample_consensus/sac_model_line record_repeated_board_evidence_state
make -C test-rvv/sample_consensus/sac_model_line record_repeated_board_select_evidence_state
make -C test-rvv/sample_consensus/sac_model_line record_repeated_board_get_distances_evidence_state
make -C test-rvv/sample_consensus/sac_model_line record_repeated_board_get_distances_vfsqrt_evidence_state
make -C test-rvv/sample_consensus/sac_model_line record_repeated_board_production_evidence_state
make -C test-rvv/sample_consensus/sac_model_line record_repeated_board_production_vse64_evidence_state
make -C test-rvv/sample_consensus/sac_model_line record_repeated_board_production_select_vse64_evidence_state
make -C test-rvv/sample_consensus/sac_model_line repeated_evidence_status
make -C test-rvv/sample_consensus/sac_model_line repeated_select_evidence_status
make -C test-rvv/sample_consensus/sac_model_line repeated_get_distances_evidence_status
make -C test-rvv/sample_consensus/sac_model_line repeated_get_distances_vfsqrt_evidence_status
make -C test-rvv/sample_consensus/sac_model_line repeated_production_evidence_status
make -C test-rvv/sample_consensus/sac_model_line repeated_production_vse64_evidence_status
make -C test-rvv/sample_consensus/sac_model_line repeated_production_select_vse64_evidence_status
make -C test-rvv/sample_consensus/sac_model_line identity_evidence_status
make -C test-rvv/sample_consensus/sac_model_line identity_shuffled_evidence_status
```

QEMU（仿真器）只用于 correctness（正确性）、构建和日志形状。性能结论只来自 board 或目标硬件。
Phase 070 的 `collect_identity_repeated_board_evidence` 和 `collect_identity_shuffled_repeated_board_evidence`
是历史候选重跑入口，不放在普通常用命令里；`record_identity_board_evidence_state` 和
`record_identity_shuffled_board_evidence_state` 重新生成 manifest / doctor 时还需要先重放
identity-strided candidate 并显式设置 `ALLOW_PHASE070_IDENTITY_REFRESH=1`。只检查现有登记状态时使用
`identity_evidence_status` 和 `identity_shuffled_evidence_status`。

## 当前可提交证据

| 类型 | 路径 | 边界 |
| --- | --- | --- |
| phase plan | `doc/phases/000-line-count-diagnostic/plan.zh.md` | 本阶段范围和证据计划。 |
| phase result | `doc/phases/000-line-count-diagnostic/result.zh.md` | 当前诊断结论主归属。 |
| repeated manifest | `test-rvv/sample_consensus/sac_model_line/doc/phases/000-line-count-diagnostic/repeated-evidence-manifest.json` | summary evidence（摘要证据），可复核 5-run board 数据。 |
| repeated doctor | `test-rvv/sample_consensus/sac_model_line/doc/phases/000-line-count-diagnostic/repeated-evidence-doctor.md` | summary evidence，Errors / Warnings / Suggestions 均为 0。 |
| repeated doctor JSON | `test-rvv/sample_consensus/sac_model_line/doc/phases/000-line-count-diagnostic/repeated-evidence-doctor.json` | Evidence Doctor 的机器可读结果。 |
| select phase plan | `doc/phases/010-line-select-diagnostic/plan.zh.md` | select 阶段范围和证据计划。 |
| select phase result | `doc/phases/010-line-select-diagnostic/result.zh.md` | select 诊断结论主归属。 |
| select repeated manifest | `test-rvv/sample_consensus/sac_model_line/doc/phases/010-line-select-diagnostic/repeated-evidence-manifest.json` | summary evidence，可复核 select 5-run board 数据。 |
| select repeated doctor | `test-rvv/sample_consensus/sac_model_line/doc/phases/010-line-select-diagnostic/repeated-evidence-doctor.md` | summary evidence，Errors / Warnings / Suggestions 均为 0。 |
| select repeated doctor JSON | `test-rvv/sample_consensus/sac_model_line/doc/phases/010-line-select-diagnostic/repeated-evidence-doctor.json` | Evidence Doctor 的机器可读结果。 |
| getDistances scalar-sqrt phase result | `doc/phases/020-line-get-distances-diagnostic/result.zh.md` | 记录当前 scalar-sqrt 形状的负向诊断。 |
| getDistances scalar-sqrt repeated manifest | `test-rvv/sample_consensus/sac_model_line/doc/phases/020-line-get-distances-diagnostic/repeated-evidence-manifest.json` | summary evidence，可复核 scalar-sqrt 5-run board 数据。 |
| getDistances scalar-sqrt repeated doctor | `test-rvv/sample_consensus/sac_model_line/doc/phases/020-line-get-distances-diagnostic/repeated-evidence-doctor.md` | summary evidence，Errors=1 / Warnings=0 / Suggestions=0。 |
| getDistances vfsqrt phase result | `doc/phases/030-line-get-distances-vfsqrt-diagnostic/result.zh.md` | 记录当前 vfsqrt 形状的正向诊断。 |
| getDistances vfsqrt repeated manifest | `test-rvv/sample_consensus/sac_model_line/doc/phases/030-line-get-distances-vfsqrt-diagnostic/repeated-evidence-manifest.json` | summary evidence，可复核 vfsqrt 5-run board 数据。 |
| getDistances vfsqrt repeated doctor | `test-rvv/sample_consensus/sac_model_line/doc/phases/030-line-get-distances-vfsqrt-diagnostic/repeated-evidence-doctor.md` | summary evidence，Errors / Warnings / Suggestions 均为 0。 |
| getDistances vfsqrt repeated doctor JSON | `test-rvv/sample_consensus/sac_model_line/doc/phases/030-line-get-distances-vfsqrt-diagnostic/repeated-evidence-doctor.json` | Evidence Doctor 的机器可读结果。 |
| production phase result | `doc/phases/040-line-production-integration/result.zh.md` | PI5 检查点主归属，记录接入后公开入口数据。 |
| production repeated manifest | `test-rvv/sample_consensus/sac_model_line/doc/phases/040-line-production-integration/production-repeated-evidence-manifest.json` | summary evidence，可复核接入后 public count/select/getDistances 5-run board 数据。 |
| production repeated doctor | `test-rvv/sample_consensus/sac_model_line/doc/phases/040-line-production-integration/production-repeated-evidence-doctor.md` | summary evidence，Errors / Warnings / Suggestions 均为 0。 |
| production repeated doctor JSON | `test-rvv/sample_consensus/sac_model_line/doc/phases/040-line-production-integration/production-repeated-evidence-doctor.json` | Evidence Doctor 的机器可读结果。 |
| production getDistances vfwcvt/vse64 phase result | `doc/phases/050-line-get-distances-vse64-store/result.zh.md` | 历史 getDistances 写回基线；记录 `getDistancesToModelRVV` direct double store 的接入后数据。 |
| production vfwcvt/vse64 repeated manifest | `test-rvv/sample_consensus/sac_model_line/doc/phases/050-line-get-distances-vse64-store/production-repeated-evidence-manifest.json` | summary evidence，可复核 Phase 050 public count/select/getDistances 5-run board 数据。 |
| production vfwcvt/vse64 repeated doctor | `test-rvv/sample_consensus/sac_model_line/doc/phases/050-line-get-distances-vse64-store/production-repeated-evidence-doctor.md` | summary evidence，Errors / Warnings / Suggestions 均为 0。 |
| production vfwcvt/vse64 repeated doctor JSON | `test-rvv/sample_consensus/sac_model_line/doc/phases/050-line-get-distances-vse64-store/production-repeated-evidence-doctor.json` | Evidence Doctor 的机器可读结果。 |
| production select vfwcvt/vse64 phase result | `doc/phases/060-line-select-vse64-compressed-store/result.zh.md` | 当前 production truth；记录 `selectWithinDistanceRVV` compressed double store 的接入后数据。 |
| production select vfwcvt/vse64 repeated manifest | `test-rvv/sample_consensus/sac_model_line/doc/phases/060-line-select-vse64-compressed-store/production-repeated-evidence-manifest.json` | summary evidence，可复核 Phase 060 public count/select/getDistances 5-run board 数据。 |
| production select vfwcvt/vse64 repeated doctor | `test-rvv/sample_consensus/sac_model_line/doc/phases/060-line-select-vse64-compressed-store/production-repeated-evidence-doctor.md` | summary evidence，Errors / Warnings / Suggestions 均为 0。 |
| production select vfwcvt/vse64 repeated doctor JSON | `test-rvv/sample_consensus/sac_model_line/doc/phases/060-line-select-vse64-compressed-store/production-repeated-evidence-doctor.json` | Evidence Doctor 的机器可读结果。 |
| identity strided load phase result | `doc/phases/070-line-identity-index-strided-load/result.zh.md` | 记录 identity-index strided load 拒绝证据和 production 回退状态。 |
| identity repeated manifest | `test-rvv/sample_consensus/sac_model_line/doc/phases/070-line-identity-index-strided-load/identity-repeated-evidence-manifest.json` | summary evidence，可复核 identity 输入 strict RVV-vs-RVV A/B。 |
| identity repeated doctor | `test-rvv/sample_consensus/sac_model_line/doc/phases/070-line-identity-index-strided-load/identity-repeated-evidence-doctor.md` | summary evidence，Errors=1 / Warnings=1 / Suggestions=2。 |
| identity shuffled repeated manifest | `test-rvv/sample_consensus/sac_model_line/doc/phases/070-line-identity-index-strided-load/identity-shuffled-repeated-evidence-manifest.json` | summary evidence，可复核 shuffled 控制组 strict RVV-vs-RVV A/B。 |
| identity shuffled repeated doctor | `test-rvv/sample_consensus/sac_model_line/doc/phases/070-line-identity-index-strided-load/identity-shuffled-repeated-evidence-doctor.md` | summary evidence，Errors=3 / Warnings=0 / Suggestions=2。 |
| evaluation / role docs | `doc/*.zh.md` | topic-local 文档，review 后可提交。 |
| evidence registry | `log/evidence_registry.json` | ignored local metadata（被忽略的本地登记信息）；如需提交 evidence summary，需用 `git add -f` 精确选择。 |

## 默认不提交

`build/`、`build/asm/`、`log/board/` raw logs（原始日志）、`log/qemu/` raw logs、远端 `/root/...` 路径、本机 `config.mk` 和私有 board 地址默认不提交。若用户明确要求提交 evidence logs（证据日志），先运行脱敏检查并单独审查。

## production_topic_doc 适用性

`doc-rvv/sample_consensus/sac_model_line-RVV.zh.md` 当前适用：Phase 040 已有 PI5 production direct（真实生产路径证据），Phase 050 完成 `getDistancesToModelRVV` direct double store 的接入后复测，Phase 060 完成 `selectWithinDistanceRVV` compressed double store 的接入后复测，并按本轮采纳条件刷新正式长期文档。文档中的当前性能数据采用 `060-line-select-vse64-compressed-store` 的接入后板卡测试数据，而不是 Phase 000/010/030 的诊断数据、Phase 040 的旧 getDistances 写回形态数据、Phase 050 的旧 select 写回形态数据或 Phase 070 被拒绝的 identity-strided candidate 数据。
