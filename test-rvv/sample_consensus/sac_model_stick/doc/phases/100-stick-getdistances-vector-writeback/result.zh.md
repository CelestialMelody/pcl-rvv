# Phase 100: stick getDistances vector writeback result

## 当前状态

本文件记录 `getDistancesToModelRVV` 的 vector writeback（向量写回）实现族结果。本阶段已完成源码替换、QEMU correctness（QEMU 正确性）、production asm gate（生产反汇编验收）、5-run board repeated（板卡重复采集）、Evidence Doctor（证据体检）和 registry（证据登记）。

结论：新实现可保留为当前 production behavior（生产行为）。接入后的 public `getDistancesToModel` 板卡结果为 positive-stable（稳定正向），speedup min / median / max 为 3.4122x / 3.6879x / 3.7150x，Evidence Doctor 为 Errors=0、Warnings=0、Suggestions=0。和 Phase 080 staged scalar lane（标量向量通道）写回的 historical baseline（历史基线）2.5722x / 2.5883x / 2.7872x 相比，本阶段是强正向信号；但这不是同一轮旧/新 RVV binary 的严格 A/B，因此文档只把它写成历史对照，不把差值写成严格 family speedup。

## 动作回填

| action | 状态 | 命令 / 产物 | 结论 |
| --- | --- | --- | --- |
| tighten asm gate first | done | `make -C test-rvv/sample_consensus/sac_model_stick check_production_asm` | 旧 staged scalar lane（标量向量通道）写回形态下失败，能抓到 `staged_sqr` / `staged_dist` 和 lane loop。 |
| implement vector writeback | done | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_stick.hpp` | 已改为 `vmflt` mask、`vmerge` penalty 选择、`vfwcvt` 扩宽和 `vse64.v` 写回。 |
| focused correctness | done | `make -C test-rvv/sample_consensus/sac_model_stick run_stick_getdistances_tests` | RVV build getDistances 3/3 通过。 |
| aggregate correctness | done | `make -C test-rvv/sample_consensus/sac_model_stick run_test_compare` | Std/RVV aggregate 11/11 + 11/11 通过。 |
| production asm | done | `make -C test-rvv/sample_consensus/sac_model_stick clean_bench_rvv && make -C test-rvv/sample_consensus/sac_model_stick check_production_asm` | 三个 helper 均命中 RVV；`getDistancesToModelRVV` 同时通过源码形态和 `vfwcvt.f.f.v` / `vse64.v` gate。 |
| board repeated | done | `make -C test-rvv/sample_consensus/sac_model_stick collect_vector_writeback_board_evidence` | 5-run 全部完成；每轮板卡 gtest 11/11 通过。 |
| Evidence Doctor / registry | done | `make -C test-rvv/sample_consensus/sac_model_stick record_vector_writeback_board_evidence_state` | 生成 Phase 100 manifest / doctor，并登记 `log/evidence_registry.json`。 |

## Correctness 和 asm 结果

QEMU correctness（QEMU 正确性）只证明输出语义和路径可运行，不证明性能。当前本地结果已证明 `vmerge` 选择顺序保持 stick 的 `radius_max_` penalty 语义：`sqr < radius_max_^2` 写 `dist`，否则写 `2 * dist`。

## Board evidence

输入为 65536 个 `PointXYZ`，`indices_` 使用相邻交换打乱，warmup 5 次，计时 200 次。计时边界只包含 public `getDistancesToModel` 入口调用，包含 dense distance（稠密距离）输出写回，不包含点云、indices 和系数构造。

| run | Std ms/iter | RVV ms/iter | speedup |
| --- | ---: | ---: | ---: |
| 01 | 1.987697 | 0.582528 | 3.4122x |
| 02 | 2.018637 | 0.587260 | 3.4374x |
| 03 | 2.151293 | 0.581583 | 3.6990x |
| 04 | 2.152424 | 0.583645 | 3.6879x |
| 05 | 2.154625 | 0.579977 | 3.7150x |

summary：Std average 2.092935 ms/iter，RVV average 0.582999 ms/iter；speedup min / median / max 为 3.4122x / 3.6879x / 3.7150x。Std/RVV checksum 都是 `16723220023521`。`getDistancesToModelRVV` asm symbol 的 RVV instruction count（RVV 指令计数）为 28。

板卡输出中出现 `Clock skew detected`，来自远端 `script/rvv-board-run.mk` 文件时间，未改变 bench log 的 dataset、iteration、checksum 或 Std/RVV 计时字段；Evidence Doctor 没有把它识别为当前 manifest 范围内的错误或警告。本结果仍保留该环境风险说明。

## Evidence Doctor 和 registry

| evidence | 路径 / 状态 |
| --- | --- |
| manifest | `doc/phases/100-stick-getdistances-vector-writeback/vector-writeback-evidence-manifest.json` |
| Evidence Doctor Markdown | `doc/phases/100-stick-getdistances-vector-writeback/vector-writeback-evidence-doctor.md`，Errors=0、Warnings=0、Suggestions=0 |
| Evidence Doctor JSON | `doc/phases/100-stick-getdistances-vector-writeback/vector-writeback-evidence-doctor.json` |
| registry | `log/evidence_registry.json` 已登记 run label `stick-phase100-vector-writeback-board` |

## Diagnostic 到 production 回填审计

| question | answer |
| --- | --- |
| evidence role | production-detail implementation-shape；板卡运行后同时作为 production-public getDistances evidence。 |
| A/B boundary | production `getDistancesToModelRVV` helper 和 public `getDistancesToModel` bench 行。 |
| 当前决策问题 | RVV-family-selection：向量写回形态是否值得替代旧 staged scalar lane 写回形态。 |
| diagnostic 是否可外推到 production | 不需要外推；本阶段直接修改 production helper。 |
| comparison-boundary / baseline mismatch 风险 | 若只看 Std/RVV public speedup，仍不能严格证明新 RVV family 优于旧 RVV family；本文把 Phase 080 数字作为 historical baseline，不写成同轮 A/B。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前 patch 已是有界 production detail probe；若板卡为负或不稳定，应等待用户决定保留或回滚。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 若要声称严格“新实现族比旧实现族更快”，需要同边界旧/新 RVV A/B；若依据本阶段 public Std/RVV 正向和历史对照，可以说明新 patch 值得保留接入。 |

## Optimization matrix 更新

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| getDistances-vector-penalty-writeback | direct indexed `indices_` | `PointXYZ` board performance；traits-gated xyz AoS production path；`Eigen::VectorXf` | production `getDistancesToModelRVV` | done: focused RVV 3/3；Std/RVV aggregate 11/11 + 11/11 | done: public `getDistancesToModel` board repeated | done: min / median / max 3.4122x / 3.6879x / 3.7150x | done: source shape has RVV mask / merge / `vfwcvt` / `vse64`; asm has `vfwcvt.f.f.v` / `vse64.v` | done: 0 / 0 / 0 | production-adopted for current implementation shape | no same-scope implementation-shape action remains; strict old/new RVV A/B is optional only if reviewer requires family delta |

## Continue / stop decision

本阶段已完成。当前 topic 内仍存在 Phase 090 留下的 dedicated point-type board performance（专门点型板卡性能）扩展范围，但它需要新 bench label 和独立板卡预算，且不影响当前 `PointXYZ` production direct 采纳结论。当前同边界 getDistances 实现形态没有未阻塞的下一动作。
