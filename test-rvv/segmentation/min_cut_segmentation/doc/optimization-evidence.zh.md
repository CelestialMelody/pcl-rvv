# min_cut_segmentation 优化证据索引

## 本文职责

本文索引已经尝试的 RVV candidate（候选实现）、证据路径和取舍。搜索空间和未来恢复条件主归属仍是 `doc/optimization-roadmap.zh.md`；phase 流水主归属是 `doc/phases/*/result.zh.md`。

## 当前结论摘要

当前 EvidenceDecision（证据决策）为 `no-production / stop-after-diagnostic`。unary 和 binary component 在板卡上有上界收益，但 buildGraph-shaped diagnostic 为 neutral，且 Evidence Doctor 报退化频率 Error，因此不建议继续当前 potential batch 的 production 接入。

## 优化方式总表

| candidate family | 代码路径 | 测试路径 | bench case | board evidence | asm evidence | decision | 边界 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| unary foreground min-distance batch | `include/impl/min_cut_segmentation_components.hpp` | `UnaryPotentialMatchesScalar` | `unary_min_distance` | `log/board/repeated/summary.md`: median `2.08x` | `computeUnaryPotentialsRVV` 附近有 RVV 指令 | diagnostic-positive | component-only |
| binary distance + expf approximation | 同上 | `BinaryPotentialMatchesScalarWithinFloatExpBudget` | `binary_exp_weight` | `log/board/repeated/summary.md`: median `2.44x` | `computeBinaryPotentialsRVV` / `expf_RVV_f32m2` | diagnostic-positive | component-only，float exp 近似 |
| buildgraph-potential-batch | 同上 | `BuildGraphPotentialBatchMatchesScalarWithinFloatBudget` | `buildgraph_potential_batch` | `log/board/buildgraph-repeated/summary.md`: median `1.02x`，2/5 退化 | buildGraph-shaped path 调用 RVV helper | rejected for production | production-shaped diagnostic only |

## 标量路径与 RVV 路径差异

| 维度 | Std path | RVV path | 风险 |
| --- | --- | --- | --- |
| unary load | 逐点读取 x/y | scratch staging 后 `vle32` | staging 成本在 graph 边界内可能被稀释 |
| unary reduction | foreground loop 内 scalar min | foreground loop 外层标量、input rows 内 RVV min | foreground 少时收益上限有限 |
| binary load | 逐边读取 source/target xyz | source/target index staging 后向量距离 | gather staging 不是 true indexed load |
| exp | double `std::exp` | float `expf_RVV_f32m2` | 不能 strict 替代 production double 语义 |
| graph write | Boost add_edge / capacity map | 同样 Boost add_edge / capacity map | graph mutation 成本主导 |

## 代码级证据索引

| 对象 | 路径 | 证据角色 |
| --- | --- | --- |
| production scalar source | `segmentation/include/pcl/segmentation/impl/min_cut_segmentation.hpp` | 语义来源；未修改 |
| topic helper | `include/impl/min_cut_segmentation_components.hpp` | Std / RVV diagnostic 和 buildGraph-shaped helper |
| test wrapper | `src/test_min_cut_segmentation.cpp` | correctness |
| bench wrapper | `src/bench_min_cut_segmentation.cpp` | component 和 buildGraph-shaped timing |
| manifest script | `script/generate_min_cut_board_evidence_manifest.py` | summary / manifest 生成 |
| phase results | `doc/phases/000-current-state-and-component-ablation/result.zh.md`、`doc/phases/010-production-shaped-buildgraph-timing/result.zh.md` | EvidenceDecision |

## 细粒度 target 字典

| target | 隔离对象 | 当前用途 |
| --- | --- | --- |
| `run_board_min_cut_repeated` | component candidates | 证明 Phase 000 上界 |
| `run_board_min_cut_buildgraph_repeated` | buildGraph-shaped candidate | 证明当前不建议 production patch |
| `run_board_evidence_doctor` | component repeated manifest | 检查 summary-only evidence |
| `run_board_buildgraph_evidence_doctor` | buildGraph repeated manifest | 触发退化频率降级 |

## 当前可提交证据

当前 `topic-only` 提交候选是测试资产、脚本和文档。summary / manifest / doctor 可作为后续单独 evidence commit，但默认不提交 raw logs。`build/` 二进制和 asm dump 也不进入 topic commit。

## 结论边界

已证据拒绝的不是 “MinCutSegmentation 永远不可优化”，而是当前 potential batch 路线不值得继续接 production。若未来要恢复，必须提出新的 bounded hypothesis（有界假设），例如改变图构建策略或通过 profile 证明另一个热点，而不是复用当前 component 2x 直接推进。
