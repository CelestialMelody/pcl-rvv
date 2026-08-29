# implicit_shape_model Optimization Matrix

本文维护 ISM topic 的跨 phase 优化矩阵。矩阵中的 positive 只能按对应 evidence role
（证据角色）解释；diagnostic（诊断）不能自动升级成 production direct（真实生产路径直连）。

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| descriptor cluster distance | single descriptor vs contiguous cluster centers | synthetic float, `FeatureSize=153`, contiguous descriptor/centers | test helper in `bench_ism.cpp` | `run_test_compare` | `descriptor_cluster_distance` | 5-run median `1.980x`, min `1.910x` | `nearestClusterDistanceRVV` | Errors=0, Warnings=2 shared group outlier context | attempted / positive diagnostic | closed by Phase 010/020 carry-over |
| sigma pairwise max-dot | single training cloud pairwise points | synthetic PointXYZ-like AoS float | test helper in `bench_ism.cpp` | `run_test_compare` | `sigma_pairwise_max_dot` | 5-run median `3.880x`, min `3.870x` | `maxPairwiseDotSigmaRVV` | Errors=0 | deferred | no same-scope next action; resume with trainISM-shaped profile phase |
| vote density Gaussian sum | radiusSearch result distances and strengths | synthetic float arrays, finite positive sigma | test helper in `bench_ism.cpp` | `run_test_compare` | `vote_density_gaussian_sum` | 5-run median `10.010x`, min `9.980x` | `densityWeightedSumRVV` / `expf_RVV_f32m2` | Errors=0, Warnings=2 shared group outlier context | deferred | no same-scope next action; resume with math / tree boundary audit |
| descriptor nearest-cluster production-shaped diagnostic | sampled keypoints vs model cluster center matrix | synthetic `pcl::Histogram<153>` + Eigen-like center layout | `findObjects()` subkernel shape, no production dispatch | `run_test_compare` | `descriptor_batch_assignment` | 5-run median `2.120x`, min `2.040x` | `descriptorBatchAssignmentRVV` | Errors=0, Warnings=0, Suggestions=2 | attempted / positive production-shaped diagnostic | closed by Phase 020 production direct |
| production direct `findObjects()` descriptor assignment | public-entry deterministic `findObjects()` fixture | `PointXYZ`, `Normal`, `FeatureSize=153`, Eigen `VectorXf` / `MatrixXf` | production public entry + `findNearestClusterIndexStd/RVV` | `run_test_compare` + `run_upstream_test_compare` | `public_find_objects_descriptor_assignment` | 5-run median `1.060x`, min `1.040x`, max `1.070x`, `B/A < 1`=`0/5` | `findNearestClusterIndexRVV` / inlined `findObjects()` | Errors=0, Warnings=0, Suggestions=2 | adopted production behavior / narrow evidence | none inside Phase 020 scope |
| full `trainISM()` / sigma / density production path | public member functions | template `PointT`, `NormalT`, object state | training and vote density entries | not yet scoped | not yet scoped | none for production direct | none | none | turn_stop_deferred with stop_condition_hit | requires new phase and expanded authorization boundary |

## point_type_expansion_queue

| scope | 当前状态 | 恢复条件 |
| --- | --- | --- |
| `FeatureSize=153`, `PointXYZ` / `Normal`, deterministic public-entry fixture | validated by Phase 020 | 已采用；仅作为代表性证据 |
| 其它 `FeatureSize` | deferred | 新增 public-entry correctness / bench / asm / board 证据 |
| 其它 `PointT` / `NormalT` 实例 | deferred | 确认 feature estimator、descriptor layout 和 model matrix 语义后再做 production direct |
| `Scalar=double` 或 density math | not_applicable to current production patch | 若替换 `std::exp` 或 double math，转入 math vectorization phase |
