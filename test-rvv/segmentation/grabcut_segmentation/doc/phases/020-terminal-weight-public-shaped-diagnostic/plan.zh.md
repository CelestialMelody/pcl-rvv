# Phase 020: terminal weight public-shaped diagnostic plan

## 阶段意图和边界

本阶段把 Phase 010 的单个 Gaussian（高斯分量）概率密度扩展到更接近 production（生产源码）的
terminal weight（端点权重）形态：复刻 `GrabCut<PointT>::initGraph()` 在 `TrimapUnknown`
分支中的两条公式：

- `fore = -log(background_GMM.probabilityDensity(color))`
- `back = -log(foreground_GMM.probabilityDensity(color))`

本阶段仍只修改 `test-rvv/segmentation/grabcut_segmentation` 下的诊断资产，不修改
`segmentation/include/pcl/segmentation/impl/grabcut_segmentation.hpp` 或
`segmentation/src/grabcut_segmentation.cpp`。真实 graph mutation（图边写入）和 max-flow（最大流）
求解继续作为标量边界。

## 候选假设

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| K=5 GMM mixture probability | 复刻 `GMM::probabilityDensity(c)` 的 `pi * probabilityDensity(i,c)` 求和后，可以判断 Phase 010 单分量收益是否能穿过 K=5 小循环。 | `K=5` 很小，额外 buffer（缓冲区）和多次 pass 可能吃掉 RVV 收益。 |
| terminal weight wrapper | 在 mixture probability 外层补 `-log` 后，能更接近 `initGraph` 的 terminal weight 成本边界。 | `std::log` 仍是标量；完整 `initGraph` 还包含 `setTerminalWeights` 和 n-link graph edge mutation。 |

## 实现动作

| 动作 | 产物 | 完成判据 |
| --- | --- | --- |
| RED terminal correctness | `src/test_grabcut.cpp` 引用尚未实现的 GMM mixture / terminal helper。 | `make run_test_compare` 编译失败，说明测试能捕获缺失 helper。 |
| GREEN helper | `include/impl/grabcut_diagnostic_reference.hpp` 增加 `pi`、mixture probability 和 terminal weight wrapper。 | Std/RVV 构建均通过；RVV 构建的 terminal weight candidate 在误差预算内。 |
| bench extension | `src/bench_grabcut.cpp` 增加 `terminal_weights` case。 | QEMU log-shape 通过；板卡 single smoke 可生成 manifest / doctor。 |
| Evidence Doctor | 扩展 `script/generate_grabcut_board_evidence_manifest.py` 的 case label。 | doctor 无 Errors；low run count 可保留为诊断 warning。 |

## 数值预算

继承 Phase 010 的 `pcl::expf_RVV_f32m2` 误差预算。terminal weight 额外经过 `-log`，测试预算为
absolute error（绝对误差）`5e-5` 与 relative error（相对误差）`3e-4` 二者取较大值。样本必须保持
mixture probability 为正，避免把 `log(0)` 语义混入本阶段。

## Continue / stop condition

若 terminal weight correctness 失败且无法解释为 fast exp（快速指数）近似误差，则停止本阶段并记录
blocked。若 correctness 通过但板卡 component 退化，则不进入 production integration loop（生产接入闭环），
只保留为诊断结论。若 terminal weight component 在板卡上仍正向，则下一步是 repeated board summary 或
更接近 public entry 的 `initGraph` no-solve（不含求解）probe。
