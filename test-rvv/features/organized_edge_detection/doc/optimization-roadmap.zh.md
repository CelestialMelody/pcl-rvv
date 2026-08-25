# Optimization Roadmap

## 当前边界

当前已完成 Phase 010 production probe（生产探针）并采纳 depth label RVV production path；Phase 020 又补齐
常见 traits-gated input point type（输入点型）证据。topic scope
（主题范围）仍是 `organized_edge_detection.hpp` 的 depth / RGB / normal edge masks 与 label writes；
当前 adopted / covered scope 覆盖 `PointXYZ`、`PointXYZI`、`PointXYZRGB`、`PointXYZRGBNormal` 与
`pcl::Label` 的真实 `compute()` / `extractEdges()` depth path。生产 gate 为 `PointT` 满足
`RVVXYZAoSFloatLayout` 且 `PointLT=pcl::Label`；自定义点型和泛型 `PointLT` 仍未证明。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `depth_labels_rvv_same_chain` | 当前源码 shape scan | `PointXYZ` depth labels，organized internal pixels | production board mean `3.016x-6.194x` | 已采纳范围仍是 `PointXYZ` / `pcl::Label` 代表证据；更宽点类型需扩展 phase | production direct correctness、asm、board production bench、Evidence Doctor | `production-adopted` | done |
| `depth_labels_point_type_expansion` | Phase 010 traits gate 审计 | `PointXYZI`、`PointXYZRGB`、`PointXYZRGBNormal` + `pcl::Label` | Phase 020 board mean `5.327x`、`5.437x`、`4.158x`、`2.822x` | `PointXYZRGBNormal` 组内较低，不能把其它点型收益外推到大 stride 点型；泛型 `PointLT` 未覆盖 | typed production correctness、production asm、5-run board、Evidence Doctor | `covered_by_phase020` | done |
| `label_indices_collection` | Phase 000 保留标量收集 | labels linear scan | 若 production 接入后收集成为主成本，可进一步优化 | index push order 是公开输出语义，vector compress / prefix count 复杂 | component ablation、order-preserving correctness、board A/B | `deferred` | production direct 后按 profile 决定 |
| `rgb_canny_gray_prep` | 源码中 RGB 灰度转换循环 | `OrganizedEdgeFromRGB` | 批量 RGB 平均可能减少前处理成本 | 主成本可能在 `Edge::detectEdgeCanny()`；RGB 字段 traits 复杂 | RGB correctness、component bench、board summary | `planned` | separate follow-up |
| `normal_canny_prep` | 源码中 normal x/y image 构造 | `OrganizedEdgeFromNormals` | 批量 normal_x / normal_y store | 主成本可能在 Canny；normal cloud 与 input cloud 尺寸 gate | normal correctness、component bench | `planned` | separate follow-up |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 000 | production probe 应先收窄到 depth-only `PointXYZ`，再做 traits expansion。 | test-helper 证据强正向，但 production 泛型边界多。 | PI1 scope、fallback matrix、production direct board bench。 | high |
| 000 | invalid neighbor lane fallback 仍保持强正向，可暂不优先优化 search-neighbor scan。 | NaN boundary case mean `3.075x`，低于 finite case 但仍 positive。 | production invalid-heavy 分布或 component ablation。 | medium |
| 010 | production direct 证据比 diagnostic 更强，depth path 可直接采纳。 | 真实 `compute()` board mean `6.194x`、`5.521x`、`3.016x`，checksum match。 | 后续只需在扩大点类型或派生入口时重跑同边界证据。 | high |
| 020 | 常见 xyz AoS 点型在同一 production helper 上保持正向，但收益应按点型单独报告。 | `PointXYZRGBNormal` median `4.216x`，比同组 finite median 低 21.6%，Doctor 给出 group outlier warning。 | 更宽点型需按 stride / layout 分组重新跑 production-public board 和 Doctor。 | medium |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| `assignLabelIndices` RVV | 顺序 push 语义敏感，当前 depth label helper 已强正向。 | production direct 后 profile 显示 label index 收集成为主成本。 |
| RGB / normal Canny 派生入口 | 当前 phase 未覆盖 Canny helper，不能从 depth label 证据外推。 | depth production probe 完成或另开派生入口诊断。 |
| 泛型 `PointLT` label field gate | 当前 production helper 直接写 `pcl::Label::label`；扩成自定义 label 点型需要输出字段 traits 设计和回退测试。 | 用户要求支持自定义 label 点型，或上游 API 出现真实 workload 需求。 |

## 默认恢复队列

| order | phase | scope | status | resume condition |
| --- | --- | --- | --- | --- |
| 1 | `030-rgb-normal-derived-entries` | RGB / normal Canny 前处理 | `turn_stop_deferred as separate follow-up` | depth path 已采纳且点型扩展已闭合；派生 Canny 前处理是不同 helper / 成本模型，建议另开 follow-up。 |
| 2 | `assign-label-indices-ablation` | label index linear scan | `deferred until profile` | 只有 production profile 显示 label index 收集成为主成本时恢复。 |
| 3 | `pointlt-label-traits-expansion` | 泛型 label output point type | `not_recommended_now` | 需要新的 output label traits 设计；当前没有性能或用户需求证据。 |
