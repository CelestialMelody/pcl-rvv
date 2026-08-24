# Phase 040: PCL IntegralImage2D Boundary Profile Plan

## 阶段意图和边界

本阶段继续 Phase 030 的 AVERAGE_3D_GRADIENT（平均 3D 梯度）剖析，但把积分图构建和查询边界从测试专用
`Integral3Profile` 替换为真实 PCL `IntegralImage2D<float, 3>`。目标是回答：

- `buildAverage3DGradientDiffBuffersRVV()` 的局部收益进入真实 `IntegralImage2D::setInput()` 后是否仍能支撑继续探索。
- Phase 030 中 `profile_component_integral_*` 和 `profile_component_query_*` 的弱 / 负向信号是否来自测试专用积分图近似，还是 PCL 真实边界也有相同趋势。

本阶段不修改 production（生产源码），不进入 PI2，也不改变 `features/include/pcl/features/impl/integral_image_normal.hpp`。
本阶段只允许修改 topic-local bench、manifest wrapper、Makefile evidence 引用和 topic-local 文档。

## 当前状态清单

| area | 当前事实 | 证据路径 |
| --- | --- | --- |
| map-prep | Phase 000 为 `partial-production-candidate`，PI1 计划已存在，PI2 仍需用户显式授权 | `010-pi1-production-integration-plan/plan.zh.md` |
| diff-buffer | Phase 020 diff-only 诊断为 size-dependent weak，不能直接 production patch | `020-average-3d-gradient-diff-buffer-diagnostic/result.zh.md` |
| production-shaped profile | Phase 030 total profile 在大图 tail 为 negative / unstable，diff-buffer productionization 当前不建议 | `030-average-3d-gradient-production-shaped-profile/result.zh.md` |
| exact PCL integral boundary | 尚未测真实 PCL `IntegralImage2D<float,3>::setInput()` 和 query loop | 本阶段新增 |
| production source | 未修改 | `git diff -- features/include/pcl/features/impl/integral_image_normal.hpp` 应为空 |

## 假设与候选族

| candidate family | 假设 | 需要验证 |
| --- | --- | --- |
| exact PCL setInput profile | PCL `IntegralImage2D::setInput()` 的 finite check、Eigen `ElementType` 和 row recurrence 可能比 Phase 030 近似更接近真实成本 | `pcl_iin_setinput_dxdy_*` board repeated |
| exact PCL query profile | PCL `getFirstOrderSum()` / `getFiniteElementsCount()` 查询成本可能解释 Phase 030 total profile 摇摆 | `pcl_iin_query_*` board repeated |
| exact PCL total profile | diff-buffer + PCL setInput + PCL query 的总边界决定 diff-buffer 继续探索是否仍有意义 | `pcl_avg3d_profile_*` board repeated |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| exact PCL setInput boundary | ordered-organized-image | `XYZPadPoint`, `float`, 4-float stride diff buffer | test-only PCL `IntegralImage2D<float,3>::setInput()` for dx/dy | existing `run_test_compare` plus QEMU bench smoke label/checksum shape | `pcl_iin_setinput_dxdy_320x240`, `pcl_iin_setinput_dxdy_641x481_tail` | 5-run board repeated | bench RVV asm contains diff helper RVV instructions; PCL setInput expected scalar | manifest + doctor | planned |
| exact PCL query boundary | ordered-organized-image | PCL integral images built from diff buffers | test-only query loop using real `getFirstOrderSum()` / `getFiniteElementsCount()` | existing correctness plus checksum stability | `pcl_iin_query_320x240`, `pcl_iin_query_641x481_tail` | 5-run board repeated | query loop expected scalar | manifest + doctor | planned |
| exact PCL total boundary | ordered-organized-image | `XYZPadPoint`, `float`, 4-float stride | diff-buffer + PCL setInput + PCL query diagnostic | existing correctness plus checksum stability | `pcl_avg3d_profile_320x240`, `pcl_avg3d_profile_641x481_tail` | 5-run board repeated | diff helper RVV attribution, PCL boundary scalar | manifest + doctor | planned |

## 实现和测试动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| A1 | 在 bench 中 include PCL `integral_image2D.h` 并新增 exact PCL profile helper | `run_bench_std BENCH_ARGS=1` 输出 `pcl_iin_*` 和 `pcl_avg3d_profile_*` label |
| A2 | 扩展 topic-local manifest case metadata | `python3 -m py_compile` 通过，manifest 能识别新增 label |
| A3 | 更新 Makefile board run label / doc refs / case-filter | repeated board 汇总、doctor、registry 覆盖 Phase 040 |
| A4 | 执行 correctness、QEMU smoke、bench build、asm、board repeated、Evidence Doctor、registry | 所有命令完成或记录真实 blocker |
| A5 | 更新 result、phase index、roadmap、matrix、evaluation、benchmark/evidence、README、queue row 和 Handoff | 文档使用当前 Phase 040 数值，不把 QEMU timing 写成性能结论 |

## Evidence Doctor 和 Registry 规则

- manifest 输入：`test-rvv/features/integral_image_normal/log/board/repeated_*/run_bench_std.log` 与 `run_bench_rvv.log`。
- manifest 输出：`test-rvv/features/integral_image_normal/log/board/evidence_manifest.json`。
- Evidence Doctor 输出：`test-rvv/features/integral_image_normal/log/board/evidence_doctor.md`。
- registry 输出：`test-rvv/features/integral_image_normal/log/evidence_registry.json`。
- 如果 Evidence Doctor Error 是 checksum / 数据契约 / manifest 解析错误，先修复并重跑。
- 如果 Error 是 degradation-frequency（退化频率）或 diagnostic 不支持 production 结论的信号，则降级 evidence decision，不把它当脚本失败。

## 板卡复跑预算和决策桶

- 本阶段默认使用 5-run repeated board，沿用 topic target `run_board_integral_image_normal_repeated`。
- decision bucket（决策桶）：
  - `positive`：mean 和 median 明显大于 1，且 0/5 或极少退化。
  - `weak_positive`：mean / median 略大于 1，但存在退化或接近 1。
  - `neutral`：mean / median 接近 1，方向无法支撑候选。
  - `negative`：mean 或 median 小于 1，或退化频率高。
  - `unstable`：同一 case 方向跨越 1 且不能用单次复跑稳定。
- 预算用完后若 bucket 稳定则闭合；若仍摇摆，写 `unstable` 并降级 production 取舍。

## Diagnostic-to-Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `production-shaped diagnostic`，使用真实 PCL `IntegralImage2D<float,3>`，但仍在 test helper 边界 |
| A/B boundary | `test helper`，Std/RVV 两侧共享同一 bench wrapper；candidate 只通过 RVV build 命中 diff-buffer helper |
| 当前决策问题 | `implementation-shape` 和 `RVV-vs-scalar` 诊断；不做 production clean adoption |
| diagnostic 是否可外推到 production | 只能弱外推到 AVERAGE_3D_GRADIENT 的积分图构建 / 查询成本趋势；不能证明 public dispatch、泛型点类型或 fallback |
| comparison-boundary / baseline mismatch 风险 | 有。PCL `IntegralImage2D` 边界更真实，但输入仍是合成 `XYZPadPoint` diff buffer，不是完整 `IntegralImageNormalEstimation` 对象状态 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | diff-buffer 不允许直接 production patch；map-prep PI1 不受本阶段负向 diff-buffer 结果取消 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 是。若后续要比较新的 diff-buffer family，必须补 production direct 或同边界 RVV-vs-RVV detail A/B |

## Phase Scope 与扩展队列

- `validated_scope`：`XYZPadPoint` 合成 organized image、`float`、4-float stride、320x240 和 641x481 tail、test-only PCL `IntegralImage2D<float,3>`。
- `unvalidated_scope`：真实 `PointInT` traits / field offset、`Scalar=double`、indices / correspondence、完整 `computeFeature()` dispatch / fallback、distance transform、normal output 写回。
- `point_type_expansion_queue`：本阶段不扩大点型。若用户授权 production patch，先回到 Phase 010 map-prep PI2；若未来恢复 diff-buffer，需要新建 production direct / generic point type phase。
- `phase_closeout_boundary`：只能关闭 exact PCL integral boundary 诊断矩阵条目，不能关闭 production topic。

## 继续 / 停止条件

继续条件：

- 板卡可用，且 board repeated / Evidence Doctor / registry target 可运行。
- Phase 040 evidence 没有数据契约错误，或错误可修复 / 可降级解释。

停止条件：

- 继续需要修改 production source、public API、其它 topic 或扩大到 PI2；此时等待用户授权。
- 板卡、工具链、rsync / ssh 或依赖不可用。
- Evidence Doctor 数据契约错误无法修复，或 registry 显示无法归属的未登记覆盖。

默认下一阶段：

- 若 exact PCL boundary 仍 negative / unstable：继续保持 diff-buffer `no-production-now`，默认恢复仍是 Phase 010 map-prep PI2 用户授权门禁。
- 若 exact PCL boundary 明显 positive：只把 diff-buffer 标为 `bounded follow-up candidate`，仍需 production direct 计划和用户授权，不能自动修改 production。

## 文档更新清单

- 新增 `040-pcl-integral-image2d-boundary-profile/result.zh.md`。
- 更新 `doc/phases/README.zh.md`、`optimization-matrix.zh.md`、`doc/optimization-roadmap.zh.md`。
- 更新 `doc/benchmark-and-evidence.zh.md`、`doc/integral_image_normal-evaluation.zh.md`、`README.zh.md`。
- 更新队列表中 integral image normal 行。
- 更新 current Handoff Packet。
