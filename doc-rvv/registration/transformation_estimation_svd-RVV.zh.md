# registration/transformation_estimation_svd RVV 说明

## Closeout 摘要

当前 `TransformationEstimationSVD` 已接入四个 RVV production path（生产路径）：

```cpp
estimateRigidTransformation(const pcl::PointCloud<PointSource>& cloud_src,
                            const pcl::PointCloud<PointTarget>& cloud_tgt,
                            Matrix4& transformation_matrix)
```

```cpp
estimateRigidTransformation(const pcl::PointCloud<PointSource>& cloud_src,
                            const pcl::Indices& indices_src,
                            const pcl::PointCloud<PointTarget>& cloud_tgt,
                            Matrix4& transformation_matrix)
```

```cpp
estimateRigidTransformation(const pcl::PointCloud<PointSource>& cloud_src,
                            const pcl::Indices& indices_src,
                            const pcl::PointCloud<PointTarget>& cloud_tgt,
                            const pcl::Indices& indices_tgt,
                            Matrix4& transformation_matrix)
```

```cpp
estimateRigidTransformation(const pcl::PointCloud<PointSource>& cloud_src,
                            const pcl::PointCloud<PointTarget>& cloud_tgt,
                            const pcl::Correspondences& correspondences,
                            Matrix4& transformation_matrix)
```

第一条是 ordered-cloud-pair（顺序点云对，`source[i]` 与 `target[i]` 配对），第二条是 source-indexed-cloud-pair（源索引点云对，`source[indices_src[i]]` 与 `target[i]` 配对），第三条是 dual-indices-cloud-pair（双索引点云对，`source[indices_src[i]]` 与 `target[indices_tgt[i]]` 配对），第四条是 correspondence-pair（对应关系点对，`source[corr.index_query]` 与 `target[corr.index_match]` 配对）。`full-cloud` 只保留为历史 case-filter / 日志目录里的 legacy alias（历史别名），不是新文档中的规范 row-source 名称。

四条路径在 `__RVV10__` 构建下先尝试 RVV fast path；共同命中条件是 `Scalar=float`、`use_umeyama_ == true`、两侧点云 dense、`n >= 16`，且 source/target 分别满足 `pcl::rvv::RVVXYZAoSFloatLayout`。source-indexed 额外要求 index 数量与 target size 一致、每个 source index 合法，并且 source cloud 大小满足 32-bit byte offset gather（32 位字节偏移离散加载）边界；dual-indices 额外要求两侧 indices 等长且合法；correspondence 额外要求 query/match 合法。不满足时保持原 `ConstCloudIterator` 标量路径。`Scalar=double` 继续保留为 fallback。

PI5 生产证据闭合：QEMU Std/RVV 各 22 个 gtest 通过；板卡 RVV smoke 22/22 通过；ordered public overload 符号内有 `vlsseg3e32.v`、`vfadd.vv`、`vfmacc.vv`、`vfredosum.vs`；source-indexed public overload 符号 `0x21fb6` 内有 `vluxseg3ei32.v`、`vlsseg3e32.v`、`vfmacc.vv`、`vfredosum.vs`；dual-indices 与 correspondence public overload 符号内保留预期 gather / FMA / reduction 指令簇。Milkv-Jupiter 5-run production direct board summary 显示 ordered public Std/RVV median 为 4K `14.372x`、64K `24.471x`、256K `23.841x`；source-indexed public Std/RVV median 为 4K `9.634x`、64K `12.217x`、256K `11.558x`；dual-indices public Std/RVV median 为 4K `6.805x`、64K `6.404x`、256K `5.964x`；correspondence public Std/RVV median 为 4K `8.649x`、64K `8.644x`、256K `7.872x`。

## 1. 函数入口作用

`TransformationEstimationSVD` 用于从 source 点云到 target 点云估计刚体变换矩阵。默认 `use_umeyama_ == true` 时，旧标量路径先把 source/target xyz 装入两个 `3 x N` Eigen 动态矩阵，再调用 `pcl::umeyama(src, tgt, false)` 得到 no-scale rigid transform（不带尺度的刚体变换）。

RVV path 保持相同 no-scale 语义，但避免构造两个动态矩阵。它直接按 row source 累加：

- source xyz sum；
- target xyz sum；
- target-source cross sum。

随后用这些累加量构造 `3 x 3` covariance（协方差）矩阵，并保留 Eigen `JacobiSVD` 求解 3x3 tail。每次 estimate 只执行一次的 SVD 后段不手写 RVV。

## 2. 当前采用的优化方式

生产源码新增 `pcl::registration::detail` 内部 helper：

| helper | 职责 |
| --- | --- |
| `estimateRigidTransformationSVDOrderedCloudPairRVV` | ordered-cloud-pair production gate 和 public overload 分流；返回 `false` 时自然 fallback。 |
| `accumulateTransformationEstimationSVDOrderedCloudPairRVV` | RVV chunk 内用 `strided_load3_f32m2` 分别加载 source/target xyz，并累加 sum/cross sum。 |
| `estimateRigidTransformationSVDSourceIndexedCloudPairRVV` | source-indexed production gate；验证 index 边界、32-bit gather 范围、dense、layout、`Scalar` 和 `use_umeyama_`。 |
| `accumulateTransformationEstimationSVDSourceIndexedCloudPairRVV` | RVV chunk 内 source 侧用 `indexed_load3_f32m2`，target 侧用 `strided_load3_f32m2`，并累加 sum/cross sum。 |
| `estimateRigidTransformationSVDDualIndicesCloudPairRVV` | dual-indices production gate；验证双 index 边界、32-bit gather 范围、dense、layout、`Scalar` 和 `use_umeyama_`。 |
| `accumulateTransformationEstimationSVDDualIndicesCloudPairRVV` | RVV chunk 内 source / target 两侧都用 `indexed_load3_f32m2`，并累加 sum/cross sum。 |
| `estimateRigidTransformationSVDCorrespondencePairRVV` | correspondence production gate；验证 query/match 边界、dense、layout、`Scalar` 和 `use_umeyama_`。 |
| `accumulateTransformationEstimationSVDCorrespondencePairRVV` | RVV chunk 内按 correspondence query/match gather 读取，并累加 sum/cross sum。 |
| `solveTransformationEstimationSVDF32` | 用累加结果构造 no-scale Umeyama 矩阵；3x3 SVD 仍走 Eigen。 |

访存使用公共 `pcl::rvv_load` wrapper（封装）。ordered path 两侧使用 `strided_load3_f32m2`；source-indexed path 的 source 侧使用 `indexed_load3_f32m2`，target 侧保持连续 strided load。字段 offset 来自 `RVVXYZAoSFloatLayout<PointSource/PointTarget>`，因此 `PointXYZI`、`PointXYZRGB` 这类 mixed-field（混合字段）xyz AoS 点型不会复用 `PointXYZ` offset。

## 3. Fallback 矩阵

| 条件 | 行为 |
| --- | --- |
| 非 `__RVV10__` 构建 | 不编译 RVV helper，直接走原标量路径。 |
| `Scalar != float` | helper 编译期返回 `false`，例如 `Scalar=double` 保持标量。 |
| `use_umeyama_ == false` | 保持 centroid / demean / correlation 标量路径。 |
| ordered source / target size 不一致 | public overload 保持原错误处理，不进入 RVV。 |
| source-indexed `indices_src.size() != cloud_tgt.size()` | public overload 保持原错误处理，不进入 RVV。 |
| source-indexed index 为负数或越界 | RVV helper 返回 `false`，自然 fallback 到 iterator 标量路径。 |
| source-indexed source cloud 超过 32-bit byte offset gather 可表达范围 | RVV helper 返回 `false`，自然 fallback。 |
| `!cloud_src.is_dense` 或 `!cloud_tgt.is_dense` | fallback 到原标量路径。 |
| `n < 16` | fallback 到原标量路径，避免小规模 RVV setup cost（向量设置成本）。 |
| source/target 不满足 `RVVXYZAoSFloatLayout` | fallback 到原标量路径。 |
| dual-indices / correspondences overload | 已接入生产 RVV helper；失败时自然回退到 iterator 标量路径。 |

## 4. 正确性与高效性证据链

| 证据层 | 结果 | 路径 |
| --- | --- | --- |
| QEMU correctness | Std 22/22、RVV 22/22 | `test-rvv/registration/transformation_estimation_svd/log/qemu/run_test_std.log`、`run_test_rvv.log` |
| Board correctness | RVV 22/22 | `test-rvv/registration/transformation_estimation_svd/log/board/test_smoke/run_test.log` |
| Ordered ASM attribution | ordered public overload 符号内有 `vlsseg3e32.v`、`vfadd.vv`、`vfmacc.vv`、`vfredosum.vs` | `test-rvv/registration/transformation_estimation_svd/build/asm/riscv/bench_transformation_estimation_svd_rvv.full.asm` |
| Source-indexed ASM attribution | source-indexed public overload 符号 `0x21fb6` 内有 `vluxseg3ei32.v`、`vlsseg3e32.v`、`vfmacc.vv`、`vfredosum.vs` | 同上 |
| Dual-indices ASM attribution | dual-indices public overload 符号内有双 gather / FMA / reduction 指令簇 | `test-rvv/registration/transformation_estimation_svd/build/asm/riscv/bench_transformation_estimation_svd_rvv.full.asm` |
| Correspondence ASM attribution | correspondence public overload 符号内有 gather / FMA / reduction 指令簇 | 同上 |
| Ordered board production performance | 4K `14.372x`、64K `24.471x`、256K `23.841x` median，bucket `positive` | `test-rvv/registration/transformation_estimation_svd/log/board/production_ordered_cloud_pair_repeated/summary.md` |
| Ordered Evidence Doctor | Errors=0、Warnings=1、Suggestions=0 | `test-rvv/registration/transformation_estimation_svd/log/board/production_ordered_cloud_pair_repeated/evidence_doctor.md` |
| Source-indexed board production performance | 4K `9.634x`、64K `12.217x`、256K `11.558x` median，bucket `positive` | `test-rvv/registration/transformation_estimation_svd/log/board/production_source_indexed_cloud_pair_repeated/summary.md` |
| Source-indexed Evidence Doctor | Errors=0、Warnings=0、Suggestions=0 | `test-rvv/registration/transformation_estimation_svd/log/board/production_source_indexed_cloud_pair_repeated/evidence_doctor.md` |
| Dual-indices board production performance | 4K `6.805x`、64K `6.404x`、256K `5.964x` median，bucket `positive` | `test-rvv/registration/transformation_estimation_svd/log/board/production_dual_indices_cloud_pair_repeated/summary.md` |
| Dual-indices Evidence Doctor | Errors=0、Warnings=1、Suggestions=0 | `test-rvv/registration/transformation_estimation_svd/log/board/production_dual_indices_cloud_pair_repeated/evidence_doctor.md` |
| Correspondence board production performance | 4K `8.649x`、64K `8.644x`、256K `7.872x` median，bucket `positive` | `test-rvv/registration/transformation_estimation_svd/log/board/production_correspondence_pair_repeated/summary.md` |
| Correspondence Evidence Doctor | Errors=0、Warnings=1、Suggestions=0 | `test-rvv/registration/transformation_estimation_svd/log/board/production_correspondence_pair_repeated/evidence_doctor.md` |

Ordered Evidence Doctor 的唯一 Warning 是 4K 规模相对 64K / 256K 的组内收益偏低。结论按 size 分开报告；不把大规模收益外推到 4K，也不把 ordered-cloud-pair 或 source-indexed-cloud-pair 结果外推到其它 row source。

## 5. 泛型点类型边界

当前 production gate 是 layout-gated generic xyz AoS（布局门控泛型 xyz 结构数组）路径，不是 exact `PointXYZ` 路径。source 和 target 分别通过 `RVVXYZAoSFloatLayout` 取各自的 `x/y/z` offset、stride 和 POD layout 前提。

`PointXYZI` / `PointXYZRGB` 已有四条 row source 的 production direct correctness/path-hit 证据。它们在 gate 允许时可以进入 production RVV path；但板卡 repeated performance 目前只按代表性 `PointXYZ` case-filter 采集。因此长期结论只能写成“代表性 mixed-field 点型 correctness 已覆盖，逐类型性能未单独证明”。

## 6. 未覆盖范围和后续阶段

- `Scalar=double` 当前明确 fallback；没有 double RVV 性能计划。
- 非 dense 或包含 NaN / Inf 的输入没有进入当前 RVV gate；保持标量路径。
- 生产 public entry 目前仍在 RVV 尝试后保留原 iterator 标量主体。实现规则更偏好抽出命名 `*_Std` helper；这是后续结构审查点，不改变当前已验证语义。

默认下一阶段是 topic-local 文档、evaluation、roadmap、matrix 和长期 `doc-rvv` 的收尾刷新，不再存在新的 row-source phase。
