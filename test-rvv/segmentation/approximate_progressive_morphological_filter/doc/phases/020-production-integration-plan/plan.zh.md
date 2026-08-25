# 020 production-integration-plan 计划

## 阶段意图和边界

本阶段是 PI1 production integration plan（生产接入计划），用于决定是否把 phase 010 的 positive full-pipeline diagnostic 转成 production patch（生产补丁）。本计划只写 topic-local 文档，不修改 production 源码。

## 候选范围

推荐的 production probe（生产探针）范围是 `ApproximateProgressiveMorphologicalFilter<PointT>::extract(Indices& ground)` 的窄路径：

- 编译期只在 `__RVV10__` 下尝试 RVV。
- 点类型优先选择 `pcl::rvv::RVVXYZAoSFloatLayout<PointT>` 或 `kRVVXYZAoSPointCompatible<PointT>` 的泛型 xyz gate；若实现风险过高，先收窄到 `pcl::PointXYZ` 并把其它 `PointT` fallback，但这只能是阶段性例外。
- 运行期 gate 包含 cloud size >= RVV 阈值、`input_->size() <= pcl::rvv::rvvMaxU32ByteOffsetElements<PointT>()`、`indices_` 内容已由 PCLBase 语义保证有效或显式回退。
- 初始 grid z-min 和 tail-compress 使用 RVV；window-open 不能写成独立加速点，生产 patch 可先保留标量 window-open，或把 RVV window-open 作为可回退细节 A/B。

## 不接入范围

| 范围 | 决策 | 理由 |
| --- | --- | --- |
| public API 变更 | 不接入 | 当前证据不需要改变用户可见 API |
| `Scalar=double` | not_applicable | APMF 当前 grid 和阈值链路使用 float；没有 double production 证据 |
| 泛型非 xyz float 点型 | fallback | traits 不满足时不能进入 RVV |
| organized / subset 特殊语义 | fallback 或后续 phase | 当前诊断只覆盖完整 input grid 与当前 ground vector |
| OpenMP + RVV 组合 | 后续 production direct 证据决定 | 诊断 helper 未覆盖真实 OpenMP 调度 |
| `copyPointCloud` 语义变化 | 需要审计 | 诊断 tail 直接用 `ground` index 访问原 cloud；production 若移除 copy，需用真实入口测试证明等价 |

## fallback 矩阵

| gate | fallback 行为 | 必须测试 |
| --- | --- | --- |
| 非 `__RVV10__` 构建 | 完全走原标量路径 | Std build compile + correctness |
| traits / layout 不满足 | 原标量路径 | 编译期或 fallback smoke，不能实例化非法 offset |
| cloud 太小 | 原标量路径 | 小规模 fallback test |
| 32-bit byte offset 可能溢出 | 原标量路径 | gate 单元测试或静态审计 |
| non-dense / invalid 点 | RVV finite mask 或标量 fallback | non-dense production direct correctness |
| window-open RVV 退化 | 可保留标量 window-open | production bench A/B，避免 window-open 成为负向主因 |

## 生产源码形态选择

有两个可审查方案：

| 方案 | 需要修改 | 优点 | 风险 / 暂停条件 |
| --- | --- | --- | --- |
| clean split | 在类声明头中新增私有 / protected `extractStd` 与 `extractRVV`，在 impl 头定义 | public entry 形态清楚：语义检查 -> RVV 短路 -> Std fallback | 超出用户点名的单个 impl 文件，需要用户确认生产范围 |
| impl-only exception | 只在当前 impl 头的 `extract` 开头尝试 RVV，失败后继续原标量主体 | 改动文件更少 | 入口仍保留大段标量主体，违反默认 clean split；必须由 reviewer 接受例外理由 |

当前推荐：先请求用户确认是否允许 clean split 触碰 `segmentation/include/pcl/segmentation/approximate_progressive_morphological_filter.h`。未确认前不进入 PI2。

## production direct 证据计划

PI2 后必须补：

1. 真实 `ApproximateProgressiveMorphologicalFilter<pcl::PointXYZ>` correctness test，覆盖 dense 和 non-dense。
2. fallback tests：非 RVV build、小规模、非覆盖点型或 traits gate。
3. production bench：public entry 或最接近 public entry 的 wrapper，不复用 test-only component timing 做 production 结论。
4. asm attribution：反汇编归属到 production RVV helper 或 inlined production symbol。
5. board repeated：至少 5 run，生成 production manifest 和 Evidence Doctor。

## 暂停条件

继续到 PI2 前需要用户确认 production 范围，因为 clean split 会触碰类声明头；impl-only exception 也需要 reviewer 明确接受。若用户确认进入 production integration loop，可按本计划实现 PI2-PI5，并在 PI5 停下等待最终采纳或回滚确认。
