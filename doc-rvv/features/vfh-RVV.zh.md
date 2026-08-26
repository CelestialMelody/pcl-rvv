# VFH RVV 生产优化说明

## 当前状态

`features/include/pcl/features/impl/vfh.hpp` 中的 `pcl::VFHEstimation<PointInT, PointNT, PointOutT>::computeFeature()`
已采用有界 RVV（RISC-V Vector，可变长度向量）生产路径。公开 API（公开接口）不变；RVV 代码只在
`__RVV10__` 构建中编译，并通过 `pcl::detail::computeVFHSignatureRVV()` 作为短路分流。

当前 adopted production behavior（已采用生产行为）只覆盖默认 VFH（Viewpoint Feature Histogram，视点特征直方图）
公开路径：`PointOutT=pcl::VFHSignature308`，`PointInT` 具备 xyz 单 `float` AoS（结构数组）布局，
`PointNT` 具备 normal 单 `float` AoS 布局，输入 cloud / normals dense，indices 是 full-cloud sequential
indices（全点云顺序索引），`normalize_distances=false`、`size_component=false`，且没有给定 centroid（质心）或
normal（法线）。当前 RVV 路径还要求 `normalize_bins=true`；关闭 bin normalize（分箱归一化）时回退标量。

## 函数语义与标量路径

VFH 输出一个 308-bin descriptor（描述子）。标量路径先计算 xyz centroid 和 normal centroid，再用
`computePointSPFHSignature()` 对每个点计算 centroid-to-point pair features（从质心到点的点对特征），写入四组
45-bin histogram（直方图）：f1/f2/f3 angular features（角度特征）和可选 f4 size component（尺寸分量）。随后
`computeFeature()` 计算 viewpoint 到质心方向与每个 normal 的夹角，写入 128-bin viewpoint histogram，最后把
4 组 45-bin 和 128-bin 复制到 `VFHSignature308`。

原标量路径还处理给定 centroid / normal、非 dense normal 的有限值过滤、`normalize_distances`、
`size_component` 和任意合法 indices。当前 RVV 路径没有改变这些语义；不满足 RVV gate（会失败的准入条件）时继续走标量主体。

## 当前采用的优化方式

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| production dispatch（生产分流） | adopted | `computeFeature()` 在未使用给定 centroid / normal 时尝试 RVV helper，失败后回到原标量主体。 | `VFHProductionRVV.DefaultPublicBoundaryHelperMatchesPublicCompute`；Phase 060 board。 | 公开 API 不变。 |
| xyz centroid | adopted via existing helper | 复用 `pcl::compute3DCentroid`；RVV build 可由 common centroid RVV 覆盖。 | correctness 和 production board 间接覆盖。 | 给定 centroid 回退。 |
| normal centroid | adopted | `computeVFHNormalCentroidRVV()` 用 strided load（跨步加载）读取 normal_x/y/z，并用 RVV reduction（规约）求均值。 | Phase 030 candidate positive；Phase 060 production board。 | 非 dense normals 回退。 |
| SPFH-like pair math | adopted | `accumulateVFHSPFHRVV()` 用 RVV 批量计算 f1/f2/f3 和 `atan2_RVV_f32m2`。 | Phase 060 post-review `production_vfh_compute_default` mean `1.63906x`。 | f4 size component 不覆盖。 |
| bin index precompute（分箱索引预计算） | adopted | RVV 先 clamp 到 `[0, bins - 1]`，再用 `vfcvt.rtz.x.f.v` 转成 `int32` bin。非负输入下向零取整等价于 `floor`。 | `dump_bench_rvv` 中可见 `vfmin.vf`、`vfmax.vf`、`vfcvt.rtz.x.f.v`。 | 直方图更新仍按标量顺序执行。 |
| viewpoint preparation（视点分量准备） | adopted | `accumulateVFHViewpointRVV()` 批量计算 normal-dot alpha 和 viewpoint bin。 | Phase 060 board。 | 只覆盖默认 viewpoint histogram。 |
| histogram scatter（直方图离散累加） | not_now | 同 bin 冲突和浮点累加顺序风险高。 | 当前采用路径已达到 production-public mean `1.63906x`。 | 需要 profile 和私有 bin / 分块合并专项证据后再考虑。 |

## Fallback 矩阵

| 条件 | 行为 | 说明 |
| --- | --- | --- |
| 非 `__RVV10__` 构建 | 标量主体 | RVV helper 不编译。 |
| `PointOutT` 不是 `pcl::VFHSignature308` | 标量主体 | RVV helper 编译期返回 `false`。 |
| `PointInT` 不满足 `pcl::rvv::RVVXYZAoSFloatLayout` | 标量主体 | 不直接按 RVV AoS byte offset 读取未知布局。 |
| `PointNT` 不满足 `VFHNormalAoSFloatLayout` | 标量主体 | normal_x/y/z 必须是单 `float` 且布局可按 AoS 读取。 |
| cloud / normals 非 dense 或大小不一致 | 标量主体 | 保持原标量有限值 / 输入处理语义。 |
| indices 不是 full-cloud sequential | 标量主体 | 当前不做 gather（离散加载）或 subset indices。 |
| `normalize_bins=false` | 标量主体 | 当前 RVV helper 只覆盖默认归一化 histogram 语义；`HelperRejectsNormalizeBinsFallbackBoundary` 覆盖该 gate。 |
| `normalize_distances=true` | 标量主体 | f4 距离归一化未进入当前 RVV 证据范围。 |
| `size_component=true` | 标量主体 | production fallback test 覆盖该 gate。 |
| 使用给定 centroid 或 normal | 标量主体 | CVFH / OUR-CVFH 常用这类路径，当前不接管。 |
| 规模小于 2 或 32-bit byte offset 风险 | 标量主体 | 避免空输入和 RVV 32-bit byte offset 溢出。 |

## 标量路径与 RVV 路径差异

| 标量阶段 | RVV 阶段 | 保留差异 |
| --- | --- | --- |
| `compute3DCentroid()` 计算 xyz centroid。 | 复用同一 public helper；RVV build 可命中 common RVV。 | 给定 centroid 不尝试 RVV。 |
| dense normals 用标量循环求 normal centroid。 | `computeVFHNormalCentroidRVV()` 规约 normal_x/y/z。 | 非 dense normals 回退。 |
| `computePairFeatures()` 逐点计算 f1/f2/f3/f4。 | `accumulateVFHSPFHRVV()` 批量计算 f1/f2/f3。 | f4 size component 回退。 |
| 标量 `floor` / clamp 计算 bin。 | Phase 060 用 RVV clamp + `vfcvt.rtz.x.f.v` 预计算 bin。 | histogram increment 仍标量。 |
| 逐 normal 计算 viewpoint alpha。 | `accumulateVFHViewpointRVV()` 批量计算 alpha 和 bin。 | 输出 histogram 顺序不变。 |
| 复制 `hist_f_` / `hist_vp_` 到输出。 | RVV helper 直接写 308-bin histogram buffer。 | 输出布局不变。 |

## 数值算例

以 45-bin angular feature 为例，标量路径的 bin 计算是：

```text
scaled = 45 * ((feature + pi) / (2 * pi))
bin = clamp(floor(scaled), 0, 44)
```

Phase 060 的 RVV helper 对一个 VL chunk（可变向量长度分块）执行同一规则：

```text
values: [-pi, 0, pi, 4]
scaled before clamp: [0, 22.5, 45, >45]
scaled after clamp: [0, 22.5, 44, 44]
vfcvt.rtz.x.f.v: [0, 22, 44, 44]
```

由于 clamp 后的 scaled 值均非负，`vfcvt.rtz.x.f.v` 的 round-towards-zero（向零取整）与 `floor` 等价。随后 helper
按 lane 顺序执行 `histogram[bin] += hist_incr`，保持原标量累加顺序。

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `VFHEstimation::computeFeature()` | production dispatch / fallback | 默认边界尝试 RVV，失败后标量 fallback。 | `VFHEstimation::compute()` | `computeVFHSignatureRVV()` 或标量主体 | production boundary | `features/include/pcl/features/impl/vfh.hpp` |
| `computeVFHSignatureRVV()` | production RVV helper | Gate、清零输出并组织完整默认 VFH RVV 计算。 | `computeFeature()` | normal centroid / SPFH / viewpoint helper | adopted production behavior | `features/include/pcl/features/impl/vfh.hpp` |
| `computeVFHNormalCentroidRVV()` | production RVV helper | normal centroid reduction。 | `computeVFHSignatureRVV()` | centroid normal vector | asm attribution | `features/include/pcl/features/impl/vfh.hpp` |
| `accumulateVFHSPFHRVV()` | production RVV helper | f1/f2/f3 pair math、bin index precompute、标量 histogram increment。 | `computeVFHSignatureRVV()` | 308-bin histogram | performance hotspot | `features/include/pcl/features/impl/vfh.hpp` |
| `accumulateVFHViewpointRVV()` | production RVV helper | viewpoint alpha、bin index precompute、标量 histogram increment。 | `computeVFHSignatureRVV()` | 308-bin histogram | performance hotspot | `features/include/pcl/features/impl/vfh.hpp` |
| `test_vfh.cpp` | correctness tests | public reference、candidate 和 production fallback tests。 | `run_test_compare` | gtest | correctness gate | `test-rvv/features/vfh/src/test_vfh.cpp` |
| `bench_vfh.cpp` | bench wrapper | `production_vfh_compute_default` 公开入口计时。 | board targets | analyze script | board performance | `test-rvv/features/vfh/src/bench_vfh.cpp` |
| `generate_vfh_evidence_manifest.py` | analysis script | 生成 Evidence Doctor manifest。 | `evidence_doctor_repeated` | `evidence_doctor.py` | evidence validation | `test-rvv/features/vfh/script/generate_vfh_evidence_manifest.py` |
| Phase 060 post-review board output | evidence summary | 5-run production-public board 结果。 | `board_repeated` | evaluation / topic doc | adopted performance evidence | `test-rvv/features/vfh/log/board/repeated-production-phase060-postreview` |
| VFH evaluation | topic-local evaluation | 决策审计、fallback matrix、测试和文档归属。 | reviewer / worker | README / Handoff | decision audit | `test-rvv/features/vfh/doc/vfh-evaluation.zh.md` |

## Bench 与证据

采用决策只引用 `production_vfh_compute_default`。命令：

```bash
make -C test-rvv/features/vfh board_repeated REPEATED_BOARD_OUTPUT_DIR=log/board/repeated-production-phase060-postreview BENCH_ARGS='--side 80 --iterations 8 --warmup 2'
make -C test-rvv/features/vfh evidence_doctor_repeated REPEATED_BOARD_OUTPUT_DIR=log/board/repeated-production-phase060-postreview
```

| run | Std ms | RVV ms | speedup |
| --- | ---: | ---: | ---: |
| run_01 | 8.86645 | 5.36115 | 1.65383x |
| run_02 | 8.82497 | 5.38891 | 1.63762x |
| run_03 | 8.79738 | 5.37904 | 1.63549x |
| run_04 | 8.76886 | 5.38770 | 1.62757x |
| run_05 | 8.81418 | 5.37161 | 1.64088x |
| mean | 8.81437 | 5.37768 | 1.63906x |

5-run checksum 均为 Std/RVV `448016`。Evidence Doctor 结果为 `Errors=0，Warnings=0，Suggestions=11`。
Suggestions 只涉及环境 metadata、binary identity，以及历史 diagnostic baseline 的 near-threshold 提示；不阻塞当前
production-public 结论。

## 正确性与高效性证据链

| 方面 | 证据 | 结论 |
| --- | --- | --- |
| correctness（正确性） | `make -C test-rvv/features/vfh run_test_compare`；Std/RVV 各 9 个测试。 | 默认 production helper 输出与标量 fallback 一致，并覆盖 over-range normal（超范围法线点积）语义。 |
| fallback（回退路径） | `HelperRejectsSizeComponentFallbackBoundary`、`HelperRejectsNormalizeBinsFallbackBoundary` 和源码 gate。 | `size_component` / `normalize_bins=false` 有直接 gtest；其它非覆盖路径由源码 gate 保持标量主体。 |
| asm（反汇编） | `make -B -C test-rvv/features/vfh dump_bench_rvv`；production helper 区域含 `vfcvt.rtz.x.f.v`。 | RVV 分箱索引预计算进入生产符号范围。 |
| performance（性能） | Phase 060 post-review board repeated mean speedup `1.63906x`。 | 目标硬件上默认公开路径值得保留。 |
| boundary（边界） | evaluation 和 matrix 明确未覆盖范围。 | 当前结论不外推到 CVFH / OUR-CVFH、泛型扩展或其它参数。 |

## 生产接入 Closeout

| 项 | 最终状态 | 证据 |
| --- | --- | --- |
| 生产补丁范围 | 只修改 `features/include/pcl/features/impl/vfh.hpp`；新增 `pcl::detail` RVV helper 和 `computeFeature()` 短路分流。 | production diff；`run_test_compare`。 |
| public API | 不改变公开 API、类成员布局或输出类型。 | 源码 diff。 |
| 当前采用方式 | Phase 060 bin index precompute + chunk-local staging + 标量顺序 histogram increment。 | `test-rvv/features/vfh/doc/phases/060-rvv-bin-index-precompute/result.zh.md`。 |
| production direct 证据 | `production_vfh_compute_default` 5-run mean `1.63906x`。 | `test-rvv/features/vfh/log/board/repeated-production-phase060-postreview`。 |
| Evidence Doctor | `0E/0W/11S`。 | `test-rvv/features/vfh/log/board/repeated-production-phase060-postreview/evidence_doctor.md`。 |
| 回退边界 | 非 RVV、非覆盖点型 / layout、非 dense、subset indices、`normalize_bins=false`、`size_component`、`normalize_distances`、给定 centroid / normal 均回退标量。 | fallback 矩阵；直接 gtest 覆盖 `size_component` 与 `normalize_bins=false`。 |
| 继续策略 | 同一默认生产边界暂无值得继续的优化；扩展范围需另起 phase。 | optimization roadmap。 |

## 后续方向

当前不建议继续尝试 histogram scatter RVV。它会引入同 bin 冲突、浮点累加顺序和私有 bin 合并成本；在没有 profile 或专项消融证明它是主瓶颈前，维护风险高于预期收益。

若继续扩大 VFH 覆盖范围，建议拆成独立 phase：点类型 / layout 扩展、`size_component`、`normalize_distances`、非 dense / subset indices、CVFH / OUR-CVFH 给定 centroid / normal 路径。每个扩展都需要重新补 production direct tests、fallback tests、asm、board repeated 和 Evidence Doctor。
