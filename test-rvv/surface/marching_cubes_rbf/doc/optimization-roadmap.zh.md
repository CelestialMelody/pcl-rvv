# Optimization Roadmap

## 当前边界

本 topic 已完成 `020-generic-normal-point-types` 阶段。当前 production patch（生产补丁）位于
`surface/include/pcl/surface/impl/marching_cubes_rbf.hpp`，真实 `MarchingCubesRBF<PointNT>::voxelizeData()`
在以下条件同时成立时命中 RVV（RISC-V Vector，可变向量扩展）路径：

- `__RVV10__` 构建；
- `input_->size() >= 16`；
- `pcl::rvv::RVVXYZNormalFloatLayout<PointNT>::value` 为 true，即 PCL traits（点类型字段特征）证明
  xyz + normal 都是单 `float` 字段，并满足当前 AoS（结构数组）布局前提。

本阶段已经用 `pcl::PointNormal`、`pcl::PointXYZINormal`、`pcl::PointXYZRGBNormal` 三个代表点型完成
production direct correctness（真实生产路径正确性）、asm attribution（反汇编归属）、板卡 production bench
和 Evidence Doctor（证据体检）。当前状态是 `adopted_production_behavior`。正式长期文档为
`doc-rvv/surface/marching_cubes_rbf-RVV.zh.md`。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| column-major RVV matrix fill | 当前源码 + Eigen column-major 写入事实 | RBF matrix fill | 降低 `4N^2` cubic kernel 成本 | 单独组件收益不能替代 production direct | correctness、asm、board matrix/full pipeline、production direct | adopted | monitor via production direct |
| RVV voxel reduction | 当前源码 voxel eval 内层 | grid evaluation | 降低 `voxels * 2N` kernel/reduction 成本 | vector reduction（向量规约）改变累加树，需要测试容差约束 | correctness、数值误差、asm、board full pipeline、production direct | adopted | monitor via production direct |
| traits-gated normal AoS production path | phase 020 + generic point type strategy | `RVVXYZNormalFloatLayout<PointNT>` 可表达的 normal-like AoS 点型 | 扩大模板入口覆盖面，减少 exact-type gate | 性能证据只覆盖代表点型；自定义点型只由 traits gate 证明布局 | QEMU 7/7、board 7/7、asm 三点型实例、board production bench、Evidence Doctor | adopted_production_behavior | no code action |
| richer repeated summary | Evidence Doctor `low_run_count` warning | production direct bench evidence | 降低 weak-positive 证据的不确定性 | 需要 analyzer 输出 per-iteration B/A values 和环境 metadata | repeated board manifest、binary hash、taskset/governor/freq/temperature | deferred | `030-production-repeated-values-summary` only if stronger stability is required |
| concrete custom normal-like point type | 泛型 gate 边界 | 用户指定或仓库常见自定义 normal 点型 | 验证 traits gate 对具体实例的编译 / 性能边界 | 当前没有具体点型和业务输入；不能凭代表点型外推性能 | compile smoke、production direct correctness、board bench | deferred | separate follow-up when a concrete type is requested |
| production active-cell reuse | `marching_cubes` adopted prepass | RBF 后续 base surface emission | 复用 base class active-cell 优化 | 不属于 RBF `voxelizeData()` 特有收益，可能已由 base class 覆盖 | production source audit | rejected for current topic | not_applicable |
| Eigen solve alternative | screening 风险 | Eigen `fullPivLu().solve()` | 可能降低硬边界成本 | 会改变算法 / 依赖边界，超出 RVV topic | 独立 solver profile 和数值审计 | rejected | separate non-RVV topic only |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| 更强 repeated board 稳定性 | 当前 production direct 为 weak-positive，Evidence Doctor 只有 `low_run_count` warning；不阻塞采纳，但不足以声称强稳定 | 用户或 reviewer 要求强稳定性证据时，扩展 analyzer 输出逐次 B/A values |
| 所有自定义 normal-like 点型性能结论 | traits gate 只证明字段和布局前提，不证明每个自定义点型的性能 | 有具体点型、输入规模和生产 direct case 时另开 follow-up |
| 替换 Eigen solve | 不是局部 RVV 实现；会扩大数值和维护边界 | 独立 profiling 证明 solve 是主要 blocker 且用户另行授权 |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| `000` | production-shaped bounded probe | full pipeline board diagnostic 正向，足以进入有界 production probe | production direct tests、fallback、asm、board、Evidence Doctor | completed by `010` |
| `010` | exact `PointNormal` adoption closeout | production direct 三个 case 为弱正向，QEMU / 板卡 correctness 通过，Evidence Doctor 无 Error | 正式 `doc-rvv`、evaluation、matrix 和 roadmap 已同步 | completed |
| `010` | generic normal point type expansion | exact-type gate 是阶段性例外，`PointXYZRGBNormal` / `PointXYZINormal` 仍 fallback | traits / layout gate、非覆盖 fallback、production direct bench、asm、board、Evidence Doctor | completed by `020` |
| `020` | per-iteration B/A evidence | `low_run_count` warning 说明当前 manifest 只有 summary speedup | analyzer 输出逐次 timing、环境 metadata、重复板卡 summary | medium; required only for stronger stability claim |
| `020` | custom point type follow-up | 代表点型已闭合，但自定义点型性能不能外推 | 具体点型、compile smoke、production direct board bench | low until a concrete type is requested |
