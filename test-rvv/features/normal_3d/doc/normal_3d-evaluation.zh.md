# normal_3d 函数级评估

## S2 函数级评估

目标入口是 `NormalEstimation<PointInT, PointOutT>::computeFeature`，位置为
`features/include/pcl/features/impl/normal_3d.hpp`。公开调用链是 `Feature::compute`
准备输出云后进入 `computeFeature`，每个 query index 先通过 `searchForNeighbors`
得到邻域索引，再调用 `computePointNormal(*surface_, nn_indices, ...)` 求局部平面 normal
和 curvature，最后用 `flipNormalTowardsViewpoint` 调整 normal 朝向。

当前源码中的主成本分为四段：

| 阶段 | 源码位置 | RVV 价值判断 |
| --- | --- | --- |
| 邻域搜索 | `searchForNeighbors` | 搜索树主导，当前 topic 不接管。 |
| centroid / covariance（均值 / 协方差） | `pcl::computeMeanAndCovarianceMatrix(cloud, indices, ...)` | common 层已经存在 `__RVV10__` 的 dense + xyz-compatible indexed RVV 路径，是本阶段首要复核对象。 |
| 3x3 平面求解 | `solvePlaneParameters` | 每个 query 只求一次小矩阵特征向量，初始阶段保留 Eigen 标量求解。 |
| normal 翻转和 NaN 输出 | `flipNormalTowardsViewpoint` 与失败分支 | 单点标量逻辑，当前阶段只做 correctness / fallback 证据。 |

初步判断：本 topic 先进入 diagnostic（诊断）阶段，不直接修改 production（生产源码）。如果板卡证据显示
normal 公开入口已经通过 common covariance RVV 获得稳定收益，本轮优先记录 production-shaped diagnostic
（生产形态诊断）和接入边界；若收益被搜索或 Eigen 求解稀释，则保持 no-production 或转向 component
ablation（组件消融）。只有同边界板卡收益、反汇编归属和 Evidence Doctor 通过后，才考虑是否需要
`normal_3d.hpp` 层的 production patch。

## Traceability Map（可追踪性地图）

| 符号 / 文件 | 层级 | 作用 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- |
| `NormalEstimation::computeFeature` | production public entry | 公开 normal estimation 主循环。 | production boundary（生产边界） | `features/include/pcl/features/impl/normal_3d.hpp` |
| `computePointNormal(cloud, indices, ...)` | production helper | 构造 covariance 并调用平面求解。 | correctness target（正确性目标） | `features/include/pcl/features/normal_3d.h` |
| `computeMeanAndCovarianceMatrixRVV` | reused production RVV helper | dense + indexed xyz-compatible covariance RVV 路径。 | candidate family（候选族） | `common/include/pcl/common/impl/centroid.hpp` |
| `src/test_normal_3d.cpp` | RVV test asset | 平面样本 correctness、过小邻域和公开入口输出。 | QEMU / board correctness（正确性） | `test-rvv/features/normal_3d/src/test_normal_3d.cpp` |
| `src/bench_normal_3d.cpp` | bench wrapper | component 与公开入口计时边界。 | board performance（板卡性能） | `test-rvv/features/normal_3d/src/bench_normal_3d.cpp` |

## 需要闭合的证据

- `run_test_compare`：Std / RVV correctness 都通过，QEMU 仅证明 correctness（正确性）和日志形状。
- `dump_bench_rvv`：RVV 指令可归属到 common covariance helper；full asm 中有
  `computeMeanAndCovarianceMatrixRVV<PointXYZ,float>`、`vluxseg3ei32.v`、`vfmacc.vv` 和 `vfredosum.vs`。
- 板卡 repeated：`component_compute_point_normal_indexed` 5-run median `1.43x`；
  `public_normal_estimation_k` 5-run median `1.02x`。
- Evidence Doctor：repeated manifest 的 Doctor 结果为 Errors=0、Warnings=0、Suggestions=5；
  public case 命中 `near_threshold_ba`，因此不能写成稳定 production 加速。

## S11 Closeout

本轮最终 EvidenceDecision（证据决策）：`no production patch for normal_3d.hpp`。
Phase 000 证明 common covariance RVV helper 已被 normal public path 复用，且在 indexed component
边界上有稳定板卡收益；但 public `NormalEstimation::compute` 的收益只有 near-threshold 弱正向，
主要可解释为 search 和 Eigen solve 稀释了 covariance helper 的局部收益。

实际新增 / 修改的 topic 资产：

| artifact | 作用 |
| --- | --- |
| `test-rvv/features/normal_3d/src/test_normal_3d.cpp` | synthetic plane correctness、过小邻域和 public entry smoke。 |
| `test-rvv/features/normal_3d/src/bench_normal_3d.cpp` | component covariance boundary 与 public normal estimation boundary 的板卡 bench。 |
| `test-rvv/features/normal_3d/script/generate_normal_3d_evidence_manifest.py` | 将单次或 repeated board compare 转成 Evidence Doctor manifest。 |
| `test-rvv/features/normal_3d/Makefile` / `board.mk` | QEMU、反汇编、board smoke、5-run repeated 和 Doctor 入口。 |
| `test-rvv/features/normal_3d/doc/phases/000-current-state-and-common-covariance-audit/result.zh.md` | Phase 000 事实、证据边界、Doctor 处理和停止条件。 |

当前不接入 production 的理由：

- `normal_3d.hpp` 没有新增 RVV family；主要 RVV 收益来自已有 common covariance helper。
- public case repeated median `1.02x`，Doctor 建议不要把 near-threshold 数字写成稳定加速。
- local flip/output loop 没有热点证据；继续写 production patch 会增加维护成本，却缺少同边界收益证明。

恢复条件：

- 真实 PCD workload 或 profile 指向 `normal_3d.hpp` local flip/output loop；
- common covariance helper 发生变化，需要 normal public path 回归；
- 用户明确要求 bounded production probe、OMP normal 对照或点型扩展评估。

## Production topic doc 适用性

当前没有 adopted production behavior（已采用生产行为），也没有 PI5 后用户确认保留的 production patch，因此
`doc-rvv/features/normal_3d-RVV.zh.md` 当前不适用，不创建。
