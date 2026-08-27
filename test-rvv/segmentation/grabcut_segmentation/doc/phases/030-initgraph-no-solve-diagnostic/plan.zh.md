# Phase 030: initGraph no-solve diagnostic plan

## 阶段意图和边界

本阶段继续保持 production（生产源码）不变，把 Phase 020 的 terminal weight（端点权重）公式推进到更接近
`GrabCut<PointT>::initGraph()` 的 no-solve diagnostic（不含求解诊断）边界。目标是把以下成本纳入
test-support wrapper（测试支撑包装层）：

- unknown trimap（未知三分图标记）下的 K=5 GMM probability（高斯混合模型概率）和 terminal `-log`。
- graph node（图节点）分配后的 terminal write sink（端点权重写入消耗点），用轻量 vector sink 表示。
- organized n-link edge mutation（规则图像邻接边写入）仍暂不进入首个 Phase 030 RED/GREEN；该路线在 Phase 000
  证据里没有成为优先生产候选。

本阶段不调用真实 `BoykovKolmogorov::solve`，不修改 `grabcut_segmentation.hpp` 或
`grabcut_segmentation.cpp`，不把 test-support helper 伪装成 production direct（真实生产入口直连）证据。

## 候选假设

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| terminal weight + graph node write | Phase 020 的 `1.54x` 诊断收益在加入 terminal write sink 后仍可见。 | sink 太轻会高估收益；sink 太重又可能偏离 production graph internals。 |
| terminal + n-link edge write | 若 graph node write 仍正向，再加入 n-link edge mutation 的轻量模拟。 | organized n-link 路线弱，混入后可能难以归因。 |

## 实现动作

| 动作 | 产物 | 完成判据 |
| --- | --- | --- |
| RED no-solve correctness | `src/test_grabcut.cpp` 引用 `computeInitGraphNoSolveReference/Candidate`。 | `make run_test_compare` 因 helper 缺失失败。 |
| GREEN helper | `include/impl/grabcut_diagnostic_reference.hpp` 增加 no-solve result 和 terminal write sink。 | Std/RVV 构建均通过；输出 terminal costs 和 sink checksum 在误差预算内。 |
| bench case | `src/bench_grabcut.cpp` 增加 `--case initgraph_no_solve`。 | QEMU log-shape 通过；板卡 smoke 后可生成 manifest。 |
| Evidence Doctor | 扩展 topic-local manifest case label。 | 若 single run 只作初筛，保留 low-run warning；若 repeated 运行，doctor 应无 run-count warning。 |

## 数值预算

沿用 Phase 020 terminal weight 预算：absolute error（绝对误差）`5e-5` 与 relative error（相对误差）
`3e-4` 二者取较大值。额外 sink 只使用 terminal weights 的加权累加 checksum，不参与 production 数值语义。

## Continue / stop condition

若 no-solve wrapper correctness 失败且无法解释，则本阶段停止为 blocked。若 board smoke 或 repeated 退化，
记录为 Phase 020 到 no-solve boundary 的收益稀释，不直接推出 no-production；下一步需要 diagnostic-to-production
mismatch audit（诊断到生产边界差异审计）。若 no-solve repeated 仍正向，则下一步可以准备 PI1 production
integration plan（生产接入计划），但 PI1 后仍必须在 PI5 等待用户确认采纳或回滚。
