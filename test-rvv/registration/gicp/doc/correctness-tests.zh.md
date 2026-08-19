# GICP 正确性测试

| TEST | 输入 | 被测路径 | 断言 | 证明范围 |
| --- | --- | --- | --- | --- |
| `GICPResidualDiagnostic.DenseRowsMatchScalarReference` | 65536 行 deterministic dense-row | residual reference vs RVV candidate | cost、translation gradient、`dCost_dR_T` 误差小于预算 | 大规模 residual 组件 |
| `GICPResidualDiagnostic.TailRowsMatchScalarReference` | 4099 行 | 同上 | 覆盖 RVV tail（尾段） | 非整分块规模 |
| `GICPResidualDiagnostic.EmptyInputKeepsFallbackShape` | 0 行 | candidate fallback | 不声称 RVV 命中 | defensive boundary（防御性边界） |
| `GICPResidualDiagnostic.IndexedGatherMatchesScalarReference` | 32768 行 indexed source/target | indexed residual reference vs RVV candidate | cost、translation gradient、`dCost_dR_T` 误差小于预算 | production-like indices gather |
| `GICPResidualDiagnostic.IndexedGatherTailMatchesScalarReference` | 4103 行 indexed source/target | 同上 | 覆盖 indexed RVV tail | 非整分块 indexed 规模 |
| `GICPHessianDiagnostic.DenseRowsMatchScalarReference` | 32768 行 dense-row | `dfddf()` loop reference vs RVV candidate | gradient、translation Hessian、`dCost_dR_T`、`dCost_dR_T*b`、`hessian_rot_tmp` 误差小于预算 | 默认 Newton Hessian 主循环诊断 |
| `GICPHessianDiagnostic.TailRowsMatchScalarReference` | 4101 行 | 同上 | 覆盖 RVV tail | 非整分块 Hessian loop |
| `GICPCovarianceDiagnostic.DefaultKMatchesScalarReference` | 2048 点、k=20 | covariance post-KNN reference vs candidate | 6 个 covariance 项逐项对拍 | 默认 GICP k-loop |
| `GICPCovarianceDiagnostic.SmallKMatchesScalarReference` | 257 点、k=7 | 同上 | 小 k 和尾段对拍 | 小邻域高风险路径 |
| `GICPProductionDirect.PublicAlignPointXYZSmoke` | deterministic `PointXYZ -> PointXYZ` 小点云 | public `GeneralizedIterativeClosestPoint::align()` | 收敛、输出规模、fitness 和 final transform 标量参考 | 生产公开入口 smoke，不证明性能 |

Hessian 和 covariance 组件测试仍是接入生产前诊断，不覆盖 KdTree、correspondence search、3x3 SVD、
Newton eigensolver 或完整 public 性能。`GICPProductionDirect` 现在作为历史 production probe
（生产探针）smoke 保留，用于证明曾尝试的接入没有破坏 public `align()` 行为；当前生产源码已回到
标量实现，性能取舍以板卡 repeated summary 和 no-production closeout 为准。
