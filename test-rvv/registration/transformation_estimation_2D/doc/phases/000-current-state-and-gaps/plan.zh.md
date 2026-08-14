# Phase 000 Plan: current-state-and-gaps

本阶段启动 `registration/transformation_estimation_2D` 的 RVV topic（主题）。目标是建立 S2 evaluation（函数级评估）和 S4 test plan（测试计划），不修改 production（生产源码），不写入长期 `doc-rvv` 主题文档。

## 阶段意图和边界

| 项目 | 本阶段范围 |
| --- | --- |
| 目标源码 | `registration/include/pcl/registration/impl/transformation_estimation_2D.hpp` 和公开声明头 `registration/include/pcl/registration/transformation_estimation_2D.h`。 |
| production 边界 | 本阶段只做源码 shape scan（形态扫描）和证据计划，不修改 production。 |
| topic 资产边界 | 创建 `test-rvv/registration/transformation_estimation_2D/` 下的 evaluation、roadmap、phase index 和 matrix。 |
| row source policy（行来源策略） | ordered-cloud-pair（顺序点云对，source/target 按相同下标一一对应）、source-indexed-cloud-pair（源索引点云对）、dual-indexed-cloud-pair（双索引点云对）、correspondence-pair（对应关系点对）全部先进入评估矩阵。 |
| 点类型 / Scalar 边界 | 先以 `PointXYZ` / `PointXYZ`、`Scalar=float` 作为最小正确性诊断入口；泛型点型和 `Scalar=double` 只列入后续证据需求。 |
| 不证明什么 | 不证明性能，不证明 production direct（真实生产路径证据），不批准 production RVV 分流。 |

## 当前状态清单

| 对象 | 当前状态 | 证据路径 |
| --- | --- | --- |
| second-pass 队列表 | `transformation_estimation_2D` 为建议优化队列第 9 项，状态为待评估。 | `doc-rvv/library-screening/registration/registration-module-second-pass.zh.md` |
| 目标源码 | 四个公开 overload 统一构造 `ConstCloudIterator`，protected helper 做 centroid、demean、correlation、angle 和 matrix。 | `registration/include/pcl/registration/impl/transformation_estimation_2D.hpp` |
| 上游专项测试 | 仓库中未发现直接命中 `TransformationEstimation2D` 的上游测试。 | `rg TransformationEstimation2D` 只命中 production 头。 |
| topic 目录 | 本阶段开始前不存在同名 `test-rvv/registration/transformation_estimation_2D` 资产。 | `git ls-files` / untracked scan 无输出。 |
| common 层 RVV | `compute3DCentroid` 和部分 `demeanPointCloud` 对普通 cloud 输入有 RVV 路径；`ConstCloudIterator` overload 仍是标量循环。 | `common/include/pcl/common/impl/centroid.hpp` |
| 相邻成熟 topic | point-to-plane LLS 已采用 `src/`、`include/`、`include/impl/`、topic-local doc suite 和 phase loop。 | `test-rvv/registration/transformation_estimation_point_to_plane_lls/` |

## 假设与候选族

| candidate family | 假设 | 本阶段处理 |
| --- | --- | --- |
| scalar-baseline characterization | 当前 protected helper 可能被动态 `Eigen::Matrix` demean 和 correlation 乘法主导。 | 记录标量流程、输入语义和测试计划。 |
| fused 2D correlation accumulator | 可直接构造 2x2 correlation，避免两份 demean 动态矩阵和 Eigen 乘法。 | 列为 Phase 010 诊断候选，先不实现。 |
| dense ordered-cloud-pair RVV path | 对 f32 AoS ordered-cloud-pair，可用 stride load（跨步加载）做 x/y 质心和中心化 correlation。 | 需要 correctness、asm 和 board 证据后才可能进入 production integration loop（生产接入闭环）。 |
| indexed / correspondences gather path | 索引入口需要 gather（离散加载）和 row-order 保持，收益不明。 | 放入 matrix；默认先做 ordered-cloud-pair，再做 family carry-over audit（实现族迁移审计）。 |
| common RVV reuse audit | common 层 `compute3DCentroid` / `demeanPointCloud` 现有 RVV 路径可能已经覆盖部分成本，但 iterator overload 隐藏了入口形态。 | 在 evaluation 中把复用 common RVV 与专用 2D fused path 分开列证据需求。 |

## 优化矩阵草案

| candidate family | row source policy | point type / Scalar / layout | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| scalar baseline | all policies | generic `PointSource` / `PointTarget`, `Scalar=float/double` | planned | planned | missing | not_run | not_run | current production scalar |
| fused correlation diagnostic | ordered-cloud-pair | `PointXYZ -> PointXYZ`, `Scalar=float`, dense finite corpus | planned | planned | missing | planned | not_run | planned Phase 010 |
| fused correlation diagnostic | source-indexed-cloud-pair | valid-index-only `PointXYZ`, `Scalar=float` | planned | planned | missing | planned | not_run | deferred until ordered-cloud-pair evidence |
| fused correlation diagnostic | dual-indexed-cloud-pair | valid dual index lists | planned | planned | missing | planned | not_run | deferred until ordered-cloud-pair evidence |
| fused correlation diagnostic | correspondence-pair | valid query / match pairs | planned | planned | missing | planned | not_run | deferred until ordered-cloud-pair evidence |
| production dispatch | ordered-cloud-pair | layout-gated f32 AoS, `Scalar=float` | not_yet_covered | not_yet_covered | missing | not_yet_covered | not_run | not_applicable in Phase 000 |

## 实现和测试动作

| 动作 | 产物 | 完成判据 |
| --- | --- | --- |
| A1 S0 偏好冻结 | `tmp/rvv-work-logs/.../s0.yaml` 和 `s0.zh.md` | `preferences_loaded`、冻结策略、产物路径和 dirty isolation 明确。 |
| A2 源码 shape scan | evaluation 的“标量流程与 RVV 边界”章节 | 四个公开入口、iterator 统一层、centroid / demean / correlation / trig 边界写清。 |
| A3 topic-local evaluation | `doc/transformation_estimation_2D-evaluation.zh.md` | S2 函数级评估包含 Traceability Map（可追踪性地图）、实现方式审计和测试计划。 |
| A4 optimization roadmap | `doc/optimization-roadmap.zh.md` | 候选搜索空间、暂缓 / 拒绝路线和默认恢复动作可恢复。 |
| A5 optimization matrix | `doc/phases/optimization-matrix.zh.md` | row source、candidate、test、bench、board、asm、doctor 和 decision 状态可审查。 |
| A6 phase index | `doc/phases/README.zh.md` | 下一轮能从 Phase 010 恢复。 |
| A7 current handoff | `tmp/rvv-work-logs/.../current-handoff/current-handoff.zh.md` | Handoff 包含 S0、质量门禁、风险和下一步。 |

## Evidence Doctor 和 registry 规则

本阶段不生成 benchmark、board summary、checksum summary、asm attribution 或 EvidenceDecision，因此不运行脚本化 Evidence Doctor（证据体检）。Handoff 中需要写 `not_run` 和人工检查结论：当前只有计划和文档，没有可用于性能或生产接入的证据。

`log/evidence_registry.json` 当前不创建。Phase 010 如果新增 QEMU / board summary 或可提交摘要，再接入 registry（证据登记表）。

## 板卡复跑预算和决策桶

本阶段不运行板卡。Phase 010 如果进入 bench 诊断，计划默认使用：

| 项目 | 默认值 |
| --- | --- |
| warm-up | 1 轮，或 topic bench wrapper 显式配置。 |
| run budget | 首次 repeated board 5 run；若 decision bucket（决策桶）与 Evidence Doctor warning 冲突，最多 1 次同边界确认复跑。 |
| positive | median 和 min 均明显大于 1，且 `B/A < 1` 频率为 0。 |
| weak_positive | median 稳定大于 1，但 min 或部分规模接近 1。 |
| neutral / unstable | median 接近 1 或跨 run 摇摆。 |
| negative | median 和多数 run 小于 1。 |

## 继续 / 停止条件

| 条件 | 决策 |
| --- | --- |
| 本阶段只建立 topic 文档和恢复入口 | 可以在 S4 边界输出 Handoff。 |
| Phase 010 诊断需要新增 test / bench scaffold | 属于当前 topic 授权范围内的下一阶段，默认恢复动作是创建 scaffold 和最小 correctness 诊断。 |
| 继续到 production patch | 需要 Phase 010/020 证据支持 production-ready 或 partial-production-candidate，并进入 PI1 计划。 |
| 继续需要修改无关 topic 或 `.agents` | 停止并报告，不在本阶段执行。 |

## 文档更新清单

本阶段写入：

```text
test-rvv/registration/transformation_estimation_2D/README.zh.md
test-rvv/registration/transformation_estimation_2D/doc/transformation_estimation_2D-evaluation.zh.md
test-rvv/registration/transformation_estimation_2D/doc/optimization-roadmap.zh.md
test-rvv/registration/transformation_estimation_2D/doc/phases/README.zh.md
test-rvv/registration/transformation_estimation_2D/doc/phases/optimization-matrix.zh.md
tmp/rvv-work-logs/registration/transformation_estimation_2D/current-handoff/current-handoff.zh.md
```

`doc-rvv/registration/transformation_estimation_2D-RVV.zh.md` 当前不适用，不创建。

## roadmap 同步动作

本阶段创建 roadmap，并把 Phase 010 默认恢复动作设为：

```text
010-scaffold-and-ordered-cloud-pair-correlation-diagnostic
```

Phase 010 应先建立 `src/`、`include/`、`include/impl/` 和 Makefile scaffold，再实现 ordered-cloud-pair same-chain correctness（同构链路正确性）与 bench build / QEMU log-shape smoke。性能结论必须等待板卡或目标硬件。
