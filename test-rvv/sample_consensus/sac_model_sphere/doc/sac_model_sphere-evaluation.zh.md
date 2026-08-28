# sac_model_sphere 函数级评估

## S2 函数入口和标量路径

本 topic 覆盖 `SampleConsensusModelSphere<PointT>` 的基础球模型距离入口：

| public entry | 标量职责 | 输出 |
| --- | --- | --- |
| `getDistancesToModel` | 对 `indices_` 中每个点计算到球心的欧氏距离，再减去半径并取绝对值。 | `std::vector<double> distances`，长度等于 `indices_->size()`。 |
| `selectWithinDistance` | 先用平方半径区间做球壳判断；命中后按 `indices_` 顺序写入 inlier，并用 `sqrt(sqr_dist)` 写入误差距离。 | `Indices inliers` 与 `error_sqr_dists_`。 |
| `countWithinDistance` | 使用同一球壳双边界判断计数，不需要计算 sqrt。 | inlier 数量。 |

热点是 direct indexed `indices_` 上的 x/y/z gather（离散加载，按索引读取不连续点字段）、平方距离、双边界 mask（掩码）和 `select` / `getDistances` 中的 `sqrt`。`computeModelCoefficients`、`optimizeModelCoefficients`、`projectPoints`、`doSamplesVerifyModel` 和 SAC 后处理不属于本阶段范围。

## 当前 RVV 状态

| helper / entry | 层级 | 当前状态 |
| --- | --- | --- |
| `countWithinDistanceRVV` | production RVV helper | 已存在，公开 `countWithinDistance` 在 `__RVV10__` 且 `RVVXYZFloatLayout<PointT>` 成立时命中。 |
| `selectWithinDistance` | production public entry | Phase 045/046 已采纳 `vcompress` production RVV patch；公开入口在 RVV gate 成立时分流到 `selectWithinDistanceRVV`。 |
| `getDistancesToModel` | production public entry | 仍是标量循环；本阶段只创建测试专用 RVV 候选，不修改 production dispatch。 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- |
| `countWithinDistance` | production public entry | 公开计数入口，当前可在 RVV 构建下分流到 `countWithinDistanceRVV`。 | SAC model callers | production boundary | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_sphere.hpp` |
| `selectWithinDistance` | production public entry | 公开 inlier 选择入口，当前可在 RVV 构建下分流到 `selectWithinDistanceRVV`。 | SAC model callers | production direct / output contract | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_sphere.hpp` |
| `getDistancesToModel` | production public entry | 公开距离数组入口，当前保持标量。 | SAC model callers | scalar baseline / output contract | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_sphere.hpp` |
| `src/test_sac_model_sphere.cpp` | production direct / diagnostic tests | 保留 gtest case；共享 helper 经 `include/test_sac_model_sphere.h` 引入。 | `run_test_compare` | correctness gate（正确性验收） | `test-rvv/sample_consensus/sac_model_sphere/src/test_sac_model_sphere.cpp` |
| `include/impl/sac_model_sphere_access.hpp` | internal test support | 对拍 public entry、Standard helper、RVV helper，并保留测试专用 candidate。 | test / bench 聚合头 | production direct / diagnostic support | `test-rvv/sample_consensus/sac_model_sphere/include/impl/sac_model_sphere_access.hpp` |
| `include/bench_sac_model_sphere.h` | bench wrapper | 用同一输入分别计时 public entry 和测试专用候选；Phase 020 后 public select 是 production direct bench。 | `src/bench_sac_model_sphere.cpp` | board performance input | `test-rvv/sample_consensus/sac_model_sphere/include/bench_sac_model_sphere.h` |
| `src/bench_sac_model_sphere.cpp` | bench entry | 保留 CLI 解析和点型选择错误返回。 | board `run_bench_compare` | bench executable entry | `test-rvv/sample_consensus/sac_model_sphere/src/bench_sac_model_sphere.cpp` |
| `doc/phases/000-sphere-select-distance-diagnostic/result.zh.md` | phase result | 保存接入前诊断命令、Evidence Doctor 和继续 / 停止判断。 | worker / reviewer | recovery pointer（恢复入口） | `test-rvv/sample_consensus/sac_model_sphere/doc/phases/000-sphere-select-distance-diagnostic/result.zh.md` |
| `doc/phases/020-select-production-integration-plan/result.zh.md` | phase result | 保存 `selectWithinDistance` PI2-PI5 生产接入、板卡复跑和 EvidenceDecision。 | worker / reviewer | production closeout evidence | `test-rvv/sample_consensus/sac_model_sphere/doc/phases/020-select-production-integration-plan/result.zh.md` |

## EvidenceDecision

当前 EvidenceDecision（证据决策）：

- `countWithinDistance`：当前 production RVV 行为已由独立 topic 复核；本轮接入后复跑中 `PointXYZ` board repeated median speedup 为 `3.5154x`。
- `selectWithinDistance`：Phase 045/046 `vcompress` production patch 已通过 production direct（真实生产路径证据）复跑并由用户确认采纳，`PointXYZ` 板卡 repeated median 为 `2.0989x`。Phase 050 证明 `PointXYZI` public entry median 为 `1.5901x`；Phase 060 证明 `PointXYZRGB` median 为 `1.6225x`、`PointXYZRGBA` median 为 `1.5528x`，其中 RGB 5/5 run 正向，RGBA 带 1/5 run 退化和长尾 warning。
- `getDistancesToModel`：当前测试专用 `RVV squared-distance + scalar sqrt/store` 候选为 negative，Phase 000 不支持按该形态进入 production。
- public `getDistancesToModel`：当前 production 仍保持标量，Std/RVV public 对比不能写成 RVV 收益。

`doc-rvv/sample_consensus/sac_model_sphere-RVV.zh.md` 已适用，用于维护 production 长期事实和正确性与高效性证据链。

## 诊断证据链

| 证据层 | 路径 / 命令 | 支撑结论 | 不支撑的结论 |
| --- | --- | --- | --- |
| QEMU correctness | `make -C test-rvv/sample_consensus/sac_model_sphere run_test_compare` | Std/RVV 两侧 6 个测试通过，覆盖 shell 边界、`PointXYZI` / RGB / RGBA layout、测试专用候选、production select helper 和 `vcompress` candidate 输出一致性。 | 不证明真实性能。 |
| 反汇编 | `make -C test-rvv/sample_consensus/sac_model_sphere dump_bench_rvv`，`build/asm/riscv/bench_sac_model_sphere_rvv.full.asm` | `countWithinDistanceRVV` 有符号级 RVV 指令；候选内联区域可见 gather/FMA/store RVV 指令。 | 候选 helper 符号级归属未闭合，manifest 的候选 `rvv_instr_count=0` 不能写成 hot symbol pass。 |
| 单次 board smoke | `SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_sphere board_smoke` | 板卡可达，test 通过，bench 输出形状可解析。 | 单次 run 不作为稳定结论。 |
| 5-run repeated board | `doc/phases/000-sphere-select-distance-diagnostic/repeated-evidence-manifest.json` | count production median `3.5192x`；select candidate median `1.4216x`；getDistances candidate median `0.7781x`。 | 诊断候选不能替代 production direct；Phase 020 后 select 采纳以生产证据链为准。 |
| Evidence Doctor（证据体检） | `doc/phases/000-sphere-select-distance-diagnostic/repeated-evidence-doctor.md` | 暴露 public select/getDistances 接近中性或退化，以及 getDistances candidate 全部退化。 | 带 Error 的条目不能被写成 clean production evidence。 |

## 生产证据链

| 证据层 | 路径 / 命令 | 支撑结论 | 不支撑的结论 |
| --- | --- | --- | --- |
| production direct correctness | `make -C test-rvv/sample_consensus/sac_model_sphere run_test_compare` | Std/RVV 两侧 6 个测试通过，`ProductionSelectWithinDistanceMatchesStandardHelper` 对拍 public entry、Standard helper 和 RVV helper，`PointXYZRGBAndRGBALayoutsMatchReference` 覆盖 RGB/RGBA layout correctness。 | 不证明未测试点型或其它规模的性能。 |
| 反汇编 | `make -C test-rvv/sample_consensus/sac_model_sphere clean_bench_rvv dump_bench_rvv` | `selectWithinDistanceRVV` 符号级 RVV instruction count 为 `27`，并可见 `vcompress.vm`；`countWithinDistanceRVV` 为 `19`。 | 不证明板卡性能，性能仍看 board repeated。 |
| 5-run production board | `doc/phases/045-select-vcompress-production-integration/production-vcompress-repeated-evidence-manifest.json` | `PointXYZ` public `selectWithinDistance` median `2.0989x`，min/max `2.0867x / 2.1404x`；public `countWithinDistance` median `3.5154x`。 | 不支持接入 `getDistancesToModel`。 |
| 5-run point-type board | `doc/phases/050-point-type-expansion/point-type-repeated-evidence-manifest.json` | `PointXYZI` public `selectWithinDistance` median `1.5901x`，min/max `1.4672x / 1.6317x`；public `countWithinDistance` median `1.9977x`。 | 不外推到 RGB/RGBA 或自定义点型。 |
| 5-run RGB/RGBA point-type board | `doc/phases/060-point-type-rgb-rgba-expansion/rgb-point-type-repeated-evidence-manifest.json`、`doc/phases/060-point-type-rgb-rgba-expansion/rgba-point-type-repeated-evidence-manifest.json` | `PointXYZRGB` public `selectWithinDistance` median `1.6225x`，min/max `1.6181x / 1.6331x`；`PointXYZRGBA` median `1.5528x`，min/max `0.9184x / 1.6354x`。 | RGBA 不能写成全 run 稳定；RGB/RGBA 也不外推到自定义点型或其它规模。 |
| Evidence Doctor | `doc/phases/045-select-vcompress-production-integration/production-vcompress-repeated-evidence-doctor.md`、`doc/phases/050-point-type-expansion/point-type-repeated-evidence-doctor.md`、`doc/phases/060-point-type-rgb-rgba-expansion/rgb-point-type-repeated-evidence-doctor.md`、`doc/phases/060-point-type-rgb-rgba-expansion/rgba-point-type-repeated-evidence-doctor.md` | select production rows clean for `PointXYZ` / `PointXYZI` / `PointXYZRGB`；`PointXYZRGBA` select 有退化和长尾 Warnings；Errors 属于未接入的 getDistances 行，已降级处理。 | 不能把 getDistances 写成 adopted，也不能隐藏 RGBA 的 1/5 run 退化。 |

## 文档归属和发布边界

| fact | 主归属 | 发布边界 |
| --- | --- | --- |
| 阶段计划、结果、optimization matrix | `test-rvv/sample_consensus/sac_model_sphere/doc/phases/` | topic-local phase docs，review 后可作为 topic test asset。 |
| 函数级评估、Traceability Map、EvidenceDecision | 本文件 | topic-local evaluation，review 后可作为 topic test asset。 |
| optimization roadmap | `test-rvv/sample_consensus/sac_model_sphere/doc/optimization-roadmap.zh.md` | topic-local roadmap，review 后可作为 topic test asset。 |
| production 长期主题文档 | `doc-rvv/sample_consensus/sac_model_sphere-RVV.zh.md` | Phase 045/046 后记录 `vcompress` adopted production behavior；Phase 050 后记录 `PointXYZI` 点型扩展证据；Phase 060 后记录 RGB/RGBA 点型扩展证据和 warning。 |
| board raw logs | `test-rvv/sample_consensus/sac_model_sphere/log/board/` | 默认 local-only；提交前只保留被文档引用的 summary / sanitized logs。 |

## doc_suite_role_inventory

| role | 状态 | 说明 |
| --- | --- | --- |
| topic_navigation | standalone:`test-rvv/sample_consensus/sac_model_sphere/README.zh.md` | Phase 010 已补当前结论、先读路径、命令、证据白名单和默认排除项。 |
| testing_overview | standalone:`test-rvv/sample_consensus/sac_model_sphere/doc/testing-overview.zh.md` | Phase 010 已补运行入口分类和 target 粒度审计。 |
| correctness_tests | standalone:`test-rvv/sample_consensus/sac_model_sphere/doc/correctness-tests.zh.md` | Phase 010 已补 TEST 字典。 |
| benchmark_and_evidence | standalone:`test-rvv/sample_consensus/sac_model_sphere/doc/benchmark-and-evidence.zh.md` | Phase 010 已补 bench label、board、manifest、Evidence Doctor 和提交边界。 |
| optimization_evidence | standalone:`test-rvv/sample_consensus/sac_model_sphere/doc/optimization-evidence.zh.md` | Phase 010 已补候选实现到证据的映射。 |
| test_support_code_map | standalone:`test-rvv/sample_consensus/sac_model_sphere/doc/test-support-code-map.zh.md` | Phase 010 已补测试支撑代码、script 和 production 对照地图；Phase 055 已更新为 `include` / `include/impl` 结构。 |
| optimization_roadmap | standalone:`test-rvv/sample_consensus/sac_model_sphere/doc/optimization-roadmap.zh.md` | 已由 Phase 000 更新。 |
| phase_index / matrix / result | standalone:`test-rvv/sample_consensus/sac_model_sphere/doc/phases/` | Phase 000 result 已写入，Phase 010 / 020 作为恢复入口。 |
| production_topic_doc | standalone:`doc-rvv/sample_consensus/sac_model_sphere-RVV.zh.md` | Phase 045/046 后 `selectWithinDistance` `vcompress` 为 adopted production behavior；Phase 050 已补 `PointXYZI` evidence；Phase 060 已补 RGB/RGBA evidence。 |

## 当前结构审计结论

用户指出本 topic 的测试代码没有采用类似
`test-rvv/registration/transformation_estimation_point_to_plane_lls/include` 的结构。审计结论是：
`.agents/config/defaults.yaml` 已记录默认 `test_support.aggregator_directory=include` 和
`test_support.internal_directory=include/impl`；`rvv-test/references/optimization-phase-loop.zh.md`
要求把 mature sibling（成熟相邻主题）中的 `include/impl` 结构作为质量门槛校准。Phase 010 曾因
test/bench 文件规模较小而先补文档、不拆 helper；Phase 050 后 bench 已加入点型 dispatch，
因此 Phase 055 已迁移 fixture、candidate、assertion 和 bench harness，同时保持 gtest 名、bench 输出和
evidence target 不变。结构缺口已关闭；Phase 060 又在该结构上补齐 RGB/RGBA 点型扩展证据。当前默认恢复入口是整理 / review-ready 检查；自定义 registered xyz 点型需要先定义代表类型，`getDistancesToModel` 需要新的 RVV sqrt/helper 或 dense-store 消融证据。
