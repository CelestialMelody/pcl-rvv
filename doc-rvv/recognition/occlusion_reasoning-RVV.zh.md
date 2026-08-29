# Occlusion Reasoning RVV 生产接入说明

## 当前状态

`recognition/include/pcl/recognition/hv/occlusion_reasoning.h` 中的 public inline
`pcl::occlusion_reasoning::filter(scene, model, f, threshold)` 与
`getOccludedCloud(scene, model, f, threshold)` 已采纳 RVV（RISC-V Vector，可变长度向量扩展）
production behavior（生产行为）。当前采用路径只覆盖 `PointXYZ` 风格的 `float` xyz AoS
（数组结构）布局、`__RVV10__` 可用且 model cloud 为 dense 的公共入口；其它布局、
非 RVV 构建、非 dense 或超出 32-bit lane index（向量通道下标）表达范围时回退标量。

`recognition/include/pcl/recognition/impl/hv/occlusion_reasoning.hpp` 中的
`pcl::occlusion_reasoning::ZBuffering<ModelT, SceneT>::filter(model, indices, thres)` 也已采纳
同类 RVV production behavior。该路径只覆盖 `ModelT` 满足 `RVVXYZAoSFloatLayout<ModelT>`、
`depth_` 已构建且 `model.is_dense` 为真的 indices 入口；`filter(model, filtered)` 仍通过 indices
后调用 `copyPointCloud`。

Phase 020 public inline board repeated summary 显示 median `1.250x`、`0/5` 退化，checksum 稳定。
Phase 010 ZBuffering board repeated summary 显示 median `1.270x`、`0/5` 退化，checksum 稳定。
按用户授权，这两个正向 production direct（真实生产路径）结果均作为采纳依据。

## 函数语义

public inline `filter()` 和 `getOccludedCloud()` 从 organized scene cloud 中按 `(u, v)` 读取 scene
depth，再把 model 点投影到同一像素位置。标量路径先算
`u = f * x / z + cx`、`v = f * y / z + cy`，越界或 invalid depth（非法深度）时跳过，再根据
`z - z_oc > threshold` 判断遮挡关系。`filter()` 保留未遮挡点，`getOccludedCloud()` 保留遮挡点，
最后都用 `copyPointCloud` 输出 filtered cloud。

`ZBuffering` 先用 `computeDepthMap()` 从 scene 构建内部 `depth_`，再由
`filter(model, indices, thres)` 投影 model 点并按 `depth_[u * cy_ + v]` 判断可见点。本轮生产补丁
还固定了矩形 depth map 的初始化和 stride（步长）索引语义，使写入和读取都使用同一索引形式。

## 当前采用的优化方式

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| public inline dispatch / fallback | adopted | `__RVV10__` + xyz AoS gate 命中时走 RVV helper，否则回退标量 | `run_test_compare`、Phase 020 board repeated | 只覆盖 public inline `filter()` / `getOccludedCloud()` |
| public inline wrapper | adopted | board bench 包含 `copyPointCloud` 后仍有稳定收益 | Phase 020 median `1.250x` | 不外推到其它 wrapper |
| ZBuffering dispatch / fallback | adopted | `__RVV10__` + xyz AoS gate 命中时走 RVV helper，否则回退标量 | `run_test_compare`、Phase 010 board repeated | 只覆盖 `filter(model, indices)` |
| RVV chunk 计算 | adopted | VL chunk（可变向量长度分块）里批量 load `x/y/z`、做 finite / bounds mask、投影和截断 | asm / board / correctness | 只处理 `float` xyz AoS |
| `vcompress` staging | adopted | 先压缩候选索引和中间 `u/v/z`，再做标量 depth tail | asm / board positive | 保持保序 append |
| scalar depth tail | adopted | scene depth / internal depth 读取仍由标量完成，避免扩大状态机风险 | correctness / board | depth gather 需要新边界 |
| `computeDepthMap()` | adopted correctness fix | 矩形初始化 / stride 语义与读取一致 | regression | 只作为当前 production patch 的组成部分 |
| smooth window | deferred | `computeDepthMap(smooth=true)` 是独立邻域 min 形态 | 当前未覆盖 | 需要 caller 证据 |

## 生产实现细节

两条 adopted RVV helper 都采用“前置投影 + 压缩暂存 + 标量尾段”：

1. 每次取一个 VL chunk。
2. 用 strided load 读出 `x/y/z`。
3. 构造 finite / bounds mask；public inline 路径还拒绝 `z == 0`。
4. 用 `vcompress` 把有效 lane 的 `index/u/v/z` 压到连续暂存区。
5. 标量 tail 逐 lane 查 scene depth 或内部 `depth_`，保持和标量实现一致的保序输出。

这个形态没有把所有逻辑都强行向量化，而是把规则的投影和筛选阶段交给 RVV，把带对象状态和
invalid depth 语义的读取留在标量尾段。这样能减少主循环算术和分支成本，同时保住输出顺序。

### VL chunk 数值图示

```text
lane:      0    1    2    3
x/y/z:    load -> finite/bounds -> project -> compress
keep?:     1    0    1    1
tmp idx:  [i0,      i2,  i3]
tmp u/v:  [u0,      u2,  u3]
scalar tail:
  scene_depth(u0, v0) -> keep / drop
  scene_depth(u2, v2) -> keep / drop
  scene_depth(u3, v3) -> keep / drop
```

一个 lane 是否最终保留，不只看投影是否合法，还要看 depth compare 结果；所以压缩暂存只负责
把候选 lane 排好，最终语义仍由标量尾段闭合。

## 覆盖范围与 fallback

| 范围 | 当前状态 | 证据 / 原因 |
| --- | --- | --- |
| `filter(scene, model, f, threshold)` | adopted | Phase 020 production direct board positive |
| `getOccludedCloud(scene, model, f, threshold)` | adopted | 正确性测试命中同一 dispatch；性能边界由 public inline filter 代表 |
| `ZBuffering::filter(model, indices, thres)` | adopted | Phase 010 production direct board positive |
| `ZBuffering::filter(model, filtered, thres)` | 间接受益 | 仍会额外走 `copyPointCloud`，未单独计时 |
| `computeDepthMap()` | adopted correctness fix | 矩形 stride 语义与读取对齐 |
| 非 RVV 构建 | scalar fallback | 条件编译直接回到 Std helper |
| 非 dense / 空 scene 分辨率 / 超大输入 | scalar fallback | helper 里显式 gate |
| 非 xyz AoS layout | scalar fallback | `RVVXYZAoSFloatLayout<ModelT>` gate 未命中 |
| `PointNormal` / `PointXYZRGB` / 其它点型 | not covered | 需要新 phase 的 point-type expansion |
| `Scalar != float` | not covered | 当前证据只覆盖 float |

## 数值算例

取一个 lane 的投影结果：

```text
x = 0.50, y = -0.25, z = 2.00
f = 100.0, cx = 3.5, cy = 2.5
u = floor(100 * 0.50 / 2.00 + 3.5) = 28
v = floor(100 * -0.25 / 2.00 + 2.5) = -10
```

这个 lane 会在 bounds mask 被剔除，不会进入 depth compare。真正进入尾段的 lane，才会继续做
scene depth 或 `depth_` 比较。

## Bench cases

| phase | case | 数据 | 计时边界 | 证明点 | 不能证明 |
| --- | --- | --- | --- | --- | --- |
| 020 | `public_inline_filter_visible_points` | `65536` 模型点，`150` / `150` / `200` 迭代参数，`5` warmup，`5` runs | `production_public_inline_filter_includes_copyPointCloud_excludes_scene_model_setup` | public inline RVV path 是否快于 public inline scalar path | 其它点型、其它 wrapper、RVV-family-selection |
| 010 | `production_filter_indices_projection_mask_compress` | 同规模 | `production_public_filter_indices_after_computeDepthMap_setup_excludes_copyPointCloud` | `ZBuffering::filter(indices)` RVV path 是否快于 scalar path | `copyPointCloud`、smooth depth window、其它点型 |

## 正确性与高效性证据链

| 层级 | 当前结果 | 能证明什么 | 不能证明什么 |
| --- | --- | --- | --- |
| correctness | `run_test_compare` pass | 公开入口 Std/RVV 输出一致，RVV build 命中 production hook | 不证明板卡性能 |
| QEMU path | `run_qemu_smoke` pass | 构建、日志形状、路径命中 | 不能当性能结论 |
| public inline asm | `check_inline_filter_rvv_asm` pass | public inline hot path 中有 RVV load / convert / divide 指令 | 不能单独证明收益 |
| ZBuffering asm | `check_occlusion_filter_rvv_asm` pass | ZBuffering hot path 中有 RVV load / convert / divide 指令 | 不能单独证明收益 |
| public inline board repeated | median `1.250x`，`0/5` 退化 | 当前 public inline RVV path 快于 scalar path | 不覆盖其它入口 / 新 family |
| ZBuffering board repeated | median `1.270x`，`0/5` 退化 | 当前 ZBuffering RVV path 快于 scalar path | 不覆盖 copy wrapper / 新 family |
| Evidence Doctor | `Errors=0 / Warnings=0 / Suggestions=1` | 结果没有阻塞异常 | 环境 metadata 仍建议补齐 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `filter(scene, model, f, threshold)` | production public entry | public inline visible filter | 上游调用者 | `occlusionReasoningFilterRVV` / `occlusionReasoningFilterStd` | production boundary | `recognition/include/pcl/recognition/hv/occlusion_reasoning.h` |
| `getOccludedCloud(scene, model, f, threshold)` | production public entry | public inline occluded filter | 上游调用者 | `occlusionReasoningFilterRVV` / `occlusionReasoningFilterStd` | production boundary | 同上 |
| `occlusionReasoningFilterStd` | production Std helper | public inline 标量 truth | fallback dispatch | `indices_to_keep` | fallback coverage | 同上 |
| `occlusionReasoningFilterRVV` | production RVV helper | 投影、mask、compress、标量尾段 | RVV dispatch | `indices_to_keep` | adopted RVV path | 同上 |
| `ZBuffering::filter(model, indices)` | production public entry | class member dispatch | `filter(model, filtered)`、上游 HV 调用 | `zBufferingFilterRVV` / `zBufferingFilterStd` | production boundary | `recognition/include/pcl/recognition/impl/hv/occlusion_reasoning.hpp` |
| `zBufferingFilterStd` | production Std helper | 标量 truth | fallback dispatch | `indices_to_keep` | fallback coverage | 同上 |
| `zBufferingFilterRVV` | production RVV helper | 投影、mask、compress、标量尾段 | RVV dispatch | `indices_to_keep` | adopted RVV path | 同上 |
| `ProductionInlineFilterHitsRvvPathAndKeepsExpectedPoints` | correctness gate | public inline filter path-hit | `run_test_compare` | gtest assert | correctness gate | `test-rvv/recognition/occlusion_reasoning/src/test_occlusion_reasoning.cpp` |
| `ProductionInlineGetOccludedCloudHitsRvvPathAndKeepsExpectedPoints` | correctness gate | public inline getOccludedCloud path-hit | `run_test_compare` | gtest assert | correctness gate | 同上 |
| `ProductionZBufferingFilterHitsRvvPathAndKeepsExpectedIndices` | correctness gate | ZBuffering path-hit | `run_test_compare` | gtest assert | correctness gate | 同上 |
| `bench_occlusion_reasoning_public_inline` | bench wrapper | public inline 生产直连计时 | `inline_board_repeated` | summary / manifest | production-public performance | `test-rvv/recognition/occlusion_reasoning/src/bench_occlusion_reasoning_public_inline.cpp` |
| `bench_occlusion_reasoning` | bench wrapper | ZBuffering 生产直连计时 | `board_repeated` | summary / manifest | production-public performance | `test-rvv/recognition/occlusion_reasoning/src/bench_occlusion_reasoning.cpp` |
| `generate_occlusion_reasoning_evidence_manifest.py` | analysis script | summary / manifest / doctor 生成 | Make target | Evidence Doctor / registry | evidence metadata | `test-rvv/recognition/occlusion_reasoning/script/generate_occlusion_reasoning_evidence_manifest.py` |
| `repeated_phase020_inline_filter_production_direct/summary.md` | evidence output summary | Phase 020 5-run board 结果 | board target | evaluation / this doc | board performance | `test-rvv/recognition/occlusion_reasoning/log/board/repeated_phase020_inline_filter_production_direct/summary.md` |
| `repeated_phase010_production_direct/summary.md` | evidence output summary | Phase 010 5-run board 结果 | board target | evaluation / this doc | board performance | `test-rvv/recognition/occlusion_reasoning/log/board/repeated_phase010_production_direct/summary.md` |

## 生产接入后的范围决策表

| 范围 | 当前状态 | 原因 | 下一步 |
| --- | --- | --- | --- |
| public inline `filter()` | adopted | Phase 020 production direct board positive | none |
| public inline `getOccludedCloud()` | adopted | 正确性 path-hit 通过，共用 dispatch | none |
| `ZBuffering::filter(model, indices)` | adopted | Phase 010 production direct board positive | none |
| `ZBuffering::filter(model, filtered)` | deferred | `copyPointCloud` 类 wrapper 成本未单独证明 | 若要继续，另开 wrapper phase |
| `computeDepthMap()` | adopted correctness fix | 矩形 stride / 初始化已对齐 | none |
| 其它点类型 / layout / `Scalar` | deferred | 未在本轮证据内 | 新 phase |
| depth-gather RVV | deferred | 需要新 production boundary 和 RVV-vs-RVV A/B | 新 phase / 新 topic |
| smooth window RVV | deferred | 需要 caller 证据 | 新 phase / 新 topic |

## 遗留风险与后续条件

- 当前证据只覆盖 `PointXYZ` 风格 AoS float model。
- `ZBuffering::filter(model, filtered)` 的类 wrapper `copyPointCloud` 成本未进入 Phase 010 结论。
- public inline bench 已覆盖其自身的 `copyPointCloud`，但不能外推到其它 wrapper 或上游调用组合。
- 继续向量化 depth compare 或 smooth window，都会把当前 production boundary 扩大成新 topic / 新 phase。
- 环境 metadata 还建议补 `taskset`、`governor`、`freq`、`temperature`。
