# plane_models normal-plane RVV 主题入口

本文是 `test-rvv/sample_consensus/plane_models` 的 topic navigation（主题导航，说明先读哪里和如何恢复）。当前 topic 只收敛 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_plane.hpp` 中已有 normal-plane RVV 生产补丁；同目录历史测试也覆盖 plane 和 normal-parallel-plane 回归。

## 当前结论

`SampleConsensusModelNormalPlane<PointT, PointNT>` 的 `selectWithinDistance`、`countWithinDistance` 和 `getDistancesToModel` 已有 RVV 生产 helper。phase 000 证明 `PointXYZ + Normal` float layout、`indices_` 顺序索引、`Scalar=float` 计算路径下，公开入口 dispatch（分流逻辑）和 fallback（回退路径）可用，protected helper hot path（受保护 helper 热点路径）在板卡上为 positive bucket（正向决策桶）。phase 040 已把公开入口 gate 收紧为 source AoS byte-offset layout，并补 `PointXYZI`、`PointXYZINormal` 代表点型 correctness（正确性）和 non-AoS source fallback 证据。phase 050 继续补了 `PointXYZI + Normal` 与 `PointXYZINormal + Normal` 的 5-run representative source helper performance（代表性 source helper 性能）证据。phase 060 已补 `PointXYZ + PointNormal`、`PointXYZ + PointXYZINormal` normal cloud public-vs-direct RVV correctness，并证明 registered float normal/curvature 但非 standard-layout 的 normal 点型会回退 Standard helper。phase 070 已补 `PointXYZI` / `PointXYZINormal` source 与 `PointNormal` / `PointXYZINormal` normal cloud 的 4 个代表性交叉组合 public-vs-direct RVV correctness。

当前 EvidenceDecision（证据决策）是：

```text
production patch retained / phase 030 repeated board positive-stable / phase 040 representative AoS source correctness closed / phase 050 representative source performance positive-stable / phase 060 representative normal layout correctness closed / phase 070 representative cross correctness closed
```

该结论不外推到泛型点类型全集、其它 normal-like layout、非法索引、`Scalar=double` 或新的 RVV 实现族选择。新增 source 点型的性能只覆盖 protected helper hot path；公开入口 dispatch 与 fallback 仍由 correctness 测试证明。

## 先读哪份文档

| 顺序 | 文档 | 作用 |
| ---: | --- | --- |
| 1 | `doc/sac_model_normal_plane-evaluation.zh.md` | 函数级评估、Traceability Map（可追踪性地图）和生产接入判断主入口。 |
| 2 | `doc/phases/README.zh.md` | phase loop（阶段循环）恢复入口和默认下一 phase。 |
| 3 | `doc/phases/000-normal-plane-current-state-and-public-entry-boundary/result.zh.md` | 公开入口、fallback、QEMU、asm、board 和 Evidence Doctor 结论。 |
| 4 | `doc/phases/010-normal-plane-test-support-structure/result.zh.md` | 本阶段 source layout、board fixture 参数和 doc-suite role 文档收敛结果。 |
| 5 | `doc/phases/020-normal-plane-evidence-registry-target-alias/result.zh.md` | topic-local manifest wrapper、Evidence Doctor / registry alias 和 freshness check 收敛结果。 |
| 6 | `doc/phases/030-normal-plane-repeated-board-summary/result.zh.md` | repeated board summary 阶段结果；5-run 板卡摘要和 Evidence Doctor 已关闭。 |
| 7 | `doc/phases/040-normal-plane-aospoint-gate-expansion/result.zh.md` | source AoS gate、代表点型 public correctness 和 non-AoS fallback 结果。 |
| 8 | `doc/phases/050-normal-plane-representative-aos-source-performance/result.zh.md` | `PointXYZI` / `PointXYZINormal` 代表性 source helper performance 结果。 |
| 9 | `doc/phases/060-normal-plane-normal-layout-expansion/result.zh.md` | 代表性 normal layout public correctness 和 fallback 结果。 |
| 10 | `doc/phases/070-normal-plane-cross-point-type-layout/result.zh.md` | 代表性 source × normal 交叉组合 public correctness 结果。 |
| 11 | `doc/testing-overview.zh.md` | Make target、QEMU、board、Evidence Doctor 和 target 粒度审计。 |
| 12 | `doc/benchmark-and-evidence.zh.md` | bench case、board summary、manifest、doctor、registry 和提交边界。 |
| 13 | `doc/optimization-roadmap.zh.md`、`doc/phases/optimization-matrix.zh.md` | 下一阶段候选、恢复条件和矩阵状态。 |

## 目录分工

| role | 主归属 | 说明 |
| --- | --- | --- |
| topic navigation | `README.zh.md` | 只放入口、常用命令、证据白名单和 production topic doc 适用性。 |
| testing overview | `doc/testing-overview.zh.md` | 测试入口分类和 target 粒度审计。 |
| correctness tests | `doc/correctness-tests.zh.md` | GTest 用例字典和 correctness（正确性）边界。 |
| benchmark and evidence | `doc/benchmark-and-evidence.zh.md` | bench（性能测试）、board、manifest、Evidence Doctor 和日志提交边界。 |
| optimization evidence | `doc/optimization-evidence.zh.md` | 已采用、暂缓和未覆盖优化方式的证据索引。 |
| test support code map | `doc/test-support-code-map.zh.md` | C++ source、Make target、bench wrapper 和 production helper 的定位关系。 |
| phase suite | `doc/phases/` | phase plan/result、optimization matrix 和恢复入口。 |
| production long-term doc | `doc-rvv/sample_consensus/selectWithinDistance_getDistancesToModel_RVV.zh.md` | 该 topic 已有 production patch，因此长期生产行为说明适用；测试工程细节仍归属 topic-local docs。 |

## 常用命令

```bash
make -C test-rvv/sample_consensus/plane_models run_normal_plane_public_tests
make -C test-rvv/sample_consensus/plane_models run_test_compare
make -C test-rvv/sample_consensus/plane_models dump_bench_rvv
make -C test-rvv/sample_consensus/plane_models run_board_test fetch_board_logs
make -C test-rvv/sample_consensus/plane_models run_board_bench_compare fetch_board_logs
make -C test-rvv/sample_consensus/plane_models run_board_evidence_doctor
make -C test-rvv/sample_consensus/plane_models evidence_status
make -C test-rvv/sample_consensus/plane_models run_board_bench_compare_repeated
make -C test-rvv/sample_consensus/plane_models record_repeated_board_evidence_state
make -C test-rvv/sample_consensus/plane_models repeated_evidence_status
make -C test-rvv/sample_consensus/plane_models run_board_bench_compare_phase050
make -C test-rvv/sample_consensus/plane_models record_phase050_evidence_state
make -C test-rvv/sample_consensus/plane_models phase050_evidence_status
```

`run_normal_plane_public_tests` 只跑 phase 000 / 040 / 060 / 070 新增的公开入口、fallback 和 helper buffer contract（缓冲区合同）测试，共 13 个 GTest。`run_test_compare` 跑完整 Std/RVV QEMU correctness；当前 Std/RVV 各 32 个 GTest。QEMU 不提供真实性能结论。板卡性能引用 `run_board_bench_compare fetch_board_logs` 的单次 board summary（板卡摘要），phase 030 的 `normal-plane-phase030-repeated-board` 5-run repeated summary（重复板卡摘要），以及 phase 050 的代表性 AoS source 点型 repeated summary。

## 当前可提交证据

| 证据 | 路径 | role |
| --- | --- | --- |
| phase 000 manifest | `doc/phases/000-normal-plane-current-state-and-public-entry-boundary/evidence-manifest.json` | Evidence Doctor 输入。 |
| phase 000 Evidence Doctor | `doc/phases/000-normal-plane-current-state-and-public-entry-boundary/evidence-doctor.md`、`.json` | 摘要证据；Errors=0、Warnings=0、Suggestions=0。 |
| phase 000 result | `doc/phases/000-normal-plane-current-state-and-public-entry-boundary/result.zh.md` | 公开入口与性能证据主归属。 |
| phase 010 result | `doc/phases/010-normal-plane-test-support-structure/result.zh.md` | source layout、board fixture 和 doc-suite closeout 主归属。 |
| evidence registry | `log/evidence_registry.json` | 证据登记表；`evidence_status` 当前为 fresh。 |
| phase 030 repeated summary | `log/board/normal-plane-phase030-repeated-board/summary.md` | 5-run board summary；`selectWithinDistance` 9.64x median、`countWithinDistance` 12.72x median、`getDistancesToModel` 12.02x median。 |
| phase 030 Evidence Doctor | `log/board/normal-plane-phase030-repeated-board/evidence-doctor.md`、`.json` | 摘要证据；Errors=0、Warnings=0、Suggestions=0。 |
| phase 040 result | `doc/phases/040-normal-plane-aospoint-gate-expansion/result.zh.md` | 代表性 AoS source dispatch correctness 和 non-AoS fallback closeout；不新增性能 summary。 |
| phase 050 repeated summary | `log/board/normal-plane-phase050-representative-aos-source-performance/summary.md` | 5-run representative source board summary；`PointXYZI` 三项 median 为 8.04x / 6.10x / 5.41x，`PointXYZINormal` 三项 median 为 7.37x / 8.52x / 8.44x。 |
| phase 050 Evidence Doctor | `log/board/normal-plane-phase050-representative-aos-source-performance/evidence-doctor.md`、`.json` | 摘要证据；Errors=0、Warnings=5、Suggestions=0，Warning 已在 phase result 中解释。 |
| phase 050 result | `doc/phases/050-normal-plane-representative-aos-source-performance/result.zh.md` | 代表性 AoS source helper performance closeout。 |
| phase 060 result | `doc/phases/060-normal-plane-normal-layout-expansion/result.zh.md` | 代表性 normal layout correctness / fallback closeout。 |
| phase 070 result | `doc/phases/070-normal-plane-cross-point-type-layout/result.zh.md` | 代表性 source × normal 交叉组合 correctness closeout；不新增性能 summary。 |

默认不提交 `build/`、`output/`、`log/board/*.log`、`log/qemu/*.log`、本机 `config.mk`、私有板卡地址或 raw logs（原始日志）。若用户要求提交日志，先运行 topic 提供或共享的 sanitize/check 流程，并单独审查日志 commit。

## production_topic_doc 适用性

`doc-rvv/sample_consensus/selectWithinDistance_getDistancesToModel_RVV.zh.md` 适用，因为当前 topic 已有 retained production patch（保留的生产补丁）。该文档只维护当前生产实现和证据链摘要；phase 计划、测试用例字典、target 字典和 registry 缺口由 topic-local docs 维护。

## 默认恢复动作

phase 070 已关闭。默认恢复动作是复核 public dispatch / fallback、完整 QEMU correctness、板卡 public alias 和 evidence freshness（证据新鲜度），然后判断是否需要扩大到更多 PCL normal-like 点型、公开入口性能探针或 `Scalar=double`：

```text
make -C test-rvv/sample_consensus/plane_models run_normal_plane_public_tests
make -C test-rvv/sample_consensus/plane_models run_test_compare
SSH_AUTH_SOCK=<injected> make -C test-rvv/sample_consensus/plane_models run_board_normal_plane_public_tests
make -C test-rvv/sample_consensus/plane_models evidence_status
make -C test-rvv/sample_consensus/plane_models repeated_evidence_status
make -C test-rvv/sample_consensus/plane_models phase050_evidence_status
```

该恢复不需要重新设计 normal-plane RVV helper，也不默认扩大到完整泛型点类型。若继续扩展，应先明确新的 scope：更多 PCL normal-like 点型 correctness、公开入口 repeated performance probe（重复性能探针），或 `Scalar=double` helper family。
