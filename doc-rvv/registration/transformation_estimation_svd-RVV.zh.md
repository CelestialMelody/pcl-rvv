# registration/transformation_estimation_svd RVV production 说明

## 当前状态

`TransformationEstimationSVD<PointSource, PointTarget, Scalar>` 当前 EvidenceDecision（证据决策）为
`production-ready / adopted for ordered-cloud-pair, source-indexed-cloud-pair, dual-indices-cloud-pair and correspondence-pair`。
真实 production（生产源码）路径在 `registration/include/pcl/registration/impl/transformation_estimation_svd.hpp`
中接入四条 public overload（公开重载）：

| row source policy（行来源策略） | public overload | 当前 production RVV |
| --- | --- | --- |
| ordered-cloud-pair（顺序点云对） | `estimateRigidTransformation(cloud_src, cloud_tgt, matrix)` | adopted。`source[i]` 与 `target[i]` 配对。 |
| source-indexed-cloud-pair（源索引点云对） | `estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, matrix)` | adopted。`source[indices_src[i]]` 与 `target[i]` 配对。 |
| dual-indices-cloud-pair（双索引点云对） | `estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, indices_tgt, matrix)` | adopted。`source[indices_src[i]]` 与 `target[indices_tgt[i]]` 配对。 |
| correspondence-pair（对应关系点对） | `estimateRigidTransformation(cloud_src, cloud_tgt, correspondences, matrix)` | adopted。`source[corr.index_query]` 与 `target[corr.index_match]` 配对。 |

四条路径的共同 production gate（生产门控）是 `__RVV10__`、`Scalar=float`、
`use_umeyama_ == true`、source/target dense、`n >= 16`，并且 source 和 target 分别满足
`pcl::rvv::RVVXYZAoSFloatLayout`。source-indexed 还要求 `indices_src.size() == cloud_tgt.size()`、
source index 合法、source cloud 大小满足 32-bit byte offset gather（32 位字节偏移离散加载）边界。
dual-indices 还要求两侧 indices 等长且全部合法。correspondence 还要求 query/match 全部合法。
gate 失败时，公开入口进入原 `ConstCloudIterator` 标量路径。

`full-cloud` 仅作为 Phase 000/010 的历史 case-filter、日志目录或 legacy alias（历史别名）保留。
当前文档、矩阵和恢复入口统一使用 `ordered-cloud-pair`。

## 函数语义

`TransformationEstimationSVD` 从 source 点云到 target 点云估计刚体变换矩阵。默认
`use_umeyama_ == true` 时，原标量路径把 source/target xyz 装入两个 `3 x N`
Eigen 动态矩阵，再调用 `pcl::umeyama(src, tgt, false)` 得到 no-scale rigid transform
（不带尺度的刚体变换）。

RVV path 保持相同 no-scale 语义。它不构造两个动态矩阵，而是在 row source 遍历中直接累加：

- source xyz sum；
- target xyz sum；
- target-source cross sum。

随后 `solveTransformationEstimationSVDF32` 用这些累加量构造 `3 x 3` covariance（协方差）矩阵，
并保留 Eigen `JacobiSVD` 求解 3x3 tail（后段）。SVD tail 每次 estimate 只执行一次，当前不手写 RVV。

当 `use_umeyama_ == false` 时，原标量路径仍按 centroid（质心）、demean（去均值）、
correlation（相关矩阵）和 3x3 SVD 执行。RVV helper 不接管这条分支。

## Dispatch 与 fallback

四个公开入口都保留原有数量检查或 correspondence 空输入检查。检查通过后，`__RVV10__` 构建才尝试对应
`pcl::registration::detail` RVV helper；helper 返回 `false` 时，公开入口继续构造 `ConstCloudIterator`
并调用现有标量 helper。

```text
estimateRigidTransformation(public overload)
  -> 原有 size / indices / correspondence 检查
  -> if __RVV10__:
       try estimateRigidTransformationSVD*RVV(...)
       if true return
  -> ConstCloudIterator 标量路径
```

fallback 矩阵：

| fallback 场景 | 触发条件 | 当前行为 / 验证 |
| --- | --- | --- |
| non-RVV build | 未定义 `__RVV10__` | 不编译 RVV helper，Std 构建走原标量路径；QEMU Std 22/22 通过。 |
| unsupported `Scalar` | `Scalar != float` | helper 编译期返回 `false`；`Scalar=double` fallback 由 production direct tests 覆盖。 |
| non-Umeyama mode | `use_umeyama_ == false` | 保持 centroid / demean / correlation 标量路径；fallback tests 覆盖。 |
| small input | `n < 16` | 回到标量路径，避免短数组 RVV setup cost（向量设置成本）；fallback tests 覆盖。 |
| non-dense cloud | `!cloud_src.is_dense` 或 `!cloud_tgt.is_dense` | 回到标量路径；fallback tests 覆盖。 |
| incompatible xyz AoS layout | source 或 target 不满足 `RVVXYZAoSFloatLayout` | 编译期不进入 RVV helper。 |
| ordered size mismatch | `cloud_src.size() != cloud_tgt.size()` | 保持原 public overload 错误处理，不尝试 RVV。 |
| source-indexed count mismatch | `indices_src.size() != cloud_tgt.size()` | 保持原 public overload 错误处理，不尝试 RVV。 |
| source-indexed invalid index | source index 为负数或越界 | RVV helper 返回 `false`，进入 iterator 标量路径。 |
| source-indexed 32-bit offset overflow | source cloud byte offset 超过 gather 可表达范围 | RVV helper 返回 `false`，进入 iterator 标量路径。 |
| dual-indices count mismatch | `indices_src.size() != indices_tgt.size()` | 保持原 public overload 错误处理，不尝试 RVV。 |
| dual-indices invalid index | source 或 target index 为负数或越界 | RVV helper 返回 `false`，进入 iterator 标量路径。 |
| correspondence invalid query/match | query 或 match 为负数或越界 | RVV helper 返回 `false`，进入 iterator 标量路径。 |

## 当前采用的优化方式

生产源码新增的 helper 分成 dispatch gate（分流门控）、RVV accumulation（向量累加）和 3x3 solve 三类：

| helper | 职责 |
| --- | --- |
| `estimateRigidTransformationSVDOrderedCloudPairRVV` | ordered-cloud-pair gate 和 public overload 分流。 |
| `accumulateTransformationEstimationSVDOrderedCloudPairRVV` | source/target 两侧用 `strided_load3_f32m2` 读取 xyz，并累加 sum/cross sum。 |
| `estimateRigidTransformationSVDSourceIndexedCloudPairRVV` | source-indexed gate，验证 source index、32-bit gather 范围、dense、layout、`Scalar` 和 `use_umeyama_`。 |
| `accumulateTransformationEstimationSVDSourceIndexedCloudPairRVV` | source 侧用 `indexed_load3_f32m2`，target 侧用 `strided_load3_f32m2`，并累加 sum/cross sum。 |
| `estimateRigidTransformationSVDDualIndicesCloudPairRVV` | dual-indices gate，验证两侧 index、dense、layout、`Scalar` 和 `use_umeyama_`。 |
| `accumulateTransformationEstimationSVDDualIndicesCloudPairRVV` | source/target 两侧都用 `indexed_load3_f32m2`，并累加 sum/cross sum。 |
| `estimateRigidTransformationSVDCorrespondencePairRVV` | correspondence gate，验证 query/match、dense、layout、`Scalar` 和 `use_umeyama_`。 |
| `accumulateTransformationEstimationSVDCorrespondencePairRVV` | 按 correspondence query/match gather 读取 source/target xyz，并累加 sum/cross sum。 |
| `solveTransformationEstimationSVDF32` | 从 fused sums 构造 no-scale Umeyama 矩阵；3x3 SVD 仍走 Eigen。 |

访存使用公共 `pcl::rvv_load` wrapper（封装）。ordered path 两侧都是跨步 segment load
（分段加载）；source-indexed path 只在 source 侧 gather，target 侧保持连续跨步读取；dual-indices 和
correspondence path 两侧都 gather。source 和 target 各自使用 `RVVXYZAoSFloatLayout<PointSource/PointTarget>`
提供的 offset、stride 和 POD layout 前提，不复用 `PointXYZ` 的固定 offset。

单个 VL chunk（可变向量长度分块）的主流程是：

```text
vsetvl_e32m2
  -> load/gather source xyz
  -> load/gather target xyz
  -> accumulate source sum, target sum, target-source cross sum
  -> chunk partial sums 进入最终 3x3 solve
```

当前实现采用 fused accumulation（融合累加）替代原 Umeyama 动态矩阵装填。它没有改变 public API，
没有改变 3x3 SVD tail，也没有改变 out-of-scope gate 的标量语义。

## 范围决策表

| 范围 | 决策 | 证据和边界 |
| --- | --- | --- |
| ordered-cloud-pair / `Scalar=float` / dense xyz AoS | adopted | production direct correctness、asm attribution、board repeated 和 Evidence Doctor 均闭合。 |
| source-indexed-cloud-pair / `Scalar=float` / dense xyz AoS / valid source index | adopted | production direct correctness、source gather asm、board repeated 和 Evidence Doctor 均闭合。 |
| dual-indices-cloud-pair / `Scalar=float` / dense xyz AoS / valid dual indices | adopted | production direct correctness、dual gather asm、board repeated 和 Evidence Doctor 均闭合。 |
| correspondence-pair / `Scalar=float` / dense xyz AoS / valid query-match | adopted | production direct correctness、correspondence gather asm、board repeated 和 Evidence Doctor 均闭合。 |
| `PointXYZI` / `PointXYZRGB` 等 gate-allowed xyz AoS 点型 | adopted with representative performance | 四条 row source 的 production direct correctness/path-hit 覆盖代表性 mixed-field 点型；性能只由 `PointXYZ` board cases 代表。 |
| 其它满足 `RVVXYZAoSFloatLayout` 的 xyz AoS 点型 | layout-gated production path | gate 命中时可进入 RVV；未逐类型上板，不能写成逐类型性能已证明。 |
| `Scalar=double` | fallback | 当前无 double RVV 数值预算和性能计划；double fallback tests 保护标量路径。 |
| non-dense 或包含 NaN / Inf 的输入 | fallback | 当前 RVV gate 要求 dense；非 dense 保持原 iterator 标量语义。 |
| `use_umeyama_ == false` | fallback | centroid / demean / correlation 分支保持标量。 |
| SVD tail | scalar retained | 每次 estimate 固定一次 3x3 Eigen SVD，当前不作为 RVV 热点。 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- |
| `TransformationEstimationSVD::estimateRigidTransformation(cloud_src, cloud_tgt, matrix)` | production public entry | ordered-cloud-pair 入口，尝试 ordered RVV 后 fallback。 | production direct boundary | `registration/include/pcl/registration/impl/transformation_estimation_svd.hpp` |
| `TransformationEstimationSVD::estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, matrix)` | production public entry | source-indexed 入口，尝试 source gather RVV 后 fallback。 | production direct boundary / gather gate | `registration/include/pcl/registration/impl/transformation_estimation_svd.hpp` |
| `TransformationEstimationSVD::estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, indices_tgt, matrix)` | production public entry | dual-indices 入口，尝试双 gather RVV 后 fallback。 | production direct boundary / dual gather gate | `registration/include/pcl/registration/impl/transformation_estimation_svd.hpp` |
| `TransformationEstimationSVD::estimateRigidTransformation(cloud_src, cloud_tgt, correspondences, matrix)` | production public entry | correspondence 入口，尝试 query/match gather RVV 后 fallback。 | production direct boundary / correspondence gate | `registration/include/pcl/registration/impl/transformation_estimation_svd.hpp` |
| `detail::estimateRigidTransformationSVD*RVV` helpers | production dispatch | `Scalar`、dense、layout、size、row-source gate 和 fallback。 | fallback coverage | `registration/include/pcl/registration/impl/transformation_estimation_svd.hpp` |
| `detail::accumulateTransformationEstimationSVD*RVV` helpers | production RVV accumulation | stride/gather load 和 fused sum/cross-sum accumulation。 | asm attribution / board performance | `registration/include/pcl/registration/impl/transformation_estimation_svd.hpp` |
| `detail::solveTransformationEstimationSVDF32` | production scalar tail | 从累加量构造 3x3 covariance 并调用 Eigen SVD。 | numerical boundary | `registration/include/pcl/registration/impl/transformation_estimation_svd.hpp` |
| `test-rvv/registration/transformation_estimation_svd/src/test_tesvd.cpp` | correctness test | public Umeyama、四条 row source、path-hit、fallback 和 representative point types。 | correctness gate | `test-rvv/registration/transformation_estimation_svd/src/test_tesvd.cpp` |
| `test-rvv/registration/transformation_estimation_svd/src/bench_tesvd.cpp` | bench wrapper | production direct public Std/RVV 和 historical diagnostic cases。 | board bench / QEMU log shape | `test-rvv/registration/transformation_estimation_svd/src/bench_tesvd.cpp` |
| `script/generate_tesvd_board_repeated_summary.py` | analysis script | 生成 board summary、manifest 和 Evidence Doctor 输入。 | evidence summary | `test-rvv/registration/transformation_estimation_svd/script/generate_tesvd_board_repeated_summary.py` |
| `log/evidence_registry.json` | evidence registry | 记录 summary / manifest / doctor freshness。 | evidence freshness | `test-rvv/registration/transformation_estimation_svd/log/evidence_registry.json` |
| `doc/transformation_estimation_svd-evaluation.zh.md` | topic-local evaluation | 候选取舍、row source 状态、Traceability Map 和最终 EvidenceDecision。 | decision audit | `test-rvv/registration/transformation_estimation_svd/doc/transformation_estimation_svd-evaluation.zh.md` |
| `doc/phases/060-dual-indices-correspondences-production-integration/result.zh.md` | phase result | dual-indices / correspondence PI2-PI5 完成事实。 | recovery pointer | `test-rvv/registration/transformation_estimation_svd/doc/phases/060-dual-indices-correspondences-production-integration/result.zh.md` |

## 正确性与高效性证据链

| 证据 | 状态 | 说明 |
| --- | --- | --- |
| QEMU correctness | done | `test-rvv/registration/transformation_estimation_svd/log/qemu/run_test_std.log`、`run_test_rvv.log`：Std 22/22、RVV 22/22。 |
| board correctness | done | `test-rvv/registration/transformation_estimation_svd/log/board/test_smoke/run_test.log`：RVV 22/22。 |
| ordered asm attribution | done | ordered public overload 符号内有 `vlsseg3e32.v`、`vfadd.vv`、`vfmacc.vv`、`vfredosum.vs`；反汇编路径为 `test-rvv/registration/transformation_estimation_svd/build/asm/riscv/bench_transformation_estimation_svd_rvv.full.asm`。 |
| source-indexed asm attribution | done | source-indexed public overload 符号 `0x21fb6` 内有 `vluxseg3ei32.v`、`vlsseg3e32.v`、`vfmacc.vv`、`vfredosum.vs`；反汇编路径为 `test-rvv/registration/transformation_estimation_svd/build/asm/riscv/bench_transformation_estimation_svd_rvv.full.asm`。 |
| dual-indices asm attribution | done | dual-indices public overload 符号内有双 gather / FMA / reduction 指令簇；反汇编路径为 `test-rvv/registration/transformation_estimation_svd/build/asm/riscv/bench_transformation_estimation_svd_rvv.full.asm`。 |
| correspondence asm attribution | done | correspondence public overload 符号内有 gather / FMA / reduction 指令簇；反汇编路径为 `test-rvv/registration/transformation_estimation_svd/build/asm/riscv/bench_transformation_estimation_svd_rvv.full.asm`。 |
| ordered board performance | done | `test-rvv/registration/transformation_estimation_svd/log/board/production_ordered_cloud_pair_repeated/summary.md`：4K `14.372x`、64K `24.471x`、256K `23.841x` median，bucket `positive`。 |
| source-indexed board performance | done | `test-rvv/registration/transformation_estimation_svd/log/board/production_source_indexed_cloud_pair_repeated/summary.md`：4K `9.634x`、64K `12.217x`、256K `11.558x` median，bucket `positive`。 |
| dual-indices board performance | done | `test-rvv/registration/transformation_estimation_svd/log/board/production_dual_indices_cloud_pair_repeated/summary.md`：4K `6.805x`、64K `6.404x`、256K `5.964x` median，bucket `positive`。 |
| correspondence board performance | done | `test-rvv/registration/transformation_estimation_svd/log/board/production_correspondence_pair_repeated/summary.md`：4K `8.649x`、64K `8.644x`、256K `7.872x` median，bucket `positive`。 |
| Evidence Doctor | done | ordered Errors=0 / Warnings=1；source-indexed Errors=0 / Warnings=0；dual-indices Errors=0 / Warnings=1；correspondence Errors=0 / Warnings=1。 |
| manifests | done | 四组 production direct summary 均指向各自 `evidence_manifest.json`，且 `evidence_role=production_direct`。 |
| evidence registry | done | `test-rvv/registration/transformation_estimation_svd/log/evidence_registry.json` 中四组 production direct summary / manifest / doctor 均为 `recorded`。 |

Correctness（正确性）：QEMU Std/RVV 和 board RVV correctness 覆盖 public Umeyama 语义锚点、四条 row source
的 production helper path-hit（路径命中）、fallback gate、representative mixed-field xyz AoS 点型，以及 determinant
sign fix（行列式符号修正）压力样本。

Performance（性能）：性能结论只来自 repeated board summary（重复板卡摘要）。QEMU bench 只作为 build、
case label、checksum 和日志形状证据，不进入速度结论。

Boundary（边界）：EvidenceDecision 只覆盖当前 gate 下的 `Scalar=float`、dense、`n >= 16`、
layout-gated xyz AoS、valid indices / correspondences 和四条 row source。`PointXYZI` / `PointXYZRGB`
已覆盖 correctness/path-hit；逐点型性能未单独证明。

Risk（风险）：ordered doctor warning 是 4K group outlier，说明不能把 64K / 256K 或同组其它 case 的收益外推到
ordered 4K。dual-indices 和 correspondence doctor warning 都是 256K long-tail / variance；当前保留
min/median/max，按 size 分开解释。source-indexed doctor 无 warning。

## Bench 与 evidence 角色

四组 production direct repeated board summary 的运行合同一致：5 runs、20 iterations、5 warm-up iterations，
`B/A = baseline ms / candidate ms`，大于 1 表示 RVV build 更快。summary 的 `case-filter` 分别是：

| run label | case-filter | evidence role | summary |
| --- | --- | --- | --- |
| `production_ordered_cloud_pair_repeated` | `public-umeyama` | `production_direct` | `test-rvv/registration/transformation_estimation_svd/log/board/production_ordered_cloud_pair_repeated/summary.md` |
| `production_source_indexed_cloud_pair_repeated` | `source-indexed-cloud-pair` | `production_direct` | `test-rvv/registration/transformation_estimation_svd/log/board/production_source_indexed_cloud_pair_repeated/summary.md` |
| `production_dual_indices_cloud_pair_repeated` | `dual-indices-cloud-pair` | `production_direct` | `test-rvv/registration/transformation_estimation_svd/log/board/production_dual_indices_cloud_pair_repeated/summary.md` |
| `production_correspondence_pair_repeated` | `correspondence-pair` | `production_direct` | `test-rvv/registration/transformation_estimation_svd/log/board/production_correspondence_pair_repeated/summary.md` |

Phase 000/010 的 `fused_full_cloud_repeated`、`source_indexed_cloud_pair_repeated`、
`dual_indices_cloud_pair_repeated` 和 `correspondence_pair_repeated` 是 historical diagnostic evidence
（历史诊断证据）。它们解释候选来源和 row-source 取数形态，不能替代 production direct evidence。

## QEMU bench compare 策略

QEMU 用于 correctness、build 和日志形状。`ALLOW_QEMU_BENCH_COMPARE=1` 的 bench smoke 只证明 bench binary、
case label、checksum 和 Evidence Doctor 输入可解析。性能结论必须引用 board / target hardware summary。

当前长期结论不引用 QEMU timing，也不把 diagnostic board speedup 外推到 production direct。

## 遗留风险与后续条件

| 风险 / 后续 | 当前处理 |
| --- | --- |
| ordered 4K group outlier | 保留单独 size 结论；不把 64K / 256K 收益外推到 4K。若 reviewer 需要更稳证据，可追加 20-run board confirmation。 |
| dual-indices 256K long-tail / variance | 保留 min/median/max；当前 median 和 bucket 仍为 positive。需要更严格发布口径时追加更多 board runs 或 per-iteration trace。 |
| correspondence 256K long-tail / variance | 同 dual-indices；当前不剔除异常 run。 |
| generic xyz AoS 点型性能 | 当前只证明 representative correctness/path-hit；如果下游热点使用 `PointXYZI`、`PointXYZRGB` 或其它 gate-allowed 点型，应补对应 board cases。 |
| `Scalar=double` | 保持 fallback。进入 double RVV 需要独立数值预算、测试、asm 和 board plan。 |
| non-dense / NaN / Inf 输入 | 保持标量路径。若要 RVV 化，需要重新定义 finite mask 语义和 fallback 测试。 |
| public entry 结构 | 当前 RVV 尝试后保留原 iterator 标量主体。后续可审查是否抽出命名 `*_Std` helper；这只影响可维护性，不改变当前已验证语义。 |

当前 topic 的 row-source production 范围已经闭合。默认后续工作是 topic-local docs、evaluation、roadmap、
optimization matrix 和证据 registry 的一致性维护；新的 `Scalar`、点类型逐项性能或非 dense 支持应作为独立扩展阶段处理。
