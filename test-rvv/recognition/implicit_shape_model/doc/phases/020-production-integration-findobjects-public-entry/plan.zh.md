# Phase 020: production integration of `findObjects()` public entry plan

## 阶段意图和边界

本阶段只做一件事：把 Phase 010 已经 positive 的 descriptor assignment 结果，真正落到
`recognition/include/pcl/recognition/impl/implicit_shape_model.hpp` 的 `findObjects()` 公开入口，
并用真实 `trainISM()` / `findObjects()` 入口证据验证它是否值得保留为 production patch。

本阶段要闭合的范围是：

- `findObjects()` 内“每个 descriptor 找最近 cluster center”这一段。
- `pcl::ism::detail::findNearestClusterIndexStd/RVV` 作为生产 helper。
- public-entry-shaped correctness smoke：`test/recognition/test_recognition_ism.cpp`。
- public-entry production-direct bench：`findObjects()` 在真实训练模型和测试云上的重复 board 采集。

本阶段不证明、也不扩大到：

- `trainISM()` 的 KMeans、sigma、weight 或 feature estimator 各子核。
- `calculateSigmas()` / `calculateWeights()` / `vote_density_gaussian_sum`。
- PointT / NormalT 泛型扩展，`PointNormal` / `PointXYZ` exact gate 之外的其它模板实例。
- 其它 public entry、其它 row source policy、其它 layout 或 `Scalar=double` 扩展。

## 当前状态清单

| area | 当前状态 | 证据 / 路径 |
| --- | --- | --- |
| Phase 000 | completed | `test-rvv/recognition/implicit_shape_model/doc/phases/000-current-state-and-gaps/result.zh.md` |
| Phase 010 | completed | `test-rvv/recognition/implicit_shape_model/doc/phases/010-production-shaped-ism-subkernel-diagnostic/result.zh.md` |
| production patch | present | `recognition/include/pcl/recognition/impl/implicit_shape_model.hpp` |
| production-shaped board repeated | positive | `test-rvv/recognition/implicit_shape_model/log/board/repeated_phase010_production_shaped_descriptor_diagnostic/summary.md` |
| production-shaped doctor | clean | `test-rvv/recognition/implicit_shape_model/log/board/repeated_phase010_production_shaped_descriptor_diagnostic/evidence_doctor.md` |
| upstream public-entry test source | available | `test/recognition/test_recognition_ism.cpp` |
| test data | available | `test/ism_train.pcd`、`test/ism_test.pcd` |
| topic-local doc suite | 已建立 | `README.zh.md`、`doc/*.zh.md`、`doc/phases/*` |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| production direct `findObjects()` helper | 真正的公开入口收益仍能保住 Phase 010 的 descriptor assignment 正向结果 | `trainISM()` 周边成本可能把收益冲淡 |
| upstream public-entry smoke | `test/recognition/test_recognition_ism.cpp` 能直接验证真实入口语义 | 需要更多 recognition 依赖，board 侧可能较重 |
| public-entry production-direct bench | 真实模型 + 测试云重复调用 `findObjects()` 可以说明接入后是否值得保留 | board 耗时更高，需有界复跑预算 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| production direct `findObjects()` descriptor assignment | public-entry training/test cloud over real `trainISM()` / `findObjects()` data | `PointXYZ`, `Normal`, `FeatureSize=153`, template `PointT` / `NormalT` | public entry + production helper | `run_upstream_test_compare` | new upstream/public-entry bench case | new production-direct repeated board summary | `findNearestClusterIndexRVV` / `findObjects()` | manifest + Doctor + registry | planned | add upstream smoke and production-direct bench, then board repeated |
| production-shaped descriptor batch assignment | sampled keypoints vs cluster centers | synthetic `pcl::Histogram<153>` + contiguous centers | `findObjects()`-shaped helper | `run_test_compare` | existing `descriptor_batch_assignment` | existing positive board | `descriptorBatchAssignmentRVV` | existing clean doctor | attempted / positive diagnostic | keep as historical evidence, do not treat as final production truth |
| full `trainISM()` path | public member function | template `PointT`, `NormalT`, object state | full training entry | `run_upstream_test_compare` | not yet separate | not yet separate | none yet | none yet | deferred | keep out of this phase unless production bench indicates training bottleneck |

## 实现和测试动作

1. 为真实 public entry 增加 upstream test target，使用仓库自带 `test/ism_train.pcd` 和 `test/ism_test.pcd`。
2. 为 public-entry bench 增加一个 `findObjects()` 重复采集 case，让板卡直接衡量生产补丁是否有收益。
3. 先跑 `run_upstream_test_compare`，确认真实入口语义没变。
4. 再跑 `check_ism_rvv_asm`，确认 production helper 命中预期 RVV 指令。
5. 板卡可用时跑新的 production-direct repeated board target，并记录 manifest、Doctor 和 registry。
6. 若 board positive，则更新 evaluation、roadmap、matrix、README、phase result 和 `doc-rvv`。

## Evidence Doctor 和 registry 规则

- 输入 manifest：新的 production-direct repeated board 输出目录下的 `evidence_manifest.json`
- summary：同目录 `summary.md`
- doctor：同目录 `evidence_doctor.md`
- registry：`test-rvv/recognition/implicit_shape_model/log/evidence_registry.json`

预期结果：

- Errors 必须为 0。
- Warnings 若仍为环境字段缺失，也只能作为可继续项。
- 若 production-direct board positive，则 `doc-rvv` 进入适用状态，且必须同步更新生产长期文档。

## 阶段完成条件

本阶段只关闭 `findObjects()` public-entry production direct 这一条条目。完成条件是：

- `run_upstream_test_compare` 通过。
- `check_ism_rvv_asm` 通过。
- 新的 production-direct repeated board summary 为 positive。
- Evidence Doctor 无 Error，或 Warning 已明确解释且不阻塞采纳。
- registry fresh。

若 public-entry board 结果 positive，且没有更强的反证，则按当前用户授权，可以直接采纳当前 patch，并进入生产长期文档 closeout。

## 板卡复跑预算和决策桶

- 默认 `REPEATED_BOARD_RUNS=5`
- `iterations` 先从 20 起步，必要时按 board 时间预算下调或上调
- `warmup_iterations=5`
- positive：median `>=1.20x` 且无 `B/A < 1`
- weak_positive：median `1.05x-1.20x`
- neutral：median `0.95x-1.05x`
- negative：median `<0.95x`

若 production-direct board 在预算内稳定 positive，可按用户授权直接采纳；若摇摆，则标 `unstable` 并暂停采纳判断。

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | production-public / production-detail |
| A/B boundary | public overload / production detail helper |
| 当前决策问题 | RVV-vs-scalar + production adoption |
| diagnostic 是否可外推到 production | partial；Phase 010 已证明 descriptor batch 形态，但仍需 public-entry 直连 |
| comparison-boundary / baseline mismatch 风险 | yes；Phase 010 仍是 synthetic batch，不是 upstream public entry |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes；当前是 positive，但若后续波动仍可做有界 board probe |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | no；本阶段只需 public RVV vs public scalar 证据，除非要比较不同 RVV family |

## Phase scope 与扩展队列

- validated_scope：descriptor assignment 的 real `findObjects()` public entry。
- unvalidated_scope：`trainISM()` 周边、sigma/density、其它 point type / layout / Scalar。
- point_type_expansion_queue：若 production-direct board positive 但只覆盖 `PointXYZ` / `Normal`，下一 phase 再审计泛型扩展。
- phase_closeout_boundary：只关闭 public-entry descriptor assignment，不关闭整个 ISM family。

## 继续 / 停止条件

若 public-entry board positive，默认继续到 production closeout；若 board negative 或 Evidence Doctor Error 无法解释，则暂停并回到候选审计。

## 文档更新清单

- `result.zh.md`
- `README.zh.md`
- `doc/implicit_shape_model-evaluation.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/testing-overview.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc-rvv/recognition/implicit_shape_model-RVV.zh.md`

## roadmap 同步动作

- 将 `findObjects()` public-entry 证据提升为最高优先级。
- 保留 `trainISM()`、sigma 和 density 作为后续扩展候选，不在本阶段收口。
