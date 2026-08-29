# implicit_shape_model 函数级评估

## 范围和目标源码

目标源码是 `recognition/include/pcl/recognition/impl/implicit_shape_model.hpp`。当前 adopted
scope（已采纳范围）只覆盖 `findObjects()` 中“每个 descriptor 找最近 cluster center”的平方 L2
距离规约和最小值选择。生产补丁新增 `pcl::ism::detail::findNearestClusterIndexStd`、
`findNearestClusterIndexRVV` 和 `findNearestClusterIndex<FeatureSize>`，公开入口调用该 helper 后继续
使用原有 vote 生成和 `ISMVoteList` 状态更新。

Phase 000/010 的局部和 production-shaped diagnostic（生产形态诊断）仍保留为历史证据；Phase 020
用真实 `findObjects()` public entry（公开入口）和 repeated board（重复板卡测试）补齐 production
direct（真实生产路径证据）。正式长期文档为
`doc-rvv/recognition/implicit_shape_model-RVV.zh.md`。

## 函数 / 函数族作用速览

| 函数 / 函数族 | 作用 | 输入 / 输出状态 | 与主流程关系 | RVV 判断 |
| --- | --- | --- | --- | --- |
| `trainISM()` | 抽取描述子、聚类、计算 sigma 和权重，形成 `ISMModel` | training clouds、classes、normals -> model | 训练主入口 | 未接 production；KMeans、feature estimator 和对象状态仍需 profile |
| `findObjects()` | 对测试 cloud 估计特征、找最近 visual word、生成 vote | model、cloud、normals -> vote list | 识别主入口 | descriptor assignment 子段已接 RVV |
| `findNearestClusterIndexStd/RVV` | 计算一个 descriptor 到所有 cluster center 的平方 L2 距离并返回最近 center | `Eigen::VectorXf` + `Eigen::MatrixXf` | `findObjects()` 内部生产 helper | Phase 020 adopted |
| `calculateSigmas()` | 每类 object 计算推荐 sigma | training clouds/classes -> sigmas | 训练阶段 helper | Phase 000 diagnostic positive，但未映射到 production |
| `calculateWeights()` | 计算 learned/statistical weights | locations、labels、clusters -> weights | 训练阶段 helper | 未接；`nth_element` 和对象状态保留标量 |
| `shiftMean()` / `getDensityAtPoint()` | radiusSearch 后累加高斯权重 | vote tree、distances、strength -> center/density | peak 提取路径 | Phase 000 diagnostic positive，但 double `std::exp` 语义未闭合 |

## 函数级结论

当前 EvidenceDecision（证据决策）是 `adopted production behavior / narrow findObjects descriptor assignment`。
Phase 020 production direct board 证据为 weak positive（弱正向）：`public_find_objects_descriptor_assignment`
5-run median `1.060x`，min `1.040x`，max `1.070x`，`B/A < 1` 为 `0/5`。Std/RVV 的语义 checksum
一致：`semantic:public_votes=494:votes_match=True:peak_density_match=True:peak_fingerprint_match=True`，
raw bit checksum 为 `7645179244906730525`。Evidence Doctor
为 `Errors=0 / Warnings=0 / Suggestions=2`。

该结论只说明当前 public RVV path（公开入口 RVV 路径）在板卡上快于当前 public scalar path
（公开入口标量路径）。它不是 RVV-family-selection（RVV 实现族选择）结论，也不证明 `trainISM()`、
sigma、density、其它点型或其它 `FeatureSize` 组合。

## 标量流程与 RVV 流程对照

| production 标量路径 | RVV / 诊断路径 | 计时边界 | 不能证明什么 |
| --- | --- | --- | --- |
| `findObjects()` 为每个 keypoint 计算 descriptor sum，跳过零 descriptor，然后逐 cluster 复制 center 并调用 `computeDistance()` | production patch 改为 `findNearestClusterIndexStd/RVV`，RVV 侧按 VL chunk 读取 descriptor 和 column-major center row，做平方差规约 | Phase 020 public-entry bench 调用真实 `findObjects()`，不含文件 I/O 和训练 | 不证明 feature estimator、VoxelGrid、vote tree 或 `trainISM()` 是热点 |
| `computeDistance()` 对 `FeatureSize` 维 descriptor 做平方差求和 | Phase 000 `nearestClusterDistanceCandidate()` 用 RVV sum reduction | 单个 descriptor 对所有 cluster center | 仅历史局部诊断，不作为最终生产性能结论 |
| `assignDescriptorBatchStd()` 对多 descriptor 执行 descriptor_sum gate 和最近 cluster assignment | Phase 010 `assignDescriptorBatchCandidate()` | synthetic descriptor batch | 仍不是真实公开入口，已被 Phase 020 production direct 取代 |
| `calculateSigmas()` 双层点循环找最大点积后 sqrt | Phase 000 `maxPairwiseDotSigmaCandidate()` | synthetic training cloud | 不包含 class 平均和真实训练分布 |
| `getDensityAtPoint()` 对 radiusSearch 结果做 `strength * exp(-d/sigma^2)` 求和 | Phase 000 `densityWeightedSumCandidate()` | 已有 distance / strength 数组 | 不包含 tree search，不等价 double `std::exp` production 语义 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `findObjects()` | production public entry | 识别入口，生成 ISM vote list | 用户调用 `ImplicitShapeModelEstimation::findObjects()` | feature estimator、nearest-cluster helper、vote list | production boundary | `recognition/include/pcl/recognition/impl/implicit_shape_model.hpp` |
| `findNearestClusterIndexStd` | production Std helper | 保留原标量最近 cluster 语义 | `findNearestClusterIndex` | `findObjects()` 的 `min_dist_inds` | fallback coverage | 同上 |
| `findNearestClusterIndexRVV` | production RVV helper | RVV VL chunk 平方差求和和最小值选择 | `findNearestClusterIndex` | `findObjects()` 的 `min_dist_inds` | production RVV path | 同上 |
| `src/test_ism.cpp` | correctness source | Std/RVV helper 对拍 | `run_test_compare` | gtest assertion | correctness gate | `test-rvv/recognition/implicit_shape_model/src/test_ism.cpp` |
| `test/recognition/test_recognition_ism.cpp` | upstream public-entry test | 原有公开入口回归测试 | `run_upstream_test_compare` | `trainISM()` / `findObjects()` | production direct correctness | 上游测试源 |
| `src/bench_ism.cpp` | bench wrapper | public-entry bench 和历史 diagnostic bench | `run_bench_*` / board target | summary analyzer | board performance input | `test-rvv/recognition/implicit_shape_model/src/bench_ism.cpp` |
| `generate_ism_evidence_manifest.py` | analysis script | 生成 summary / manifest / Evidence Doctor 输入 | `record_evidence_state_public_entry` | registry / doctor | evidence manifest | `test-rvv/recognition/implicit_shape_model/script/generate_ism_evidence_manifest.py` |
| Phase 020 summary | evidence output summary | 5-run production direct board 统计 | `public_entry_board_repeated` | evaluation / `doc-rvv` | board performance | `test-rvv/recognition/implicit_shape_model/log/board/repeated_phase020_public_entry_findobjects_production_direct/summary.md` |
| Phase 020 result | phase result | 生产接入闭环事实回填 | phase loop | README / evaluation / Handoff | recovery pointer | `test-rvv/recognition/implicit_shape_model/doc/phases/020-production-integration-findobjects-public-entry/result.zh.md` |
| production 长期文档 | production topic doc | 当前 adopted 行为、fallback、证据链 | reviewer / maintainer | 源码和测试证据 | long-term maintenance | `doc-rvv/recognition/implicit_shape_model-RVV.zh.md` |

## 实现方式审计

| 实现维度 | 当前状态 | 证据 | 边界 / 恢复条件 |
| --- | --- | --- | --- |
| dispatch / fallback | adopted | `findNearestClusterIndex<FeatureSize>()` 在 `__RVV10__` 构建下统一走 RVV，其它构建走 Std | public entry 未改 API；dispatch 不再依赖 153 魔法值 |
| layout / traits gate | adopted narrow evidence | descriptor 为 `Eigen::VectorXf`，centers 为 column-major `Eigen::MatrixXf`，RVV 使用 `outerStride()` 跨步加载 center row | 证据当前覆盖 `FeatureSize=153`、`PointXYZ` / `Normal`；其它实例仍需要补 production direct 以扩证据，不是因为代码门禁 |
| reduction | adopted | `vle32` + `vlse32` + `vfmul` + `vfred` 反汇编 gate 通过 | 规约顺序不同，语义用 assignment / vote checksum 和上游测试覆盖 |
| math helper | not_applicable with evidence | 当前生产 patch 不替换 `std::exp` 或三角函数 | density 方向另开数学语义审计 |
| production scope | adopted narrow | Phase 020 summary + Doctor + registry | 只接 `findObjects()` descriptor assignment，不接训练和 vote density |

## 测试计划和 bench 计划

| 测试 / target | 层级 | 作用 |
| --- | --- | --- |
| `run_test_compare` | correctness aggregate（正确性汇总入口） | Std/RVV 诊断 helper 和 production helper 对拍 |
| `run_upstream_test_compare` | production direct correctness（真实生产路径正确性） | 用上游 ISM 测试源和 PCD fixture 验证公开入口语义 |
| `check_ism_rvv_asm` | asm gate（反汇编验收） | 确认 RVV build 的 bench 二进制中出现 descriptor helper 需要的 load / stride-load / reduction 指令 |
| `public_entry_board_repeated` | board repeated（板卡重复采集） | 采集 public `findObjects()` bench 的 5-run production direct 性能信号 |
| `record_evidence_state_public_entry` | registry（证据登记） | 写入 Phase 020 manifest、doctor 和 registry |
| `check_public_entry_evidence_freshness` | freshness check（新鲜度检查） | 检查摘要证据和文档引用是否一致 |

## 当前证据

| 证据层级 | 当前结果 | 路径 / 命令 |
| --- | --- | --- |
| topic correctness | passed | `make -C test-rvv/recognition/implicit_shape_model run_test_compare` |
| upstream public entry | passed | `make -C test-rvv/recognition/implicit_shape_model run_upstream_test_compare` |
| QEMU smoke | passed for minimal public-entry bench reproducer after feature K-search fix | `run_bench_std ... --case-filter public_find_objects_descriptor_assignment --iterations 1 --warmup-iterations 0` |
| asm | passed | `make -C test-rvv/recognition/implicit_shape_model check_ism_rvv_asm` |
| board production direct | median `1.060x`，min `1.040x`，max `1.070x`，0/5 退化 | `log/board/repeated_phase020_public_entry_findobjects_production_direct/summary.md` |
| Evidence Doctor | `Errors=0 / Warnings=0 / Suggestions=2` | `log/board/repeated_phase020_public_entry_findobjects_production_direct/evidence_doctor.md` |
| registry | refreshed with Phase 020 doc refs | `log/evidence_registry.json` |

## 生产接入后的最终证据更新

`production_patch_scope`：生产 patch 只修改 `recognition/include/pcl/recognition/impl/implicit_shape_model.hpp`。
它新增 internal detail helper，并把 `findObjects()` 中原本逐 cluster 构造 `clusters_center` 后调用
`computeDistance()` 的循环替换为 `findNearestClusterIndex<FeatureSize>()`。公开 API、模型字段、vote 输出和其它入口不变。

`covered_path`：当前证明范围是 `public_find_objects_descriptor_assignment`，输入为 deterministic model
和测试专用 `SyntheticIsmFeature`，点型为 `pcl::PointXYZ` / `pcl::Normal`，`FeatureSize=153`，
clusters=`184`，descriptors=`512`，目标硬件为当前 board。

`fallback_matrix`：

| 条件 | 行为 | 证据 |
| --- | --- | --- |
| 非 RVV 构建 | `findNearestClusterIndexStd()` | Std build correctness / upstream test passed |
| `__RVV10__` | `findNearestClusterIndexRVV()` | RVV build correctness、asm、board |
| 非 `__RVV10__` | `findNearestClusterIndexStd()` | 标量 fallback 语义 |
| `number_of_clusters == 0` | helper 返回 `0`，保持原 min index 默认语义 | helper 源码审计 |
| 零 descriptor sum | `findObjects()` 原有 gate 继续 `continue` | public-entry bench checksum / 上游测试 |
| 非 Phase 020 入口 | 保持原标量或未接状态 | 未修改 `trainISM()`、sigma、density、vote tree |

`production_board_bench`：Phase 020 summary 中 `public_find_objects_descriptor_assignment`
5-run median `1.060x`，min `1.040x`，max `1.070x`，`B/A < 1` 为 `0/5`。这是弱正向但稳定的生产直连结果。

`decision_delta`：Phase 010 synthetic descriptor batch 曾有 median `2.120x`，Phase 020 公开入口收益降为
`1.060x`。最终采纳以 Phase 020 production direct 证据为准；诊断结果只解释为什么值得做有界生产探针。

## doc_suite_role_inventory

| role | 状态 | evidence |
| --- | --- | --- |
| topic_navigation | `standalone:test-rvv/recognition/implicit_shape_model/README.zh.md` | 当前结论、阅读顺序、命令和提交边界已刷新 |
| testing_overview | `standalone:test-rvv/recognition/implicit_shape_model/doc/testing-overview.zh.md` | target 粒度含 public-entry targets |
| correctness_tests | `standalone:test-rvv/recognition/implicit_shape_model/doc/correctness-tests.zh.md` | gtest 与 upstream test 分层 |
| benchmark_and_evidence | `standalone:test-rvv/recognition/implicit_shape_model/doc/benchmark-and-evidence.zh.md` | Phase 000/010/020 case 和 evidence 路径 |
| optimization_evidence | `standalone:test-rvv/recognition/implicit_shape_model/doc/optimization-evidence.zh.md` | adopted / deferred candidate 状态 |
| optimization_roadmap | `standalone:test-rvv/recognition/implicit_shape_model/doc/optimization-roadmap.zh.md` | 后续 train/sigma/density/generic 扩展条件 |
| test_support_code_map | `standalone:test-rvv/recognition/implicit_shape_model/doc/test-support-code-map.zh.md` | production helper、test、bench、script 可定位 |
| phase_index | `standalone:test-rvv/recognition/implicit_shape_model/doc/phases/README.zh.md` | Phase 020 为默认当前结果 |
| evaluation_production | `standalone:test-rvv/recognition/implicit_shape_model/doc/implicit_shape_model-evaluation.zh.md` | 本文件 |
| production_topic_doc | `standalone:doc-rvv/recognition/implicit_shape_model-RVV.zh.md` | 已适用并创建 |

## 生产接入判断

当前 production patch 采纳为窄范围 production behavior。理由是：Phase 020 真实公开入口 evidence role
为 `production_direct`，A/B boundary（对照边界）为 `public_overload`，checksum 一致，board 5-run
稳定弱正向，Evidence Doctor 无 Error / Warning；同时当前实现只替换一个局部规约 helper，非 RVV fallback
简单，公开 API 不变。

## 遗留风险和下一步

- `trainISM()` 的 KMeans、随机中心和 feature estimator 没有被本阶段生产接入覆盖。
- `calculateSigmas()` 虽有局部 diagnostic positive，但训练入口收益和数据分布未闭合。
- `vote_density_gaussian_sum` 使用 float `expf_RVV_f32m2` 诊断，不代表 double `std::exp` production 语义。
- 当前 production direct 的板卡证据只覆盖 `FeatureSize=153`、`PointXYZ` / `Normal` 和 deterministic public-entry fixture；这不等于生产分流只接受 153。
- 本轮没有值得继续自动推进的同 scope 优化方向。进一步尝试会进入新的 phase：training profile、sigma production-shaped diagnostic、density math audit 或 point type / FeatureSize expansion，需要先冻结新的证据边界。
