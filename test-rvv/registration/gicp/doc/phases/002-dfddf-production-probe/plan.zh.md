# Phase 002 Plan: dfddf Production Probe

## 阶段意图和边界

本阶段把 `OptimizationFunctorWithIndices::dfddf()` 的逐 correspondence 累加主循环做成
有界 production probe（生产探针）。目标是验证 Phase 001 中 `dfddf-loop-dense`
诊断的正向收益，能否穿过真实 `PointCloud + tmp_idx_src_ / tmp_idx_tgt_` 索引访存和
public GICP entry（公开入口）。

validated_scope：

- entry：`pcl::GeneralizedIterativeClosestPoint<pcl::PointXYZ, pcl::PointXYZ>::align()`
  触发的默认 Newton `dfddf()` 路径。
- row source：当前 `tmp_idx_src_` / `tmp_idx_tgt_` 生成的 source-indexed / target-indexed
  correspondence。
- point type / Scalar / layout：`PointXYZ -> PointXYZ`、`Scalar=float`、RVV xyz AoS float
  traits 命中。
- size：public bench 默认 1024 点，必要时扩展 4096 点；component diagnostic 保留已有
  32768 / 65536 rows。

unvalidated_scope：

- 非 `Scalar=float`、非 xyz AoS float layout、非 `PointXYZ` 代表点型。
- `PointNormal`、`PointXYZINormal`、混合 source/target 点型、correspondence-pair 入口。
- 其它 optimizer、其它 row source policy、KdTree / covariance / SVD 阶段收益。

本阶段不把 production patch 视为 adopted production behavior。PI5 之后仍需用户确认是否采纳或回滚。

## 当前状态清单

| item | current state |
| --- | --- |
| cost-only production probe | public board repeated 为 neutral：1024 median `1.019x`，4096 median `1.011x`；不建议采纳 |
| Hessian diagnostic | `dfddf_loop_dense_repeated` median `1.410x`，bucket `positive` |
| correctness | QEMU Std/RVV aggregate 10 tests pass；board RVV smoke 10 tests pass |
| production code | 已有 `operatorCostRVV()` probe；本阶段新增 `dfddfLoopRVV()` 后再判断整体 public entry |
| docs / matrix | roadmap 和 matrix 已把 `dfddf()` 标为下一未阻塞候选 |

## 候选族和假设

候选族：`dfddf indexed-gather RVV loop`。

假设：`dfddf()` 默认 Newton 路径每次内层迭代都会扫描所有 correspondences。与 cost-only
`operator()` 相比，它同时累加 translation gradient、translation Hessian、`dCost_dR_T`、
translation-rotation 中间矩阵和 rotation-rotation 中间矩阵，循环体更重，RVV 化更可能抵消
索引 gather 与 reduction 成本。

风险：

- 生产路径从 dense-row 变成 indexed gather，访存成本可能吞掉收益。
- 每个 VL chunk 有多项 reduction，可能被 reduce latency 限制。
- 完整 public entry 还包含 KdTree、covariance、correspondence、solver 后处理，局部收益可能被稀释。

## 优化矩阵

| candidate family | row source | point type / Scalar / layout | test | bench | board | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `dfddf()` production indexed-gather RVV loop | `tmp_idx_src_` / `tmp_idx_tgt_` | `PointXYZ` / `float` / xyz AoS | public smoke + aggregate | production public align | repeated board 5 runs | production bench RVV asm | Evidence Doctor | pending |

## 实现和测试动作

| action | artifact / command | completion |
| --- | --- | --- |
| 新增 production helper | `registration/include/pcl/registration/gicp.h`、`impl/gicp.hpp` | RVV helper 有明确 fallback gate |
| 接入 `dfddf()` 主循环 | `OptimizationFunctorWithIndices::dfddf()` | 命中 RVV 后跳过原逐点循环，后处理沿用标量代码 |
| QEMU correctness | `make run_test_compare` | Std/RVV 均通过 |
| QEMU smoke / asm | `make run_bench_all_smoke dump_bench_rvv` | bench 可运行，asm 含 RVV 指令 |
| board correctness | `make run_board_test_smoke` | RVV smoke 通过 |
| board public repeated | `make run_board_bench_gicp_production_public_repeated` | 5-run summary + doctor |

## Evidence Doctor 和 registry

public repeated summary 使用
`test-rvv/registration/gicp/script/generate_gicp_board_repeated_summary.py` 生成 manifest，
再运行 `../../script/evidence_doctor.py`。如果出现 Error，先修复或降级证据；Warning 必须在
result 和 evaluation 中解释；Suggestion 进入 roadmap 或下一阶段。

## 板卡复跑预算和决策桶

- default runs：5。
- warmup：public bench 当前 `--warmup-iterations 2`。
- positive：median B/A >= `1.05x` 且 0/5 B/A < 1。
- weak-positive：median B/A >= `1.02x` 且没有明显 checksum / doctor Error。
- neutral：`0.98x <= median B/A < 1.02x` 或 near-threshold warning 主导。
- negative：median B/A < `0.98x`。
- unstable：正负方向混杂且复跑预算耗尽。

若 1024 点为 positive / weak-positive，可追加 4096 点 3-run 或 5-run 验证扩展规模；若仍 neutral
或 negative，不继续扩大本候选。

## Diagnostic 到 Production 错配审计

| question | answer |
| --- | --- |
| evidence role | Phase 001 `dfddf-loop-dense` 是 diagnostic；本阶段目标是 production-public |
| A/B boundary | dense-row test helper -> public GICP entry |
| 当前决策问题 | RVV-vs-scalar production public GICP PointXYZ align |
| diagnostic 是否可外推到 production | 不能直接外推；只能作为允许 bounded production probe 的线索 |
| comparison-boundary / baseline mismatch 风险 | 有，dense arrays 不含 PointCloud indexed gather、public optimizer 和 solver 外围 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段只有 Phase 001 positive 才继续；若 production public neutral / negative 则不建议采纳 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 若保留 cost-only 与 dfddf 同时启用，需要解释两个 family 的叠加边界；最终采纳仍需用户确认 |

## 文档更新清单

完成后更新：

- `doc/phases/002-dfddf-production-probe/result.zh.md`
- `doc/phases/README.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/gicp-evaluation.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/optimization-evidence.zh.md`

`doc-rvv/registration/gicp-RVV.zh.md` 只有在 PI5 通过且用户确认采纳后才创建或更新。

## 继续 / 停止条件

继续：QEMU correctness 通过且板卡可用时，跑到 public repeated summary 和 Evidence Doctor。

停止并等待用户：production public 证据完成后，无论 positive 还是 neutral / negative，都保留当前 patch，
报告 diff、命令、summary、doctor 和建议，由用户确认采纳、回滚或继续扩展。
