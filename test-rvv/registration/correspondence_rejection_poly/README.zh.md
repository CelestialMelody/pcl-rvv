# correspondence_rejection_poly RVV 诊断入口

本目录保存 `registration/correspondence_rejection_poly` 的 test-rvv（RVV 测试资产）诊断和生产探针证据。用户已确认不接入 production RVV path（生产 RVV 路径），两个目标 production（生产源码）文件已还原为当前仓库标量实现：

- `registration/include/pcl/registration/impl/correspondence_rejection_poly.hpp`
- `registration/include/pcl/registration/correspondence_rejection_poly.h`

## 当前结论

历史 Phase 030 的 EvidenceDecision（证据决策）是 `rollback/no-production`：它按公开入口分发、`Standard` / `RVV` helper 分层接入受控 production direct probe（真实生产入口探针），QEMU / board correctness（正确性）通过，但板卡 repeated board（重复板卡测试）为 negative，两个 production-direct case 均 5/5 退化。

当前 closeout 是 `rollback/no-production`。Phase 050 为便于用户检查真实接入形态，曾临时恢复 production patch 并重新运行 production-direct smoke、板卡重复性能测试和 Evidence Doctor；用户检查后已明确确认不接入，worker 已还原两个目标 production 文件。回滚前 replay 仍为 negative：2048 median `0.904x`、8192 median `0.977x`，两组都是 5/5 degradation，Evidence Doctor 为 `Errors=2`。

`doc-rvv/registration/correspondence_rejection_poly-RVV.zh.md` 仍不适用，因为没有 adopted production behavior（已采纳生产行为）。当前 no-production 结论主归属在 `doc/correspondence_rejection_poly-evaluation.zh.md`、`doc/phases/050-production-patch-replay-user-validation/result.zh.md` 和 current Handoff；Phase 030 result 只作为历史 run 记录。

## 先读哪份文档

| 读者问题 | 首选入口 | 说明 |
| --- | --- | --- |
| 当前为什么尚未采纳 production（生产源码） | `doc/correspondence_rejection_poly-evaluation.zh.md`、current Handoff | 这里区分历史负向证据、Phase 050 临时补丁 replay、用户确认回滚和当前 clean production。 |
| Phase 030 探针做了什么 | `doc/phases/030-pi1-production-integration-plan/result.zh.md` | 这里记录历史 production direct 探针的 `Standard` / `RVV` 分层、board negative 和 Evidence Doctor 处理。 |
| 测试数据是否只靠随机 | `doc/correctness-tests.zh.md` | 确定性样本和固定种子随机压力样本分别说明。 |
| bench target 能证明什么 | `doc/benchmark-and-evidence.zh.md` | 这里解释 case-filter、计时边界、QEMU / board 边界、registry 和提交白名单。 |
| helper / script 在证据链里的位置 | `doc/test-support-code-map.zh.md` | 这里提供聚合入口、internal helper、src、script、output 和 production 对照。 |
| 继续当前 topic 该从哪里恢复 | `doc/optimization-roadmap.zh.md`、`doc/phases/README.zh.md`、current Handoff | 当前默认不接 production；若继续只能另开 profile / ablation phase 解释退化来源。 |

## 常用命令

| 命令 | 证据角色 | 输出 |
| --- | --- | --- |
| `make -C test-rvv/registration/correspondence_rejection_poly run_test_compare` | QEMU correctness（QEMU 正确性验证） | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| `make -C test-rvv/registration/correspondence_rejection_poly run_board_test_smoke` | board correctness（板卡正确性验证） | `log/board/test_smoke/run_test.log` |
| `make -C test-rvv/registration/correspondence_rejection_poly dump_bench_rvv` | asm attribution（反汇编归因）输入 | `build/asm/riscv/bench_correspondence_rejection_poly_rvv.asm` |
| `make -C test-rvv/registration/correspondence_rejection_poly run_bench_qemu_smoke` | QEMU bench smoke（QEMU 性能测试小型验证） | `log/qemu/analyze_bench_compare_edge_batch.log`、`log/qemu/analyze_bench_compare_edge_gather_staging.log`、`log/qemu/analyze_bench_compare_acceptance.log` |
| `make -C test-rvv/registration/correspondence_rejection_poly run_board_bench_edge_gather_staging_repeated` | board production-shaped diagnostic（板卡生产形态诊断） | `log/board/edge_gather_staging_repeated/summary.md`、`evidence_doctor.md` |
| `make -C test-rvv/registration/correspondence_rejection_poly run_board_bench_production_direct_repeated` | production-direct 验证 | `log/board/production_direct_repeated/summary.md`、`evidence_doctor.md` |

`production-direct` target 可以直接运行。它不会应用或回滚 production patch；证据含义由运行时工作区决定。当前 production patch 已还原，所以直接运行时只测当前标量 public entry；Phase 050 的 production RVV evidence 来自回滚前的临时 patch replay。

### `production-direct` 的边界

`production-direct` bench case 内部调用的是 PCL 生产类公开入口 `rejector.getRemainingCorrespondences(correspondences, remaining)`；它测到的是当前生产源码在这个公开入口上的行为。target 本身不会修改源码、生成补丁或自动让标量路径变成 RVV。

因此复现“接入生产后的真实 production-direct”需要两个条件同时成立：

- 生产 header / impl 中临时存在 public entry dispatch -> RVV helper -> Standard fallback 的生产探针补丁。
- 运行当前 topic 的 `run_bench_production_direct_smoke` 或 `run_board_bench_production_direct_repeated` target。

如果 production patch 不存在，target 仍可运行，但结果只能登记为普通 public-entry timing，不能作为 production RVV 接入证据。

## 文档导航

| 文档 | 作用 |
| --- | --- |
| `doc/correspondence_rejection_poly-evaluation.zh.md` | 函数级评估、Traceability Map（可追踪性地图）、EvidenceDecision 主归属 |
| `doc/testing-overview.zh.md` | 测试矩阵、入口形态和证据边界 |
| `doc/correctness-tests.zh.md` | gtest case（单元测试用例）说明 |
| `doc/benchmark-and-evidence.zh.md` | bench case、QEMU / board 边界、Evidence Doctor 和 registry（证据登记表）说明 |
| `doc/optimization-evidence.zh.md` | candidate family（候选族）取舍和证据索引 |
| `doc/test-support-code-map.zh.md` | `include/`、`include/impl/`、`src/`、script 和 output 关系 |
| `doc/optimization-roadmap.zh.md` | phase loop（阶段循环）恢复队列和后续候选 |
| `doc/phases/030-pi1-production-integration-plan/result.zh.md` | 历史 production probe 结果、rollback 证据和停止条件 |

## 目录分工

| 目录 / 文件 | 主职责 | 提交边界 |
| --- | --- | --- |
| `src/` | gtest correctness 和 bench wrapper（性能测试包装） | topic-local test asset，审查后可纳入 topic commit |
| `include/correspondence_rejection_poly.h` | 测试支撑聚合入口 | topic-local test asset |
| `include/impl/correspondence_rejection_poly_candidates.hpp` | 标量 reference（参考链路）、test-only RVV candidate（测试专用 RVV 候选）和 checksum helper | topic-local test asset；当前不修改 production |
| `script/` | topic-bound manifest（主题绑定证据清单）和 repeated board summary（重复板卡摘要）脚本 | topic-local test asset |
| `doc/` | evaluation、测试说明、bench 证据、优化证据和代码地图 | topic-local documentation，review-required |
| `doc/phases/` | phase plan/result、optimization matrix（优化矩阵）和恢复入口 | phase_docs，review-required |
| `log/board/*/summary.md`、`log/**/evidence_doctor.md` | summary artifact（摘要证据产物） | 被本文档引用时可进入 summary-only 审查；raw logs 仍 local-only |
| `build/`、raw `run_bench_*.log`、board run 子目录 | 可再生成或本机原始证据 | 默认不提交 |

## 测试数据

测试同时包含 deterministic corpus（确定性样本集）和 seeded random stress（固定种子随机压力样本）。确定性样本覆盖明确边界；固定种子随机样本覆盖乱序 correspondence、点云扰动、不同 cardinality 和 threshold。随机样本可复现，不是不可重复 fuzz。

## 当前可提交证据

以下文件已经被 topic-local 文档引用，可作为 summary-only（只提交摘要）审查候选。提交 raw logs 仍需要用户明确授权和脱敏检查。

| 路径 | 证据角色 |
| --- | --- |
| `test-rvv/registration/correspondence_rejection_poly/log/qemu/run_test_std.log` | QEMU Std correctness。 |
| `test-rvv/registration/correspondence_rejection_poly/log/qemu/run_test_rvv.log` | QEMU RVV correctness。 |
| `test-rvv/registration/correspondence_rejection_poly/log/board/test_smoke/run_test.log` | board correctness smoke（板卡正确性小型验证）。 |
| `test-rvv/registration/correspondence_rejection_poly/log/qemu/production_direct/evidence_doctor.md` | 当前 QEMU production-direct smoke 的 Evidence Doctor，Errors=0。 |
| `test-rvv/registration/correspondence_rejection_poly/log/board/production_direct_repeated/summary.md` | 当前 Phase 050 production-direct negative summary（生产直连负向摘要）。 |
| `test-rvv/registration/correspondence_rejection_poly/log/board/production_direct_repeated/evidence_doctor.md` | 当前 production adoption（生产采用）失败的 Evidence Doctor 报告，Errors=2。 |
| `test-rvv/registration/correspondence_rejection_poly/log/evidence_registry.json` | evidence freshness（证据新鲜度）登记表。 |

## doc-rvv 适用性

`doc-rvv/registration/correspondence_rejection_poly-RVV.zh.md` 当前仍判为 `not_applicable`，因为用户已确认不采纳 Phase 050 临时 production patch，且生产文件已还原。Phase 050 production-direct board 证据为 negative；no-production closeout 不创建或更新长期生产文档。
