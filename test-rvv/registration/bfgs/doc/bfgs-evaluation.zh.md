# bfgs RVV 函数级评估

## 范围和目标源码

本评估覆盖 `registration/include/pcl/registration/bfgs.h`。该头文件包含两部分：

- `Eigen::PolynomialSolver<Scalar, 2>` 的二次多项式求根特化，用于 line search（线搜索）插值里的导数根查找。
- `BFGS<FunctorType>` 优化器模板，以及 `BFGSDummyFunctor` 回调接口。

`bfgs.h` 是 header-only（仅头文件模板）实现。仓库内当前确认的 caller（调用方）是 `GeneralizedIterativeClosestPoint` 的 BFGS 路径：`registration/include/pcl/registration/gicp.h` 定义 `OptimizationFunctorWithIndices : BFGSDummyFunctor<double, 6>`，`registration/include/pcl/registration/impl/gicp.hpp` 在 `estimateRigidTransformationBFGS()` 中构造 `BFGS<OptimizationFunctorWithIndices>` 并逐步调用 `minimizeOneStep()`。

## 函数 / 函数族作用速览

| 函数 / 函数族 | 作用 | 输入 / 输出状态 | 与主流程关系 | RVV 判断 |
| --- | --- | --- | --- | --- |
| `Eigen::PolynomialSolver<Scalar, 2>` | 求二次多项式根，辅助三次插值找导数为 0 的候选点。 | 输入多项式系数，输出 roots 和 `hasRealRoot`。 | 只被 `interpolate()` 的 cubic 分支用于 line-search 候选步长。 | 固定小规模标量数学，`not_rvv_target`。 |
| `BFGSSpace::Status` | 表达优化状态，例如 `Success`、`NoProgress`、`NotStarted`。 | 状态枚举，无向量数据。 | `minimize*()`、`lineSearch()` 和 caller convergence（收敛）检查的控制信号。 | 控制流，不适合作为 RVV 目标。 |
| `BFGSDummyFunctor` | 定义 caller 必须提供的目标函数接口。 | 输入状态向量 `x`，输出函数值、梯度或二者组合。 | `BFGS` 通过 `operator()`、`df()`、`fdf()` 回调真实 registration cost（配准代价）。 | caller 成本边界，不在本 topic 内重写。 |
| `BFGS::Parameters` | 保存 Fletcher / GSL 风格 line-search 参数和迭代上限。 | 标量阈值、步长、插值阶数和循环次数。 | 控制 `minimize()` 与 `lineSearch()` 的搜索策略。 | 参数配置，不是 RVV 目标。 |
| `minimize()` / `minimizeInit()` / `minimizeOneStep()` | 对外优化流程：初始化，然后重复执行一步 BFGS。 | 读写 `x`、`f`、`gradient`、`x0/g0/p`、`pnorm/g0norm/fp0`。 | GICP BFGS caller 的主要入口；`minimizeOneStep()` 内含 direction update。 | direction update 已做 test-only RVV 诊断，board negative。 |
| `lineSearch()` | 沿当前方向选择步长 `alpha`，包含 bracketing（括区间）和 sectioning（缩区间）。 | 读写 `alpha`、函数值、方向导数和 line-search cache。 | 每个 `minimizeOneStep()` 先调用它，再更新位置和搜索方向。 | 标量分支和 functor 回调为主，`not_rvv_target`。 |
| `interpolate()` / `checkExtremum()` | 在线搜索区间内用二次 / 三次插值选择下一候选 `alpha`。 | 输入端点函数值 / 导数和允许区间，输出候选步长。 | `lineSearch()` 的步长候选生成器。 | 固定小矩阵 / 标量数学，`not_rvv_target`。 |
| `moveTo()` / `slope()` / `applyF*()` / `updatePosition()` / `changeDirection()` | 维护 `x_alpha`、`g_alpha`、函数值 / 导数 cache，并把 line-search 结果写回当前状态。 | 读写 cache key、候选位置、候选梯度、函数值和方向导数。 | `lineSearch()` 与 `minimizeOneStep()` 之间的状态胶水。 | `moveTo+slope` correctness 已过，但 direction-update board negative 后不默认扩展。 |

## 函数级结论

当前结论是 `diagnostic_stop_no_production`：BFGS 局部诊断已经闭合，不修改 production 源码。理由是：

1. Phase 010 已证明 `BFGSDiagnostic.DirectionUpdate*` 和 `BFGSDiagnostic.MoveToAndSlopeMatchesScalarReference` 在 Std / RVV build 下同构，而且 QEMU smoke / Evidence Doctor 清洁。
2. Phase 020 在 Milkv-Jupiter 上完成 5-run board repeated diagnostic，`direction-update-vector6` median B/A=`0.648x`、`direction-update-vector128` median B/A=`0.740x`，两个 case 都是 negative。
3. `lineSearch()` 的主成本包含函数回调 `functor(x_alpha)`、`functor.df()`、`functor.fdf()` 和标量控制流，局部向量运算不一定是入口主成本。
4. `minimizeOneStep()` 的 BFGS direction update（方向更新）虽包含 dot（点积）、norm（范数）、向量差和向量线性组合，但当前 test-only RVV helper 在真实板卡上慢于 Std build，不解锁 caller-shaped audit（调用方形态审计）或 production integration（生产接入）。

## 函数族评估表

| 函数 / 区域 | 标量路径 | 可 RVV 化片段 | 当前判断 | 需要补的证据 |
| --- | --- | --- | --- | --- |
| `moveTo(alpha)` | `x_alpha = x0 + alpha * p`，更新 cache key | 向量 SAXPY（标量乘加向量）形态；可能由 Eigen 自动向量化 | `planned_diagnostic` | 更细 asm attribution（反汇编归属）、board smoke |
| `slope()` | `g_alpha.dot(p)` | dot reduction（点积规约） | `planned_diagnostic` | 反汇编归属和数值预算 |
| `minimizeInit()` | 调用 `functor.fdf()` 后初始化 `x0/g0/p` 和 cache | `norm()`、向量缩放和 dot | `planned_diagnostic` | functor cost 占比、caller smoke |
| `minimizeOneStep()` direction update | 计算 `dx0`、`dg0`、`dxg/dgg/dxdg/dgnorm`，再形成新方向 `p` | 多个点积、范数、向量线性组合，可做 fused diagnostic（融合诊断候选） | `board negative / no-production` | 已有 correctness、QEMU smoke、board repeated；不进入 caller hotspot |
| `lineSearch()` | bracketing（括区间）和 sectioning（缩区间）循环，频繁调用 functor | 主要是标量分支、缓存和函数回调 | `not_rvv_target` | 只保留数值语义测试；不作为 RVV 主路径 |
| `interpolate()` / `PolynomialSolver` | 二次 / 三次插值和小矩阵多项式求根 | 固定小规模标量数学 | `not_rvv_target` | 不进行 RVV 优化；后续 correctness 覆盖边界即可 |

## 标量流程与 RVV 诊断流程对照

| 阶段 | 当前标量流程 | 计划中的诊断流程 | 保留边界 |
| --- | --- | --- | --- |
| 初始化 | `functor.fdf()` 生成目标函数值和梯度；`p = -gradient / ||gradient||` | 先复刻同一 Eigen baseline，不改变 functor | functor 是 caller 主成本候选，不在 BFGS topic 内重写 |
| line search | 根据 Fletcher 条件选择 `alpha`，通过 `applyF/DF/FDF` 缓存函数值和梯度 | 不向量化 line-search 控制流 | 保持缓存语义、`NoProgress` 和 `Success` 状态 |
| 更新位置 | `x = x_alpha`、`g = g_alpha` | `moveTo()` + `slope()` 可检查是否自动生成向量指令 | 不改变输出顺序和 cache key |
| BFGS direction update | 多次 dot/norm 后计算 `A/B`，再更新 `p` | test-only fused helper 已建并完成 board repeated；结果为 negative | test-only diagnostic 不能替代 production direct，且当前不支持继续 |
| 收敛检查 | GICP caller 的 `checkGradient()` 检查 translation / rotation 梯度范数 | 不改 | GICP convergence（收敛）语义保持 production 标量 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `BFGS<FunctorType>` | production template helper | BFGS 优化器主体 | `GeneralizedIterativeClosestPoint` BFGS 路径 | `FunctorType::fdf/df/operator()` | production boundary（生产边界），本阶段不修改 | `registration/include/pcl/registration/bfgs.h` |
| `BFGSDummyFunctor<double, 6>` | production callback interface | GICP functor 的基类，固定 6 维状态 | `gicp.h` | `BFGS` | caller boundary（调用方边界） | `registration/include/pcl/registration/gicp.h` |
| `estimateRigidTransformationBFGS()` | production caller | 设置 GICP 临时状态，逐步运行 BFGS | `GeneralizedIterativeClosestPoint::computeTransformation()` | `BFGS::minimizeInit()` / `minimizeOneStep()` | caller-shaped diagnostic 的真实来源 | `registration/include/pcl/registration/impl/gicp.hpp` |
| `direction_update_std()` | scalar reference | BFGS direction update 同构参考 | `src/test_bfgs.cpp` | `direction_update_candidate()` | correctness gate（正确性门禁） | `test-rvv/registration/bfgs/include/impl/bfgs_references.hpp` |
| `direction_update_candidate()` | test-only diagnostic helper | RVV direction update 候选 | `src/test_bfgs.cpp`、`src/bench_bfgs.cpp` | `direction_update_std()` | diagnostic candidate（诊断候选） | `test-rvv/registration/bfgs/include/impl/bfgs_candidates.hpp` |
| `bench_bfgs.cpp` | bench wrapper | 输出 QEMU smoke case、checksum 和 Total Time | Makefile target | manifest / doctor / registry | qemu_smoke_only | `test-rvv/registration/bfgs/src/bench_bfgs.cpp` |
| `generate_bfgs_qemu_evidence_manifest.py` | analysis script | 把 QEMU smoke 转成通用 manifest | Makefile target | `evidence_doctor.py` | evidence manifest | `test-rvv/registration/bfgs/script/generate_bfgs_qemu_evidence_manifest.py` |
| `log/qemu/evidence_doctor.md` | evidence output summary | QEMU smoke doctor 摘要 | `run_qemu_smoke_evidence_doctor` | reviewer / phase result | summary-only evidence | `test-rvv/registration/bfgs/log/qemu/evidence_doctor.md` |
| `generate_bfgs_board_repeated_summary.py` | analysis script | 解析 board repeated `run-*`，生成 summary / manifest | Makefile target | `evidence_doctor.py` | board diagnostic evidence | `test-rvv/registration/bfgs/script/generate_bfgs_board_repeated_summary.py` |
| `log/board/direction_update_repeated/summary.md` | evidence output summary | Phase 020 board B/A 摘要 | `run_board_bench_bfgs_direction_update_repeated` | reviewer / phase result | board diagnostic evidence | `test-rvv/registration/bfgs/log/board/direction_update_repeated/summary.md` |

## 实现方式审计

| 实现维度 | 当前状态 | 证据 | 边界 / 恢复条件 |
| --- | --- | --- | --- |
| dispatch / fallback | `not_applicable_current_phase` | 未修改 production，没有 `__RVV10__` dispatch | 只有 caller hotspot + board 证据成立并获授权，才进入 PI1 |
| vector expression baseline | `partial` | `dump_bench_rvv` 已生成 filtered asm，但未做更细符号归属 | 下一阶段若出现 board 信号，再细化 asm attribution |
| fused direction update | `attempted / correctness passed / board negative` | Std/RVV gtest 通过；QEMU smoke clean；board 5-run negative；doctor Errors=2 | 停止默认生产推进 |
| move-to + slope combined diagnostic | `correctness passed / bench deferred / no default expansion` | gtest 对拍通过；未作为当前 QEMU smoke 主证据 | direction-update 已 negative，不默认扩展 |
| line-search scalar control | `not_applicable with evidence` | 控制流、cache key 和 functor 回调为主 | 后续只做 boundary correctness |
| caller scope | `GICP-only confirmed / not_unblocked` | `rg` 只确认 GICP 路径使用 BFGS；局部 direction-update board negative | 不外推到 NDT；默认不做 GICP hotspot audit |
| production scope | `no_production` | 无 production diff、无 board evidence | `doc-rvv` 不适用 |

## 测试计划和 bench 计划

| 测试 / target | 层级 | 作用 | 当前状态 |
| --- | --- | --- | --- |
| `BFGSDiagnostic.DirectionUpdateVector6MatchesScalarReference` | diagnostic correctness | GICP `Vector6d` 形态与标量 reference 同构 | done |
| `BFGSDiagnostic.DirectionUpdateLongVectorMatchesScalarReference` | diagnostic correctness | 128 维连续 double 向量同构 | done |
| `BFGSDiagnostic.DirectionUpdateZeroDxdgBoundaryMatchesScalarReference` | boundary / regression | 除零边界保持标量语义 | done |
| `BFGSDiagnostic.DirectionUpdateEmptyInputKeepsScalarFallbackShape` | boundary | 空输入 fallback shape 保持清楚 | done |
| `BFGSDiagnostic.MoveToAndSlopeMatchesScalarReference` | numerical consistency | `moveTo(alpha)` + `slope()` 同构 | done |
| `BFGSPublicApiSmoke.QuadraticFunctorMinimizeOneStepRuns` | unit / public API smoke | `BFGS<QuadraticFunctor>` 可运行且状态有限 | done |
| `run_bench_direction_update_smoke` | QEMU log-shape smoke | 只证明 bench 可运行和日志形状可解析 | done |
| `run_bench_move_to_slope_smoke` | QEMU smoke | 追加局部链路 smoke | available |
| `run_bench_gicp_shaped_vector6_smoke` | caller-shaped smoke | 追加 GICP 形态 smoke | available |
| `dump_bench_rvv` | asm attribution | 生成 filtered RVV asm | done |
| `run_qemu_smoke_evidence_doctor` | QEMU manifest / doctor | 生成 manifest 并跑 Evidence Doctor | done |
| `record_qemu_correctness_state`、`record_qemu_smoke_evidence_state` | registry | 登记当前证据文件 | done |
| `collect_board_bfgs_direction_update_repeated` | board repeated performance | Phase 020 board 诊断 | done / negative |
| `run_board_bench_bfgs_direction_update_repeated` | board collect + summary + doctor + registry | Milkv-Jupiter 5-run repeated | done / negative |

## 当前状态

Phase 020 已完成 test-only diagnostic scaffold、QEMU correctness、QEMU smoke、filtered asm 和 board repeated / Evidence Doctor 链路。当前没有 production 改动；board evidence 为 negative。

## 诊断证据链

当前证据能证明：

1. test-only direction update 和 move-to+slope helper 在 Std / RVV build 下与标量 reference 同构。
2. RVV build 在 QEMU 下可以执行 direction-update smoke，并输出可解析 checksum。
3. RVV bench 二进制存在 RVV 指令，Evidence Doctor 对 `qemu_smoke_only` manifest 没有异常。
4. Milkv-Jupiter board repeated 中，test-only RVV direction-update helper 对 `Vector6d` 和 `vector128` 均慢于 Std build。

当前证据不能证明：

1. QEMU timing 代表目标硬件性能。
2. RVV 指令属于 production `BFGS` 热点或 GICP caller 热点。
3. 修改 `registration/include/pcl/registration/bfgs.h` 值得接入生产。

## 生产接入判断

当前不进入 production integration loop。`bfgs.h` 是模板头文件，GICP 使用的维度是 6，任何生产改动都会影响所有潜在 `FunctorType` 实例。Phase 020 已给出负向板卡证据；没有正向 board、caller hotspot 和用户授权时，生产接入风险高于收益。

## 文档归属矩阵

| 信息类型 | 主归属 | 当前路径 |
| --- | --- | --- |
| S2 evaluation、初始生产判断、Traceability Map | evaluation | 本文件 |
| 阶段计划和完成条件 | phase plan / result | `doc/phases/000-current-state-and-gaps/plan.zh.md`、`doc/phases/010-diagnostic-scaffold-and-asm-probe/result.zh.md`、`doc/phases/020-board-direction-update-diagnostic/result.zh.md` |
| 跨阶段候选搜索空间 | optimization roadmap | `doc/optimization-roadmap.zh.md` |
| 候选证据状态 | optimization matrix | `doc/phases/optimization-matrix.zh.md` |
| 测试体系和 target 粒度审计 | topic-local docs | `doc/testing-overview.zh.md`、`doc/correctness-tests.zh.md`、`doc/benchmark-and-evidence.zh.md` |
| 长期 production 行为 | `doc-rvv` | not_applicable，未新建 |

## 默认下一步

默认停止在 diagnostic closeout：不进入 `030-caller-hotspot-audit`，不扩展 `move-to+slope`，不触碰 production。若用户明确要求继续，只建议做 bounded negative-analysis（负向原因分析），例如拆解 intrinsic overhead、reduction 顺序和 Eigen 自动向量化边界。
