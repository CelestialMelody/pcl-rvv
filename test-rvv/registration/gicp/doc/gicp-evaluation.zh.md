# GICP 函数级评估

## 范围和目标源码

目标源码是 `registration/include/pcl/registration/impl/gicp.hpp` 与公开声明 `registration/include/pcl/registration/gicp.h`。本评估覆盖 `GeneralizedIterativeClosestPoint` 的局部数学组件、Phase 001 cost-only production probe、Phase 002 `dfddf()` production probe、Phase 003 clean adoption 尝试，以及 Phase 005 rollback closeout。当前结论是 no-production：生产源码已回到原标量实现。

## 函数族作用速览

| 函数 / 函数族 | 作用 | 输入 / 输出状态 | 与主流程关系 | RVV 判断 |
| --- | --- | --- | --- | --- |
| `computeTransformation()` | GICP 主迭代，构造 covariance、correspondence、Mahalanobis 矩阵并调用 optimizer | 读 input / target / indices，写 `mahalanobis_` 和 final transformation | public `align()` 的真实入口 | 主成本混合 KdTree、transform、correspondence、矩阵和 solver，不能直接生产接入 |
| `computeCovariances()` | 每点先 `nearestKSearch`，再对 K 个邻居求 mean / covariance，随后 3x3 SVD | 读点云和 kdtree，写 covariance vector | 主流程预处理 | 只允许先诊断 KNN 后的 covariance accumulation |
| correspondence 后 Mahalanobis 更新 | 对每个 correspondence 计算 `R*C1*R^T+C2` 并求逆 | 读 covariance 和 correspondence，写 `mahalanobis_` | 每轮迭代 | 3x3 小矩阵为主，先暂缓 |
| `OptimizationFunctorWithIndices::{operator(),df,fdf}` | 扫描 correspondence，计算 residual、`M*d`、cost、gradient 和 `dCost_dR_T` | 读 source / target / indices / `mahalanobis_` | Newton 和 BFGS 都会调用 | residual dense-row accumulation 是本轮首选诊断 |
| `OptimizationFunctorWithIndices::dfddf()` | 在 residual 基础上累加 gradient 和 6x6 hessian 输入 | 同上 | Newton 默认 optimizer 热路径 | `dfddfLoopRVV()` 曾有 public `weak_positive` 证据，但最终因收益偏低回退 |
| `estimateRigidTransformationBFGS()` | 可选 BFGS 优化器 | 读临时状态，写 transformation | `useBFGS()` 后生效 | `test-rvv/registration/bfgs` 已显示小向量局部更新板卡负向，不作为本轮生产候选 |
| `estimateRigidTransformationNewton()` | 默认 Newton 优化器，含 eigensolver 和 line search | 读 functor，写 transformation | 默认 optimizer | solver 控制流和 6x6 eigensolver 保持标量 |

## 标量路径与诊断映射

GICP public entry（公开入口）先准备 source / target covariance，再在每轮迭代中 transform source、做 correspondence estimation（对应关系估计）、更新 Mahalanobis 矩阵，然后调用 Newton 或 BFGS optimizer。当前 diagnostic（诊断）显式拆成这些组件：

| diagnostic 组件 | 对应 production 源码 | 计时边界 | 能证明 | 不能证明 |
| --- | --- | --- | --- | --- |
| residual / Mahalanobis dense-row accumulation | `OptimizationFunctorWithIndices::{operator(),df,fdf}` 的 per-correspondence 扫描 | 不含 KdTree、correspondence search、optimizer solver、输出 transform | 局部 residual math pipeline 是否值得继续 | public entry end-to-end speedup、production dispatch |
| residual / Mahalanobis indexed-gather accumulation | `OptimizationFunctorWithIndices::{operator(),df,fdf}` 中 `tmp_idx_src_` / `tmp_idx_tgt_` 的索引访存形态 | 不含 optimizer 调用频率、`computeRDerivative()`、PointT traits | dense-row 收益是否被 indices gather 吞掉 | public entry end-to-end speedup、production dispatch |
| covariance post-KNN accumulation | `computeCovariances()` 中 `nearestKSearch` 之后的 mean / covariance loop | 不含 KdTree search 和后续 3x3 SVD | KNN 后 k-loop 是否有独立收益 | covariance 完整预处理收益 |
| dfddf loop dense-row accumulation | `OptimizationFunctorWithIndices::dfddf()` 中逐 correspondence accumulator loop | 不含 production PointCloud + indices gather、旋转二阶导后处理和 eigensolver | 默认 Newton Hessian 主循环是否有 RVV 候选价值 | 已由 Phase 002 production-public probe 验证 PointXYZ 代表入口 |

## Row-source 数据流判定

`OptimizationFunctorWithIndices::{operator(),df,fdf}` 的 production 数据流不能简单标成单一 row source。上游 `computeTransformation()` 先由 `CorrespondenceEstimation` 生成 `Correspondence{index_query,index_match}`，这一层是 correspondence-pair（对应关系点对）。随后代码把 correspondence 拆成 `source_indices[]` 和 `target_indices[]`，再调用 `estimateRigidTransformationNewton()` 或 `estimateRigidTransformationBFGS()`；functor 入口实际读取的是两个索引数组，因此在 functor 边界更接近 dual-indexed-cloud-pair（双索引点云对）。

```text
Correspondence{index_query, index_match}
  -> source_indices[i] = index_query
  -> target_indices[i] = index_match
  -> functor:
       source[source_indices[i]]
       target[target_indices[i]]
       mahalanobis_[source_indices[i]]
```

本 topic 的 residual diagnostic 没有严格复现 production 的 correspondence-pair 或 dual-indexed-cloud-pair 入口成本，而是使用 post-correspondence dense-row（对应关系之后的预展开连续行）输入来隔离 residual / Mahalanobis math pipeline。换言之，它测试的是更理想化的数据流：点对和 Mahalanobis 数据已展开，主要测 `d`、`M*d`、cost / gradient reduction，不测 point cloud indexed gather。该理想化边界在板卡上已经转为 `positive`，并已通过 Phase 001/002 承接到 production-public probe；不能把 dense-row 诊断本身外推成泛型生产结论。

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `computeTransformation()` | production public path | GICP 主迭代入口 | `Registration::align()` | covariance、correspondence、optimizer | production boundary（生产边界） | `registration/include/pcl/registration/impl/gicp.hpp` |
| `OptimizationFunctorWithIndices::fdf()` | production hot component | cost + gradient residual 累加 | Newton / BFGS optimizer | `computeRDerivative()` | diagnostic source mapping（诊断来源映射） | `registration/include/pcl/registration/impl/gicp.hpp` |
| `OptimizationFunctorWithIndices::dfddf()` | production scalar path | Hessian 主累加入口 | Newton optimizer | 标量循环 | current production path | `registration/include/pcl/registration/impl/gicp.hpp` |
| historical `dfddfLoopRVV()` probe | rolled-back production probe | indexed gather + vector reduction 主循环 | 曾由 `dfddf()` 调用 | 标量 Hessian 后处理 | no-production evidence | 已从生产源码移除 |
| `residual_accumulate_std()` | diagnostic reference | 标量同构参考链路 | `test_gicp.cpp`、`bench_gicp.cpp` | candidate 对拍 | correctness reference（正确性参考） | `include/impl/gicp_references.hpp` |
| `residual_accumulate_candidate()` | candidate formula / reduction | RVV dense-row 组件候选 | `test_gicp.cpp`、`bench_gicp.cpp` | QEMU / board evidence | pre-production diagnostic（接入生产前诊断） | `include/impl/gicp_candidates.hpp` |
| `indexed_residual_accumulate_candidate()` | candidate formula / indexed gather | RVV indexed residual 候选 | `test_gicp.cpp`、`bench_gicp.cpp` | QEMU / board evidence | pre-production diagnostic | `include/impl/gicp_candidates.hpp` |
| `covariance_post_knn_candidate()` | candidate formula / reduction | RVV post-KNN k-loop 候选 | `test_gicp.cpp`、`bench_gicp.cpp` | QEMU / board evidence | pre-production diagnostic | `include/impl/gicp_candidates.hpp` |
| `generate_gicp_board_repeated_summary.py` | analysis script | 汇总 repeated board 并生成 manifest | `Makefile` | Evidence Doctor | board summary（板卡摘要） | `script/generate_gicp_board_repeated_summary.py` |
| `production_public_align_pointxyz_dfddf_clean_repeated/summary.md` | evidence output summary | clean adoption 1024 点 repeated | board target | evaluation / long doc | production-public evidence | `log/board/production_public_align_pointxyz_dfddf_clean_repeated/summary.md` |
| `production_public_align_pointxyz_dfddf_clean_4096_repeated/summary.md` | evidence output summary | clean adoption 4096 点 repeated | board target | evaluation / long doc | production-public expansion | `log/board/production_public_align_pointxyz_dfddf_clean_4096_repeated/summary.md` |

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | `diagnostic`，接入生产前组件诊断 |
| A/B boundary | `test helper`，Std/RVV 两侧都是 `bench_gicp` 的同名组件 |
| 当前决策问题 | `RVV-vs-scalar` 是否足以建议 bounded production probe（有界生产探针） |
| diagnostic 是否可外推到 production | unknown。indexed residual 已覆盖一种 production-like gather，但当前仍不含 optimizer 调用频率、PointT traits、`computeRDerivative()` 和 public entry profile |
| comparison-boundary / baseline mismatch 风险 | yes。组件输入是预展开 dense arrays，production 是 PointCloud + indices + Eigen 小矩阵状态 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 默认 no。若 residual 组件强正向且补齐 public profile，才建议 PI1 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes。生产接入前必须有 production direct tests、fallback、asm 和板卡 repeated |

Phase 003 曾把该审计回填到 production-public 边界：clean summary 的 A/B boundary 是 public
GICP entry，决策问题是当时 RVV path 相对 scalar path 是否值得保留。Phase 005 已按用户确认回退
生产源码，因此这些 production-public summary 现在只作为 no-production 决策证据，不能再写成当前生产行为。

## 生产接入判断

当前判断是：cost-only production probe 为 `production_public / neutral / do_not_adopt_current_patch`；`dfddf()` 主循环为 `production_public / weak_positive / rollback_due_to_low_benefit`。Phase 000 的板卡 repeated 结果来自 `test-rvv/registration/gicp/log/board/component_diagnostic_repeated/summary.md`：

| diagnostic 组件 | board result | 判断 |
| --- | --- | --- |
| residual / Mahalanobis dense-row accumulation | median `1.347x`，0/5 B/A < 1，bucket `positive` | 已作为 Phase 001/002 的生产探针线索 |
| residual / Mahalanobis indexed-gather accumulation | median `1.255x`，0/5 B/A < 1，bucket `positive` | 说明索引访存未完全吞掉收益 |
| covariance post-KNN accumulation | median `1.151x`，0/5 B/A < 1，bucket `weak_positive` | 次级候选；当前不优先于已弱正向的 `dfddfLoopRVV()` |

QEMU correctness 见 `test-rvv/registration/gicp/log/qemu/run_test_std.log` 和 `test-rvv/registration/gicp/log/qemu/run_test_rvv.log`，当前两侧各 10 个 GTest 通过。QEMU smoke 和 asm 见 `test-rvv/registration/gicp/log/qemu/run_bench_all_rvv.log`、`test-rvv/registration/gicp/build/asm/riscv/bench_gicp_rvv.asm` 和 `test-rvv/registration/gicp/log/qemu/evidence_doctor.md`；它们只证明路径、日志形状和 bench binary 中有 RVV 指令，不证明 production hot path。

Evidence Doctor 见 `test-rvv/registration/gicp/log/board/component_diagnostic_repeated/evidence_doctor.md`。当前 Errors=0，Warnings=5。其中 reduction contract mismatch 是当前 diagnostic 的有意差异：baseline 为 scalar order，candidate 为 RVV chunk reduction。covariance 和 indexed residual 还有 long-tail warning；这些差异要求把组件结论保持为 pre-production diagnostic，不能写成 production A/B。

Phase 001 的 cost-only 生产探针给出生产证据：`production-public-align-pointxyz`
在 1024 点 5-run repeated 中 median `1.019x`，在 4096 点 3-run 扩展中 median `1.011x`，
均为 `neutral`。这说明只接 `OptimizationFunctorWithIndices::operator()` 的 cost path
不能穿透完整 GICP public entry。

Phase 002 将 `dfddf-loop-dense` 诊断推进到 `dfddfLoopRVV()` 生产探针后，public GICP
`PointXYZ -> PointXYZ` 在 1024 点 5-run repeated 中 median `1.068x`，在 4096 点 3-run
扩展中 median `1.070x`，均为 `weak_positive`。Phase 003 删除 cost-only helper 后重新复测，
clean production public 结果仍为 `weak_positive`：1024 点 5-run median `1.080x`，4096 点
3-run median `1.058x`。QEMU 和板卡 smoke 均为 10 tests pass，full asm 显示
`OptimizationFunctorWithIndices::dfddf()` 调用 `dfddfLoopRVV()`，helper 中有 RVV gather / FMA /
reduction 指令。

最终不保留 `dfddfLoopRVV()` 生产补丁。public 证据只覆盖 `PointXYZ -> PointXYZ`、
`Scalar=float` 和 xyz AoS layout，收益也只有弱正向；Phase 004 的 gather width 微调没有改善。
当前没有适用的长期 `doc-rvv` 生产文档，结论保留在 topic-local no-production closeout 中。
