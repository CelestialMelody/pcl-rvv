# registration/transformation_estimation_point_to_plane_lls 函数级 RVV 评估

## 范围

- 主题：`transformation_estimation_point_to_plane_lls`
- 主文件：`registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls.hpp`
- 公开入口：`TransformationEstimationPointToPlaneLLS::estimateRigidTransformation`
- 专项目录：`test-rvv/registration/transformation_estimation_point_to_plane_lls/`
- 模块依据：`doc-rvv/library-screening/registration/registration-module-second-pass.zh.md` 建议优化队列第三项。

## 函数级结论

本主题收敛为 bench（性能测试）诊断主题，不接入 production（生产源码）分流。目标函数每个 source/target 对生成 point-to-plane LLS（点到平面线性最小二乘）的一行 `a,b,c,nx,ny,nz`，并累加 6x6 / 6x1 normal-equation（法方程），最后交给 Eigen inverse（矩阵求逆）求解，再构造 4x4 变换矩阵。

本轮 diagnostic（诊断代码）只把 PointNormal 全云和 correspondences（对应关系）路径中的字段读取、finite mask（有限值掩码）和逐点 `a/b/c/d` 公式放入 RVV staging（RVV 暂存阶段）。normal-equation 累加仍由 scalar tail（标量尾段）按原 lane 顺序执行，避免直接改变规约树。这个设计在 QEMU correctness（QEMU 正确性验证，不代表真实性能）下通过，但 board evidence（板卡证据）显示 full-cloud 只有 `0.96x` 到 `0.99x`，correspondences 只有 `0.52x` 到 `0.58x`。收益不足且 gather（索引读取）路径明显退化，因此不应进入生产接入。

## 函数族评估

| 函数 / 路径 | 决策 | 覆盖与边界 |
| --- | --- | --- |
| PointNormal 全云 normal-equation 构造 | bench 诊断完成，不接生产 | 连续 `PointNormal`，`n >= 64`，`vlmax_e32m2 <= 64`，RVV 只做公式 staging，累加保持标量顺序。 |
| correspondences normal-equation 构造 | bench 诊断完成，不接生产 | 从 correspondences 展开 `uint32_t` source/target index，再用 RVV indexed gather（索引读取）；板卡明显慢于标量。 |
| Eigen solver / `constructTransformationMatrix` | 保留标量 | 6x6 solve 不是批量 VL chunk（可变向量长度分块）热点，且数值边界由 Eigen 管理。 |
| production 泛型模板 | 暂不接入 | 当前只验证 `pcl::PointNormal`，未把泛型 normal 字段 traits、Scalar=double、生产公开入口分流全部闭合。 |

## RVV 诊断设计

新增 `transformation_estimation_point_to_plane_lls_diag.hpp`，只位于 `test-rvv`：

- `accumulate_std_full` / `accumulate_std_correspondences`：保留标量 reference path（参考链路）。
- `accumulate_candidate_full`：连续 `PointNormal` 通过 `vlse32.v` stride load（跨步加载）读取 source xyz、target xyz 和 target normal，执行 finite mask、`a/b/c/d` staging，再 `vcompress` 保序压缩到固定 64 lane buffer（固定长度临时缓冲）。
- `accumulate_candidate_correspondences`：先把 correspondences 展开为 source/target index，再用 `vluxei32.v` gather load（索引加载）读取字段。
- `accumulate_staged_rows`：从压缩后的 `a/b/c/d,nx,ny,nz` 回到标量 normal-equation tail，保持累加顺序，避免把规约树变化和公式 staging 混在一起。

该 diagnostic 不使用 `_rm` intrinsic（显式舍入模式指令），不修改 FRM/FCSR（浮点舍入环境和状态寄存器）。

## 测试计划与结果

专项测试：`test_transformation_estimation_point_to_plane_lls.cpp`

| 测试 | 层级 | 作用 |
| --- | --- | --- |
| `StdDiagnosticMatchesPublicEstimator` | reference path | 证明 test-rvv 标量诊断与公开 estimator 在全云入口一致。 |
| `FullCloudCandidateMatchesStd` | RVV candidate | 大规模连续 PointNormal 全云对拍，RVV 构建要求命中 staging。 |
| `CorrespondenceCandidateMatchesStd` | RVV candidate / gather | 乱序、重复 correspondences 对拍，覆盖索引读取路径。 |
| `SmallInputFallsBackForIsolatedSizeGate` | fallback（回退路径） | 单独覆盖 `n < 64` 规模 gate，不混入 invalid 数据。 |
| `InvalidLaneMaskMatchesStd` | finite mask | 单独覆盖 NaN/Inf lane 被剔除后 normal-equation 一致。 |

QEMU `run_test_compare` 已通过：std 构建 5 个测试通过，RVV 构建 5 个测试通过。

## Bench 计划与结果

专项 bench：`bench_transformation_estimation_point_to_plane_lls.cpp`

| case | 入口 | 规模 | 证明点 |
| --- | --- | --- | --- |
| `lls normal-equation full-cloud pointnormal 65536` | `estimate_candidate_full` | 64K | 连续 PointNormal staging 的中等规模信号。 |
| `lls normal-equation correspondences pointnormal 65536` | `estimate_candidate_correspondences` | 64K | indexed gather 诊断信号。 |
| `lls normal-equation full-cloud pointnormal 262144` | `estimate_candidate_full` | 256K | 连续路径放大后是否能摊薄 staging 成本。 |
| `lls normal-equation correspondences pointnormal 262144` | `estimate_candidate_correspondences` | 256K | gather 路径放大后是否仍退化。 |

QEMU bench compare（性能测试对比）可解析，checksum（校验和）对齐；QEMU timing（QEMU 计时）只作为日志形状和路径证据，不写性能结论。

板卡结果如下：

| case | Std ms/iter | RVV ms/iter | speedup | 结论 |
| --- | ---: | ---: | ---: | --- |
| full-cloud 64K | 6.7628 | 6.8200 | 0.99x | 基本持平但略慢，不能接生产。 |
| correspondences 64K | 5.0639 | 9.7184 | 0.52x | gather + staging 明显退化。 |
| full-cloud 256K | 26.8654 | 27.9267 | 0.96x | 放大规模后仍慢。 |
| correspondences 256K | 20.1490 | 34.7913 | 0.58x | gather 路径仍明显慢。 |

## 反汇编结果

`dump_bench_rvv` 生成 `build/asm/riscv/bench_transformation_estimation_point_to_plane_lls_rvv.asm`。摘要检查命中：

- `vsetvli ... e32,m2`
- `vlse32.v`
- `vluxei32.v`
- `vfmul.vv`、`vfadd.vv`、`vfsub.vv`
- `vcpop.m`
- `vcompress.vm`

这些指令证明 RVV staging 路径存在，但不能单独证明加速。

## 生产接入判断

不接 production。原因是：当前最保守、最接近生产语义的 RVV 组织方式只把逐点公式和 finite mask 放入 RVV，仍需把 lane 压缩回标量累加；板卡 full-cloud 没有正收益，correspondences gather 退化明显。若改成真正的 vector reduction（向量规约）可能减少 scalar tail 成本，但会改变 double 累加顺序，必须重新证明 normal-equation 数值语义、解矩阵稳定性和可接受误差预算。当前没有必要为弱或负收益增加生产模板分流、traits gate 和维护成本。

## 遗留风险与后续条件

vector reduction（向量规约）方案尚未评估。它可能减少当前 scalar tail 成本，但会改变累加顺序，并可能影响 6x6 solve 后的变换矩阵。只有后续有明确误差预算、adversarial case（对抗边界样本）和板卡收益证据时，才值得重新评估。

泛型点类型尚未闭合。当前 diagnostic 只覆盖 `pcl::PointNormal`；生产接入前必须证明 source/target normal 字段 traits、`Scalar=double` fallback、indices 和 correspondences 的边界都可维护。由于本轮板卡性能不成立，这些不是当前阶段必须完成的工作。

非法 index 边界尚未单独测试。当前 helper 会跳过负 index 和越界 index，但本轮专项测试只覆盖乱序、重复 correspondences 与有效 index gather。补齐非法 index case 可以证明 defensive helper（防御性辅助函数）行为，但由于本轮不接 production，它不是当前 closeout 的阻塞项。
