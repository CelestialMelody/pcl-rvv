# NDT 函数级评估

## 范围和目标源码

目标源码是 `registration/include/pcl/registration/impl/ndt.hpp`。当前 topic 评估 `NormalDistributionsTransform` 的 3D NDT registration（正态分布变换配准）实现，首轮只做 staged derivative accumulation（分阶段暂存后的导数累加）diagnostic，不修改 production。

## 标量流程速览

`computeTransformation` 每轮先调用 `computeDerivatives` 计算 score、6 维 gradient 和 6x6 hessian，再用 `JacobiSVD` 解牛顿方向，并通过 More-Thuente line search（线搜索）迭代步长。`computeDerivatives` 对每个 transformed source point 查找 target voxel neighborhood（目标体素邻域），随后：

1. 取原始点 `x` 和变换后点到 voxel mean 的差 `x_trans`。
2. 读取 voxel inverse covariance（逆协方差）`c_inv`。
3. 调 `computePointDerivatives` 生成 point Jacobian / Hessian。
4. 调 `updateDerivatives` 累加 score、gradient 和 hessian。

phase 000 未覆盖邻域搜索、点导数预计算、line search 和 Eigen solver。phase 010 已补 public-entry profile（公开入口性能剖析）：在当前确定性 `PointXYZ` 样本中，`updateDerivatives` / `updateHessian` 是主要自耗时热点，`neighbor search` 和 `SVD` 不是主因。

## 数学函数审计

`ndt.hpp` 中的数学函数不是同一类热点。`computeTransformation` 初始化 Gaussian 常量时调用 `std::log` 和常量输入的 `std::exp(-0.5)`，通常每次 alignment（配准求解）只发生一次；`computeAngleDerivatives` 调 `std::sin` / `std::cos` 计算 3 个欧拉角，调用频率也低。当前最值得关注的是 `updateDerivatives` 和 `updateHessian` 中的 double `std::exp`，它跟随 point-neighbor sample（点-邻域样本）执行。

已有 `common/include/pcl/common/impl/rvv_math.hpp` 提供的是 float RVV math helper（RVV 数学辅助函数），包括 `expf_RVV_f32m2`、`logf_RVV_f32m2` 和 `sincos_finite_domain_RVV_f32m2`。NDT 当前公式使用 double，因此本阶段候选没有直接复用这些 helper。若下一步继续数学函数路线，需要先另开或并入 math helper phase，按 `rvv-math-vectorization` 为 double `exp` 建立语义合同、误差测试、QEMU / 板卡证据和 NDT caller smoke。

## 函数级结论

EvidenceDecision：`bench-only/no-production`。当前 RVV candidate 数值正确、能生成 RVV 指令，但板卡 repeated evidence 稳定负向，不建议接入 `ndt.hpp`。phase 010 的 public-entry profile 只支持继续搜索方向，不支持 production 接入；phase 020 的 topic-local double `exp` RVV 原型也没有改变负向 decision bucket，因此不建议为 NDT 继续推进公共 double exp helper。

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- |
| `NormalDistributionsTransform::computeDerivatives` | production hot path candidate | 遍历点和邻域，累计导数 | 源码事实 | `registration/include/pcl/registration/impl/ndt.hpp` |
| `updateDerivatives` | production helper | 单个 point-neighbor sample 的 score/gradient/hessian 公式 | 诊断对照 | `registration/include/pcl/registration/impl/ndt.hpp` |
| `derivative_accumulate_std` | diagnostic reference | staged sample 标量 reference | correctness baseline | `include/impl/ndt_references.hpp` |
| `derivative_accumulate_candidate` | diagnostic candidate | RVV staged accumulation | attempted candidate | `include/impl/ndt_candidates.hpp` |
| `bench_ndt` | bench wrapper | 输出 case timing 和 checksum | board evidence | `src/bench_ndt.cpp` |
| Board summary | evidence output | 5 次 repeated A/B | performance decision | `log/board/derivative_accumulation_repeated/summary.md` |
| Public-entry summary | evidence output | 真实 `align()` 公开入口 3 次 repeated | profile / continue decision | `log/board/public_entry_profile_repeated/summary.md` |
| gprof report | evidence output | 函数级自耗时和调用图 | hotspot attribution | `log/board/public_entry_gprof_probe/gprof.txt` |
| Double exp probe summary | evidence output | topic-local double `exp` 原型 5 次 repeated | negative diagnostic | `log/board/double_exp_probe_repeated/summary.md` |

## 当前证据

| 类型 | 路径 | 结论 |
| --- | --- | --- |
| correctness | `test-rvv/registration/ndt/log/qemu/run_test_std.log`、`test-rvv/registration/ndt/log/qemu/run_test_rvv.log`、`test-rvv/registration/ndt/log/board/test_smoke/run_test.log` | pass |
| QEMU smoke / asm | `test-rvv/registration/ndt/log/qemu/evidence_doctor.md`、`test-rvv/registration/ndt/build/asm/riscv/bench_ndt_rvv.asm` | doctor clean；RVV 指令存在 |
| board performance | `test-rvv/registration/ndt/log/board/derivative_accumulation_repeated/summary.md` | negative |
| public-entry profile | `test-rvv/registration/ndt/log/board/public_entry_profile_repeated/summary.md`、`test-rvv/registration/ndt/log/board/public_entry_gprof_probe/gprof.txt` | public speedup neutral；导数更新为主要热点 |
| double exp probe | `test-rvv/registration/ndt/log/board/double_exp_probe_repeated/summary.md`、`test-rvv/registration/ndt/log/board/double_exp_probe_repeated/evidence_doctor.md` | correctness pass；board still negative |
| Evidence Doctor | `test-rvv/registration/ndt/log/board/derivative_accumulation_repeated/evidence_doctor.md` | Errors=2，Warnings=2；结论降级为 negative diagnostic |

## 诊断证据链

local diagnostic helper 证明：在 staged input 上，RVV candidate 与标量 reference 保持数值一致，并实际生成 RVV 指令。board repeated 证明：当前 staged RVV 形态在目标硬件上稳定慢于标量。它不能证明：真实 public entry 没有 RVV 空间，也不能替代 production direct evidence。

## 生产接入判断

不建议把当前 RVV candidate 接入 production。进入 production integration loop 的前置条件至少包括：

1. 真实 public-entry profile 证明 `updateDerivatives` 是主热点，而不是 neighbor search / point derivative / solver。
2. 新 candidate 减少 staged buffer 和多遍 reduction 成本，并在同一 diagnostic boundary 下转正。
3. PI1 明确 fallback、dispatch、点类型、`Scalar` 和 `__RVV10__` 关闭行为。

当前没有这些证据，因此停在用户确认点。

## 遗留风险和下一步

默认建议：不接入 production，保留 topic-local diagnostic 作为负向证据。phase 020 已评估 double `exp` helper 路线且收益不足；若用户仍希望继续，下一步应设计更少 staging 的 fused formula RVV-vs-RVV A/B。若不继续，当前 topic 可以停在 no-production 结论。
