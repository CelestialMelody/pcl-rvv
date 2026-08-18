# Phase 000: current-state-and-gaps 计划

## 阶段意图和边界

本阶段为 `registration/include/pcl/registration/bfgs.h` 建立 RVV topic（主题）的起点。目标是先回答 BFGS（Broyden-Fletcher-Goldfarb-Shanno，拟牛顿无约束优化方法）在 PCL registration（配准）模块中是否有值得诊断的局部热点，而不是直接修改 production（生产源码）头文件。

本阶段覆盖：

- 目标源码：`registration/include/pcl/registration/bfgs.h`。
- topic-local 文档：`test-rvv/registration/bfgs/doc/` 和 `doc/phases/`。
- 初始结论：`diagnostic`（诊断）计划中；production 源码保持不变。

本阶段不覆盖：

- 不修改 `registration/include/pcl/registration/bfgs.h`。
- 不进入 production integration loop（生产接入闭环）。
- 不声称 GICP end-to-end（端到端）性能收益；不把 NDT 写成当前 caller。
- 不把 Eigen（线性代数库）内部的现有自动向量化当作 RVV 生产证据。

## 当前状态清单

| 对象 | 当前状态 | 路径 / 证据 | 本阶段动作 |
| --- | --- | --- | --- |
| second-pass 状态 | `bfgs` 是第 12 个建议优化 topic，状态为待评估，限定为局部诊断 | `doc-rvv/library-screening/registration/registration-module-second-pass.zh.md` | 更新为 evaluation 已建 / Phase 000 计划已建 |
| production 源码 | BFGS 类模板和 `Eigen::PolynomialSolver<Scalar, 2>` 特化都在同一头文件内 | `registration/include/pcl/registration/bfgs.h` | 只读源码并记录标量路径 |
| topic-local scaffold | 不存在 | `test-rvv/registration/bfgs/` | 新建 README、evaluation、phase index、roadmap 和 matrix |
| test / bench | 不存在 | `test-rvv/registration/bfgs/src/` 未创建 | 本阶段只规划，不实现 |
| QEMU / asm / board 证据 | 不存在 | `test-rvv/registration/bfgs/log/` 未创建 | 标为 missing，不形成性能结论 |
| `doc-rvv` 长期主题文档 | 不适用 | `doc-rvv/registration/bfgs-RVV.zh.md` 不应新建 | 保持 not_applicable |

## 假设与候选族

| candidate family | 假设 | 风险 / 未知 | 本阶段处理 |
| --- | --- | --- | --- |
| Eigen vector expression baseline | `x_alpha = x0 + alpha * p`、`dot()`、`norm()`、`p += ...` 已由 Eigen 表达式承载，编译器可能自动生成 RVV | 需要反汇编才能区分 Eigen 自动向量化和手写 RVV 的空间 | 列入 roadmap，下一阶段可做 compile / asm probe |
| direction-update fused diagnostic | `minimizeOneStep()` 中 `dx0`、`dg0`、多个 dot/norm 和方向更新是当前文件最像批量向量运算的区域 | 每次优化迭代只处理参数维度，常见维度可能很小；收益可能被 functor `fdf()` 和 line search 稀释 | 作为首个诊断候选，不进 production |
| line-search scalar control | `lineSearch()`、`interpolate()`、`PolynomialSolver` 以标量控制流和函数回调为主 | 不适合作为 RVV 主路径；改动风险高 | 标记 `not_applicable`，只保留数值语义审计 |
| GICP caller hotspot audit | GICP 是否真的受 BFGS 局部向量状态更新影响，需要上游调用频率或 profile | 没有 caller workload 时无法判断 topic 是否值得继续；NDT 当前没有 direct caller 证据 | 下一阶段可做 GICP caller-shaped smoke（调用方形态小型验证）或 profile 计划 |

## 优化矩阵

| candidate family | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Eigen vector expression baseline | `moveTo()`、`slope()`、`minimizeInit()`、`minimizeOneStep()` 内 Eigen 向量表达式 | planned | planned | missing | planned | missing | `planned_diagnostic` |
| direction-update fused diagnostic | `minimizeOneStep()` BFGS update block | planned | planned | missing | planned | missing | `planned_diagnostic` |
| line-search scalar control | `lineSearch()` / `interpolate()` | not_applicable with evidence：标量分支、回调和二次/三次插值为主 | not_applicable | not_applicable | not_applicable | not_applicable | `not_rvv_target` |
| GICP caller hotspot audit | confirmed GICP BFGS 调用侧 | planned only，需后续 workload | planned only | missing | not_applicable | missing | `deferred_until_caller_evidence` |

## 实现和测试动作

| action | 产物 | 完成判据 | 依赖 |
| --- | --- | --- | --- |
| 建立 topic-local scaffold | README、evaluation、phase README、roadmap、matrix、doc-suite skeleton | 文件存在，路径符合 `artifact_layout`；内容区分诊断和 production 边界 | 本计划 |
| 写 S2 函数级评估 | `doc/bfgs-evaluation.zh.md` | 写清公开入口、标量路径、可 RVV 化片段、不可 RVV 化片段和初步生产判断 | 本计划 |
| 建立 roadmap / matrix | `doc/optimization-roadmap.zh.md`、`doc/phases/optimization-matrix.zh.md` | matrix 中 planned / not_applicable / deferred 不伪装成 adopted | 本计划 |
| doc-suite shape scan | README 和 doc-suite skeleton | 标明哪些文档已有内容，哪些在下一阶段随 test/bench 落实 | 本计划 |
| 更新 module second-pass 状态 | registration second-pass 表 | `bfgs` 从待评估改成 Phase 000 已建 / 诊断计划中 | evaluation 完成后 |

## Evidence Doctor 和 registry 规则

本阶段不生成 benchmark（性能测试）、board summary（板卡摘要）、checksum summary（校验和摘要）或 asm attribution（反汇编归属）证据，因此 Evidence Doctor（证据体检）为 `not_applicable`。下一阶段若新增 bench 或 asm summary，必须先生成 topic-local manifest（证据清单）或写清人工检查边界，再运行 `test-rvv/script/evidence_doctor.py` 或等价检查。

`test-rvv/registration/bfgs/log/evidence_registry.json` 本阶段不创建；Handoff 中记录 `evidence_registry_status=not_available_initial_topic`。

## 阶段完成条件

本阶段完成需要满足：

- S2 evaluation 已创建，且明确 `diagnostic` 初始判断。
- README、phase index、roadmap 和 optimization matrix 已创建。
- `doc-rvv/registration/bfgs-RVV.zh.md` 明确不适用，且未新建。
- second-pass 状态表同步到“评估已建 / 诊断计划中”。
- 当前 topic 路径的 untracked 文件被列入 artifact tracking，不误判为提交完成。

## 板卡复跑预算和决策桶

本阶段不跑板卡。下一阶段若进入 diagnostic bench，默认采用 5-run repeated board（重复板卡测试）作为第一轮预算，decision bucket（决策桶）至少区分 `positive`、`weak-positive`、`neutral`、`negative` 和 `unstable`；QEMU 只可作为 correctness（正确性）或 log-shape（日志形状）证据。

## 继续 / 停止条件

默认下一阶段是 `010-diagnostic-scaffold-and-asm-probe`：

1. 建立最小 test / bench harness，复刻 BFGS 向量状态更新和 line-search cache 行为。
2. 先跑 correctness / QEMU smoke。
3. 再做 Eigen 自动向量化和候选 diagnostic 的 asm probe。
4. 只有看到足够的 caller hotspot 或局部板卡收益，才讨论 bounded production probe（有界生产探针）。

合法停止条件：本阶段只被用户授权为“开启新 topic”，且生产实现和板卡证据都需要后续阶段；继续到 test/bench 实现会新增更多测试资产和运行成本。Handoff 必须把下一阶段入口写清。

## 文档更新清单

- `test-rvv/registration/bfgs/README.zh.md`
- `test-rvv/registration/bfgs/doc/bfgs-evaluation.zh.md`
- `test-rvv/registration/bfgs/doc/testing-overview.zh.md`
- `test-rvv/registration/bfgs/doc/correctness-tests.zh.md`
- `test-rvv/registration/bfgs/doc/benchmark-and-evidence.zh.md`
- `test-rvv/registration/bfgs/doc/optimization-evidence.zh.md`
- `test-rvv/registration/bfgs/doc/test-support-code-map.zh.md`
- `test-rvv/registration/bfgs/doc/optimization-roadmap.zh.md`
- `test-rvv/registration/bfgs/doc/phases/README.zh.md`
- `test-rvv/registration/bfgs/doc/phases/optimization-matrix.zh.md`

## Roadmap 同步动作

Phase 000 需要把首轮搜索空间固定为三个诊断方向：Eigen baseline asm、direction update diagnostic、caller hotspot audit。任何 production 接入方向都必须保持 `deferred_until_diagnostic_evidence`，不能在本阶段升级。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic planning only；本阶段没有性能证据 |
| A/B boundary | 尚未建立；下一阶段若建立 bench，必须区分 Eigen baseline、test-only candidate 和 production public helper |
| 当前决策问题 | 是否值得继续建立 BFGS 局部诊断；不是 production adoption |
| diagnostic 是否可外推到 production | no；BFGS 是 header-only 模板，caller 维度、functor 成本和 Eigen 自动向量化都会改变真实边界 |
| comparison-boundary / baseline mismatch 风险 | yes；Eigen 表达式、编译器自动向量化和 test-only fused candidate 可能不是同一边界 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 只有存在 caller hotspot 证据且用户授权时允许 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes；如果未来已有 Eigen 自动 RVV baseline，则手写 RVV family 必须和同 production boundary 的自动向量化或标量 helper 对比 |
