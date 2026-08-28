# sac_model_plane 函数级评估

## 当前结论

`SampleConsensusModelPlane<PointT>` 的三个基础平面距离入口已经有 production RVV（生产 RVV）
实现。Phase 000 采纳 indexed gather（按索引离散加载）+ shared distance kernel（共享距离内核）
覆盖三入口；Phase 010 进一步只为 `selectWithinDistanceRVV` 和 `countWithinDistanceRVV`
采纳 identity-index strided load（恒等索引跨步加载）。`getDistancesToModelRVV` 保持 gather-only。

## 入口语义

| public entry | 标量语义 | 输出 |
| --- | --- | --- |
| `selectWithinDistance` | 对 `indices_` 中每个点计算 `abs(a*x+b*y+c*z+d)`，小于阈值时保序写入 inliers 和 `error_sqr_dists_`。 | `Indices inliers`、`error_sqr_dists_`。 |
| `countWithinDistance` | 对同一距离谓词计数。 | `std::size_t` count。 |
| `getDistancesToModel` | 对 `indices_` 中每个点写出距离。 | `std::vector<double> distances`。 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `selectWithinDistance` / `countWithinDistance` / `getDistancesToModel` | production public entry | 公开入口，执行 RVV dispatch（分流逻辑）或 Standard fallback（回退路径）。 | PCL SAC callers | Standard / RVV helper | production boundary | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_plane.hpp` |
| `selectWithinDistanceRVV` / `countWithinDistanceRVV` | production helper | Phase 000 gather，Phase 010 identity chunk 下 strided load。 | public entry | `sacModelPlaneRVVLoadXYZ` | adopted RVV path | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_plane.hpp` |
| `getDistancesToModelRVV` | production helper | gather-only distance store。 | public entry | `sacModelPlaneRVVLoadXYZ(..., false, ...)` | adopted Phase 000 path | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_plane.hpp` |
| `src/test_sac_model_plane.cpp` | test | 公开入口 / helper correctness。 | `run_test_compare` | gtest assertions | correctness gate | `test-rvv/sample_consensus/sac_model_plane/src/test_sac_model_plane.cpp` |
| `src/bench_sac_model_plane.cpp` | bench | public Std/RVV timing，支持 identity/shuffled。 | board targets | summary / manifest | board performance | `test-rvv/sample_consensus/sac_model_plane/src/bench_sac_model_plane.cpp` |
| `script/generate_board_evidence_manifest.py` | evidence wrapper | 生成 JSON manifest（证据清单）。 | manifest targets | Evidence Doctor | evidence metadata | `test-rvv/sample_consensus/sac_model_plane/script/generate_board_evidence_manifest.py` |
| `doc/phases/010-identity-index-strided-load/result.zh.md` | phase result | Phase 010 计划回填、证据解释和决策。 | workflow recovery | topic docs | EvidenceDecision | `test-rvv/sample_consensus/sac_model_plane/doc/phases/010-identity-index-strided-load/result.zh.md` |

## EvidenceDecision

| evidence layer | 状态 | 路径 / 命令 | 结论 |
| --- | --- | --- | --- |
| correctness | pass | `make -C test-rvv/sample_consensus/sac_model_plane run_test_compare` | Std/RVV 两个构建各 7 个 gtest 通过，覆盖 `PointXYZ`、`PointXYZI`、`PointXYZRGB/RGBA`、`PointXYZINormal`、identity indices、显式空 `indices_` 和 select buffer 合同。 |
| board correctness | pass with environment warning | `run_board_base_plane_public_tests` | 板卡 RVV gtest 7/7 通过；Makefile clock skew 是环境 warning。 |
| asm attribution | pass | `make -B -C test-rvv/sample_consensus/sac_model_plane dump_bench_rvv` | select/count 有 identity 分支与 `vlsseg3e32`；getDistances 仅 gather。 |
| board performance | pass / adopted narrow | Phase 010 identity/shuffled repeated summaries | select/count identity 分别为 3.3896x / 2.1969x，shuffled 分别为 3.1895x / 1.6663x。 |
| Evidence Doctor | pass with handled warning | `doc/phases/010-identity-index-strided-load/evidence-doctor.md` | adopted select/count 无 Error/Warning；getDistances long-tail Warning 用于拒绝该入口 identity 分支。 |
| production decision | adopted_narrow_select_count | `doc-rvv/sample_consensus/sac_model_plane-RVV.zh.md` | 保留 Phase 000 三入口 RVV；Phase 010 只新增 select/count identity fast path。 |

## 文档归属审计

| role | 状态 | 路径 |
| --- | --- | --- |
| topic_navigation | standalone | `test-rvv/sample_consensus/sac_model_plane/README.zh.md` |
| testing_overview | standalone | `doc/testing-overview.zh.md` |
| correctness_tests | standalone | `doc/correctness-tests.zh.md` |
| benchmark_and_evidence | standalone | `doc/benchmark-and-evidence.zh.md` |
| optimization_evidence | standalone | `doc/optimization-evidence.zh.md` |
| optimization_roadmap | standalone | `doc/optimization-roadmap.zh.md` |
| test_support_code_map | standalone | `doc/test-support-code-map.zh.md` |
| phase_index | standalone | `doc/phases/README.zh.md` |
| evaluation_production | standalone | `doc/sac_model_plane-evaluation.zh.md` |
| production_topic_doc | standalone | `doc-rvv/sample_consensus/sac_model_plane-RVV.zh.md` |

## 后续队列

显式空 `indices_` correctness 缺口已由 Phase 025 关闭。当前 topic 的 RVV performance candidate 已无未阻塞默认项。`030-evidence-registry-hardening`
仍可在准备提交或归档证据时执行，但它不是新的性能优化方式，不改变 Phase 010 的采纳判断。
