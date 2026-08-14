# Phase 020 结果：production-shaped gather 诊断

> 当前状态提示：本文是 Phase 020 的历史阶段结果。Phase 030 已继续执行 production direct probe（真实生产入口探针），并因板卡 repeated benchmark（重复性能测试）负向回滚生产补丁。当前 EvidenceDecision 以 `030-pi1-production-integration-plan/result.zh.md`、evaluation、optimization matrix 和 current Handoff 为准：`rollback/no-production`；当前 correctness 为 8 tests passed。

## 实际执行范围

本阶段在 topic-local test support（测试支撑）中加入 production-shaped gather diagnostic（生产形态 gather 诊断），用于评估真实 correspondence index（对应关系索引）读 `PointXYZ`、计算 source / target squared distance（平方距离）并暂存到连续数组后，RVV edge formula（边长公式）是否仍有目标硬件收益。

production 源码未修改：

- `registration/include/pcl/registration/impl/correspondence_rejection_poly.hpp`
- `registration/include/pcl/registration/correspondence_rejection_poly.h`

实际触碰范围仍限于：

- `test-rvv/registration/correspondence_rejection_poly/**`
- `doc-rvv/registration/correspondence_rejection_poly-RVV.zh.md`
- `tmp/rvv-work-logs/registration/correspondence_rejection_poly/**`

## 计划动作回填

| action | 状态 | 证据路径 | 结论 |
| --- | --- | --- | --- |
| B1 test support helper | done | `include/impl/correspondence_rejection_poly_candidates.hpp` | 新增 `EdgePair`、`make_edge_pairs`、`edge_similarity_gather_reference`、`edge_similarity_gather_candidate`。 |
| B2 correctness test | done | `src/test_correspondence_rejection_poly.cpp`、`log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` | Phase 020 时 Std / RVV QEMU 各 7 tests passed；同路径当前已在 Phase 030 刷新为 8 tests passed。 |
| B3 bench case | done | `src/bench_correspondence_rejection_poly.cpp` | 新增 `edge-gather-staging` case，计时包含 gather staging、RVV formula 和 checksum。 |
| B4 manifest / board parser | done | `script/generate_crpoly_evidence_manifest.py`、`script/summarize_crpoly_board_repeated.py` | 新 case label 可进入 QEMU / board manifest。 |
| B5 Makefile targets | done | `Makefile` | 新增 QEMU smoke、doctor 和 board repeated target。 |
| B6 QEMU / asm | done | `log/qemu/edge_gather_staging/evidence_doctor.md`、`build/asm/riscv/bench_correspondence_rejection_poly_rvv.asm` | QEMU doctor Errors=0；asm 中 RVV 指令存在，归属仍为 bench binary partial。 |
| B7 board repeated | done | `log/board/edge_gather_staging_repeated/summary.md`、`evidence_manifest.json`、`evidence_doctor.md` | decision bucket 为 `weak_positive`；doctor Errors=0、Warnings=0、Suggestions=0。 |
| B8 registry / docs closeout | done | `log/evidence_registry.json`、本文件、evaluation、roadmap、matrix、Handoff | Phase 020 当时 EvidenceDecision 为 `diagnostic/no-production`，下一步是 PI1；该结论已被 Phase 030 production-direct negative 覆盖。 |

## Production-shaped gather 结果

| case | speedup_values | median | degradation_frequency | decision_bucket | Evidence Doctor | 处理 |
| --- | --- | --- | --- | --- | --- | --- |
| `edge-gather-staging candidate 64K` | 1.104x, 1.116x, 1.100x, 1.099x, 1.095x | 1.100x | 0/5 | `weak_positive` | Errors=0, Warnings=0, Suggestions=0 | 真实 index gather + squared-distance staging 后仍保持弱正向。 |
| `edge-gather-staging candidate 256K` | 1.124x, 1.119x, 1.109x, 1.111x, 1.132x | 1.119x | 0/5 | `weak_positive` | Errors=0, Warnings=0, Suggestions=0 | 比 Phase 010 预构造距离数组更稳定，但仍是 diagnostic wrapper。 |

`speedup = std_ms / rvv_ms`，大于 1 表示 RVV build 更快。当前 case 的计时边界为：

1. 由 edge pair 读取 `pcl::Correspondences` 的 `index_query` / `index_match`。
2. 从 `PointXYZ` source / target 点云读两端点。
3. 标量计算 source / target squared distance 并写入连续 staging buffer（暂存数组）。
4. 调用 `edge_similarity_batch_candidate` 用 RVV 完成 `min/max >= threshold` 公式段。
5. 对输出 mask 做 checksum。

该边界比 Phase 010 的 `edge_length_batch` 更接近 `thresholdEdgeLength` 的真实数据流，但仍不包含 random sampling（随机采样）、完整 `thresholdPolygon` 多边形控制流、acceptance rate、histogram / Otsu 和 `remaining_correspondences` 输出构造。

## Evidence Doctor 结果

| report | Errors | Warnings | Suggestions | 处理 |
| --- | --- | --- | --- | --- |
| `log/qemu/edge_gather_staging/evidence_doctor.md` | 0 | 0 | 0 | 作为 QEMU log-shape / manifest 合同证据，不用于性能结论。 |
| `log/board/edge_gather_staging_repeated/evidence_doctor.md` | 0 | 0 | 0 | 作为 production-shaped board diagnostic 证据。 |

Phase 010 的 acceptance confirmation 仍保留为负向 / 中性证据：`log/board/acceptance_filter_confirm/evidence_doctor.md` 有 Warnings=1、Suggestions=1，因此 `accept_rate_filter` 不进入 production 候选。

## Evidence registry 状态

`log/evidence_registry.json` 已登记本阶段新增或刷新的证据：

- `log/qemu/run_test_std.log`
- `log/qemu/run_test_rvv.log`
- `log/board/test_smoke/run_test.log`
- `log/qemu/analyze_bench_compare_edge_gather_staging.log`
- `log/qemu/edge_gather_staging/evidence_manifest.json`
- `log/qemu/edge_gather_staging/evidence_doctor.md`
- `log/board/edge_gather_staging_repeated/summary.md`
- `log/board/edge_gather_staging_repeated/evidence_manifest.json`
- `log/board/edge_gather_staging_repeated/evidence_doctor.md`
- `build/asm/riscv/bench_correspondence_rejection_poly_rvv.asm`

raw board run logs 位于 `log/board/edge_gather_staging_repeated/run-01` 到 `run-05`，默认本机保留，不进入提交边界。

## EvidenceDecision

Phase 020 EvidenceDecision：`diagnostic/no-production`，但 `edge_gather_staging` 当时具备 `production-shaped-diagnostic/continue` 信号。Phase 030 已复核 production direct 并改写当前结论为 `rollback/no-production`。

理由：

- correctness：Phase 020 时 QEMU 和 board gtest 均通过 7 tests，新增 gather staging candidate 与 reference 输出一致；当前 correctness 已刷新为 8 tests。
- performance：目标硬件 5-run repeated board 显示 64K median 1.100x、256K median 1.119x，degradation 0/5，Evidence Doctor 无发现。
- boundary：candidate 仍在 test support helper 内，未接 production dispatch；没有 production direct test、fallback matrix、generic point type traits、`Scalar=double` 或完整 public entry RVV bench。
- risk：完整 `getRemainingCorrespondences` 仍包含随机采样、sample counter 更新、acceptance rate、histogram / Otsu 和输出 append；当前局部正向不能外推为端到端 production 收益。

Phase 020 的生产接入判断：不修改 production。该阶段建议的 PI1 已在 Phase 030 执行并因板卡负向回滚。

## 继续 / 停止决策

Phase 020 完成。当前 topic-local 诊断阶段没有必须继续执行的未阻塞测试资产；继续需要进入 production integration loop 的 PI1 计划或扩展新的 component ablation（组件消融）。因此本轮停在授权边界：

- 默认下一动作：历史记录为等待用户确认是否进入 PI1 production integration plan；当前已被 Phase 030 关闭。
- 不推荐动作：直接修改 `correspondence_rejection_poly.hpp`。
- 可另开或后续补充动作：accept-rate scalar-tail attribution、full-entry production-shaped end-to-end bench、generic point type / `Scalar=double` 策略评估。
