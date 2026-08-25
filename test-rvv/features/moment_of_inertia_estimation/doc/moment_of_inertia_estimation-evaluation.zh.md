# Moment of inertia estimation RVV evaluation

## S2 函数级评估

`pcl::MomentOfInertiaEstimation<PointT>::compute()` 通过 `PCLBase` 的 `input_` 和 `indices_` 读取点云，产出 mass center（质心）、AABB（axis aligned bounding box，轴对齐包围盒）、OBB（oriented bounding box，有向包围盒）、特征值/特征向量、moment of inertia（惯性矩）和 eccentricity（离心率）。

关键路径分层：

| stage | production path | hot loop | RVV priority |
| --- | --- | --- | --- |
| mean/AABB | `computeMeanValue()` 遍历 `indices_`，累加 xyz 并维护 min/max | per point | high |
| covariance | `computeCovarianceMatrix(Eigen::Matrix&)` 以质心为中心累计 3x3 covariance（协方差矩阵） | per point | high |
| eigen solver | `computeEigenVectors()` 调用 Eigen `SelfAdjointEigenSolver` | once per matrix | scalar boundary |
| angle scan | `compute()` 对 theta/phi 旋转轴，调用 inertia 和 projected covariance | outer angle loop × points | high but must be phased |
| moment of inertia | `calculateMomentOfInertia()` 对每个点做 cross product（叉积）平方和 | per point per axis | high |
| projected cloud covariance | `getProjectedCloud()` 分配并写投影点云，随后对 projected cloud 求 covariance | per point per axis | high, phase 010 |
| OBB | `computeOBB()` 把点投影到 eigen axes 并维护 extrema（极值） | per point | high |

当前判断：phase 000 的 reduction summary（规约摘要）和 phase 010 的 projected covariance fusion（投影协方差融合）都形成 positive diagnostic（正向诊断）证据。phase 030 的 mean/AABB-only 候选已回滚并保留为 historical rejected probe；phase 040 将 phase 010 的 projected covariance fusion 接入真实 public `compute()`，板卡结果为 positive，Evidence Doctor 无 Error，因此当前 production patch 已采纳。phase 050 进一步把常见 PointXYZ-like typed scope（`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA`、`PointXYZRGBNormal`）也证实为 positive。

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `MomentOfInertiaEstimation<PointT>::compute()` | production public entry | 真实公开入口，串联 mean/covariance/eigen/angle scan/OBB | 用户代码 | private helpers | production-public boundary；phase040 已采纳 projected covariance RVV path | `features/include/pcl/features/impl/moment_of_inertia_estimation.hpp` |
| `computeMeanValue()` | production helper | 质心和 AABB 规约，当前保持原标量实现 | `compute()` | getters | scalar baseline；phase030 RVV helper 已回滚 | 同上 |
| `computeProjectedCovarianceRVV()` | production RVV helper | PointXYZ-like traits gate + indexed gather + vector reduction 直接计算 projected covariance | `compute()` angle scan | `computeEccentricity()` | adopted production implementation | 同上 |
| `computeCovarianceMatrix(Eigen::Matrix&)` | production helper | indexed cloud covariance 标量规约 | `compute()` | `computeEigenVectors()` | scalar path reconstruction | 同上 |
| `calculateMomentOfInertia()` | production helper | 单轴惯性矩标量规约 | angle scan | `moment_of_inertia_` | scalar path reconstruction | 同上 |
| `getProjectedCloud()` + projected `computeCovarianceMatrix()` | production helper pair | 先写 projected cloud（投影点云）再计算协方差 | angle scan | `computeEccentricity()` | scalar path reconstruction；phase 010 candidate 来源 | 同上 |
| `moi::computeReductionSummaryStd/RVV` | diagnostic reference / candidate | 对齐 phase 000 的均值、AABB、协方差、单轴惯性矩和 OBB extrema summary | `src/test_moi.cpp`、`src/bench_moi.cpp` | test/bench assertions | diagnostic correctness and bench | `include/impl/moi_reductions.hpp` |
| `moi::computeProjectedCovarianceStd/RVV` | diagnostic reference / candidate | 对齐 phase 010 的 materialized projection reference 和 fused RVV covariance helper | `src/test_moi.cpp`、`src/bench_moi.cpp` | test/bench assertions | diagnostic correctness and bench；不证明 public dispatch | `include/impl/moi_reductions.hpp` |
| `test_moi` | correctness gate | Std/RVV 同输入对拍，覆盖 tail、非连续 indices、fallback isolation（回退路径隔离）和边界值 | Makefile `run_test_*` | QEMU log | correctness evidence | `src/test_moi.cpp` |
| `bench_moi` | bench wrapper | helper-only 和 production-public 计时；case-filter 包含 `moi_reductions`、`moi_projected_covariance`、`moi_public_compute` 和 typed public cases；dataset label 按 case-filter 输出真实点型 | Makefile `run_bench_*` / board target | summary / Evidence Doctor | diagnostic and production-public performance evidence | `src/bench_moi.cpp` |
| phase 010 board summary | evidence output summary | projected covariance helper 的 5-run 板卡摘要 | `make run_board_moi_phase010_repeated` | phase result / Evidence Doctor / registry | diagnostic board evidence | `log/board/repeated_phase010_projected_covariance_diagnostic/summary.md` |
| phase 030 board summary | evidence output summary | public `compute()` 的 5-run historical production-public 板卡摘要 | `make run_board_moi_phase030_public_compute_repeated` | phase result / Evidence Doctor / registry | historical rejected board evidence；不支持采纳 | `log/board/repeated_phase030_public_compute_production/summary.md` |
| phase 040 board summary | evidence output summary | public `compute()` 的 5-run production-public 板卡摘要 | `make run_board_moi_phase040_projected_covariance_repeated` | phase result / Evidence Doctor / registry | production-public board evidence；当前已采纳 | `log/board/repeated_phase040_projected_covariance_production/summary.md` |
| phase 050 typed board summaries | evidence output summary | common PointXYZ-like typed public `compute()` 的 5-run production-public 板卡摘要 | phase050 typed board targets | phase result / Evidence Doctor / registry | production-public typed scope evidence；当前已采纳 | `log/board/repeated_phase050_*_production/summary.md` |

## 文档归属矩阵

| fact | primary owner | reference policy |
| --- | --- | --- |
| 阶段计划、结果、矩阵和继续动作 | `doc/phases/` | Handoff 只引用路径 |
| 候选搜索空间和下一 phase | `doc/optimization-roadmap.zh.md` | phase result 回填新增/拒绝路线 |
| 函数级评估、诊断证据链和 production typed scope | 本 evaluation | 已采纳 production 行为写入 `doc-rvv/features/moment_of_inertia_estimation-RVV.zh.md` |
| board summary / Evidence Doctor | `log/board` summary + doctor | 文档引用摘要路径，不复制 raw log |

## 当前诊断证据链

| phase | candidate | correctness | asm | board / target performance | Evidence Doctor | production boundary |
| --- | --- | --- | --- | --- | --- | --- |
| 000 | fused xyz reductions | `make run_test_compare` 通过 | `vluxei32`、`vfredusum/vfredmin/vfredmax`、`vfmacc` | 5-run median 2.082x，bucket `positive` | Errors=0，Warnings=0，Suggestions=1 | diagnostic helper-only；不证明 public `compute()` |
| 010 | projected covariance fusion | `make run_test_compare` 通过 | `measureProjectedCovariance` 范围内有 `vluxei32`、`vfmacc`、`vfnmsac`、`vfredusum` | 5-run median 1.247x，bucket `positive` | Errors=0，Warnings=0，Suggestions=1 | diagnostic helper-only；不包含完整 angle scan、projection allocation 或 Eigen eccentricity |
| 030 | mean/AABB-only production helper | historical `make run_test_compare` 通过；RED/GREEN 已记录 | historical public bench binary 有 `vluxei32`、`vfred*` | public compute 5-run median 1.018x，bucket `neutral`，2/5 退化 | Errors=1，Warnings=0，Suggestions=2 | historical production-public；不支持采纳，已回滚 |
| 040 | projected covariance production helper | `make run_test_compare` 通过；Std 3/3、RVV 4/4 | public bench binary 有 `vluxei32`、`vfmacc`、`vfnmsac`、`vfredusum` | public compute 5-run median 1.984x，bucket `positive`，0/5 退化 | Errors=0，Warnings=0，Suggestions=1 | production-public；当前已采纳 |
| 050 | PointXYZ-like typed public compute | `make run_test_compare` 通过；Std 8/8、RVV 14/14；补充 std build、空 input / indices、非 float xyz layout 的 fallback isolation | typed public bench binary 继续有 `vluxei32`、`vfmacc`、`vfnmsac`、`vfredusum` | `PointXYZI` 2.132x，`PointXYZRGB` 2.395x，`PointXYZRGBA` 2.487x，`PointXYZRGBNormal` 2.784x；全部 bucket `positive`，`0/5` 退化 | 四组均 Errors=0，Warnings=0，Suggestions=1 | production-public common typed scope；不再外推到 custom 点型或 `Scalar=double` |

`indices_` 的非连续 / 重排路径继续由 helper 对拍覆盖；`rvvMaxU32ByteOffsetElements<PointT>()` 对应的 oversized input guard 属于源码级门禁，本轮不硬造超大点云，记为 `not_applicable with evidence`。

## 当前 production 判断

`production_topic_doc_applicability`：adopted。当前存在 adopted production behavior（已采用生产行为），因此创建并维护 `doc-rvv/features/moment_of_inertia_estimation-RVV.zh.md`。

`production_decision`：`adopted_projected_covariance_production`。phase040 的 projected covariance production-public evidence 为 `positive`，Evidence Doctor 无 Error；phase030 mean/AABB-only probe 已回滚并保留为历史拒绝证据。phase050 证明同一 adopted production behavior 对 common PointXYZ-like typed scope 也成立，但没有引出新的默认算法优化方向。
