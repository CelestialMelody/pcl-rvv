# VFH 函数级评估

## 当前结论

`features/include/pcl/features/impl/vfh.hpp` 已采用 Phase 060 的有界 RVV（RISC-V Vector，可变长度向量）
生产路径。`pcl::VFHEstimation<PointInT, PointNT, PointOutT>::computeFeature()` 在默认 VFH
（Viewpoint Feature Histogram，视点特征直方图）公开入口中尝试
`pcl::detail::computeVFHSignatureRVV()`；helper 返回 `false` 时继续执行原标量主体。

当前采用范围是 `PointOutT=pcl::VFHSignature308`、`PointInT` 具备 xyz 单 `float` AoS（结构数组）布局、
`PointNT` 具备 normal 单 `float` AoS 布局、dense full-cloud sequential indices（全点云顺序索引）、
`normalize_bins=true`、`normalize_distances=false`、`size_component=false`，且未使用给定 centroid（质心）或 normal（法线）。
当前不覆盖 CVFH / OUR-CVFH 的给定 centroid / normal 路径、非 dense normals、子集 indices、
`size_component`、`normalize_distances`、`Scalar=double` 或未单独验证的点型 / 参数组合。

Phase 060 的 production-public board evidence（公开生产入口板卡证据）显示：
`production_vfh_compute_default` 5-run checksum 全一致，mean Std `8.81437 ms`、mean RVV `5.37768 ms`、
mean speedup `1.63906x`，Evidence Doctor（证据体检）为 `0E/0W/11S`。按用户确认的采纳口径，板卡生产结果有收益即可采纳，因此当前 EvidenceDecision（证据决策）为 `adopted production behavior`。

## 函数 / 函数族作用速览

| 函数 / 函数族 | 作用 | 输入 / 输出状态 | 当前 RVV 判断 |
| --- | --- | --- | --- |
| `VFHEstimation::compute()` | 公开入口，输出单个 308-bin VFH descriptor（描述子）。 | 读 input / surface / normals / indices，写 `PointCloud<VFHSignature308>`。 | public API（公开接口）不变；调用 `computeFeature()`。 |
| `computeFeature()` | 计算 xyz centroid、normal centroid、SPFH-like histogram、viewpoint histogram 并复制输出。 | 维护 `hist_f_` / `hist_vp_`，最后写 `output[0].histogram`。 | RVV 短路分流位于标量主体之前；失败后走原主体。 |
| `computeVFHSignatureRVV()` | 生产 RVV helper，覆盖当前默认 VFH 边界。 | 读 surface、normals、indices、viewpoint，直接写 308-bin 输出数组。 | 当前采用；内部保留标量顺序 histogram increment。 |
| `computePointSPFHSignature()` | 标量 SPFH-like 组件，计算 f1/f2/f3/f4 并写 4 组 45-bin histogram。 | 读 cloud / normals / indices，写 `hist_f_[0..3]`。 | 保留为 fallback；RVV path 在 production helper 内复刻 f1/f2/f3 默认路径，不覆盖 f4 size component。 |
| `computePairFeatures()` | PFH / FPFH / VFH 共享点对公式。 | 读 centroid / point / normals，写四元组。 | 不直接修改共享 helper；VFH production helper 内使用 batch RVV 公式，避免扩大到其它调用方。 |

## 标量路径重建

标量公开路径先计算 xyz centroid 和 normal centroid。随后 `computePointSPFHSignature()` 逐点调用
`computePairFeatures()`，将 f1/f2/f3 映射到三组 45-bin angular histogram（角度直方图）；若
`size_component_` 打开，还会把 f4 写入第四组 45-bin size component（尺寸分量）。最后
`computeFeature()` 根据 viewpoint 到质心的方向与每个法线的夹角，写 128-bin viewpoint histogram，
再把 45 + 45 + 45 + 45 + 128 个 bin 复制到 `VFHSignature308`。

原标量路径支持给定 centroid / normal、非 dense normals 的有限值过滤、`normalize_distances`、`size_component`
和任意合法 indices。当前 RVV 分流只接管默认 dense full-cloud sequential indices 路径；其它语义仍由标量主体保持。

## 当前采用的优化方式

Phase 060 的生产 RVV 路径分四段：

| 阶段 | RVV 接管内容 | 保留标量内容 | 采用理由 |
| --- | --- | --- | --- |
| centroid | xyz centroid 使用现有 `pcl::compute3DCentroid`；RVV build 可命中 common centroid RVV。 | 给定 centroid 路径回退。 | 复用现有 common helper，避免在 VFH 内重复实现。 |
| normal centroid | `computeVFHNormalCentroidRVV()` 用 strided load（跨步加载）读取 normal_x/y/z，并用 `vfredosum` 规约。 | 非 dense normals 或给定 normal 回退。 | Phase 030 证明 normal centroid 是正向候选。 |
| SPFH-like pair math | `accumulateVFHSPFHRVV()` 用 RVV 计算 centroid-to-point pair math、`atan2_RVV_f32m2` 和 f1/f2/f3 bin index。 | f4 size component 和最终 histogram increment 保持标量。 | 算术密集，Phase 040-060 生产证据显示收益可穿透公开入口。 |
| viewpoint | `accumulateVFHViewpointRVV()` 用 RVV 计算 normal-dot alpha 和 viewpoint bin index。 | 128-bin histogram increment 保持标量顺序。 | 分箱前准备是 O(N) 热点，且不改变输出顺序。 |

直方图加一类更新仍按 lane（向量通道）顺序标量执行。直接并行 scatter（离散写回）会遇到同 bin 冲突和浮点累加顺序风险；当前没有 profile（性能剖析）证据要求为这部分引入私有 bin 或分块合并方案。

## Fallback 矩阵

| 条件 | 当前行为 | 证据 / 理由 |
| --- | --- | --- |
| 非 RVV 构建 | 不编译 RVV helper，直接走标量主体。 | `__RVV10__` 条件编译；Std tests 通过。 |
| `PointOutT` 不是 `VFHSignature308` | helper 编译期返回 `false`。 | production gate 使用 `std::is_same_v`。 |
| `PointInT` 不满足 xyz 单 `float` AoS 布局 | helper 返回 `false`。 | 使用 `pcl::rvv::RVVXYZAoSFloatLayout<PointInT>`。 |
| `PointNT` 不满足 normal 单 `float` AoS 布局 | helper 返回 `false`。 | 使用本地 `VFHNormalAoSFloatLayout<PointNT>`。 |
| cloud / normals 非 dense 或大小不一致 | helper 返回 `false`。 | 原标量主体处理对应语义。 |
| indices 不是 full-cloud sequential | helper 返回 `false`。 | 逐项检查 `indices[i] == i`。 |
| `normalize_bins=false` | helper 返回 `false`。 | `VFHProductionRVV.HelperRejectsNormalizeBinsFallbackBoundary` 覆盖。 |
| `normalize_distances=true` | helper 返回 `false`。 | f4 距离归一化未进入当前证据范围。 |
| `size_component=true` | helper 返回 `false`。 | `VFHProductionRVV.HelperRejectsSizeComponentFallbackBoundary` 覆盖。 |
| 给定 centroid 或 normal | `computeFeature()` 不尝试 RVV。 | 保持 CVFH / OUR-CVFH 调用路径的原语义。 |
| 点数小于 2 或 32-bit byte offset 风险 | helper 返回 `false`。 | gate 检查 `indices.size()` 和 `rvvMaxU32ByteOffsetElements`。 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `VFHEstimation::compute()` | production public entry | 公开入口，分配单个 VFH 输出并调用 `computeFeature()`。 | 用户代码 | `computeFeature()` | production boundary（生产边界）来源 | `features/include/pcl/features/impl/vfh.hpp` |
| `VFHEstimation::computeFeature()` | production dispatch / fallback | 在默认边界先尝试 RVV，失败后执行原标量主体。 | `compute()` | `computeVFHSignatureRVV()` 或标量流程 | production dispatch（生产分流）证据 | `features/include/pcl/features/impl/vfh.hpp` |
| `pcl::detail::computeVFHSignatureRVV()` | production RVV helper | Gate、清零输出、计算 centroid / histogram 并返回是否命中 RVV。 | `computeFeature()` | `computeVFHNormalCentroidRVV()`、`accumulateVFHSPFHRVV()`、`accumulateVFHViewpointRVV()` | adopted production behavior | `features/include/pcl/features/impl/vfh.hpp` |
| `computeVFHNormalCentroidRVV()` | production RVV helper | normal centroid RVV 规约。 | `computeVFHSignatureRVV()` | Eigen vector output | asm attribution（反汇编归属）和 production math | `features/include/pcl/features/impl/vfh.hpp` |
| `accumulateVFHSPFHRVV()` | production RVV helper | f1/f2/f3 pair math 与 bin index precompute。 | `computeVFHSignatureRVV()` | 308-bin histogram buffer | production performance 主路径 | `features/include/pcl/features/impl/vfh.hpp` |
| `accumulateVFHViewpointRVV()` | production RVV helper | viewpoint alpha 与 bin index precompute。 | `computeVFHSignatureRVV()` | 308-bin histogram buffer | production performance 主路径 | `features/include/pcl/features/impl/vfh.hpp` |
| `include/vfh.h` | RVV test asset | topic-local 聚合入口。 | test / bench sources | `include/impl/vfh_reference.hpp` | test support boundary（测试支撑边界） | `test-rvv/features/vfh/include/vfh.h` |
| `src/test_vfh.cpp` | correctness tests | reference、candidate、production helper hit 和 fallback gate。 | `run_test_compare`, `board_smoke` | gtest | correctness gate（正确性验收） | `test-rvv/features/vfh/src/test_vfh.cpp` |
| `src/bench_vfh.cpp` | bench wrapper | 输出 diagnostic case 和 `production_vfh_compute_default`。 | `run_bench_*` / board targets | analyze script | board performance（板卡性能） | `test-rvv/features/vfh/src/bench_vfh.cpp` |
| `script/generate_vfh_evidence_manifest.py` | analysis script | 把 repeated board summary 转成 Evidence Doctor manifest（证据清单）。 | `evidence_doctor_repeated` | `evidence_doctor.py` | evidence validation（证据校验） | `test-rvv/features/vfh/script/generate_vfh_evidence_manifest.py` |
| `log/board/repeated-production-phase060-postreview` | evidence output summary | Phase 060 post-review production-public 5-run 结果和 Doctor。 | board targets | evaluation / doc-rvv | adopted performance evidence | `test-rvv/features/vfh/log/board/repeated-production-phase060-postreview` |
| `doc-rvv/features/vfh-RVV.zh.md` | production topic doc | 长期维护说明，只保存当前 adopted production 行为。 | reviewer / maintainer | production source + evidence paths | long-term production fact | `doc-rvv/features/vfh-RVV.zh.md` |

## 测试和 Bench 入口

| target / case | 层级 | 作用 |
| --- | --- | --- |
| `run_test_compare` | correctness aggregate（正确性汇总入口） | Std / RVV 构建运行 public reference、candidate 和 production helper tests。 |
| `dump_bench_rvv` | asm attribution（反汇编归属） | 构建 RVV bench 并导出反汇编，确认 production helper 与关键 RVV 指令存在。 |
| `board_smoke` | board correctness + bench smoke | 板卡上运行 RVV test 和单次 bench compare，证明可运行和日志形状。 |
| `board_repeated` | board performance evidence（板卡性能证据） | 5-run repeated board compare，是 production speedup 的来源。 |
| `evidence_doctor_repeated` | Evidence Doctor（证据体检） | 从 repeated board summary 生成 manifest 并检查 Error / Warning / Suggestion。 |
| `production_vfh_compute_default` | production-public bench case | 真实 `VFHEstimation::compute()` 公开入口，采纳决策只引用该 case 的 production board 数据。 |

## 当前证据判断

| 证据层 | 结果 | 含义 | 限制 |
| --- | --- | --- | --- |
| correctness | passed | `make -C test-rvv/features/vfh run_test_compare` 通过，Std/RVV 各 9 个测试。 | 只证明当前测试点型、默认 helper 命中、`size_component` / `normalize_bins=false` fallback gate 和 over-range normal 语义，不能外推到所有模板实例。 |
| QEMU | correctness / log-shape only | 支持构建和路径可运行。 | 不提供性能结论。 |
| asm | passed | `dump_bench_rvv` 中 production symbols 可见，`accumulateVFHSPFHRVV` / `accumulateVFHViewpointRVV` 区域含 `vfcvt.rtz.x.f.v`。 | 只能证明指令路径，不证明收益大小。 |
| board repeated | positive production-public | Phase 060 post-review `production_vfh_compute_default` mean speedup `1.63906x`，5-run checksum `448016` 一致。 | 只覆盖 `--side 80 --iterations 8 --warmup 2` 的目标板卡输入。 |
| Evidence Doctor | suggestion only | Phase 060 `0E/0W/11S`，无 Error / Warning。 | 建议补环境 metadata 和 binary identity；不阻塞当前采纳。 |

## 正确性与高效性证据链

| 方面 | 当前证据 | 结论 |
| --- | --- | --- |
| public entry | `computeFeature()` 真实调用 `computeVFHSignatureRVV()`；production helper hit test 覆盖默认边界。 | 当前 RVV 路径不是 test-only diagnostic。 |
| fallback | `size_component` 与 `normalize_bins=false` fallback test；源码 gate 覆盖非 RVV、点型 / layout、dense、indices、其它参数和给定 centroid / normal。 | 非覆盖路径继续使用标量主体。 |
| 数值语义 | RVV bin helper 先 clamp 到 `[0, bins - 1]`，非负后用 `vfcvt.rtz.x.f.v` 等价于 `floor`；histogram increment 保持标量顺序。 | 没有引入并行 scatter 的同 bin 冲突或累加顺序变化。 |
| 性能 | Phase 060 post-review repeated board mean `1.63906x`。 | 目标硬件上公开默认 VFH 路径值得保留。 |
| 证据边界 | Doctor `0E/0W/11S`；QEMU 不用于性能。 | 当前 adopted 范围保持有界。 |

## 文档归属

| 信息类型 | 主归属 | 引用方式 |
| --- | --- | --- |
| 当前 adopted production 行为 | `doc-rvv/features/vfh-RVV.zh.md` | 本文只保留决策审计和边界摘要。 |
| Phase 040-060 取舍 | `doc/phases/*/result.zh.md` 与 `doc/phases/optimization-matrix.zh.md` | 长期文档只引用最终采用和被替代状态。 |
| 测试与 bench 字典 | `doc/testing-overview.zh.md`、`doc/correctness-tests.zh.md`、`doc/benchmark-and-evidence.zh.md` | README 和 doc-rvv 只列入口和关键结果。 |
| 代码地图 | `doc/test-support-code-map.zh.md` | Traceability Map 提供关键路径。 |
| 候选搜索空间 | `doc/optimization-roadmap.zh.md` | Handoff 只引用默认恢复队列。 |

## 后续方向

当前同一 default VFH production boundary 内没有建议继续推进的高优先级优化。剩余 histogram scatter 并行化风险较高，不建议在没有 profile 或专项消融前继续；泛型点类型、`size_component`、`normalize_distances`、非 dense / subset indices、CVFH / OUR-CVFH 调用路径属于范围扩展，需要新 phase 逐项补 correctness、fallback、asm、board 和 Evidence Doctor。
