# registration/transformation_estimation_2D 函数级 RVV 评估

## 范围和目标源码

目标源码：

```text
registration/include/pcl/registration/transformation_estimation_2D.h
registration/include/pcl/registration/impl/transformation_estimation_2D.hpp
```

专项目录：

```text
test-rvv/registration/transformation_estimation_2D/
```

本文是 S2 evaluation（函数级评估）和当前 production candidate closeout（生产候选收尾）主归属。当前 production patch 覆盖三条 RVV dispatch：traits-gated ordered-cloud-pair、Phase 091 用户已确认采纳的 source-indexed `PointXYZ -> PointXYZ` narrow dispatch，以及 Phase 107 当前接入的 dual-indexed `PointXYZ -> PointXYZ` exact patch candidate。Phase 099 已完成 source-indexed generic PointXYZ-like diagnostic，结果为 mixed-negative，不修改 production dispatch。Phase 103/104 source-indexed generic public widening 仍是 guarded probe；Phase 106 独立 20-run public variance 为 12 positive、1 weak_positive、3 negative，board Doctor `1/27/0`，不支持 clean-adopt。Phase 100 已完成 dual-indexed generic PointXYZ-like diagnostic，结果为 negative，不能把 Phase 107 exact dual-indexed patch 扩成泛型 production dispatch。Phase 101 已完成 correspondence generic PointXYZ-like diagnostic，结果为 negative，也不修改 production dispatch。Phase 094 correspondence `PointXYZ -> PointXYZ` public probe 曾为 positive，但 Phase 107 同边界 family A/B 在 256K 仍有退化频率；correspondence production dispatch 已按用户规则试接入后退回。Phase 095 staged-dual profile、Phase 096 locality/order profile、Phase 097 component ablation、Phase 098 chunked xyz staging 和 Phase 101 correspondence generic 均为 negative，不能切换或扩大 correspondence production dispatch。Phase 080 的接入后证据只覆盖四类代表性点型和四组 mixed pair；未逐类型上板的点型和 `Scalar=double` 仍保持未覆盖结论。

## 函数级结论

`TransformationEstimation2D` 实现 2D rigid transformation（二维刚体变换）估计。公开入口接收 source / target 点云、source indices（源索引）、dual indices（双侧索引）或 correspondences（对应关系），然后统一成两个 `ConstCloudIterator` 进入 protected helper。

当前标量路径的主工作可以分成五段：

1. 分别对 source 和 target 计算 3D centroid（质心），随后把 z 分量置零。
2. 分别生成 source / target 的 demean matrix（去中心化矩阵）。
3. 用 `cloud_src_demean * cloud_tgt_demean.transpose()` 生成 correlation matrix（相关矩阵）`H`。
4. 用 `atan2(H01 - H10, H00 + H11)` 得到 2D 旋转角。
5. 用 `cos/sin` 和 `centroid_tgt - R * centroid_src` 写回 4x4 transform matrix（变换矩阵）。

Phase 020 证明 test-only two-pass centered fused 2D correlation candidate（测试专用两遍中心化融合 2D 相关项候选）在顺序点云对（ordered-cloud-pair，source/target 按相同下标一一对应）、dense finite `PointXYZ -> PointXYZ`、`Scalar=float` 下有诊断价值。Phase 050 的真实板卡 public repeated 为 4K `4.222x`、64K `5.310x`、256K `4.947x`，Doctor `0/0/0`，因此保留 exact PointXYZ 历史窄范围证据。Phase 030 在不修改 production 的前提下，把 source-indexed、dual-indexed 和 correspondence row source 物化成顺序点云对并复用同一数学 candidate；Phase 090 按同输入语义刷新 materialize，并新增 direct gather candidate。Phase 070 在 test-rvv 中使用 PCL traits 分别审计 source / target，覆盖四类代表性点型和四组 mixed pair。Phase 080 已将同一 gate 接入真实 public ordered-cloud-pair dispatch；Phase 080 当时 aggregate Std/RVV correctness 各 80/80；Phase 103 后当前 aggregate 为 84/84。public QEMU Doctor `0/0/0`，production asm 406 RVV lines，board Doctor `0/6/0`；其中 PointNormal->PointNormal 64K 为 negative bucket。用户已在 2026-08-17 确认当前优化可以采纳，因此该 patch 记录为 adopted-by-user，同时保留 representative-scope caveat（代表性范围边界）。Phase 091 已为 source-indexed `PointXYZ -> PointXYZ` 补接入后 correctness / fallback、QEMU、production asm、Milkv-Jupiter board 和 Doctor；用户已在 2026-08-18 确认采纳该 narrow row-source patch。Phase 092 又补同一 production boundary 内的 direct-vs-materialize RVV family A/B，结论为 `weak_positive`，支持保留 current direct gather，但不修改 production dispatch。Phase 099 进一步在 test-rvv 中补 source-indexed generic PointXYZ-like same-type、mixed pair、extra-field 和 fallback evidence；correctness Std/RVV 64/64，board mixed-negative，不能把 091 exact gate 直接扩大为泛型 production dispatch。Phase 107 已把 dual-indexed `PointXYZ -> PointXYZ` 接入当前 production patch candidate，并补接入后 correctness、QEMU/asm、family A/B 和 board evidence。

Phase 100 进一步在 test-rvv 中补 dual-indexed generic PointXYZ-like same-type、mixed pair、extra-field 和 fallback evidence；correctness Std/RVV 72/72，board negative，不能把 Phase 107 exact dual-indexed patch 扩大为泛型 production dispatch。Phase 101 继续在 test-rvv 中补 correspondence generic PointXYZ-like same-type、mixed pair、extra-field 和 fallback evidence；historical correctness Std/RVV 80/80，board negative，不能把 Phase 094 guarded exact evidence 扩大为泛型 production dispatch。Phase 103/104 把 source-indexed generic widening 放到真实 public boundary 后仍是 guarded；Phase 106 独立 20-run variance 出现 3 个 negative bucket，`PointNormal->PointNormal 256K` 为 `7/20` below-1，board Doctor `1/27/0`，所以 full source-indexed generic widening 不能 clean-adopt。Phase 107 试接入 correspondence `PointXYZ -> PointXYZ` 后，family A/B 256K 仍有 `4/20` below-1，overall negative；因此当前 production header 不保留 correspondence RVV dispatch。Phase 095 已补 staged-dual profile；direct/staged D/S 为 `0.833x / 0.729x / 0.848x`，Doctor `3/2/0`，因此 staged-dual 只记录为 profile-only negative，不进入 production dispatch。Phase 096 已补 identity、reverse、shuffled、strided 四类 query/match 分布的 locality/order profile；overall `negative`，Doctor `12/5/0`，继续支持不切换 staged-dual dispatch。Phase 097 又补 strided component ablation；full-anchor D/S 为 `0.834x / 0.761x / 0.841x`，Doctor `3/2/0`，只保留 ingress / gather / materialize 成本线索。Phase 098 已补 chunked xyz staging candidate；20-run board D/C 为 `0.813x / 0.941x / 0.983x`，Doctor `3/3/0`，因此 chunked staging 也只记录为 profile-only negative。

## 公开入口和 row source policy

| 公开入口 | row source policy（行来源策略） | source row | target row | 当前 RVV 状态 |
| --- | --- | --- | --- | --- |
| `estimateRigidTransformation(cloud_src, cloud_tgt, matrix)` | ordered-cloud-pair（顺序点云对，source/target 按相同下标一一对应） | `cloud_src[k]` | `cloud_tgt[k]` | production patch retained；不满足 gate 时回退标量路径。 |
| `estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, matrix)` | source-indexed-cloud-pair（源索引点云对） | `cloud_src[indices_src[k]]` | `cloud_tgt[k]` | Phase 091 `PointXYZ -> PointXYZ` adopted narrow production dispatch；Phase 103/104/106 generic widening 仍是 guarded 且 full variance 不支持 clean adoption；失败回退标量路径；其它点型仍标量。 |
| `estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, indices_tgt, matrix)` | dual-indexed-cloud-pair（双索引点云对） | `cloud_src[indices_src[k]]` | `cloud_tgt[indices_tgt[k]]` | Phase 107 exact `PointXYZ -> PointXYZ` production patch candidate；失败回退标量路径；其它点型仍标量。 |
| `estimateRigidTransformation(cloud_src, cloud_tgt, correspondences, matrix)` | correspondence-pair（对应关系点对） | `cloud_src[index_query]` | `cloud_tgt[index_match]` | Phase 107 试接入后因同边界 family A/B negative 已退回；当前没有 RVV production dispatch。 |

RowSourcePolicy 只描述每一行从哪里来。它不等同于完整优化族。ordered-cloud-pair 的诊断结果和 production-public negative result 都不能直接批准 indexed 或 correspondences production。

## PointXYZ-like 泛型 Gate

Phase 070 的 generic candidate 使用公共
`pcl::rvv::RVVXYZAoSFloatLayout<PointT>::value`。它只证明输入点型的字段和 AoS 访存前提：

- `pcl::traits::has_xyz<PointT>::value` 为 true；
- x/y/z 是 PCL traits 注册的单个 `float`；
- x/y/z offset 可由 `pcl::traits::offset` 取得；
- `pcl::traits::POD<PointT>::type` 为 standard-layout；
- `sizeof(PointT) == sizeof(POD)`；
- stride 和字段 offset 满足 float alignment。

source 和 target 分别实例化 gate，分别记录 offset、stride、POD 和 sizeof；mixed pair
不能共享这些假设。runtime dense/finite、点数和 `Scalar` 仍是独立门控。额外 intensity、
normal 字段不参与当前 2D 计算，但这个输入字段 gate 不能外推到需要写回完整 `PointT`
或读取 normal/RGB 的其它算法。

Phase 070 已证明的代表性范围：

| 组合 | correctness | board representative | 当前角色 |
| --- | --- | --- | --- |
| 四类 same-type | pass | 4K/64K/256K | test-only generic candidate |
| `PointXYZI -> PointXYZ`、`PointXYZ -> PointXYZI` | pass | 64K | test-only mixed candidate |
| `PointNormal -> PointXYZINormal`、`PointXYZINormal -> PointNormal` | pass | 64K | test-only mixed candidate |

未逐类型上板的自定义 traits 点型、RGB/RGBA、`Scalar=double` 和其它 row source 不在此
性能结论内。

## 标量流程与 RVV 候选流程对照

| 流程段 | 当前标量 production | RVV 诊断 / probe |
| --- | --- | --- |
| 入口检查 | ordered-cloud-pair / source-indexed / dual-indices 检查规模关系；correspondences overload 不做逐项 index 边界检查。 | 诊断先复刻公开入口规模关系；非法 index / correspondence 只在明确 public semantics 后写成测试合同。 |
| row 读取 | `ConstCloudIterator` 隐藏不同 row source 的访存差异。 | ordered-cloud-pair 用 segment stride load（跨步分段加载）读取 x/y/z；Phase 030 的 indexed / correspondences candidate 先按 index / query-match 物化成顺序点云对；Phase 090 的 direct gather candidate 直接按 selected row 读取 x/y/z。Phase 091 已把 source-indexed `PointXYZ -> PointXYZ` direct gather 接入并获用户确认；Phase 107 已把 dual-indexed `PointXYZ -> PointXYZ` direct gather 接入当前 production patch candidate；Phase 107 correspondence direct gather 因 family A/B 256K 退化频率已退回 production dispatch。 |
| centroid | `compute3DCentroid(ConstCloudIterator&)` 逐点检查 `pcl::isFinite`，source 和 target 分别计数。 | 诊断候选只在 dense finite 输入命中 RVV；非有限输入退回 public path。 |
| demean / correlation | `demeanPointCloud(ConstCloudIterator&, ..., Eigen::Matrix&)` 写出 4xN 动态矩阵，再由 Eigen 乘法生成 `H`。 | 两遍中心化：先求 x/y 质心，再直接累加中心化后的 `H00/H01/H10/H11`，避免显式矩阵写出。 |
| angle 和 transform | `atan2`、`cos`、`sin` 每次 estimate 只执行一次。 | 保留标量。热点候选在逐点累加和矩阵写出阶段。 |

需要单独审计的语义边界：当前 centroid 会因为 `pcl::isFinite` 同时检查 x/y/z 而受 z 有限性影响；后续 demean 仍全量写出 iterator 行。测试已记录 x/y 非有限会传播为非有限矩阵，z 非有限会强制 test-only candidate fallback。

## 实现方式审计

| 实现维度 | 当前状态 | 证据 | 边界 / 恢复条件 |
| --- | --- | --- | --- |
| dispatch / fallback（分流 / 回退） | traits-gated ordered-cloud-pair adopted | `git diff -- registration/include/pcl/registration/impl/transformation_estimation_2D.hpp`；失败条件回退现有标量 iterator helper。 | 已获用户确认采纳；不扩大到其它 row source。 |
| layout / traits gate（布局 / 字段门控） | Phase 080 已接入 production public generic gate | `RVVXYZAoSFloatLayout<PointT>`；source/target 独立 offset/stride；四类点型和 mixed pair 通过。 | 未逐类型上板的自定义点型、RGB/RGBA 和其它 row source 不继承证据。 |
| row source policy | ordered-cloud-pair adopted; source-indexed narrow dispatch adopted; source-indexed generic diagnostic mixed-negative; source-indexed generic public variance negative for full widening; dual-indexed exact in current Phase 107 patch candidate; dual-indexed generic diagnostic negative; correspondence trial rolled back; correspondence staged-dual、locality/order、component ablation、chunked staging and generic diagnostic negative | `run_test_compare` 84/84；source-indexed public probe/fallback、source-indexed generic same/mixed/fallback、dual-indexed public probe/fallback、dual-indexed generic same/mixed/fallback、correspondence public probe/fallback、correspondence generic same/mixed/fallback、Phase 092 / 093 / 094 materialize family equivalence、Phase 095 staged-dual equivalence、Phase 096 locality/order equivalence 和 Phase 098 chunked staging equivalence 通过；Phase 097 component no-solve 只作 checksum smoke。 | source-indexed / dual-indexed / correspondence 均不覆盖未证实的泛型 production dispatch；dual-indexed exact 只在当前补丁候选中，不自动提交；source-indexed generic public widening 经 Phase 106 不能 clean-adopt full representative scope；correspondence direct 已因 Phase 107 negative 退回；source-indexed、dual-indexed 与 correspondence generic 都只完成 test-rvv diagnostic。 |
| staging / reduction（暂存 / 规约） | retained traits-gated production shape | `accumulateFused2DRVV` 使用两遍 vector reduction；Phase 020 board diagnostic 为 `weak_positive`；Phase 080 public generic asm 可归属 production symbols。 | Production-public board 有一个 negative bucket；采纳不扩大到其它 row source 或未验证点型。 |
| formula / FMA（公式 / 融合乘加） | attempted / not adopted | `log/qemu/production_public/asm_attribution.md` 可见 public boundary 中的 `vfmacc` 和 `vfredosum`。 | 目标硬件收益不成立，FMA 指令存在不等于 production-ready。 |
| production scope（生产范围） | traits-gated ordered-cloud-pair adopted-by-user；source-indexed narrow dispatch adopted-by-user；source-indexed direct family kept；source-indexed generic widening guarded / no clean adoption；dual-indexed exact Phase 107 patch candidate；correspondence exact trial rolled back；correspondence staged-dual rejected for switch；correspondence locality/order、component ablation、chunked staging profile 和 generic widening negative | Phase 080 production direct correctness、QEMU、production-symbol asm、board summary、Doctor；Phase 107 ordered generic public board 16/16 positive。Phase 091/107 source-indexed public correctness、QEMU、asm、board summary、Doctor。Phase 092 source-indexed family A/B 为弱正向，Doctor 0/0/2。Phase 106 source-indexed generic public variance 为 12 positive、1 weak_positive、3 negative，Doctor 1/27/0。Phase 107 dual-indexed family A/B 为 `1.085x / 1.691x / 1.678x`，Doctor 0/3/0。Phase 107 correspondence family A/B 为 256K `4/20` below-1、overall negative，Doctor 0/4/0。Phase 095-098 和 Phase 101 均未支持 correspondence production switch。 | adopted 仅覆盖 ordered-cloud-pair patch 和 source-indexed `PointXYZ -> PointXYZ`；dual-indexed exact 保留在当前 production patch candidate，等待用户检查；source-indexed generic widening 不能 clean-adopt full representative scope；correspondence direct 已退回；未逐类型上板自定义点型、`Scalar=double` 继续独立。 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| ordered-cloud-pair public overload | production public entry | 检查 source / target size 后按 gate 尝试 RVV helper，失败时回退默认 iterator。 | registration 上游调用方。 | RVV helper 或 protected iterator helper。 | public semantics、ordered-cloud-pair row source 和 production dispatch。 | `registration/include/pcl/registration/impl/transformation_estimation_2D.hpp` |
| source-indexed overload | production public entry | source 由 indices 指定，target 顺序扫描；Phase 091 scope 下先尝试 narrow RVV helper。 | registration 上游调用方。 | source-indexed RVV helper 或 protected iterator helper。 | source-indexed adopted narrow dispatch and scalar fallback。 | 同上 |
| dual-indices overload | production public entry | source 和 target 都由 index list 指定；Phase 107 exact scope 下可命中 narrow RVV helper。 | registration 上游调用方。 | dual-indexed RVV helper 或 protected iterator helper。 | dual-indexed Phase 107 production patch candidate / scalar fallback。 | 同上 |
| correspondences overload | production public entry | 从 correspondences 抽 query / match 两列；当前不尝试 RVV production dispatch。 | registration 上游调用方。 | protected iterator helper。 | correspondence scalar fallback；Phase 107 RVV trial 已退回。 | 同上 |
| protected iterator helper | production scalar helper | 计算 centroids、demean matrices、correlation、angle 和 transform。 | 四个公开入口。 | `getTransformationFromCorrelation`。 | scalar baseline 和 candidate 对拍参考。 | 同上 |
| `compute3DCentroid(ConstCloudIterator&)` | common scalar helper | iterator 版本逐点计算 centroid。 | protected iterator helper。 | centroid values。 | finite semantics reference。 | `common/include/pcl/common/impl/centroid.hpp` |
| `demeanPointCloud(ConstCloudIterator&, Eigen::Matrix&)` | common scalar helper | iterator 版本写出 Eigen dynamic matrix。 | protected iterator helper。 | correlation matrix multiply。 | cost source and replacement boundary。 | `common/include/pcl/common/impl/centroid.hpp` |
| `estimateFused2DStd` | test support | 标量两遍中心化 reference。 | gtest / bench。 | `solveTransform2DFromAccumulation`。 | same-chain reference。 | `test-rvv/registration/transformation_estimation_2D/include/impl/te2d_candidates.hpp` |
| `estimateFused2DCandidate` | test support | RVV 构建下尝试 ordered-cloud-pair candidate。 | gtest / bench。 | `accumulateFused2DRVV` 或 fallback。 | diagnostic candidate。 | 同上 |
| `RVVXYZAoSFloatLayout<PointT>` | test support trait gate | 分别审计 source / target 的 xyz traits、POD、sizeof、offset、alignment 和 stride。 | generic gtest / bench。 | `estimateFused2DCandidate<PointSource, PointTarget>`。 | generic gate / layout evidence。 | `include/impl/te2d_candidates.hpp` |
| `estimateFused2DSourceIndexedCandidate` | test support | 读取 source indices，物化 source row 后复用 fused candidate。 | gtest / bench。 | `accumulateFused2DRVV` 或 fallback。 | source-indexed diagnostic；展开成本计入 bench。 | 同上 |
| `estimateFused2DDualIndexedCandidate` | test support | 读取 source / target 两侧 indices，物化后复用 fused candidate。 | gtest / bench。 | `accumulateFused2DRVV` 或 fallback。 | dual-indexed diagnostic；展开成本计入 bench。 | 同上 |
| `estimateFused2DCorrespondenceCandidate` | test support | 读取 query / match，物化点对后复用 fused candidate。 | gtest / bench。 | `accumulateFused2DRVV` 或 fallback。 | correspondence diagnostic；展开成本计入 bench。 | 同上 |
| `estimateFused2DSourceIndexedDirectGatherCandidate` | test support | source 侧按 index gather x/y/z，target 顺序读取；模板形态可使用 source/target 独立 PointXYZ-like traits gate。 | gtest / bench。 | direct scalar / RVV accumulator 或 fallback。 | source-indexed direct gather diagnostic；Phase 099 generic 仍是 test support，不是 production。 | 同上 |
| `tryTransformationEstimation2DSourceIndexedCloudPairRVV` | production helper | source 侧按 source index gather x/y/z，target 顺序读取；只覆盖 `PointXYZ -> PointXYZ`。 | source-indexed public overload。 | production RVV accumulation 或 false fallback。 | Phase 091 adopted narrow production dispatch。 | `registration/include/pcl/registration/impl/transformation_estimation_2D.hpp` |
| `estimatePublicSourceIndexedMaterializedOrdered2D` | test support | 物化 selected source rows 后调用真实 ordered-cloud-pair public overload。 | Phase 092 gtest / bench。 | production ordered-cloud-pair public dispatch。 | source-indexed production-detail family A/B 对照。 | `test-rvv/registration/transformation_estimation_2D/include/impl/te2d_candidates.hpp` |
| `runSourceIndexedGenericCase` | bench support | 用独立 lambda 边界运行 source-indexed generic point-type candidate，避免继承 ordered generic 或 Phase 091 exact source-indexed 结论。 | `source-indexed-generic-xyz-point-types` case-filter。 | `estimateFused2DSourceIndexedDirectGatherCandidate<PointSource,PointTarget>`。 | Phase 099 source-indexed generic diagnostic；mixed-negative，不接 production。 | `test-rvv/registration/transformation_estimation_2D/src/bench_te2d.cpp` |
| `tryTransformationEstimation2DDualIndexedCloudPairRVV` | production helper | source / target 两侧按独立 index stream gather x/y/z；只覆盖 `PointXYZ -> PointXYZ`。 | dual-indexed public overload。 | production RVV accumulation 或 false fallback。 | Phase 107 production patch candidate；不自动提交。 | `registration/include/pcl/registration/impl/transformation_estimation_2D.hpp` |
| `estimatePublicDualIndexedMaterializedOrdered2D` | test support | 物化 selected source/target rows 后调用真实 ordered-cloud-pair public overload。 | Phase 093 gtest / bench。 | production ordered-cloud-pair public dispatch。 | dual-indexed production-detail family A/B 对照。 | `test-rvv/registration/transformation_estimation_2D/include/impl/te2d_candidates.hpp` |
| `runDualIndexedGenericCase` | bench support | 用独立 lambda 边界运行 dual-indexed generic point-type candidate，避免继承 ordered generic、source-indexed generic 或 Phase 093 exact dual-indexed 结论。 | `dual-indexed-generic-xyz-point-types` case-filter。 | `estimateFused2DDualIndexedDirectGatherCandidate<PointSource,PointTarget>`。 | Phase 100 dual-indexed generic diagnostic；negative，不接 production。 | `test-rvv/registration/transformation_estimation_2D/src/bench_te2d.cpp` |
| historical `tryTransformationEstimation2DCorrespondencePairRVV` trial | historical production helper | Phase 107 曾试接入 correspondence direct gather，随后因 family A/B negative 从 production header 移除。 | 无当前 production 调用者。 | 不适用。 | historical trial evidence；当前不属于 production dispatch。 | current header no longer contains this helper |
| `estimatePublicCorrespondenceMaterializedOrdered2D` | test support | 物化 selected query / match rows 后调用真实 ordered-cloud-pair public overload。 | Phase 094 gtest / bench。 | production ordered-cloud-pair public dispatch。 | correspondence production-detail family A/B 对照。 | `test-rvv/registration/transformation_estimation_2D/include/impl/te2d_candidates.hpp` |
| `estimatePublicCorrespondenceStagedDualIndexed2D` | test support | 把 correspondence query / match 拷成连续 source / target indices 后调用真实 dual-indexed public overload。 | Phase 095 gtest / bench。 | production dual-indexed public dispatch。 | correspondence staged-dual profile；结果为 negative，不接 production。 | `test-rvv/registration/transformation_estimation_2D/include/impl/te2d_candidates.hpp` |
| `estimateFused2DDualIndexedDirectGatherCandidate` | test support | source / target 两侧按独立 index stream gather x/y/z；模板形态可使用 source/target 独立 PointXYZ-like traits gate。 | gtest / bench。 | direct scalar / RVV accumulator 或 fallback。 | dual-indexed direct gather diagnostic；Phase 100 generic 仍是 test support，不是 production。 | 同上 |
| `estimateFused2DCorrespondenceDirectGatherCandidate` | test support | 从 correspondences 读取 query / match，再 gather source / target；模板形态可使用 source/target 独立 PointXYZ-like traits gate。 | gtest / bench。 | direct scalar / RVV accumulator 或 fallback。 | correspondence direct gather diagnostic；Phase 101 generic 仍是 test support，不是 production。 | 同上 |
| `runCorrespondenceGenericCase` | bench support | 用独立 lambda 边界运行 correspondence generic point-type candidate，避免继承 ordered、source-indexed、dual-indexed generic 或 Phase 094 exact correspondence 结论。 | `correspondence-generic-xyz-point-types` case-filter。 | `estimateFused2DCorrespondenceDirectGatherCandidate<PointSource,PointTarget>`。 | Phase 101 correspondence generic diagnostic；negative，不接 production。 | `test-rvv/registration/transformation_estimation_2D/src/bench_te2d.cpp` |
| `generate_te2d_asm_summary.py` | analysis script | 解析 Std / RVV objdump 并生成 asm attribution summary。 | `generate_asm_attribution_summary`。 | `asm_attribution.md/json`。 | asm attribution。 | `test-rvv/registration/transformation_estimation_2D/script/generate_te2d_asm_summary.py` |
| `generate_te2d_qemu_evidence_manifest.py` | analysis script | 把 QEMU smoke log 和 asm summary 翻译成 manifest。 | `run_qemu*_evidence_doctor`。 | `evidence_doctor.py`。 | QEMU smoke contract。 | `test-rvv/registration/transformation_estimation_2D/script/generate_te2d_qemu_evidence_manifest.py` |
| `generate_te2d_board_repeated_summary.py` | analysis script | 解析 board repeated `run-*` 日志并生成 summary / manifest。 | `run_board_bench_*_repeated`。 | `evidence_doctor.py`。 | board diagnostic、generic row-source diagnostic 或 production-public probe summary。 | `test-rvv/registration/transformation_estimation_2D/script/generate_te2d_board_repeated_summary.py` |
| row-source QEMU summary | evidence output summary | 记录三类 row-source wrapper、9 个 case、manifest 和 Doctor。 | `run_bench_row_source_smoke`。 | `run_qemu_row_source_evidence_doctor`。 | QEMU log shape / asm path evidence。 | `test-rvv/registration/transformation_estimation_2D/log/qemu/row_source/` |
| row-source direct gather QEMU summary | evidence output summary | 记录三类 direct gather row-source wrapper、9 个 case、manifest 和 Doctor。 | `run_bench_row_source_direct_gather_smoke`。 | `run_qemu_row_source_direct_gather_evidence_doctor`。 | QEMU log shape / asm path evidence。 | `test-rvv/registration/transformation_estimation_2D/log/qemu/row_source_direct_gather/` |
| source-indexed family A/B QEMU summary | evidence output summary | 记录 direct source-indexed public 与 materialize+ordered public 两个 RVV family、6 个 comparison、manifest 和 Doctor。 | `run_bench_source_indexed_family_ab_smoke`。 | `run_qemu_source_indexed_family_ab_evidence_doctor`。 | QEMU production-detail path evidence；不证明性能。 | `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_family_ab/` |
| source-indexed generic point-type QEMU summary | evidence output summary | 记录 source-indexed generic 16 个代表性 point-type / mixed comparison、manifest、Doctor 和 asm。 | `run_bench_source_indexed_generic_xyz_point_types_smoke`。 | `run_qemu_source_indexed_generic_evidence_doctor`。 | QEMU diagnostic path evidence；不证明性能或 production dispatch。 | `test-rvv/registration/transformation_estimation_2D/log/qemu/source_indexed_generic_xyz_point_types/` |
| dual-indexed family A/B QEMU summary | evidence output summary | 记录 direct dual-indexed public 与 materialize+ordered public 两个 RVV family、6 个 comparison、manifest 和 Doctor。 | `run_bench_dual_indexed_family_ab_smoke`。 | `run_qemu_dual_indexed_family_ab_evidence_doctor`。 | QEMU production-detail path evidence；不证明性能。 | `test-rvv/registration/transformation_estimation_2D/log/qemu/dual_indexed_family_ab/` |
| dual-indexed generic point-type QEMU summary | evidence output summary | 记录 dual-indexed generic 16 个代表性 point-type / mixed comparison、manifest、Doctor 和 asm。 | `run_bench_dual_indexed_generic_xyz_point_types_smoke`。 | `run_qemu_dual_indexed_generic_evidence_doctor`。 | QEMU diagnostic path evidence；不证明性能或 production dispatch。 | `test-rvv/registration/transformation_estimation_2D/log/qemu/dual_indexed_generic_xyz_point_types/` |
| correspondence generic point-type QEMU summary | evidence output summary | 记录 correspondence generic 16 个代表性 point-type / mixed comparison、manifest、Doctor 和 asm。 | `run_bench_correspondence_generic_xyz_point_types_smoke`。 | `run_qemu_correspondence_generic_evidence_doctor`。 | QEMU diagnostic path evidence；不证明性能或 production dispatch。 | `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_generic_xyz_point_types/` |
| correspondence staging profile QEMU summary | evidence output summary | 记录 direct correspondence public、staged-dual public 和 materialize+ordered public 三组 RVV label，9 个 comparison、manifest 和 Doctor。 | `run_bench_correspondence_staging_profile_smoke`。 | `run_qemu_correspondence_staging_profile_evidence_doctor`。 | QEMU staging/profile path evidence；不证明性能。 | `test-rvv/registration/transformation_estimation_2D/log/qemu/correspondence_staging_profile/` |
| diagnostic board repeated summary | evidence output summary | 记录 `ordered-cloud-pair-fused` 的 5-run B/A、bucket 和 checksum 边界。 | board run logs。 | evaluation / phase result / Handoff。 | weak-positive diagnostic。 | `test-rvv/registration/transformation_estimation_2D/log/board/ordered_cloud_pair_repeated/summary.md` |
| production-public board repeated summary | evidence output summary | 记录 `ordered-cloud-pair-public` 的 5-run B/A、bucket 和 Doctor。 | board run logs。 | Phase 050 / evaluation / Handoff。 | 当前窄范围 production candidate 的板卡证据。 | `test-rvv/registration/transformation_estimation_2D/log/board/ordered_cloud_pair_public_repeated/summary.md` |
| source-indexed family A/B board summary | evidence output summary | 记录 materialize+ordered public RVV / direct source-indexed public RVV 的 5-run B/A、bucket 和 Doctor。 | board run logs。 | Phase 092 / evaluation / Handoff。 | production-detail family selection，当前 weak-positive。 | `test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_family_ab_repeated/summary.md` |
| source-indexed generic point-type board summary | evidence output summary | 记录 source-indexed generic 16 个代表性 point-type / mixed case 的 5-run B/A、bucket 和 Doctor。 | board run logs。 | Phase 099 / evaluation / Handoff。 | source-indexed generic diagnostic，当前 mixed-negative。 | `test-rvv/registration/transformation_estimation_2D/log/board/source_indexed_generic_xyz_point_types_repeated/summary.md` |
| dual-indexed family A/B board summary | evidence output summary | 记录 materialize+ordered public RVV / direct dual-indexed public RVV 的 B/A、bucket 和 Doctor。 | board run logs。 | Phase 093 / Phase 107 / evaluation / Handoff。 | production-detail family selection；Phase 107 当前 patch candidate positive with 4K caveat。 | `test-rvv/registration/transformation_estimation_2D/log/board/dual_indexed_family_ab_phase107_repeated/summary.md` |
| dual-indexed generic point-type board summary | evidence output summary | 记录 dual-indexed generic 16 个代表性 point-type / mixed case 的 5-run B/A、bucket 和 Doctor。 | board run logs。 | Phase 100 / evaluation / Handoff。 | dual-indexed generic diagnostic，当前 negative。 | `test-rvv/registration/transformation_estimation_2D/log/board/dual_indexed_generic_xyz_point_types_repeated/summary.md` |
| correspondence generic point-type board summary | evidence output summary | 记录 correspondence generic 16 个代表性 point-type / mixed case 的 5-run B/A、bucket 和 Doctor。 | board run logs。 | Phase 101 / evaluation / Handoff。 | correspondence generic diagnostic，当前 negative。 | `test-rvv/registration/transformation_estimation_2D/log/board/correspondence_generic_xyz_point_types_repeated/summary.md` |
| correspondence staging profile board summary | evidence output summary | 记录 direct correspondence public RVV / staged-dual public RVV 的 20-run D/S、bucket 和 Doctor。 | board run logs。 | Phase 095 / evaluation / Handoff。 | production-detail staging/profile，当前 negative / no dispatch switch。 | `test-rvv/registration/transformation_estimation_2D/log/board/correspondence_staging_profile_repeated/summary.md` |
| generic point-type board repeated summary | evidence output summary | 记录四类 same-type、四组 mixed pair 的 16-case 5-run B/A、point type metadata 和 Doctor。 | board run logs。 | Phase 070 / PI1 plan。 | generic test-only representative performance。 | `test-rvv/registration/transformation_estimation_2D/log/board/generic_xyz_point_types_repeated/summary.md` |
| generic point-type asm summary | evidence output summary | 记录 `generic_candidate_lambda_boundary` 和关键 RVV 指令归属。 | generic QEMU/objdump target。 | Phase 070 / PI1 plan。 | test-only asm attribution。 | `test-rvv/registration/transformation_estimation_2D/log/qemu/generic_xyz_point_types/asm_attribution.md` |
| `doc/phases/050-pi2-production-patch-and-direct-evidence/result.zh.md` | phase result | 记录 PI2-PI5 probe、最新 board positive 和当前 review 边界。 | Phase 050 plan。 | README / roadmap / Handoff。 | current closeout truth。 | `test-rvv/registration/transformation_estimation_2D/doc/phases/050-pi2-production-patch-and-direct-evidence/result.zh.md` |
| `doc/phases/092-source-indexed-family-ab/result.zh.md` | phase result | 记录 source-indexed direct-vs-materialize 同边界 RVV family A/B 和 weak-positive caveat。 | Phase 092 plan。 | README / roadmap / Handoff。 | current source-indexed family-selection truth。 | `test-rvv/registration/transformation_estimation_2D/doc/phases/092-source-indexed-family-ab/result.zh.md` |
| `doc/phases/093-dual-indexed-family-ab/result.zh.md` | phase result | 记录 dual-indexed direct-vs-materialize 同边界 RVV family A/B 的历史 positive candidate 和 4K 长尾。 | Phase 093 plan。 | README / roadmap / Handoff。 | historical dual-indexed family-selection evidence；Phase 107 已刷新当前 patch candidate。 | `test-rvv/registration/transformation_estimation_2D/doc/phases/093-dual-indexed-family-ab/result.zh.md` |
| `doc/phases/099-source-indexed-generic-xyz-point-type-expansion/result.zh.md` | phase result | 记录 source-indexed generic PointXYZ-like candidate 的 correctness、QEMU/asm、board mixed-negative 和 no-production-change 决策。 | Phase 099 plan。 | README / roadmap / Handoff。 | current source-indexed generic diagnostic truth。 | `test-rvv/registration/transformation_estimation_2D/doc/phases/099-source-indexed-generic-xyz-point-type-expansion/result.zh.md` |
| `doc/phases/100-dual-indexed-generic-xyz-point-type-expansion/result.zh.md` | phase result | 记录 dual-indexed generic PointXYZ-like candidate 的 correctness、QEMU/asm、board negative 和 no-production-change 决策。 | Phase 100 plan。 | README / roadmap / Handoff。 | current dual-indexed generic diagnostic truth。 | `test-rvv/registration/transformation_estimation_2D/doc/phases/100-dual-indexed-generic-xyz-point-type-expansion/result.zh.md` |
| `doc/phases/101-correspondence-generic-xyz-point-type-expansion/result.zh.md` | phase result | 记录 correspondence generic PointXYZ-like candidate 的 correctness、QEMU/asm、board negative 和 no-production-change 决策。 | Phase 101 plan。 | README / roadmap / Handoff。 | current correspondence generic diagnostic truth。 | `test-rvv/registration/transformation_estimation_2D/doc/phases/101-correspondence-generic-xyz-point-type-expansion/result.zh.md` |

## 测试计划和 bench 计划

| 测试 / 证据 | 层级 | 当前状态 |
| --- | --- | --- |
| scalar public semantics tests | public input semantics（公开入口输入语义） | pass：Std/RVV 84/84；包含 ordered-cloud-pair、source-indexed、dual-indexed、correspondence valid boundary，size mismatch、非有限 x/y/z、generic fallback、source-indexed generic fallback、dual-indexed generic fallback、correspondence generic fallback、source-indexed / dual-indexed / correspondence public fallback、direct-gather fallback、Phase 092 / 093 / 094 family equivalence、Phase 095 staged-dual equivalence 和 Phase 096 locality/order equivalence。 |
| fused accumulator same-chain tests | numerical consistency（数值一致性） | pass：Std / RVV 84/84；四类 ordered same-type、四类 source-indexed generic same-type、四类 dual-indexed generic same-type、四类 correspondence generic same-type、四组 mixed pair、extra fields、near-cancellation、row-source direct gather、source-indexed / dual-indexed / correspondence public probe、source/dual/correspondence family materialize 对拍、correspondence staged-dual 对拍和 locality/order 三路 public family 对拍。 |
| row source adapter tests | production-shaped diagnostic（生产形态诊断） | pass：三类 materialize-to-ordered 与 direct gather candidate 均与 public scalar overload 对拍；不覆盖非法 index 安全合同。 |
| fallback / gate tests | fallback tests（回退路径测试） | pass for diagnostic dense finite gate、non-finite fallback、小规模 fallback；production patch 保留同一 gate。 |
| QEMU bench smoke | QEMU correctness / log shape | pass：diagnostic、production-public、source-indexed public、source-indexed generic、dual-indexed generic、correspondence generic、row-source materialize/direct、correspondence staging/locality/component ablation 和 generic smoke 均可生成 manifest / doctor；generic、source-indexed generic、dual-indexed generic 和 correspondence generic 各覆盖 16 cases，source-indexed public 覆盖 3 cases。 |
| asm attribution | disassembly（反汇编） | pass for path hit：production-public probe 可归属 public overload 或 `runPublicCase` 内联边界。 |
| board repeated bench | board performance（板卡性能） | diagnostic summary 为 `weak_positive`；Phase 050 production-public 为 `4.222x / 5.310x / 4.947x`；Phase 080 generic public 16 cases 中 15 个 `B/A<1=0/5`，PointNormal->PointNormal 64K 为 `1/5`，Doctor 0/6/0；Phase 091 source-indexed public 为 `4.103x / 4.818x / 4.575x`，Doctor 0/0/0；Phase 092 source-indexed family A/B 为 `1.089x / 1.044x / 1.037x`，Doctor 0/0/2；Phase 099 source-indexed generic 为 mixed-negative，Doctor 5/10/1；Phase 093 dual-indexed family A/B 为 `1.018x / 1.671x / 1.554x`，Doctor 0/3/1；Phase 100 dual-indexed generic 为 negative，Doctor 13/17/1；Phase 094 correspondence public 为 `4.517x / 3.705x / 3.149x`，Doctor 0/2/0；correspondence family A/B 为 `1.081x / 1.639x / 1.406x`，Doctor 1/3/0；Phase 095 correspondence staged-dual D/S 为 `0.833x / 0.729x / 0.848x`，Doctor 3/2/0；Phase 096 locality/order D/S identity `0.850x / 0.621x / 0.756x`、reverse `0.851x / 0.690x / 0.759x`、shuffled `0.928x / 0.974x / 0.978x`、strided `0.893x / 0.887x / 0.927x`，Doctor 12/5/0；Phase 097 component ablation full-anchor D/S 为 `0.834x / 0.761x / 0.841x`，Doctor 3/2/0；Phase 101 correspondence generic 为 negative，Doctor 11/19/2。 |
| row-source board repeated bench | board performance（板卡性能） | materialize 已按同输入语义刷新；direct gather 已完成。source-indexed direct 为 1.199x / 1.135x / 1.130x；dual/correspondence 有 unstable/negative。 |
| Evidence Doctor | evidence validation（证据体检） | production-public QEMU/board 为 0/0/0；source-indexed public QEMU/board 为 0/0/0；source-indexed family A/B QEMU 为 0/0/0、board 为 0/0/2；source-indexed generic QEMU 为 0/0/0、board 为 5/10/1；dual-indexed family A/B QEMU 为 0/0/0、board 为 0/3/1；dual-indexed generic QEMU 为 0/0/0、board 为 13/17/1；correspondence generic QEMU 为 0/0/0、board 为 11/19/2；correspondence public QEMU 为 0/0/0、board 为 0/2/0；correspondence family A/B QEMU 为 0/0/0、board 为 1/3/0；correspondence staging profile QEMU 为 0/0/0、board 为 3/2/0；correspondence locality/order profile QEMU 为 0/0/0、board 为 12/5/0；correspondence component ablation QEMU 为 0/0/0、board 为 3/2/0；row-source materialize board 为 1/1/6；row-source direct board 为 5/6/1；generic QEMU 为 0/0/0、generic board 为 0/3/2，Warning/Suggestion 已按 case 解释。 |

## 生产接入判断

当前判断：`adopted-by-user / traits-gated ordered-cloud-pair` 加
`adopted-by-user / source-indexed PointXYZ->PointXYZ narrow dispatch`；Phase 107 又把
`dual-indexed PointXYZ->PointXYZ` 接入当前 production patch candidate。correspondence
exact RVV 已试接入后退回，当前没有 correspondence production dispatch。理由：

- Production-public repeated board 是性能主证据。它覆盖真实公开入口 probe，三个规模分别为 `4.222x / 5.310x / 4.947x`，每个规模 `B/A<1` 为 `0/5`。
- Evidence Doctor 对 board production-public manifest 给出 Errors=0、Warnings=0、Suggestions=0；这支持保留窄范围 patch，但不扩大 gate。
- QEMU smoke 和 asm attribution 只证明路径命中和指令归属，真实性能仍只引用板卡结果。
- Phase 020 的 diagnostic `weak_positive` 只说明 test-only helper 有探索价值，不能替代 Phase 050 的 production-public evidence。
- 当前 production patch 位于 `registration/include/pcl/registration/impl/transformation_estimation_2D.hpp`；用户已确认当前优化可以采纳；不命中 gate 的输入仍保持标量路径。
- Phase 091 的 source-indexed public repeated board 覆盖真实 source-indexed public overload，
  三个规模分别为 `4.103x / 4.818x / 4.575x`，每个规模 `B/A<1=0/5`，Doctor `0/0/0`。
  用户已在 2026-08-18 确认采纳。它证明该 public RVV path 快于 public scalar path，但不证明 direct gather
  family 优于 materialize/staging，也不覆盖 dual/correspondence。
- Phase 092 的 same-boundary family A/B 显示 materialize+ordered public RVV 相对 direct
  source-indexed public RVV 的 B/A 为 `1.089x / 1.044x / 1.037x`，overall `weak_positive`，
  board Doctor `0/0/2`。这支持保留 current direct gather family，但不构成新的 production
  dispatch 修改或其它 row source 结论。
- Phase 107 的 same-boundary family A/B 显示 materialize+ordered public RVV 相对 direct
  dual-indexed public RVV 的 B/A 为 `1.085x / 1.691x / 1.678x`，overall `positive`，
  board Doctor `0/3/0`。这支持当前 exact `PointXYZ -> PointXYZ` dual-indexed direct
  gather family进入当前补丁候选；4K `1/5` below-1 保留为 caveat，不覆盖泛型点类型。
- Phase 094 的 correspondence public probe 曾显示 public RVV 相对 public scalar 为 positive；
  但 Phase 107 同 boundary family A/B 在 256K 有 `4/20` below-1，overall decision bucket
  为 negative，board Doctor `0/4/0`。因此 correspondence direct RVV 已按用户规则退回，
  当前不能写成 guarded retained dispatch 或 clean-adopt。
- Phase 095 的 correspondence staged-dual profile 显示 direct/staged D/S 为
  `0.833x / 0.729x / 0.848x`，D/S<1 分别为 `20/20`、`20/20`、`15/20`，board Doctor
  `3/2/0`。因此 staged query/match indices + dual-indexed public RVV 不能作为
  correspondence production dispatch switch。
- Phase 096 locality/order profile 和 Phase 097 component ablation 继续为 negative：
  Phase 096 的四类 query/match 分布均未支持 staged-dual switch；Phase 097 的 strided full-anchor
  D/S 为 `0.834x / 0.761x / 0.841x`，board Doctor `3/2/0`。component no-solve 只说明
  complete materialize 写回成本高、prematerialized ordered full 很快，不能直接推出新的生产实现族。
- Phase 101 的 correspondence generic point-type diagnostic 覆盖四类 same-type 和四组
  mixed pair，QEMU Doctor `0/0/0`，asm 归属 860 RVV lines，但 5-run board Doctor 为
  `11/19/2`，多数 64K/256K 和 mixed pair 为 negative。因此它只关闭 generic widening
  诊断，不恢复 Phase 107 已退回的 exact correspondence dispatch。

### 诊断证据链

Phase 030 的证据链是：三类 row-source 的合法行配对测试 -> materialize-to-ordered candidate correctness -> 9-case QEMU smoke -> `row_source_lambda_boundary` asm attribution -> 5-run `Milkv-Jupiter` board repeated。Phase 090 按同输入语义刷新后，materialize board Doctor 为 1/1/6。该链条能证明候选的输入展开、数学路径和真实板卡表现可复核；它仍不能证明 production dispatch 或 gather kernel 的收益。

Phase 090 的证据链是：三类 row-source direct gather 与 public overload 对拍 -> selected-row
finite fallback tests -> `row-source-direct-gather` QEMU smoke -> `row_source_lambda_boundary`
asm attribution -> 5-run `Milkv-Jupiter` board repeated -> Evidence Doctor 5/6/1。source-indexed
direct gather 三个规模无退化，可进入有界 PI1；dual-indexed 和 correspondence 的 negative /
unstable bucket 只能作为 diagnostic-only 或 guarded probe 输入。该链条不能直接证明或拒绝
production dispatch。

Phase 091 的 source-indexed production probe 证据链是：真实 public overload RVV hit ->
small / non-dense / selected non-finite / non-covered point type fallback -> `source-indexed-public`
QEMU smoke -> `production_public_source_indexed_boundary` asm attribution -> 5-run
`Milkv-Jupiter` board repeated -> Evidence Doctor 0/0/0。该链条支持 bounded production
candidate；用户已确认采纳该 narrow production patch。

Phase 092 的 source-indexed family A/B 证据链是：materialize selected source rows 后调用真实
ordered-cloud-pair public overload -> 与 direct source-indexed public overload 做 correctness /
checksum 对拍 -> `source-indexed-family-ab` QEMU smoke -> `source_indexed_family_ab_lambda_boundary`
asm attribution -> 5-run `Milkv-Jupiter` board repeated -> Evidence Doctor 0/0/2。该链条只回答
source-indexed `PointXYZ -> PointXYZ` 的 RVV-family-selection，结论为弱正向保留 current direct
gather。

Phase 093 / 107 的 dual-indexed family A/B 证据链是：materialize selected source/target rows 后调用真实
ordered-cloud-pair public overload -> 与 direct dual-indexed public overload 做 correctness /
checksum 对拍 -> `dual-indexed-family-ab` QEMU smoke -> `dual_indexed_family_ab_lambda_boundary`
和 `production_public_dual_indexed_boundary` asm attribution -> Phase 107 5-run `Milkv-Jupiter`
board repeated -> Evidence Doctor 0/3/0。该链条只回答 dual-indexed `PointXYZ -> PointXYZ`
的 RVV-family-selection；64K / 256K 是主决策规模，4K `1/5` below-1 必须保留为 caveat。

Phase 094 / 107 的 correspondence 证据链分两层：public probe 先用真实 correspondence public
overload 做 Std/RVV 对比，QEMU Doctor `0/0/0`、`production_public_correspondence_boundary`
47 RVV lines、board 为 `4.517x / 3.705x / 3.149x`、Doctor `0/2/0`；随后 family A/B
在同一 RVV binary 内比较 direct correspondence public RVV 与 materialize+ordered public RVV，
`correspondence_family_ab_lambda_boundary` 12 RVV lines。Phase 107 family A/B 为
4K `1.091x`、64K `1.645x`、256K `1.385x`，但 256K 有 `4/20` 低于 1，overall
negative，Doctor `0/4/0`。该链条证明 public path 正向不能覆盖 family A/B negative；
当前 production header 已移除 correspondence RVV dispatch。

Phase 095 的 correspondence staging profile 证据链是：staged-dual public helper 与 direct
correspondence public overload 做 matrix / checksum 对拍 -> `correspondence-staging-profile`
QEMU smoke -> `production_public_dual_indexed_boundary` 和 `production_public_correspondence_boundary`
asm attribution -> 20-run `Milkv-Jupiter` board repeated -> Evidence Doctor `3/2/0`。该链条只说明
staged-dual 不适合作为当前 correspondence dispatch switch，也不恢复 Phase 107 已退回的
correspondence direct dispatch。

Phase 096 的 correspondence locality/order 证据链是：identity / reverse / shuffled /
strided 四类 query/match profile -> direct/staged/materialize 三条 public RVV 对拍 ->
`correspondence_locality_order_profile_lambda_boundary` asm attribution -> 20-run board repeated
-> Evidence Doctor `12/5/0`。该链条只说明 locality/order 不能支持 staged-dual switch。

Phase 097 的 correspondence component ablation 证据链是：scan / extract / gather /
materialize / full-anchor 多 label -> `correspondence_component_ablation_lambda_boundary`
asm attribution -> 10-run board repeated -> Evidence Doctor `3/2/0`。该链条只说明
complete materialize 写回成本高、prematerialized ordered full 很快，但 no-solve component 与
full estimate 的 sink 不同，不能据此 clean-adopt chunked staging；Phase 107 的退回结论来自
后续同边界 production-detail family A/B。

### Generic 诊断证据链

Phase 070 的 generic 诊断证据链是：source/target 独立 traits gate -> 四类 same-type 和四组
mixed correctness -> extra-field 与 small/non-dense/non-finite fallback -> 16-case QEMU
smoke -> `generic_candidate_lambda_boundary` asm attribution -> Milkv-Jupiter 5-run
representative board summary -> Evidence Doctor `0/3/2`。Phase 080 在此基础上补充真实
public direct correctness -> `generic-xyz-point-types-public` QEMU -> `production_public_generic_boundary`
asm -> public board repeated -> Evidence Doctor `0/6/0`。该接入后链条只覆盖代表性点型组合，
且 PointNormal->PointNormal 64K 为 negative，不能外推到未逐类型上板的自定义点型。

Phase 099 的 source-indexed generic 诊断链条是：source / target 独立
`RVVXYZAoSFloatLayout` gate -> 四类 same-type、四组 mixed correctness -> selected-row
finite / small / non-dense / double fallback -> source-indexed generic QEMU smoke ->
`source_indexed_generic_candidate_lambda_boundary` 反汇编归属 -> 5-run board repeated ->
Evidence Doctor `5/10/1`。该链条的 mixed-negative 结果只关闭当前 source-indexed generic
diagnostic widening，不回滚 Phase 091 adopted narrow patch。

Phase 100 的 dual-indexed generic 诊断链条是：source / target 两条 index stream 分别通过
`RVVXYZAoSFloatLayout` gate -> 四类 same-type、四组 mixed correctness -> selected source /
target finite、small、non-dense、double fallback -> dual-indexed generic QEMU smoke ->
`dual_indexed_generic_candidate_lambda_boundary` 880 RVV lines -> 5-run board repeated ->
Evidence Doctor `13/17/1`。该链条证明候选路径和 fallback 可复核，但 negative 结果只关闭
当前 dual-indexed generic diagnostic widening，不回滚 Phase 093 exact candidate，也不能
外推到 correspondence。

Phase 101 的 correspondence generic 诊断链条是：source/query 与 target/match 两侧分别通过
`RVVXYZAoSFloatLayout` gate -> 四类 same-type、四组 mixed correctness -> selected query /
match finite、small、non-dense、double fallback -> correspondence generic QEMU smoke ->
`correspondence_generic_candidate_lambda_boundary` 860 RVV lines -> 5-run board repeated ->
Evidence Doctor `11/19/2`。该链条证明候选路径和 fallback 可复核，但 negative 结果只关闭
当前 correspondence generic diagnostic widening，不恢复 Phase 107 已退回的 exact correspondence dispatch，
也不能外推到 source-indexed 或 dual-indexed。

## 文档归属

| 信息 | 主归属 |
| --- | --- |
| 当前函数级评估、候选取舍和生产接入判断 | 本文 |
| 测试体系、correctness、bench / evidence、代码地图 | `doc/testing-overview.zh.md`、`doc/correctness-tests.zh.md`、`doc/benchmark-and-evidence.zh.md`、`doc/test-support-code-map.zh.md` |
| 跨阶段 candidate frontier（候选前沿） | `doc/optimization-roadmap.zh.md` |
| 阶段计划、结果和早停检查 | `doc/phases/` |
| 候选矩阵状态 | `doc/phases/optimization-matrix.zh.md` |
| 未来 production 长期行为 | `doc-rvv/registration/transformation_estimation_2D-RVV.zh.md` 记录当前 ordered generic、Phase 091 source-indexed narrow patch、Phase 107 dual-indexed exact patch candidate、correspondence 已退回状态和证据边界 |

## Remaining Risk 与后续扩展条件

| 风险 | 当前为什么未闭合 | 后续闭合条件 |
| --- | --- | --- |
| source-indexed / dual-indexed / correspondence RVV | materialize 与 direct gather 的 correctness、QEMU、asm 和板卡已完成；source-indexed narrow production patch 已采纳，且同边界 family A/B 已补为 weak-positive；source-indexed generic mixed-negative 且 Phase 106 public variance negative；dual-indexed exact Phase 107 family A/B 为 positive 但 4K 有 caveat，Phase 100 generic diagnostic 为 negative；correspondence Phase 107 family A/B 为 negative，Phase 095 staged-dual、Phase 096 locality/order、Phase 097 component ablation、Phase 098 chunked staging 和 Phase 101 generic diagnostic 均为 negative。 | dual-indexed exact 当前保留在 production patch candidate，需用户检查后才可写成最终 adopted；correspondence 已退回，只有新的 bounded candidate 才可恢复；source-indexed / dual-indexed / correspondence generic production widening 只有另开 PI1-PI5 并重新补接入后证据才可恢复；不能继承其它 row source 结论。 |
| production ordered-cloud-pair RVV | 已获用户确认采纳，但还没有提交。 | 用户明确授权后再进入 commit phase；继续保留 fallback 和代表性性能边界。 |
| 泛型点型 | Phase 080 已完成代表性 production public 接入后证据并由用户确认采纳；board 仍有一个 negative bucket。 | 未逐类型上板的自定义点型、RGB/RGBA 和其它 layout 继续独立 phase。 |
| 泛型 production 接入 | Phase 080 已按 generic gate 重跑真实 public dispatch；board 有一个 negative bucket；用户已确认采纳。 | 不自动提交或回滚；其它 point type / row source 继续独立 phase。 |
| `Scalar=double` | double reduction、误差预算和目标硬件证据未覆盖。 | double-specific correctness、asm 和 board repeated evidence。 |
| 负向归因 | correspondence direct gather 的 diagnostic 负向不能单因归因为 gather、reduction 或 wrapper 成本；Phase 094 曾证明 public probe positive，但 Phase 107 同边界 family A/B 的 256K 退化频率仍未解决。Phase 095 已排除 staged query/match indices + dual-indexed public 作为净收益路线；Phase 096 又显示 identity、reverse、shuffled、strided 四类 locality/order profile 不支持 staged-dual switch；Phase 097 进一步显示 component ablation 只能提供 bottleneck 线索；Phase 098 chunked staging 和 Phase 101 generic point-type 也为 negative。 | correspondence 后续若继续，必须定义新的 bounded candidate；不能切 staged-dual、不能泛型扩宽，也不能 clean-adopt。 |
