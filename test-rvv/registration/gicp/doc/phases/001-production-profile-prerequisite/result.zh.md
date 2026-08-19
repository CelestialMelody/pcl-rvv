# Phase 001 Result: Production Cost Probe And Hessian Follow-up

## 当前状态

本阶段已把 Phase 000 的 residual cost-only（只计算 cost 的残差代价）候选接入
`registration/include/pcl/registration/impl/gicp.hpp`，作为 `operatorCostRVV()`
有界生产探针。探针只在 `__RVV10__`、`Scalar=float`、source / target 都满足
`RVVXYZAoS` xyz 单 float layout、match 数不少于 32 且 32-bit byte offset 可表达时命中；
其它模板实例自然回到原标量路径。

## 已完成动作

| action | evidence | result |
| --- | --- | --- |
| 修正生产探针矩阵布局 | `registration/include/pcl/registration/impl/gicp.hpp` | `Eigen::Matrix3d` 默认列主序，RVV gather 已按 `0,3,6 / 1,4,7 / 2,5,8` 取 `M*d` |
| 生产公开入口 correctness smoke | `make run_test_compare` | Std/RVV 均通过；新增 `GICPProductionDirect.PublicAlignPointXYZSmoke` |
| 生产公开入口 1024 点板卡 repeated | `log/board/production_public_align_pointxyz_repeated/summary.md` | median `1.019x`，min `1.014x`，max `1.030x`，bucket `neutral` |
| 生产公开入口 4096 点扩展复跑 | `log/board/production_public_align_pointxyz_4096_repeated/summary.md` | median `1.011x`，min `1.009x`，max `1.022x`，bucket `neutral`；3-run 只作扩展验证 |
| `dfddf()` 主循环诊断 | `log/board/dfddf_loop_dense_repeated/summary.md` | median `1.410x`，min `1.396x`，max `1.427x`，bucket `positive` |

## Evidence Doctor

| evidence | Errors | Warnings | Suggestions | 处理 |
| --- | ---: | ---: | ---: | --- |
| `production_public_align_pointxyz_repeated/evidence_doctor.md` | 0 | 1 | 1 | near-threshold，不能写成稳定收益 |
| `production_public_align_pointxyz_4096_repeated/evidence_doctor.md` | 0 | 2 | 1 | 3-run 且 near-threshold，只能支撑“扩大点数后仍 neutral” |
| `dfddf_loop_dense_repeated/evidence_doctor.md` | 0 | 1 | 0 | reduction order warning，诊断证据需降级，不能直接外推 production |

## EvidenceDecision

当前 cost-only 生产探针是 `production_public / neutral / do_not_adopt_current_patch`。
不建议把当前 `operatorCostRVV()` cost-only patch 作为正式 adopted production behavior。

但 `dfddf-loop-dense` 是新的强正向诊断候选：它对应
`OptimizationFunctorWithIndices::dfddf()` 中默认 Newton optimizer 每轮都会执行的逐 correspondence
累加主循环。该候选仍没有 production direct、indexed gather、fallback 和 asm attribution 证据，
因此当前结论是 `diagnostic / recommend_next_bounded_dfddf_production_probe`。

## 下一步

本阶段的下一步已经由 Phase 002 承接。

建议不要采纳当前 cost-only 生产探针。若继续生产接入，应把下一阶段限定为
`dfddf()` 主循环有界生产探针：先把当前 test-only dense-row 诊断扩展到 production 的
PointCloud + indices gather 形态，再做 public GICP PointXYZ correctness、asm 和板卡 repeated。
