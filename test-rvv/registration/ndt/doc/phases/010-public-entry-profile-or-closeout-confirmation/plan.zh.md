# Phase 010 Plan: Public Entry Profile Or Closeout Confirmation

## 阶段意图和边界

本阶段回答：真实 `NormalDistributionsTransform::align` public entry（公开入口）运行时，是否有足够证据继续围绕 `updateDerivatives`、double `std::exp` 或导数累加数学核做 RVV 优化。

本阶段不修改 `registration/include/pcl/registration/impl/ndt.hpp`，不接入 production，不实现 double exp RVV helper，也不把 QEMU timing（QEMU 计时）写成性能结论。

## 当前状态

Phase 000 已证明 staged derivative accumulation（分阶段暂存后的导数累加）test helper 在板卡上稳定 negative：hessian median 0.320x，gradient median 0.401x。该证据不能证明真实 public entry 没有优化空间，也不能证明标量 `exp` 是唯一根因。

源码审计显示：

- `computeTransformation` 的 `std::log` / `std::exp(-0.5)` 是 Gaussian 常量初始化，通常每次 alignment 只执行一次。
- `computeAngleDerivatives` 的 `std::sin` / `std::cos` 每次 derivative pass（导数计算轮次）只处理 3 个角。
- `updateDerivatives` / `updateHessian` 中的 double `std::exp` 随 point-neighbor sample（点-邻域样本）执行，是数学函数热点候选，但当前 `rvv_math.hpp` 只有 float helper，不能直接替换。

## 候选和假设

| candidate / action | 要验证的假设 | 当前预期 |
| --- | --- | --- |
| public-entry align bench | 真实 `align()` 中导数数学核是否可能主导总耗时 | 先取得公开入口计时和可复现 checksum |
| board-side function attribution | `updateDerivatives` / `updateHessian` / neighbor search / solver 的相对占比 | 若工具可用，作为继续 math helper 的前置证据 |
| double exp RVV helper | 只有 profile 显示 double `exp` 或导数核占比足够高时才进入 | 本阶段只记录 gap，不实现 |

## 实现和测试动作

1. 在 topic-local `src/bench_ndt.cpp` 增加 public-entry `align()` case，构造确定性 `PointXYZ` source / target 点云，使用真实 `NormalDistributionsTransform<PointXYZ, PointXYZ>`。
2. 扩展 bench 参数，支持 public case 的点数、最大迭代次数、分辨率和 search method（邻域搜索方式）选择。
3. QEMU 只跑 log-shape smoke（日志形状冒烟），确认 case 可执行、输出可解析。
4. 板卡上跑有界 public-entry bench / profile。若板卡缺少 profile 工具，记录为工具 blocker（阻塞项），不伪造内部热点结论。

## 证据和完成条件

| evidence | 完成条件 | 结论边界 |
| --- | --- | --- |
| QEMU smoke | public case 能运行并输出 checksum | 只证明构建和日志形状 |
| board public bench | 至少 3 次 repeated 或等价有界预算 | 只说明公开入口计时，不等于内部热点归因 |
| function attribution | `perf` / 等价采样能给出函数占比 | 用于决定是否进入 double exp helper 或 fused formula |

若 public-entry profile 显示热点主要在 neighbor search、voxel grid、line search 或 solver，本 topic 停止当前 NDT RVV 接入建议。若 `updateDerivatives` / double `exp` 占比明确且足够高，下一 phase 才进入 `rvv-math-vectorization` 或 fused formula RVV-vs-RVV A/B。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-public profile / public-entry bench |
| A/B boundary | public overload，production source read-only |
| 当前决策问题 | 是否继续 RVV 搜索，不是 clean adoption |
| diagnostic 是否可外推到 production | public-entry 计时可说明端到端成本；函数采样只说明热点归属，不能替代 production RVV patch 证据 |
| comparison-boundary / baseline mismatch 风险 | 有；当前 production 没有 NDT RVV patch，Std/RVV build 差异可能来自编译宏、Eigen 或其它库，不代表 NDT RVV family |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 默认不允许；除非 profile 明确导数核占比高且用户授权 PI1 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要；本阶段不会 clean adopt |

## 板卡复跑预算和停止条件

预算：public-entry bench 初始 3 runs；若 decision bucket（决策桶）摇摆，再最多补 2 runs。profile 工具若不可用或采样符号不可读，本阶段停止在 `profile_tool_blocked`，并只保留 public-entry bench 证据。

合法停止条件：

- public-entry profile 显示导数数学核不是主要热点。
- 板卡 profile 工具不可用，且继续需要 production 插桩授权。
- public-entry bench 不稳定且预算耗尽。
- 用户要求停止或进入 PI1 前确认点。

## 文档更新清单

完成后更新本 phase result、optimization matrix、roadmap、benchmark/evidence、optimization evidence 和 evaluation。`doc-rvv` 仍不适用，除非用户之后明确进入 production integration loop 并完成 PI5。
