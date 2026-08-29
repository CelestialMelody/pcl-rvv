# Phase 010: production-shaped ISM subkernel diagnostic plan
## 阶段意图和边界

本阶段只证明一件事：把 Phase 000 已经正向的 `descriptor_cluster_distance` 子核，包进更接近
`findObjects()` 的 production-shaped diagnostic（生产形态诊断）边界后，RVV 收益是否仍然稳定。

本阶段要保留的 production 形态是：

- 多个 sampled keypoints 依次做 descriptor-to-cluster assignment（描述子到聚类中心分配）。
- 每个 descriptor 先走 `descriptor_sum` gate，接近 `findObjects()` 中的零和过滤。
- 描述子仍使用 synthetic `pcl::Histogram<153>` / contiguous cluster center 形态。

本阶段不证明、也不修改：

- `recognition/include/pcl/recognition/impl/implicit_shape_model.hpp`
- feature estimator、VoxelGrid、KMeans、radiusSearch、vote tree state、`findObjects()` 的 vote 生成
- `trainISM()` / `findObjects()` 完整入口收益
- `vote_density_gaussian_sum` 的 double `std::exp` production 语义

## 当前状态清单

| area | 当前状态 | 证据 / 路径 |
| --- | --- | --- |
| Phase 000 | completed | `test-rvv/recognition/implicit_shape_model/doc/phases/000-current-state-and-gaps/result.zh.md` |
| 局部公式 board repeated | positive | `log/board/repeated_phase000_ism_local_formula_diagnostic/summary.md` |
| Evidence Doctor | no error | `log/board/repeated_phase000_ism_local_formula_diagnostic/evidence_doctor.md` |
| registry freshness | fresh | `make -C test-rvv/recognition/implicit_shape_model check_evidence_freshness` |
| production source | unchanged | `recognition/include/pcl/recognition/impl/implicit_shape_model.hpp` |
| topic-local doc suite | 已建立 | `README.zh.md`、`doc/*.zh.md`、`doc/phases/*` |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| production-shaped descriptor batch assignment | 多个 keypoints 的 descriptor assignment 仍能保留 Phase 000 的 RVV 优势 | 外层 batch 和 gate 可能吞掉局部收益 |
| descriptor_sum gate + assignment | `descriptor_sum` gate 可保留入口形态，同时不引入不必要的 production state | gate 可能让合成输入过于理想化 |
| later sigma / density branches | 训练期 sigma 或 vote density 仍是后续候选 | 语义、double `std::exp` 和 tree boundary 更复杂 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| production-shaped descriptor batch assignment | sampled keypoints vs cluster centers | synthetic `pcl::Histogram<153>` batch + contiguous centers | `findObjects()`-shaped helper in `bench_ism.cpp` | `run_test_compare` | new batch case in `bench_ism.cpp` | repeated board after helper lands | `descriptorBatchAssignmentRVV` | repeated manifest + doctor | planned | add failing test first, then helper and bench |
| sigma pairwise max-dot | single training cloud pairwise points | synthetic PointXYZ-like AoS float | `calculateSigmas()` local helper | `run_test_compare` | existing `sigma_pairwise_max_dot` | Phase 000 positive | `maxPairwiseDotSigmaRVV` | existing positive diagnostic | phase_deferred + unblocked | only after descriptor batch result or if descriptor branch weakens |
| vote density Gaussian sum | radiusSearch result distances and strengths | synthetic float arrays, finite positive sigma | `shiftMean()` / `getDensityAtPoint()`-shaped helper | `run_test_compare` | existing `vote_density_gaussian_sum` | Phase 000 positive but semantic / math risk higher | `densityWeightedSumRVV` / `expf_RVV_f32m2` | existing positive diagnostic | deferred | requires tree / double exp audit before production-shaped use |

## 实现和测试动作

1. 先补一个会失败的 `src/test_ism.cpp` 新用例，覆盖 batch descriptor assignment 的 Std/RVV 对拍。
2. 再补 `include/impl/ism_diagnostics.hpp` 的 production-shaped batch helper，保留 `descriptor_sum` gate 和 multi-keypoint loop。
3. 再补 `src/bench_ism.cpp` 的新 bench case，让板卡 summary 直接输出 batch 形态的 speedup、checksum 和关键计数。
4. 必要时扩展 `script/generate_ism_evidence_manifest.py` 的语义 checksum 解析，但保留 raw bit checksum 供 drift review。
5. 运行 `make -C test-rvv/recognition/implicit_shape_model run_test_compare`。
6. 运行 `make -C test-rvv/recognition/implicit_shape_model check_ism_rvv_asm`。
7. 板卡可用时运行 `board_repeated record_evidence_state_repeated check_evidence_freshness`。
8. 回填 phase result、evaluation、roadmap、matrix 和 README。

## Evidence Doctor 和 registry 规则

- 输入 manifest：`log/board/repeated_phase010_ism_production_shaped_diagnostic/evidence_manifest.json`
- summary：`log/board/repeated_phase010_ism_production_shaped_diagnostic/summary.md`
- doctor：`log/board/repeated_phase010_ism_production_shaped_diagnostic/evidence_doctor.md`
- registry：`log/evidence_registry.json`

预期结果：

- Errors 必须为 0；若出现 checksum error，先区分是语义契约、输入边界还是实现 bug。
- Warnings 允许保留，但必须解释 batch 形态相对 Phase 000 的外推边界。
- Suggestion 中的环境字段缺失仍是可继续项，但若重复 board 方向变化，则补齐后再判断。

## 阶段完成条件

本阶段只关闭 production-shaped descriptor batch assignment 这一个条目。完成条件是：

- Std/RVV correctness 通过。
- RVV asm 归属明确。
- 5-run board median 仍为正向。
- Doctor 无 Error，Warning 可解释。
- registry fresh。

若 batch 形态把 descriptor path 压到 neutral / weak，仍可继续，但必须先写清它是被哪一层 production 形态稀释，而不是直接关闭 topic。

## 板卡复跑预算和决策桶

- 默认 `REPEATED_BOARD_RUNS=5`
- `iterations=100`
- `warmup_iterations=5`
- positive：median `>=1.20x` 且无 `B/A < 1`
- weak_positive：median `1.05x-1.20x`
- neutral：median `0.95x-1.05x`
- negative：median `<0.95x`

预算耗尽后若 bucket 稳定，就按当前 bucket 写结果；若仍摇摆，标 `unstable` 并暂停生产接入判断。

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | production-shaped helper |
| 当前决策问题 | RVV-vs-scalar 和 implementation-shape |
| diagnostic 是否可外推到 production | unknown；比 Phase 000 更接近 `findObjects()`，但仍不含 feature estimator / vote tree |
| comparison-boundary / baseline mismatch 风险 | yes；batch 和 gate 仍是合成输入 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no；若 batch 证据弱，先回到 descriptor family 或转 sigma / density audit |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes；若 future phase 进入 production patch，仍需同边界证据 |

## Phase scope 与扩展队列

- validated_scope：Phase 000 的单 descriptor / contiguous centers / PointXYZ-like AoS / float 局部公式。
- unvalidated_scope：真实 FPFH、真实 sampled cloud、KMeans labels、radiusSearch、vote tree state、double `std::exp`、完整 `findObjects()` / `trainISM()`。
- point_type_expansion_queue：当前 not_applicable；还没有 production patch。
- phase_closeout_boundary：只关闭 batch descriptor assignment，不关闭完整 ISM production 结论。

## 继续 / 停止条件

若 production-shaped descriptor batch 仍然稳定 positive，下一 phase 默认转向 `sigma_pairwise_max_dot` 的训练期形态，或继续细化 descriptor 入口的 row source / point type 组合。若 batch 形态弱化到 neutral / negative 且无法解释，则暂停并整理哪一层 production 形态吞掉了收益。

## 文档更新清单

- `result.zh.md`
- `README.zh.md`
- `doc/implicit_shape_model-evaluation.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- 必要时更新 `doc/benchmark-and-evidence.zh.md` 和 `doc/testing-overview.zh.md`

## roadmap 同步动作

- 新增 `descriptor batch assignment` 为 Phase 010 主候选。
- 保留 `sigma pairwise max-dot` 作为后续 train-shaped 候选。
- 保留 `vote density Gaussian sum`，但继续压后到 math / tree boundary 审计之后。
