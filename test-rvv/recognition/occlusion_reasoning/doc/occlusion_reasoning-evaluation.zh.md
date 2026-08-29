# Occlusion Reasoning 函数级评估

## 函数级结论

`recognition/include/pcl/recognition/impl/hv/occlusion_reasoning.hpp` 中的
`ZBuffering<ModelT, SceneT>::filter(model, indices, thres)` 已采用 RVV（RISC-V Vector，
可变长度向量扩展）production path。当前证据覆盖 `PointXYZ` / `float` / AoS（数组结构）
布局下的 `filter(model, indices)` 入口；非 RVV 构建、非 dense 输入、非 xyz AoS 布局、
空 `depth_`、超出 32-bit lane index（向量通道下标）表达范围的输入都回退到标量路径。

板卡 production direct（真实生产路径证据）显示 median speedup `1.270x`，5/5 run 没有退化，
checksum 稳定。按本轮用户授权，正向 production direct 结果可采纳，因此当前结论为
`adopted production behavior`。

`recognition/include/pcl/recognition/hv/occlusion_reasoning.h` 中的 public inline
`filter(scene, model, f, threshold)` 与 `getOccludedCloud(scene, model, f, threshold)` 也已采纳
同类 RVV production path。该公共入口的 board repeated 证据显示 median speedup `1.250x`，
5/5 run 没有退化，checksum 稳定；它证明的是 public inline wrapper 的真实生产路径收益，
不外推到 `filter(model, filtered)` 之外的其它 wrapper 成本。

## 范围和目标源码

本评估覆盖 `pcl::occlusion_reasoning::ZBuffering<ModelT, SceneT>` 的两处生产事实：

- `filter(model, indices, thres)`：新增 RVV dispatch（分流逻辑），用 RVV 批量完成投影、
  bounds mask（边界掩码）和候选索引压缩，再用标量 tail（尾段）读取 `depth_` 并保持输出顺序。
- `computeDepthMap()`：修复矩形 depth map 的初始化长度和 stride（步长）索引，使写入和
  `filter()` 读取都使用 `u * cy_ + v`。

`filter(model, filtered, thres)` 仍通过 `filter(model, indices, thres)` 后调用 `copyPointCloud`。
当前 bench 只计时 indices 入口，不把 `copyPointCloud` 成本写入性能结论。

`filter(scene, model, f, threshold)` 和 `getOccludedCloud(scene, model, f, threshold)` 则属于
public inline free function 入口：它们与 `ZBuffering` 共用同一 dispatch 逻辑，但 bench 计时边界
包含 `copyPointCloud`，因此不能直接拿来和 Phase 010 的 indices-only 结果混写。

## 函数族作用速览

| 函数 / 函数族 | 作用 | 输入 / 输出状态 | 与主流程关系 | RVV 判断 |
| --- | --- | --- | --- | --- |
| `ZBuffering::computeDepthMap()` | 从 scene 点云构造内部 `depth_` | 读 scene，写 `depth_` 和 `f_` | `filter()` 的前置状态 | 已修复矩形 stride；未做 RVV |
| `ZBuffering::filter(model, indices)` | 投影 model 点并按 `depth_` 判断可见点 | 读 model 和 `depth_`，写 indices | 本 topic production 入口 | adopted |
| `ZBuffering::filter(model, filtered)` | 调用 indices 版本后 `copyPointCloud` | 读 model，写 filtered cloud | public class wrapper | 间接受益，copy 成本未计入 bench |
| `filter(scene, model, f, threshold)` | 投影 model 点并按 scene depth 判断可见 / 遮挡点 | 读 scene / model，写 filtered cloud | public inline production 入口 | adopted |
| `getOccludedCloud(scene, model, f, threshold)` | 与 public inline `filter()` 共用同一 dispatch，只是 keep_occluded 取反 | 读 scene / model，写 occluded cloud | public inline production 入口 | adopted |

## 标量流程与 RVV 流程对照

| 阶段 | 标量流程 | RVV 流程 | 证据 |
| --- | --- | --- | --- |
| depth map 构建 | `computeDepthMap()` 写 `depth_[u * cy_ + v]` | 保持标量 | `ProductionDepthMapRectangularResolutionRegression` |
| 点投影 | 逐点计算 `f * x / z + cx/cy` | VL chunk（可变向量长度分块）跨步加载 `x/y/z` 后批量除法和转换 | `run_test_compare`、asm gate |
| bounds / finite mask | 逐点判断边界和 depth finite | RVV 先筛掉非 finite / `z==0` 和越界 lane | production direct gtest |
| depth compare | 逐点读取 `depth_` 并比较 `z - thres` | RVV 压缩候选后由标量 tail 读取 depth，保持顺序 append | board summary |
| 输出 | `indices_to_keep` 保序写入 | `vcompress` 暂存候选 index，再按原顺序 push | checksum 一致 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `ZBuffering::filter(model, indices)` | production public entry | 公开入口和 dispatch | `filter(model, filtered)`、上游 HV 调用 | `zBufferingFilterRVV` 或 `zBufferingFilterStd` | production boundary | `recognition/include/pcl/recognition/impl/hv/occlusion_reasoning.hpp` |
| `zBufferingFilterStd` | production Std helper | 保留原标量语义 | fallback dispatch | `indices_to_keep` | fallback coverage | 同上 |
| `zBufferingFilterRVV` | production RVV helper | RVV 投影、mask、compress + 标量 depth tail | RVV dispatch | `indices_to_keep` | adopted RVV path | 同上 |
| `occlusionReasoningFilterStd` | production Std helper | 保留 public inline 原标量语义 | fallback dispatch | `indices_to_keep` | fallback coverage | `recognition/include/pcl/recognition/hv/occlusion_reasoning.h` |
| `occlusionReasoningFilterRVV` | production RVV helper | RVV 投影、mask、compress + 标量 depth tail | RVV dispatch | `indices_to_keep` | adopted RVV path | `recognition/include/pcl/recognition/hv/occlusion_reasoning.h` |
| `ProductionZBufferingFilterHitsRvvPathAndKeepsExpectedIndices` | correctness gate | 验证真实入口命中 RVV 并保持 indices | `run_test_compare` | gtest assertion | production direct correctness | `test-rvv/recognition/occlusion_reasoning/src/test_occlusion_reasoning.cpp` |
| `bench_occlusion_reasoning` | bench wrapper | 计时真实 `filter(model, indices)` | `board_repeated` | summary / manifest | production-public performance | `test-rvv/recognition/occlusion_reasoning/src/bench_occlusion_reasoning.cpp` |
| `ProductionInlineFilterHitsRvvPathAndKeepsExpectedPoints` | correctness gate | 验证 public inline `filter()` 命中 RVV 并保留 filtered points | `run_test_compare` | gtest assertion | production direct correctness | `test-rvv/recognition/occlusion_reasoning/src/test_occlusion_reasoning.cpp` |
| `ProductionInlineGetOccludedCloudHitsRvvPathAndKeepsExpectedPoints` | correctness gate | 验证 public inline `getOccludedCloud()` 命中 RVV 并保留 occluded points | `run_test_compare` | gtest assertion | production direct correctness | `test-rvv/recognition/occlusion_reasoning/src/test_occlusion_reasoning.cpp` |
| `bench_occlusion_reasoning_public_inline` | bench wrapper | 计时 public inline `filter(scene, model, f, threshold)` 完整 wrapper | `inline_board_repeated` | summary / manifest | production-public performance | `test-rvv/recognition/occlusion_reasoning/src/bench_occlusion_reasoning_public_inline.cpp` |
| `generate_occlusion_reasoning_evidence_manifest.py` | analysis script | 生成 summary 和 Evidence Doctor manifest | Makefile evidence target | doctor / registry | evidence summary | `test-rvv/recognition/occlusion_reasoning/script/generate_occlusion_reasoning_evidence_manifest.py` |
| `repeated_phase010_production_direct/summary.md` | evidence output summary | 5-run board 结果 | board target | evaluation / doc-rvv | board performance | `test-rvv/recognition/occlusion_reasoning/log/board/repeated_phase010_production_direct/summary.md` |
| `repeated_phase020_inline_filter_production_direct/summary.md` | evidence output summary | 5-run board 结果 | board target | evaluation / doc-rvv | board performance | `test-rvv/recognition/occlusion_reasoning/log/board/repeated_phase020_inline_filter_production_direct/summary.md` |

## 实现方式审计

| 实现维度 | 当前状态 | 证据 | 边界 / 恢复条件 |
| --- | --- | --- | --- |
| dispatch / fallback | adopted | `run_test_compare`、production hook | 只在 `__RVV10__` + eligible layout + dense + valid depth 时走 RVV |
| layout / traits gate | adopted | `RVVXYZAoSFloatLayout<ModelT>` | 不外推到其它字段布局或 `Scalar=double` |
| staging / compress | adopted | asm gate、board positive | RVV 压缩候选 index/u/v/z，depth 读取和 append 保持标量 |
| depth map 构建 | adopted correctness fix | rectangular regression | 仅修复 stride / 初始化；未向量化 scatter/min |
| public inline production scope | adopted production direct | Phase 020 result、doc-rvv | 关闭 public inline `filter()` / `getOccludedCloud()` 当前范围 |
| ZBuffering production scope | adopted production direct | Phase 010 result、doc-rvv | 关闭 `filter(model, indices)` 当前范围 |

## 验证结果

| 证据 | 结果 | 说明 |
| --- | --- | --- |
| correctness | pass | `make -C test-rvv/recognition/occlusion_reasoning run_test_compare` |
| QEMU smoke | pass | `run_qemu_smoke` 等价运行 correctness；不作为性能证据 |
| public inline asm | pass | `check_inline_filter_rvv_asm` 命中 `vlse32` / `vlsseg3e32`、`vfdiv`、`vfcvt` |
| ZBuffering asm | pass | `check_occlusion_filter_rvv_asm` 命中 `vlse32` / `vlsseg3e32`、`vfdiv`、`vfcvt` |
| public inline board repeated | positive | median `1.250x`，min/max `1.240x/1.260x`，`B/A < 1 = 0/5` |
| ZBuffering board repeated | positive | median `1.270x`，min/max `1.260x/1.270x`，`B/A < 1 = 0/5` |
| Evidence Doctor | clean enough | Phase 020 与 Phase 010 都是 `Errors=0`、`Warnings=0`、`Suggestions=1`；建议补环境 metadata |
| registry | fresh | `log/evidence_registry.json` 已记录 Phase 020 / Phase 010 summary / manifest / doctor |

## 生产接入后的最终证据更新

| 字段 | 当前事实 |
| --- | --- |
| `production_patch_scope` | 修改 `recognition/include/pcl/recognition/impl/hv/occlusion_reasoning.hpp`，新增 Std/RVV helper、test hook、RVV dispatch，并修复 depth map 矩形 stride |
| `covered_path` | `ZBuffering::filter(model, indices, thres)`，`PointXYZ` / `float` / AoS，dense model，已构造 `depth_`，board target |
| `fallback_matrix` | 非 RVV 构建、非 eligible layout、非 dense、空 `depth_`、超大输入回退标量；`filter(model, filtered)` 仍经 indices 后 copy |
| `production_direct_tests` | `run_test_compare` 中真实入口 path-hit 和矩形 depth map regression |
| `production_asm` | `check_occlusion_filter_rvv_asm` 通过，归属到 production bench hot path |
| `production_board_bench` | `repeated_phase010_production_direct/summary.md` positive |
| `decision_delta` | Phase 000 diagnostic 的 1.9x 被 production direct 缩窄为 1.27x；最终结论以 production direct 为准 |
| `public_inline_patch_scope` | 修改 `recognition/include/pcl/recognition/hv/occlusion_reasoning.h`，新增 public inline dispatch、Std/RVV helper 和 test hook |
| `public_inline_covered_path` | `filter(scene, model, f, threshold)` 与 `getOccludedCloud(scene, model, f, threshold)`，`PointXYZ` / `float` / AoS / dense |
| `public_inline_fallback_matrix` | 非 RVV 构建、非 eligible layout、非 dense、空 scene 分辨率、超大输入回退标量 |
| `public_inline_tests` | `run_test_compare` 中 public inline path-hit 和输出点验证 |
| `public_inline_asm` | `check_inline_filter_rvv_asm` 通过，归属到 public inline bench hot path |
| `public_inline_board_bench` | `repeated_phase020_inline_filter_production_direct/summary.md` positive |
| `public_inline_decision_delta` | Phase 020 以 public wrapper 边界重测，median `1.250x`；该数据成为 public inline adopted 结论的当前 truth |

## 文档归属与 closeout

长期 production 行为、当前采用方式、fallback 矩阵和正确性与高效性证据链归属
`doc-rvv/recognition/occlusion_reasoning-RVV.zh.md`。本 evaluation 只保留决策审计、阶段证据入口
和未覆盖范围；阶段过程归属 `doc/phases/**`，board 统计主归属是 summary / Evidence Doctor。

## 后续方向

当前 topic 没有继续推进的高优先级未阻塞方向。`depth-gather-rvv`、`smooth-window-min-rvv` 或
更宽点类型扩展都需要另起 production boundary、补专门的 correctness / asm / board / doctor 证据；
它们不应在本轮 adopted path 上继续叠加。
