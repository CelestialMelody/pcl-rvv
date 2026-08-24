# Integral Image Normal 函数级评估

## 范围和目标源码

目标源码是 `features/include/pcl/features/impl/integral_image_normal.hpp`。
目标类是 `pcl::IntegralImageNormalEstimation<PointInT, PointOutT>`，公开入口通过
`Feature::compute()` 进入 protected `computeFeature(PointCloudOut&)`，再根据是否使用完整 organized cloud
调用 `computeFeatureFull()` 或 `computeFeaturePart()`。

当前已完成 Phase 050 production probe（生产探针）。Phase 000 先评估 `computeFeature()` 开头的 map preparation（地图预处理）片段：

- depth-change map：对相邻右侧和下侧深度做有限值和阈值检查，把发生深度突变的点标为 0。
- distance map initialization：把 depth-change map 转成初始距离图，0 表示障碍，非 0 表示大距离。

Phase 050 在用户授权后把这个前缀接入 `features/include/pcl/features/impl/integral_image_normal.hpp`，
并用真实 public `IntegralImageNormalEstimation<PointXYZ, Normal>::compute()` 入口采集 production direct
（真实生产路径）证据。

Phase 020 评估 `initAverage3DGradientMethod()` 的 diff_x / diff_y buffer 写入片段。该片段只覆盖测试专用 `XYZPadPoint`、float、4-float stride 布局，不覆盖 production `PointInT` 泛型点类型。Phase 030 进一步用 production-shaped profile（生产形态剖析）把 diff-buffer、积分图构建和 normal query 拆开，判断 Phase 020 的局部收益是否能支撑生产探针。Phase 040 再把积分图构建和查询换成真实 PCL `IntegralImage2D<float,3>` 边界，复核 Phase 030 是否被测试专用积分图近似误导。

## 函数 / 函数族作用速览

| 函数 / 函数族 | 作用 | 输入 / 输出状态 | 与主流程关系 | RVV 判断 |
| --- | --- | --- | --- | --- |
| `computeFeature()` | 准备 depth-change map、distance map，并分派 full / part 输出 | 读取 organized `input_`，写 `distance_map_` 和 `output` | 公开计算主入口 | Phase 050 已接入 map-preparation 前缀 RVV；distance transform 两遍扫描有行内依赖，暂缓 |
| `computeFeatureFull()` | 对完整 organized cloud 写边界 NaN，并逐像素计算 normal | `distanceMap`、`output`、border policy | full cloud 主输出 | 每点调用 `computePointNormal*`，先保留标量 |
| `computeFeaturePart()` | 对 `indices_` 子集逐项计算 normal | `indices_`、`distanceMap`、`output[idx]` | indexed 输出路径 | gather（离散加载）和输出下标语义更复杂，本阶段不覆盖 |
| `computePointNormal()` | 根据 normal method 计算单点 normal / curvature | integral images、rect size、viewpoint | per-point normal solver | Eigen / cross product / integral image 查询混合，先不做 RVV |
| `initAverage3DGradientMethod()` | 构造 diff_x / diff_y 并建立积分图 | 读相邻点 xyz，写连续 float buffer | AVERAGE_3D_GRADIENT 前置 | Phase 020 test helper 正确；性能是 size-dependent weak diagnostic，不建议直接接 production |

## 标量流程与 RVV 流程对照

标量流程先把 `depthChangeMap` 全部填成 255，再遍历 `height - 1` 行和 `width - 1` 列。每个像素读取
`z`、右邻 `z` 和下邻 `z`，用 `max_depth_change_factor_ * (abs(depth) + 1) * 2` 作为阈值。
右向或下向检查失败时，当前像素和对应邻居被置 0。随后 distance map 初始化把 0 映射成 0.0f，
非 0 映射成 `width + height`。后续两遍 distance transform（距离传播）存在 row dependency（行内依赖），
本阶段不声明可 RVV 化。

Phase 050 production RVV 流程只替换前两个片段。depth-change map 的 0 写入是 idempotent（幂等写入，多次写 0 结果相同），
因此可用 mask store（掩码存储）分别写当前、右邻和下邻位置。distance map 初始化是连续 byte-to-float
select（字节到浮点选择）。该路径只在 `__RVV10__` 且 `pcl::rvv::RVVXYZAoSFloatLayout<PointInT>` 成立时尝试；
其它点类型或非 RVV 构建走同语义 Std helper。

Phase 020 候选 RVV 流程使用 strided load/store（跨步加载 / 存储）分别读取 `x/y/z` 三个字段，计算右减左和下减上，再按 4-float stride 写入 diff_x / diff_y。这个形态只证明测试专用 4-float stride 点型；生产接入还要证明真实 `PointInT` layout、`integral_image_DX_ / DY_.setInput()` 成本和完整 AVERAGE_3D_GRADIENT 路径。

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `computeFeature()` | production public-derived entry | 真实生产入口中的 map preparation 和输出分派 | `Feature::compute()` | `computeFeatureFull()` / `computeFeaturePart()` | Phase 050 production-public evidence（生产公开入口证据） | `features/include/pcl/features/impl/integral_image_normal.hpp` |
| `computeFeatureFull()` | production scalar continuation | 完整 organized cloud 的 normal 输出 | `computeFeature()` | `computePointNormal()` / `computePointNormalMirror()` | 本阶段不覆盖 | `features/include/pcl/features/impl/integral_image_normal.hpp` |
| `computeFeaturePart()` | production indexed continuation | indices 子集 normal 输出 | `computeFeature()` | `computePointNormal()` / `computePointNormalMirror()` | 本阶段不覆盖 | `features/include/pcl/features/impl/integral_image_normal.hpp` |
| `src/test_integral_image_normal.cpp` | correctness | 对拍 test helper，并用真实 public `compute()` 验证 production distance map | `run_test_compare` | `include/integral_image_normal.h` / production header | diagnostic + production direct correctness | done |
| `src/bench_integral_image_normal.cpp` | diagnostic + production direct bench wrapper | 测 map-prep、diff-buffer、AVERAGE_3D_GRADIENT profile 和 `prod_compute_*` public 入口 | `run_bench_*` / board target | `include/integral_image_normal.h` / production header | diagnostic + production-public performance | done |
| `log/board/repeated-summary.md` | evidence summary | 5-run board repeated summary（重复板卡摘要） | manifest wrapper | `evidence_doctor.py` | diagnostic performance summary（诊断性能摘要） | local summary-only evidence |
| `log/board/evidence_doctor.md` | Evidence Doctor | 检查 repeated manifest 的数据契约和异常信号 | `evidence_manifest.json` | phase result / Handoff | evidence validation（证据体检） | local summary-only evidence |
| `doc/testing-overview.zh.md` | topic-local doc suite | 测试入口分类和 target 粒度审计 | README / Handoff | correctness / benchmark docs | reviewability（可审查性） | standalone |
| `doc/benchmark-and-evidence.zh.md` | topic-local doc suite | bench case、Evidence Doctor 和提交边界 | README / evaluation | phase result / Handoff | evidence ownership（证据归属） | standalone |
| `doc/test-support-code-map.zh.md` | topic-local doc suite | test support helper、bench wrapper 和 script 定位 | README / reviewer | source files / output | traceability（可追踪性） | standalone |

## 实现方式审计

| 实现维度 | 当前状态 | 证据 | 边界 / 恢复条件 |
| --- | --- | --- | --- |
| dispatch / fallback | `__RVV10__` 下先尝试 `tryInitializeMapPrepRVV`，失败自然调用 `initializeMapPrepStd` | production diff + `run_test_compare` | 用户已确认采纳当前 patch |
| layout / traits gate | production 使用 `pcl::rvv::RVVXYZAoSFloatLayout<PointInT>`，实际只读取 z 字段 | public compute correctness + board repeated | 其它点型只证明 fallback 编译语义，不外推性能 |
| staging / stores | depth-change 采用幂等 0 写入，distance init 用 byte-to-float select | `run_test_compare` 通过，board checksum 一致 | production 需证明真实 `input_->points` 布局和 fallback |
| row source policy | `ordered-organized-image` completed for diagnostic | organized width × height | indices 子集不覆盖 |
| production scope | map-prep 为 `adopted weak-positive production-public`；diff-buffer 降级为 `weak/unstable exact-PCL production-shaped diagnostic / no-production-now` | Phase 050 result + `doc-rvv/features/integral_image_normal-RVV.zh.md` | 不外推到未验证点型、indices 或其它 normal method |

## 测试和 Bench 计划

| 测试 / target | 层级 | 作用 |
| --- | --- | --- |
| `run_test_compare` | unit / boundary correctness（单元与边界正确性） | Std / RVV 两个构建都必须和标量参考一致 |
| `dump_test_rvv` | asm attribution（反汇编归属） | 检查 RVV test binary 中候选 helper 是否出现关键向量指令 |
| `board_smoke` | board smoke（板卡小型验证） | 板卡可用时验证 RVV binary 可运行，并运行诊断 bench |

## 诊断证据链

Phase 050 已闭合 map-preparation production probe 的 evidence（证据）：

- `run_test_compare` 通过 Std/RVV correctness（正确性）对拍，包含真实 public `compute()` 后的 distance map 对拍；当前两侧各 5 个 gtest。
- `dump_test_rvv` / `dump_bench_rvv` 生成的 RVV binary 反汇编包含候选范围内的向量 load、mask compare、masked byte store 和 `vsetvli`。
- 当前板卡 5-run repeated summary 显示 `map_prep_320x240` mean speedup 5.25x，`map_prep_641x481_tail` mean speedup 5.11x，checksum 一致。
- public `compute()` production direct case 显示 `prod_compute_avg_depth_320x240` median / mean speedup 1.06x，`prod_compute_avg_depth_641x481_tail` median / mean speedup 1.06x，checksum 一致。
- Evidence Doctor（证据体检）当前为 Errors=3 / Warnings=21 / Suggestions=8；production-direct 两项只有 1/5 退化 warning，不是 correctness 或 checksum error。

Phase 020 / 030 / 040 已闭合 diff-buffer helper 的 diagnostic evidence：

- `run_test_compare` 通过 Std/RVV correctness 对拍，当前两侧各 5 个 gtest，其中 diff-buffer 仍只证明 test helper。
- `dump_test_rvv` 生成的 RVV binary 反汇编包含 `vlse32.v`、`vfsub.vv` 和 `vsse32.v`。
- 当前板卡 5-run repeated summary 显示 `avg3d_diff_641x481_tail` mean speedup 1.37x，但 `avg3d_diff_320x240` mean speedup 1.05x，仍是 near-threshold。
- Phase 030 的 production-shaped profile 在当前 Phase 050 复跑中显示完整 `avg3d_profile_320x240` mean speedup 1.10x、min 1.00x；`avg3d_profile_641x481_tail` mean speedup 1.09x、min 1.00x。该结果仍是 production-shaped diagnostic，不替代 production direct。
- Phase 040 的 exact PCL profile 显示 `pcl_avg3d_profile_320x240` mean speedup 1.01x；`pcl_avg3d_profile_641x481_tail` mean speedup 1.03x。`pcl_iin_query_320x240` 为 mean 0.98x 且 3/5 低于 1。
- Evidence Doctor 输出 Errors=3，Warnings=21，Suggestions=8；Error 来自非 production-direct 的 profile/component 退化频率，不是 checksum 或 correctness 错误。结论仍必须把 diff-buffer 降级为 weak / unstable exact-PCL production-shaped diagnostic。

这些证据不能替代 map-prep 之外的 production evidence（生产证据）。当前已采用的 production RVV 只覆盖
`computeFeature()` 的 map-prep 前缀；真实 `PointInT` 更宽点型性能、indices 输出、distance transform、
integral image 构建、full / part normal 输出和其它 normal method 都尚未闭合。

## 文档套件归属审计

| role | status | 说明 |
| --- | --- | --- |
| topic_navigation | `standalone:README.zh.md` | 入口、当前结论、常用命令和 doc suite inventory。 |
| testing_overview | `standalone:doc/testing-overview.zh.md` | target 粒度和测试边界。 |
| correctness_tests | `standalone:doc/correctness-tests.zh.md` | 每个 gtest 的输入、断言和不能证明的范围。 |
| benchmark_and_evidence | `standalone:doc/benchmark-and-evidence.zh.md` | board repeated、manifest、doctor、registry 和提交边界。 |
| optimization_evidence | `standalone:doc/optimization-evidence.zh.md` | candidate 状态和证据索引。 |
| optimization_roadmap | `standalone:doc/optimization-roadmap.zh.md` | 后续搜索空间和默认恢复队列。 |
| test_support_code_map | `standalone:doc/test-support-code-map.zh.md` | helper、bench、script 和 output 定位。 |
| phase_index / result / matrix | `standalone:doc/phases/**` | phase loop 恢复入口和 EvidenceDecision。 |
| production_topic_doc | `standalone:doc-rvv/features/integral_image_normal-RVV.zh.md` | 用户已确认采纳当前 map-prep production patch，长期文档使用 Phase 050 板卡数据。 |

## 生产接入判断

当前判断分两层：

- map-prep 现在是 `adopted weak-positive production-public`。生产 patch 已在 worktree 中，长期文档已创建为 `doc-rvv/features/integral_image_normal-RVV.zh.md`。
- diff-buffer 是 `weak/unstable exact-PCL production-shaped diagnostic / no-production-now`。Phase 040 已补 exact PCL `IntegralImage2D` boundary profile，结果没有证明该片段能穿过 PCL setInput/query 总链路形成稳定收益；只有 production direct evidence 或同边界 RVV-vs-RVV family A/B 明确反转该结论，且用户授权 production patch 时，才建议恢复生产探针讨论。

当前 adopted production behavior 仅限 map-prep 前缀；没有证据支持继续扩大 production patch。
