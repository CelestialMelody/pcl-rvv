# Phase 000 计划：当前状态与规则网格采样诊断

## 阶段意图和边界

本阶段从 `doc-rvv/library-screening/surface/surface-function-evaluation-queue.zh.md` 的 on_nurbs triangulation 建议项进入函数级评估。目标是判断 `surface/src/on_nurbs/triangulation.cpp` 中未裁剪 NURBS surface 的规则网格参数写入和 `Evaluate` 扫描是否值得继续做 RVV（RISC-V Vector，可变长度向量扩展）生产接入。

本阶段覆盖 `createVertices` 风格的参数网格写入、`convertSurface2PolygonMesh` 和 `convertSurface2Vertices` 的未裁剪 surface 路径。`createIndices` 先作为标量输出构造审计和 checksum（校验和）对象，不把 `pcl::Vertices` 的小 vector 分配写成 RVV 收益来源。`convertTrimmedSurface2PolygonMesh`、`isInside`、curve sampling 和 curve-on-surface sampling 本阶段只做源码边界记录，因为它们混入 inverse mapping（反向参数映射）和 curve/surface 双重 `Evaluate`。

不修改 production 源码。所有候选实现先放在 `test-rvv/surface/triangulation`，证据角色为接入前诊断。若板卡结果显示规则参数写入被 `ON_NurbsSurface::Evaluate` 主成本稀释，本阶段可以收敛为 no-production（不接入生产）或只保留诊断；若 full-path（完整路径）仍稳定正向，再进入后续 production integration loop（生产接入闭环）计划。

## S0 偏好和产物解析

| 字段 | 本阶段冻结值 |
| --- | --- |
| `preferences_loaded` | defaults 已读取；`.agents/local/user-preferences.yaml` 不存在；prompt override 指定 topic、持续推进和板卡可用。 |
| 注释策略 | 测试资产、diagnostic 和 prototype 使用中文详细注释；production 注释若后续出现，只保留维护边界。 |
| 文档策略 | closeout 当前状态优先；长期 `doc-rvv` 只在 production 行为被用户确认采纳后适用。 |
| 证据策略 | `summary-only`；raw log 默认不提交。 |
| commit 策略 | 用户未要求提交，本阶段不创建 commit。 |
| instruction feedback | `report-only`，除非发现可复用规则缺口，否则不修改 `.agents`。 |

解析出的 topic 产物：

| 产物 | 路径 | 发布边界 |
| --- | --- | --- |
| topic test dir | `test-rvv/surface/triangulation` | 当前 topic 测试资产 |
| evaluation | `test-rvv/surface/triangulation/doc/triangulation-evaluation.zh.md` | 可审查 topic-local 文档 |
| phase root | `test-rvv/surface/triangulation/doc/phases` | 阶段计划 / 结果 |
| optimization matrix | `test-rvv/surface/triangulation/doc/phases/optimization-matrix.zh.md` | 阶段矩阵 |
| roadmap | `test-rvv/surface/triangulation/doc/optimization-roadmap.zh.md` | 跨阶段候选 |
| evidence registry | `test-rvv/surface/triangulation/log/evidence_registry.json` | 本阶段可先人工记录，脚本化登记可作为后续动作 |

dirty isolation（脏工作区隔离）：本阶段只允许修改 `test-rvv/surface/triangulation/**` 和必要的 surface 队列表状态。仓库中已有 registration、bilateral_upsampling、marching_cubes 等未提交修改均视为无关 topic，忽略且不回滚。

## 当前状态清单

| 对象 | 当前状态 | 本阶段处理 |
| --- | --- | --- |
| `surface/src/on_nurbs/triangulation.cpp` | production 标量实现；未见 `__RVV10__` 分支。 | 只读源码，不改 production。 |
| `test/surface/test_on_nurbs.cpp` | 上游测试通过 fitted surface 调用 `convertSurface2PolygonMesh` 和 `convertSurface2Vertices`，依赖外部 PCD 输入。 | 作为 public caller（公开调用方）语义参考，不直接复用其大输入路径。 |
| `test-rvv/surface/triangulation` | 本阶段前不存在。 | 建立 `src/`、`include/`、`doc/` 和 board harness。 |
| 相邻成熟 topic | `organized_fast_mesh`、`marching_cubes` 已有 `src/`、`include/` 和 topic-local doc suite。 | 只采用结构成熟度，不迁移算法结论或性能数字。 |
| board availability（板卡可用性） | 用户声明板卡可用，`test-rvv/config.mk` 存在。 | 本阶段计划 board smoke 和 repeated bench；若工具失败再记录真实 blocker。 |

## 标量路径重建

`convertSurface2PolygonMesh` 先检查 u/v 两个 knot vector（节点向量）是否有效，然后按 knot 范围生成 `(resolution + 1)^2` 个参数点，按每个 grid cell 输出两个 triangle，最后逐点调用 `ON_NurbsSurface::Evaluate(u, v, 1, 3, point)` 并把三维结果写回 `pcl::PointXYZ`。`convertSurface2Vertices` 的参数网格和 `Evaluate` 扫描相同，只是输出 cloud 和 vertices 由调用方提供。

可向量化片段是规则参数点 `(u, v, 0)` 的写入。这里是 AoS stride（结构数组跨步）写入：`PointXYZ::x/y/z` 在点结构内按固定 offset 存储，但相邻点之间有 `sizeof(PointXYZ)` 的 stride。后续 `Evaluate` 是 OpenNURBS 内部标量调用，不能由本阶段 RVV 直接接管。`createIndices` 输出 `std::vector<pcl::Vertices>`，每个三角形内部又有小 vector push，不适合作为首个 RVV 内核；可以通过 reserve（预留容量）做标量工程候选，但必须和 RVV 参数写入分开归因。

## 候选族和矩阵

| candidate family | 证据角色 | 范围 | 预期收益 | 主要风险 |
| --- | --- | --- | --- | --- |
| `param_grid_rvv_store` | 接入前诊断 | 只替换 `createVertices` 风格的规则参数点写入。 | 大规模 `resolution` 下减少循环标量乘加和 AoS 字段写入开销。 | full surface 采样可能被 `Evaluate` 主成本稀释。 |
| `index_reserve_scalar` | 接入前诊断 / 消融 | 对 polygons 和每个 triangle 的顶点 vector 做标量预分配。 | 减少分配噪声，帮助隔离 `createIndices` 成本。 | 不是 RVV 收益，不能支撑 RVV production 结论。 |
| `evaluate_batch_deferred` | 暂缓 | 尝试批处理或缓存 `Evaluate` 周边数据。 | 如果 `Evaluate` 主导，可能需要更深层优化。 | 触碰 OpenNURBS 语义和外部库边界，当前阶段不做。 |

## 实现和测试动作

| 动作 | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| 建立测试支撑结构 | `include/triangulation.h`、`src/test_triangulation.cpp`、`src/bench_triangulation.cpp`、`Makefile`、`board.mk` | 源码只 include 聚合头，内部 helper 职责清楚。 |
| 构造确定性 Evaluate-like fixture（求值替身夹具） | test support helper | 先用测试专用标量曲面求值替身覆盖 full-path 计时形状；真实 OpenNURBS direct 证据因当前 RISC-V `libpcl_surface` 缺少 on_nurbs 符号暂列工具链缺口。 |
| 实现 `param_grid_rvv_store` | `__RVV10__` 下使用 RVV strided store；非 RVV build 走标量 reference。 | correctness test 对比参数点和 full surface 输出 checksum。 |
| 建立 bench | `run_bench_*`，case-filter 覆盖 `param_grid_*` 与 `surface_eval_*` | 输出 `Dataset`、`Iterations`、`Warmup Iterations`、case 平均耗时、`Total Time` 和 checksum。 |
| QEMU correctness / asm | `make run_test_compare`、`make dump_bench_rvv` | QEMU 只证明正确性和 RVV 指令存在；asm 能归属到 test support helper。 |
| board evidence | `make deploy_board`、`make run_board_test`、`make run_board_bench_compare` 或 topic alias | 板卡日志和 repeated summary 能支撑性能 decision bucket。 |
| Evidence Doctor | 脚本化或人工 `Errors / Warnings / Suggestions` | 任何性能或 EvidenceDecision 前记录异常和降级边界。 |

## Diagnostic 到 production 错配审计

| question | answer |
| --- | --- |
| evidence role | `diagnostic`，接入前测试专用候选。 |
| A/B boundary | `test helper`，baseline 和 candidate 都在 `test-rvv/surface/triangulation`。 |
| 当前决策问题 | `RVV-vs-scalar`，先判断规则参数网格写入是否值得继续。 |
| diagnostic 是否可外推到 production | unknown。若 full-path surface 采样仍稳定正向且 fallback 简单，才能进入 production probe；单独 `param_grid` 正向不能外推。 |
| comparison-boundary / baseline mismatch 风险 | 有。`index_reserve_scalar` 会改变非 RVV 分配成本，必须作为单独消融解释。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 只有 `surface_eval_*` full-path 在板卡上至少 weak-positive 且 asm 归属闭合时才允许；若只有 param-grid 局部正向，不进入 production。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 本阶段没有已有 adopted family；若后续出现多个生产候选，需补同边界 family A/B。 |

## 板卡复跑预算和决策桶

默认先跑一次 board compare（标量 / RVV 性能对比）确认可运行，再做 `run_count=5` 的 repeated summary。若 `surface_eval_*` 的方向接近 1.0、`B/A < 1` 频率高或 Evidence Doctor 给出波动 Warning，允许同边界再复跑一次；最多两批。`speedup >= 1.20` 且方向稳定为 `positive`，`1.05 <= speedup < 1.20` 为 `weak_positive`，`0.95 <= speedup < 1.05` 为 `neutral`，`speedup < 0.95` 为 `negative`，跨批方向反转为 `unstable`。

## 文档更新清单

本阶段结束前回填：

- `000-current-state-and-grid-sampling-diagnostic/result.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/triangulation-evaluation.zh.md`
- 必要的 README、testing overview、correctness、benchmark/evidence、optimization evidence 和 code map 文档。

## 继续 / 停止条件

默认继续到测试、QEMU、asm 和板卡证据闭环。合法停止条件仅包括：生产接入需要用户确认、板卡或工具链真实失败、Evidence Doctor Error 无法修复、dirty isolation 不安全、或本阶段矩阵与 roadmap 均无授权未阻塞动作。

下一阶段默认入口：如果 full-path 板卡正向，进入 `010-production-integration-plan`；如果局部正向但 full-path 被 `Evaluate` 稀释，进入 `010-evaluate-cost-ablation-or-no-production-closeout`；如果测试支撑或 doc suite 未闭合，优先进入结构补齐 phase。
