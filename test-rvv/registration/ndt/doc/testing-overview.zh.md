# NDT 测试总览

本文说明 `test-rvv/registration/ndt` 的 test（测试）、bench（性能测试）、QEMU、board（板卡）和 Evidence Doctor（证据体检）入口。细节分别归到 correctness、benchmark/evidence 和 evaluation 文档。

## 测试类型和入口

| 类别 | target / 文件 | 证明范围 | 当前状态 |
| --- | --- | --- | --- |
| correctness aggregate（正确性汇总入口） | `make run_test_compare` | Std/RVV 两种构建下，diagnostic candidate 与标量 reference 数值一致 | adopted |
| correctness alias（正确性细分入口） | `make run_test_derivative` | `NDTDerivativeDiagnostic.*` gtest filter | adopted |
| QEMU smoke（QEMU 小型验证） | `make record_qemu_smoke_evidence_state` | RVV bench 可构建、日志形状可解析、filtered asm 有 RVV 指令 | adopted |
| board smoke（板卡小型验证） | `make run_board_test_smoke` | RVV test binary 在板卡上通过 4 个 TEST | adopted |
| board repeated（板卡重复性能） | `make run_board_bench_repeated` | 5 次 Std/RVV bench compare，生成 summary、manifest 和 doctor | adopted |
| double exp probe repeated（double exp 消融重复性能） | `make run_board_bench_ndt_double_exp_repeated` | topic-local double `exp` RVV 原型的 5 次 Std/RVV bench compare | adopted; negative |
| doctor / registry（证据体检和登记） | `make run_evidence_doctor`、`make evidence_status` | 检查异常信号和登记证据 freshness（新鲜度） | adopted |
| production direct（真实生产路径证据） | 无 | 未修改 production，未验证 public entry | not_applicable with evidence |

## 输入和覆盖矩阵

| 维度 | 当前覆盖 | 未覆盖 |
| --- | --- | --- |
| source path | staged point-neighbor sample（已暂存点-邻域样本） | 真实 `target_cells_` neighbor search（邻域搜索） |
| 点类型 | synthetic double sample；不依赖 `PointT` 字段 | `PointXYZ`、`PointNormal`、泛型 traits（字段特征） |
| `Scalar` | diagnostic double math | production 模板 `Scalar=float/double` 分流 |
| NDT 阶段 | `updateDerivatives` 的 score/gradient/hessian 累加 | `computeAngleDerivatives`、`computePointDerivatives`、line search、Eigen SVD |
| fallback | `__RVV10__` 关闭时走标量 reference | production fallback / dispatch 未接入 |

## 推荐流程

1. `run_test_compare` 确认数值一致。
2. `record_qemu_smoke_evidence_state` 确认 RVV 指令和 QEMU smoke manifest。
3. `run_board_test_smoke` 确认板卡 correctness。
4. `run_board_bench_repeated` 生成 repeated board summary、manifest、Evidence Doctor 和 registry。
5. 若评估 phase 020 double exp 原型，运行 `run_board_bench_ndt_double_exp_repeated`。
6. `evidence_status` 检查登记和文档引用。

当前性能结论只能来自 board repeated；QEMU timing 不写入 EvidenceDecision。
