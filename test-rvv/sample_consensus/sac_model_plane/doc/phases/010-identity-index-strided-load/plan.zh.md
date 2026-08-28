# Phase 010 Plan: identity-index strided load

## 阶段意图和边界

本阶段评估 identity indices（恒等索引，`indices[i] == i`）下是否值得新增 strided load
（跨步加载）快速路径。当前 production RVV 已覆盖 direct indexed gather；本阶段只允许在
`sac_model_plane` 三条 RVV helper 内增加局部 load strategy（加载策略）选择，不改变 public API，
不扩大到其它模型、`Scalar=double`、correspondence 或 normal 字段。

## 当前状态

Phase 000 已采纳 indexed gather + shared distance kernel：`selectWithinDistance` median 3.1965x、
`countWithinDistance` median 1.6678x、`getDistancesToModel` median 2.3695x。源码中的
`SampleConsensusModel` cloud-only 构造会生成恒等 `indices_`，这使默认整云路径可能不需要 gather。

## 候选方案

| candidate | 思路 | 风险 |
| --- | --- | --- |
| chunk-local identity fast path | 每个 VL chunk 先检查 `indices[i + lane] == i + lane`；成立时用 `strided_load3_f32m2` 从 `input_->points[i]` 读取 xyz。 | scalar identity check 成本可能抵消收益；shuffled chunks 必须继续走 gather。 |
| whole-vector pre-scan | 入口先扫描全部 `indices_` 是否恒等，整轮固定选择 strided 或 gather。 | 多一次 O(n) 扫描，select/count/getDistances 每次调用都会付成本。 |
| 保持 gather | 不增加复杂度。 | 默认整云路径可能留下可避免的 gather 成本。 |

本阶段先采用 chunk-local 方案；若 shuffled board 退化或 identity 提升不明显，撤回 fast path，仅保留 bench case。

## 优化矩阵

| candidate family | row source | point type / layout | correctness | bench / board | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| identity-index strided load | identity direct cloud | `PointXYZ` / `RVVXYZAoSFloatLayout` | planned: public entry vs Standard | planned: identity vs shuffled board A/B | planned: `vlse` / `vlsseg` present, gather still present for shuffled | planned | pending |
| fallback to gather | shuffled direct cloud | `PointXYZ` | planned: existing shuffled correctness | planned: no material regression vs Phase 000 | planned: gather remains | planned | pending |

## 实现和测试动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| bench mode split | `src/bench_sac_model_plane.cpp` | 支持 `identity` 和 `shuffled` 参数，输出 dataset label。 |
| pre-fast-path baseline | board logs / phase result | 用当前 gather RVV 采集 identity baseline。 |
| correctness test | `src/test_sac_model_plane.cpp` | 增加 identity public entry 对拍；先验证当前实现已正确，再用于保护 fast path。 |
| production fast path | `impl/sac_model_plane.hpp` | 三条 RVV helper 在 chunk identity 时使用 strided load，否则 gather。 |
| evidence rerun | QEMU、asm、board、Evidence Doctor | identity positive 且 shuffled 不退化时保留；否则撤回 production fast path。 |

## Evidence Doctor 和 registry

本阶段沿用 `script/generate_board_evidence_manifest.py`。如果新增 identity repeated 输出目录，manifest
必须能标识 dataset / layout，Evidence Doctor 必须在 EvidenceDecision 前运行。

## 板卡复跑预算和决策桶

先跑 single-run board smoke 做方向判断；若 identity fast path 相对 pre-fast-path baseline 的改善超过
5% 且 shuffled 不低于 Phase 000 median 的 95%，升级到 5-run repeated。若收益低于 3%、shuffled 退化超过
5% 或 Evidence Doctor 出现 Error，撤回 fast path。

## 继续 / 停止条件

若 identity fast path 保留，更新 `doc-rvv`、evaluation、roadmap、matrix 和 phase result。若不值得保留，
记录 rejected with evidence，当前 topic 保持 Phase 000 adopted 实现，然后判断是否进入点型扩展 phase。
