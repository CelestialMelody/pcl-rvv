# Phase 000 Plan: current-state-and-component-ablation

## 阶段意图和边界

本阶段为 `features/include/pcl/features/impl/rops_estimation.hpp` 开启 ROPS（Rotational Projection Statistics，旋转投影统计）RVV topic。目标是先建立 component ablation（组件消融）测试面，按筛选表建议把完整 descriptor（描述子）拆成 projection / distribution matrix（投影 / 分布矩阵）/ central moments（中心矩）三块。Phase 000 只做 central moments 的 test-first scaffold（测试优先脚手架）和 same-chain correctness（同构正确性），不修改 production（生产源码），不证明完整 `ROPSEstimation::computeFeature()` 已经适合接入 RVV。

## 当前状态清单

| area | current state | path |
| --- | --- | --- |
| screening queue | ROPS descriptor projection 排在 features retained candidate 第 6 项，建议先做 projection / central moments / distribution matrix 分块消融 | `doc-rvv/library-screening/features/features-retained-candidate-rescreen.zh.md` |
| production source | `computeFeature()` 中每个 keypoint 会构建局部 surface、计算 LRF、transform、按 3 个轴和多次角度旋转，随后为 3 个 projection 生成 distribution matrix 并计算 5 个 moments | `features/include/pcl/features/impl/rops_estimation.hpp` |
| upstream test | 有 `test_rops_estimation`，依赖 `rops_cloud.pcd`、indices 和 triangles，只检查输出非空和 invalid parameter | `test/features/test_rops_estimation.cpp` |
| RVV topic assets | 本阶段新建 `test-rvv/features/rops_estimation/` | `test-rvv/features/rops_estimation` |
| production status | 未修改 production；本阶段只允许 test-rvv diagnostic asset | `not_applicable` |
| board status | 用户说明板卡可用；本阶段先跑 QEMU correctness 和反汇编，若进入 bench 再按有界预算跑板卡 | 当前会话 |

## 假设与候选族

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| central moments RVV | `computeCentralMoments()` 双层 bins loop 只有小矩阵，但每个 descriptor 会重复 `axis × rotation × projection` 次 | `number_of_bins_=5` 起步，Eigen matrix after distribution | 消除 `std::pow` 和双层标量循环中的重复表达式，形成可测试 component | bins 默认 5x5 太小，RVV setup 可能大于收益；entropy 的 `std::log` 仍是标量或需 math helper | same-chain correctness、QEMU、asm，后续板卡 component bench | planned | Phase 000 |
| distribution matrix scatter | 筛选表建议分块消融 | rotated local cloud -> matrix bin scatter | 可能比 moments 更接近主要成本 | scatter/bin 更新不规则，RVV 可能只适合辅助计算 bin index | 独立 correctness / bench / board | deferred | Phase 010 |
| projection / rotateCloud component | 筛选表和 MOI 投影规约经验 | local point transform / rotate + AABB | AoS xyz load、matrix multiply 和 min/max 有 RVV 规约机会 | LRF、mesh、local point gather 和 Eigen 矩阵状态复杂 | same-chain helper、asm、board | deferred | Phase 020 |

## 优化矩阵

本阶段矩阵主路径见 `../optimization-matrix.zh.md`。Phase 000 只能把 `central moments RVV` 从 `planned` 推进到 `attempted`、`rejected` 或 `phase_deferred + unblocked`；不能关闭完整 ROPS descriptor topic。

## 实现和测试动作

| action | artifact / command | expected evidence | completion criteria |
| --- | --- | --- | --- |
| A1 写 RED test | `src/test_rops_estimation.cpp` 调用尚不存在的 `rops::computeCentralMomentsRVV()` | `make -C test-rvv/features/rops_estimation run_test_rvv` 编译失败 | failure 来自缺少 candidate symbol，不是 Makefile 或依赖错误 |
| A2 写最小 Std/RVV helper | `include/rops_estimation.h`、`include/impl/rops_components.hpp` | `computeCentralMomentsRVV()` 在非 RVV 构建自然走 Std；RVV 构建复刻 production 公式 | `run_test_compare` 通过 |
| A3 扩展 correctness cases | `src/test_rops_estimation.cpp` | 覆盖 sparse / zero / tail-like bins shape；entropy 和 moments 与 production oracle 对拍 | Std/RVV 均通过 |
| A4 反汇编归属 | `make dump_test_rvv` | RVV build 的 test binary 中能看到当前 helper 的 RVV 指令，或明确记录编译器内联/小规模未向量化原因 | asm boundary 写入 result |
| A5 决定是否进入 bench | `result.zh.md`、roadmap、matrix | 若 central moments 小矩阵不值得单独 bench，说明理由并把 distribution matrix / projection 排入下一 phase | 不因一个 helper 通过就声明 topic 完成 |

## Evidence Doctor 和 registry 规则

Phase 000 若只做 correctness 和反汇编，不生成性能结论，Evidence Doctor（证据体检）为 `not_applicable`，但 result 必须人工写清未运行原因。若新增 bench 或 board summary，必须生成 manifest 并运行 `test-rvv/script/evidence_doctor.py`，再用 `test-rvv/script/evidence_registry.py` 登记。

## 板卡复跑预算和决策桶

Phase 000 默认不跑完整 board bench，因为 central moments 默认是 5x5 小矩阵，先用 correctness 和 asm 判断是否值得做 component bench。若本阶段新增 bench，板卡预算为 5 run，decision bucket 为：`positive >= 1.20x`，`weak-positive 1.05x-1.20x`，`neutral 0.95x-1.05x`，`negative < 0.95x`，跨桶摇摆为 `unstable`。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic（诊断）；test helper component boundary（测试 helper 组件边界） |
| A/B boundary | test helper：production private helper oracle vs test-only candidate |
| 当前决策问题 | implementation-shape；先判断 central moments 是否值得作为 RVV family 的首个组件 |
| diagnostic 是否可外推到 production | no。它只覆盖 distribution matrix 之后的小矩阵 moments，不覆盖 mesh local surface、LRF、rotateCloud、distribution matrix scatter、descriptor normalization 或 public `compute()` |
| comparison-boundary / baseline mismatch 风险 | yes。production 调用频率高但单次矩阵小，helper-only 成本和完整 descriptor 成本可能不一致 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no for central moments alone；若 central moments 不成立，仍可继续 distribution matrix 或 projection 消融 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes，任何 production 采纳都必须等 production direct 证据和用户确认 |

## Phase scope 与扩展队列

| scope type | 内容 |
| --- | --- |
| `validated_scope` | `pcl::PointXYZ` / `pcl::Histogram<135>` 作为 production oracle 实例；`Eigen::MatrixXf` 5x5 central moments component；float moments |
| `unvalidated_scope` | 完整 `computeFeature()`、LRF、transform、rotateCloud、distribution matrix scatter、feature normalization、其它点型、`number_of_bins_` 非默认规模、production dispatch |
| `point_type_expansion_queue` | 不适用于 Phase 000；若后续进入 production，必须为 `PointXYZI`、`PointXYZRGBA`、`PointNormal` 和 PointXYZ-like traits 单独建 phase |
| `phase_closeout_boundary` | 只能关闭 central moments component 的 correctness / asm 条目，不能关闭 ROPS topic |

## Topic maturity audit

| area | current shape scan | decision | next action |
| --- | --- | --- | --- |
| production boundary | 未接 RVV，公开入口是 `Feature::compute()` -> private `computeFeature()`；内部状态包含 mesh triangles、KdTree、LRF、Eigen 和 descriptor normalization | `not_applicable with evidence` for production patch | Phase 000 不改 production |
| RVV test support architecture | 当前 topic 没有 test-rvv 资产 | `applied` | 新建 `src/`、`include/`、`include/impl/`、doc/phases |
| target granularity | 当前 topic 没有 Makefile | `phase_deferred + unblocked` | 本阶段补 `run_test_compare` 和 `dump_test_rvv`；bench / board target 在 component 值得测时补 |
| topic-local doc suite | 当前 topic 没有 README、evaluation、roadmap、matrix | `phase_deferred + unblocked` | 本阶段建立最小 evaluation / roadmap / matrix / phase result |
| evidence freshness | 当前 topic 没有 registry | `not_applicable with evidence` for correctness-only phase | 若生成 summary evidence 再补 registry |

## 经验迁移审计

| sibling 经验维度 | sibling topic 里的机制 | 当前 topic 是否适用 | 状态 | 证据 / 理由 | 下一步 |
| --- | --- | --- | --- | --- | --- |
| structure maturity | MOI topic 使用 `src/`、`include/`、`include/impl`、phase docs、roadmap 和 matrix | 适用 | applied | ROPS 是复杂 features topic，reviewer 需要同等测试资产入口 | Phase 000 新建同类结构 |
| fused indexed reduction | MOI 的 xyz gather + reduction 在 projection / covariance 中正向 | 部分适用 | deferred | ROPS central moments 输入是小矩阵，不是 indexed xyz cloud；projection / rotateCloud 才可能复用 | 后续 Phase 020 审计 |
| evidence model | MOI 使用 correctness、asm、board repeated、Evidence Doctor 分层 | 适用但裁剪 | applied | Phase 000 先 correctness + asm；bench 证据不强行虚构 | result 回填 |
| production boundary | MOI 已有 production 接入经验 | 暂不适用 | rejected | ROPS 当前还没有 component evidence，且完整路径被 mesh/Eigen/bin scatter 稀释 | 不进入 production |

## 文档更新清单

本阶段创建或更新：

- `test-rvv/features/rops_estimation/doc/rops_estimation-evaluation.zh.md`
- `test-rvv/features/rops_estimation/doc/optimization-roadmap.zh.md`
- `test-rvv/features/rops_estimation/doc/phases/README.zh.md`
- `test-rvv/features/rops_estimation/doc/phases/optimization-matrix.zh.md`
- `test-rvv/features/rops_estimation/doc/phases/000-current-state-and-component-ablation/result.zh.md`

`doc-rvv/features/rops_estimation-RVV.zh.md` 本阶段不适用，因为没有最终生产行为、用户确认保留的 production patch（生产补丁）或 PI5 生产证据闭环。

## 继续 / 停止条件

默认继续完成 A1-A5。合法停止条件只包括：交叉工具链 / 依赖不可用、QEMU correctness 无法构建、production oracle 语义不可访问、dirty isolation 不安全、板卡目标不可达且本阶段已经需要板卡，或后续 production 接入需要用户确认。完成 central moments helper 后，若仍有未阻塞的 distribution matrix / projection 消融动作，不能声明整个 topic `ready_for_review`。
