# min_cut_segmentation 函数级评估

## S2 函数级评估

`MinCutSegmentation<PointT>::extract` 会先通过 `buildGraph()` 构造带 source / sink 的 Boost graph，再调用 `boykov_kolmogorov_max_flow` 求解，最后由 `assembleLabels()` 读取残量网络生成两类 indices。当前 topic 只评估 graph 构造阶段中的 potential（势能权重）计算，不触碰 max-flow solver（最大流求解器）或公开 API（公开接口）。

关键热点如下：

| 位置 | 标量语义 | RVV 价值 | 当前边界 |
| --- | --- | --- | --- |
| `calculateUnaryPotential` | 对单个 input point 遍历 `foreground_points_`，用 x/y 平面平方距离取最小值，再计算 sink weight | 可按多个 input point 同时做 foreground min reduction（最小值规约） | foreground 少时收益有限；生产仍要更新 graph edge |
| `calculateBinaryPotential` | 对 source/target 点取三维平方距离，乘 `inverse_sigma_` 后调用 double `std::exp(-distance)` | 距离公式可 RVV 化；已有 common `expf_RVV_f32m2` 可做有限域 float 诊断 | float 近似不等于生产 double `std::exp`；需要数值预算 |
| `buildGraph` KNN 和 addEdge | 每个 index 先 KNN，再双向 addEdge，使用 `edge_marker_` 去重 | 不是当前首阶段目标 | search / Boost graph mutation 可能主导 |
| `assembleLabels` | 扫 source out edges 并按 residual capacity 填 cluster | 不是当前首阶段目标 | solver 后处理较薄，暂不启动 |

最终判断：`no-production / stop-after-diagnostic`。Phase 000 component ablation（组件消融）显示 unary 与 binary potential 在板卡上均为 positive，但 Phase 010 production-shaped buildGraph timing（生产形态图构建计时）降为 neutral，并被 Evidence Doctor 标记退化频率 Error。当前 potential batch 不建议接入 production。

## Traceability Map

| 对象 | 路径 | 角色 |
| --- | --- | --- |
| production source | `segmentation/include/pcl/segmentation/impl/min_cut_segmentation.hpp` | 标量语义来源；本阶段不修改 |
| upstream test | `test/segmentation/test_segmentation.cpp` | 现有 `MinCutSegmentationTest` public entry smoke（公开入口小型验证） |
| topic tests | `test-rvv/segmentation/min_cut_segmentation/src/test_min_cut_segmentation.cpp` | same-chain correctness |
| topic bench | `test-rvv/segmentation/min_cut_segmentation/src/bench_min_cut_segmentation.cpp` | component timing |
| topic helpers | `test-rvv/segmentation/min_cut_segmentation/include/impl/min_cut_segmentation_components.hpp` | test-only reference / RVV candidates |
| phase plan | `test-rvv/segmentation/min_cut_segmentation/doc/phases/000-current-state-and-component-ablation/plan.zh.md` | 当前阶段合同 |
| phase 000 result | `test-rvv/segmentation/min_cut_segmentation/doc/phases/000-current-state-and-component-ablation/result.zh.md` | component 证据和 Phase 010 入口 |
| phase 010 plan | `test-rvv/segmentation/min_cut_segmentation/doc/phases/010-production-shaped-buildgraph-timing/plan.zh.md` | production-shaped diagnostic 合同 |
| phase 010 result | `test-rvv/segmentation/min_cut_segmentation/doc/phases/010-production-shaped-buildgraph-timing/result.zh.md` | no-production 决策 |
| optimization matrix | `test-rvv/segmentation/min_cut_segmentation/doc/phases/optimization-matrix.zh.md` | 跨阶段证据状态 |

## 诊断证据链

Phase 000 已形成四层证据：same-chain correctness（同构正确性）、QEMU path evidence（QEMU 路径证据）、asm attribution（反汇编归属）和 board performance（板卡性能）。这些证据只说明 test-only component helper 值得继续，不证明真实 production dispatch 已存在。

Phase 010 已把计时边界扩大到 buildGraph-shaped helper：包含 KNN、Boost graph 写入和 duplicate marker，但仍排除 max-flow solver 与真实 `extract()` public dispatch。该边界的 5-run board median 只有 1.02x，且 2/5 run 低于 1.0；因此 component 证据只能保留为诊断上界，不能支持 production patch。
