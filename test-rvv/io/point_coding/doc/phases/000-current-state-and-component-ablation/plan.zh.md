# Phase 000：当前状态与 point coding 组件消融计划

## 阶段意图和边界

本阶段启动 `include/pcl/compression/point_coding.h` 的函数级评估，只做 component ablation（组件消融，隔离局部计算成本），不修改 production（生产源码），不把结论外推到 `OctreePointCloudCompression` 的完整 public entry（公开入口）。

当前 validated_scope（计划验证范围）是 `PointCoding::encodePoints/decodePoints` 的 leaf-level 坐标差分、`[-127,127]` clamp（截断）和 decode 公式；row source 是 source-indexed leaf（由 leaf 内 indices 指定输入点）；点类型先用 `pcl::PointXYZ` 代表 `x/y/z` AoS（结构数组）布局；`Scalar` 固定为现有源码里的 float 输出和 double reference point 输入。

当前 unvalidated_scope（未验证范围）包括完整 octree traversal（八叉树遍历）、entropy context（熵编码上下文）、leaf size 真实分布、`push_back` 与 coder vector 的整体内存成本、其它 `PointT` 字段布局、非 `PointXYZ` 点型、以及生产 public entry 的 fallback（回退路径）和板卡端到端收益。

## 当前状态清单

| area | 状态 |
| --- | --- |
| production 源码 | `io/include/pcl/compression/point_coding.h` 只有标量循环；本阶段不改。 |
| topic test 资产 | 尚不存在，本阶段按 `test-rvv/io/point_coding` 新建。 |
| evaluation 文档 | 尚不存在，本阶段先用 phase plan 冻结 S2 事实，后续补 `doc/point_coding-evaluation.zh.md`。 |
| roadmap / matrix | 尚不存在，本阶段创建后维护。 |
| board 证据 | 用户说明板卡可用；本阶段 bench 前先完成 QEMU correctness 和日志形状。 |

## 假设与候选族

| candidate family | 计划验证问题 | 风险 |
| --- | --- | --- |
| scalar same-chain reference | 复刻当前源码公式，作为测试和 bench 的 A/B baseline（基线）。 | 若复刻漏掉 `static_cast<int>` 截断语义，会误报 RVV 正确性。 |
| RVV gather encode candidate | 使用 indexed gather（离散加载）读取 `x/y/z`，执行量化和 clamp。 | leaf trip count 可能短，gather 和写回成本可能吞掉收益。 |
| RVV contiguous decode candidate | diff vector 连续读取，连续写回 `x/y/z`。 | decode 可能正向，但不能证明 encode 或完整 octree 正向。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| RVV gather encode | source-indexed leaf | `PointXYZ` / float output / AoS | test-only component helper | `run_test_compare` | planned `run_board_bench_compare` after bench exists | planned | planned | planned | planned | 写 RED test，补 candidate，实现 bench。 |
| RVV contiguous decode | contiguous output segment | `PointXYZ` / float output / AoS | test-only component helper | `run_test_compare` | planned `run_board_bench_compare` after bench exists | planned | planned | planned | planned | 写 RED test，补 candidate，实现 bench。 |

## 实现和测试动作

1. 写最小 `Makefile` 和 correctness test，先让测试因 candidate API 缺失失败，满足 test-driven development（测试驱动开发）RED 步。
2. 新增稳定聚合头 `include/point_coding.h` 和内部 helper，补标量 reference 与 RVV candidate。
3. 运行 `make run_test_compare`，确认 Std/RVV 构建均通过。
4. 新增 bench 入口和板卡目标后，只在 QEMU 跑窄范围 smoke（日志形状），性能结论在板卡跑。
5. 生成或人工记录 Evidence Doctor（证据体检）结果后，再写 phase result、roadmap、matrix 和 Handoff。

## Evidence Doctor 和 registry 规则

本阶段初始 RED/正确性阶段不生成性能结论。bench 和 board summary 生成后，必须使用 topic-local manifest wrapper 或全局 `test-rvv/script/evidence_doctor.py` 检查；如果 manifest 尚未补齐，result 必须写 `metadata_incomplete` warning，不能把单次 bench 写成生产性能结论。当前 evidence registry 尚未接入，恢复时人工检查 `log/qemu` 和 `log/board` 是否有未登记覆盖。

## 板卡复跑预算和决策桶

首个性能阶段采用 bounded rerun budget（有界复跑预算）：默认 5-run repeated board；若 speedup 稳定大于 1.20 记为 `positive`，1.05 到 1.20 记为 `weak_positive`，0.95 到 1.05 记为 `neutral`，低于 0.95 记为 `negative`，跨桶摇摆记为 `unstable`。预算耗尽后不无限复跑。

## 继续 / 停止条件

默认继续到 correctness、bench、QEMU smoke、asm、board repeated summary 和 Evidence Doctor 均形成可审查结果。只有 production 权限扩大、板卡不可达、工具失败、证据矛盾或 dirty isolation 不安全时才停止。

## 文档更新清单

本阶段后续需补齐 `README.zh.md`、`doc/point_coding-evaluation.zh.md`、`doc/optimization-roadmap.zh.md`、`doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md` 和本 phase `result.zh.md`。当前不创建 `doc-rvv/io/point_coding-RVV.zh.md`，因为没有 adopted production behavior（已采用生产行为）。

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | diagnostic（诊断）/ component ablation。 |
| A/B boundary | test helper（测试 helper），不是 public overload。 |
| 当前决策问题 | RVV-vs-scalar component viability（局部组件是否值得继续）。 |
| diagnostic 是否可外推到 production | no；只能说明 point coder 局部公式是否有收益线索。 |
| comparison-boundary / baseline mismatch 风险 | low for same-chain helper；high for full octree，因为 traversal、entropy 和 leaf distribution 不在计时边界内。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no；若局部 component 都不正向，默认先不进入 combined octree production probe。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes；本阶段不做 clean adoption。 |
