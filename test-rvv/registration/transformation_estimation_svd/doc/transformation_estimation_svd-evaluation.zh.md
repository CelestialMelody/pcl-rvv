# transformation_estimation_svd 函数级评估

## 范围和当前结论

本评估覆盖 `pcl::registration::TransformationEstimationSVD<PointSource, PointTarget, Scalar>` 的 rigid transformation estimation（刚体变换估计）路径。目标源码是：

- `registration/include/pcl/registration/transformation_estimation_svd.h`
- `registration/include/pcl/registration/impl/transformation_estimation_svd.hpp`

当前 EvidenceDecision（证据决策）是 `production-ready / adopted for ordered-cloud-pair, source-indexed-cloud-pair, dual-indices-cloud-pair and correspondence-pair`。production（生产源码）已经接入四个 RVV fast path（RVV 快路径）：

- ordered-cloud-pair（顺序点云对）：`source[i]` 与 `target[i]` 配对。
- source-indexed-cloud-pair（源索引点云对）：`source[indices_src[i]]` 与 `target[i]` 配对。
- dual-indices-cloud-pair（双索引点云对）：`source[indices_src[i]]` 与 `target[indices_tgt[i]]` 配对。
- correspondence-pair（对应关系点对）：`source[corr.index_query]` 与 `target[corr.index_match]` 配对。

`full-cloud` 不是当前规范 row-source 名称，只作为 Phase 000/010 的历史 case-filter、日志目录或 legacy alias（历史别名）保留。新文档、矩阵和恢复入口统一使用 `ordered-cloud-pair`。

## S0 偏好冻结

| 字段 | 当前值 |
| --- | --- |
| `preferences_loaded` | defaults loaded；local override absent；prompt override 指定 RVV worker、目录和目标源码。 |
| `comment_policy_frozen` | test-rvv / diagnostic / prototype 使用详细中文注释；production 只写维护边界、fallback（回退路径）、dispatch（分流逻辑）、数值风险和数据布局。 |
| `documentation_policy_frozen` | closeout current-state-first（当前状态优先）；复杂数值函数需要数值算例；长期文档不保留对话流程话术。 |
| `evidence_policy_frozen` | summary-only；raw logs 默认 local-only；QEMU 只证明 correctness / log shape（正确性 / 日志形状），性能结论必须来自 board / target hardware（板卡或目标硬件）。 |
| `commit_policy_frozen` | 不自动 commit；若进入提交阶段，topic、evidence logs 和 agent assets 分开。 |
| `agent_asset_feedback_policy` | report-only；未获 workflow improvement 授权时不修改 `.agents`。 |

## 标量路径和 RVV 接入边界

| 公开入口 | 当前源码语义 | RVV 状态 |
| --- | --- | --- |
| ordered-cloud-pair | 检查 source / target size 一致；`__RVV10__` 构建先尝试 `estimateRigidTransformationSVDOrderedCloudPairRVV`，失败后构造两个 `ConstCloudIterator` 进入共同 iterator helper。 | adopted。Phase 020 已接入 dense ordered-cloud-pair、`Scalar=float`、xyz AoS layout-gated production RVV path。 |
| source-indexed-cloud-pair | 检查 `indices_src.size() == cloud_tgt.size()`；`__RVV10__` 构建先尝试 `estimateRigidTransformationSVDSourceIndexedCloudPairRVV`，失败后用 source indices iterator + target sequential iterator 进入共同 helper。 | adopted。Phase 040 已接入 source-side indexed gather、target-side strided load 和生产 fallback gate。 |
| dual-indices-cloud-pair | source / target 都通过 indices iterator；数量不一致直接返回。 | adopted。Phase 060 已接入双 gather、两侧 index 合法性、fallback gate 和 production board evidence。 |
| correspondence-pair | source / target 都由 `pcl::Correspondences` 生成 iterator，一侧用 query，一侧用 match。 | adopted。Phase 060 已接入 correspondence gather、query/match 合法性检查、fallback gate 和 production board evidence。 |
| iterator helper + `use_umeyama_ == true` | 分配两个 `3 x N` Eigen 动态矩阵，逐点复制 xyz，调用 `pcl::umeyama(src, tgt, false)`。这是类默认路径。 | 四条 RVV path 避免动态矩阵装填；其它入口仍保留这条标量 truth（标量事实）。 |
| iterator helper + `use_umeyama_ == false` | 重置 iterator，分别 `compute3DCentroid`，再 `demeanPointCloud` 生成动态矩阵，最后 `cloud_src_demean * cloud_tgt_demean.transpose()` 和 3x3 SVD。 | rejected / fallback for current scope。该分支不是当前 production RVV 范围。 |
| `getTransformationFromCorrelation` | 只处理 3x3 H、SVD、determinant sign fix（行列式符号修正）、translation（平移）和 debug loss。 | 每次 estimate 只执行一次，不是逐点热点；当前保持 Eigen 标量后段。 |

## 实现方式审计

| candidate family | 范围 | 证据需求 | 当前状态 |
| --- | --- | --- | --- |
| `fused_ordered_cloud_pair_accum` | dense ordered-cloud-pair `PointXYZ` / `float` 诊断候选；历史 alias 为 `fused_full_cloud_accum`。 | QEMU correctness、asm、board repeated、Evidence Doctor、production direct。 | adopted。Phase 010 diagnostic positive，Phase 020 已接 production。 |
| `source_indexed_fused_accum` | source indices + target sequential。source 侧用 indexed gather，target 侧连续读取。 | policy-specific correctness、index 合法性、32-bit gather gate、asm、board repeated、production direct。 | adopted。Phase 030 diagnostic positive，Phase 040 已接 production。 |
| `dual_indices_fused_accum` | source indices + target indices。 | 双 gather、两侧 index 合法性、correctness、asm、board repeated、production direct。 | adopted。Phase 050 diagnostic positive，Phase 060 已接 production。 |
| `correspondence_fused_accum` | correspondence query/match。 | correspondence 语义审计、correctness、asm、board repeated、production direct。 | adopted。Phase 050 diagnostic positive，Phase 060 已接 production。 |
| `production_direct_dispatch` | 真实 public ordered/source-indexed/dual-indices/correspondence overload 分流。 | production helper、fallback tests、public bench、asm attribution 和 board repeated。 | adopted for all four row sources。 |
| `generic_xyz_aos_gate` | `PointXYZI`、`PointXYZRGB` 和其它满足 `RVVXYZAoSFloatLayout` 的 xyz AoS 点型。 | traits gate、representative correctness、fallback boundary、代表性板卡性能边界说明。 | correctness adopted；未逐类型上板，不能写成逐类型性能已证明。 |
| `Scalar=double` | 所有 row source 的 double 输出矩阵。 | 需要独立 double RVV 数值预算和性能计划。 | rejected / fallback。当前没有 double RVV 计划。 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 上游 / 消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- |
| `TransformationEstimationSVD::estimateRigidTransformation(cloud_src, cloud_tgt, matrix)` | production public entry | ordered-cloud-pair 公开入口，数量一致后先尝试 ordered RVV fast path，失败后进入 iterator helper | ICP / registration 估计器 | production direct boundary（真实生产路径边界） | `registration/include/pcl/registration/impl/transformation_estimation_svd.hpp` |
| `TransformationEstimationSVD::estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, matrix)` | production public entry | source-indexed 公开入口，数量一致后先尝试 source-indexed RVV fast path，失败后进入 iterator helper | registration 调用方 | production direct boundary / gather gate | 同上 |
| `TransformationEstimationSVD::estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, indices_tgt, matrix)` | production public entry | dual-indices 公开入口，数量一致后先尝试 dual-indices RVV fast path，失败后进入 iterator helper | registration 调用方 | production direct boundary / dual gather gate | 同上 |
| `TransformationEstimationSVD::estimateRigidTransformation(cloud_src, cloud_tgt, correspondences, matrix)` | production public entry | correspondence 公开入口，先尝试 correspondence RVV fast path，失败后进入 iterator helper | registration 调用方 | production direct boundary / correspondence gate | 同上 |
| iterator helper | production shared helper | 根据 `use_umeyama_` 选择动态矩阵 Umeyama 或 centroid / demean / correlation | 所有 fallback public entry | scalar truth | 同上 |
| `detail::estimateRigidTransformationSVDOrderedCloudPairRVV` | production dispatch / fallback | ordered gate：`Scalar=float`、dense、`use_umeyama_ == true`、xyz AoS layout 和 small-input gate | ordered public entry | production RVV dispatch / fallback gate | 同上 |
| `detail::accumulateTransformationEstimationSVDOrderedCloudPairRVV` | production RVV helper | 用 `strided_load3_f32m2` 加载 source/target xyz，累加 sum/cross sum | ordered dispatch helper | production asm / board performance | 同上 |
| `detail::estimateRigidTransformationSVDSourceIndexedCloudPairRVV` | production dispatch / fallback | source-indexed gate：source indices 合法性、32-bit byte offset、dense、layout、`Scalar=float` 和 size gate | source-indexed public entry | production RVV dispatch / fallback gate | 同上 |
| `detail::accumulateTransformationEstimationSVDSourceIndexedCloudPairRVV` | production RVV helper | source 侧 `indexed_load3_f32m2`，target 侧 `strided_load3_f32m2`，累加 sum/cross sum | source-indexed dispatch helper | production asm / board performance | 同上 |
| `detail::estimateRigidTransformationSVDDualIndicesCloudPairRVV` | production dispatch / fallback | dual-indices gate：两侧 indices 等长且合法、dense、layout、`Scalar=float`、`use_umeyama_ == true` | dual-indices public entry | production RVV dispatch / fallback gate | 同上 |
| `detail::accumulateTransformationEstimationSVDDualIndicesCloudPairRVV` | production RVV helper | source / target 两侧都以 indexed gather 加载，并累加 sum/cross sum | dual-indices dispatch helper | production asm / board performance | 同上 |
| `detail::estimateRigidTransformationSVDCorrespondencePairRVV` | production dispatch / fallback | correspondence gate：query/match 合法、dense、layout、`Scalar=float`、`use_umeyama_ == true` | correspondence public entry | production RVV dispatch / fallback gate | 同上 |
| `detail::accumulateTransformationEstimationSVDCorrespondencePairRVV` | production RVV helper | correspondence query/match 以 gather 读取，并累加 sum/cross sum | correspondence dispatch helper | production asm / board performance | 同上 |
| `include/tesvd.h` | test support aggregator | 稳定测试支撑入口 | `src/test_tesvd.cpp`、`src/bench_tesvd.cpp` | reviewer navigation（审查定位） | `test-rvv/registration/transformation_estimation_svd/include/tesvd.h` |
| `include/impl/tesvd_support.hpp` | fixture / samples | 构造点云、indices、correspondences、刚体变换和共享统计结构 | candidate / correctness / bench | correctness / bench input | `test-rvv/registration/transformation_estimation_svd/include/impl/tesvd_support.hpp` |
| `include/impl/tesvd_candidates.hpp` | reference / candidate helper | ordered/source-indexed/dual/correspondence fused reference、RVV candidate、checksum | gtest / bench wrapper | correctness / bench input | `test-rvv/registration/transformation_estimation_svd/include/impl/tesvd_candidates.hpp` |
| `src/test_tesvd.cpp` | correctness test | 验证 public Umeyama、四条 row source candidate、production direct path-hit、fallback 和 mixed-field 点型 | QEMU / board test target | correctness gate（正确性验收） | `test-rvv/registration/transformation_estimation_svd/src/test_tesvd.cpp` |
| `src/bench_tesvd.cpp` | bench wrapper | 输出 public production direct 和 fused diagnostic 的可解析 timing / checksum | QEMU / board bench target | log shape / board bench | `test-rvv/registration/transformation_estimation_svd/src/bench_tesvd.cpp` |
| `script/generate_tesvd_board_repeated_summary.py` | analysis script | 汇总 board repeated summary、manifest 和 Evidence Doctor 输入 | board repeated targets | production / diagnostic evidence summary | `test-rvv/registration/transformation_estimation_svd/script/generate_tesvd_board_repeated_summary.py` |
| `doc/phases/060-dual-indices-correspondences-production-integration/result.zh.md` | phase result | dual-indices / correspondence PI1-PI5 完成事实和 EvidenceDecision | roadmap / reviewer | recovery pointer | `test-rvv/registration/transformation_estimation_svd/doc/phases/060-dual-indices-correspondences-production-integration/result.zh.md` |

## 正确性与高效性证据链

当前 production 证据链已经闭合到 board / target hardware 层：

1. correctness：`make -C test-rvv/registration/transformation_estimation_svd run_test_compare` 中 Std / RVV 构建各 22 个 gtest 通过。测试覆盖 public Umeyama 语义锚点、四条 row source 的 fused reference、production helper path-hit（路径命中）、fallback gate、`PointXYZI` / `PointXYZRGB` 代表性 xyz AoS layout，以及 determinant sign fix stress。
2. QEMU path evidence：`ALLOW_QEMU_BENCH_COMPARE=1 make -C test-rvv/registration/transformation_estimation_svd run_bench_compare BENCH_ARGS="--case-filter source-indexed-cloud-pair --iterations 3 --warmup-iterations 1"` 只证明 bench binary、case label 和 checksum 形状可解析；QEMU timing 不进入性能结论。
3. board correctness：`make -C test-rvv/registration/transformation_estimation_svd run_board_test_smoke` 在板卡 RVV binary 上 22/22 通过。
4. ordered asm attribution：`dump_bench_rvv` 生成 bench RVV 反汇编；ordered production public overload 符号内可见 `vlsseg3e32.v`、`vfadd.vv`、`vfmacc.vv` 和 `vfredosum.vs`。
5. source-indexed asm attribution：同一反汇编中 source-indexed public overload 符号 `0x21fb6` 内可见 `vluxseg3ei32.v`、`vlsseg3e32.v`、`vfmacc.vv` 和 `vfredosum.vs`。
6. dual-indices asm attribution：dual-indices public overload 符号内可见双 gather / FMA / reduction 指令簇，和 source-indexed 一样保持 `vluxseg3ei32.v` 与 `vlsseg3e32.v` 的组合特征。
7. correspondence asm attribution：correspondence public overload 符号内可见 gather / FMA / reduction 指令簇；`vlse32` base-pointer 修正保持在生产 helper 内，不回退到旧写法。
8. ordered production board performance：`run_board_bench_production_ordered_cloud_pair_repeated` 生成 `log/board/production_ordered_cloud_pair_repeated/summary.md`；public Std/RVV median 为 4K `14.372x`、64K `24.471x`、256K `23.841x`，decision bucket 为 `positive`。
9. source-indexed production board performance：`run_board_bench_production_source_indexed_cloud_pair_repeated` 生成 `log/board/production_source_indexed_cloud_pair_repeated/summary.md`；public Std/RVV median 为 4K `9.634x`、64K `12.217x`、256K `11.558x`，decision bucket 为 `positive`。
10. dual-indices production board performance：`run_board_bench_production_dual_indices_cloud_pair_repeated` 生成 `log/board/production_dual_indices_cloud_pair_repeated/summary.md`；public Std/RVV median 为 4K `6.805x`、64K `6.404x`、256K `5.964x`，decision bucket 为 `positive`。
11. correspondence production board performance：`run_board_bench_production_correspondence_pair_repeated` 生成 `log/board/production_correspondence_pair_repeated/summary.md`；public Std/RVV median 为 4K `8.649x`、64K `8.644x`、256K `7.872x`，decision bucket 为 `positive`。
12. Evidence Doctor：ordered production doctor 为 Errors=0、Warnings=1、Suggestions=0；source-indexed production doctor 为 Errors=0、Warnings=0、Suggestions=0；dual-indices 和 correspondence production doctor 都是 Errors=0、Warnings=1、Suggestions=0。各条 warning 都按 size 分开解释，不把大规模收益或长尾波动外推成别的 row source 结论。
13. production decision：四条 row source policy 在上述 gate 下都进入 production；`Scalar=double`、非 dense、`use_umeyama_ == false` 和不满足 layout gate 的点型保持标量。

## 泛型点类型边界

当前 production gate 不是 exact `PointXYZ` gate。source 和 target 分别使用 `pcl::rvv::RVVXYZAoSFloatLayout<PointSource/PointTarget>`，它证明 PCL traits 注册了单个 `float` 的 `x/y/z` 字段、POD standard-layout、`sizeof(PointT)==sizeof(POD)`、stride 和字段 offset 满足 f32 AoS 读取前提。

`PointXYZI` 和 `PointXYZRGB` 已通过四条 row source 的 production-direct correctness/path-hit 测试。它们不是“暂不接 production”；它们在 gate 允许时可以命中 RVV。当前 board repeated performance 只以代表性 `PointXYZ` case-filter 汇总，不是逐点型性能矩阵。因此可以声明：

- correctness（正确性）和分流边界覆盖代表性 mixed-field xyz AoS 点型。
- 性能结论来自代表性 `PointXYZ` board evidence。
- 不能声明 `PointXYZI` / `PointXYZRGB` 或所有泛型 xyz AoS 点型已经逐类型上板证明。

## 未闭合项

- `Scalar=double` 已明确 fallback。当前不建议继续 double RVV，除非用户另开 double 数值预算和性能计划。
- 非 dense 或包含 NaN / Inf 的输入没有进入当前 RVV gate；保持标量路径。
- 生产 public entry 目前仍在 RVV 尝试后保留原 iterator 标量主体。实现规则更偏好抽出命名 `*_Std` helper；这是后续结构审查点，不改变当前已验证语义。

## 生产接入判断

当前接入 production。范围为四条 row source policy、`Scalar=float`、source/target 分别满足 `RVVXYZAoSFloatLayout`、`use_umeyama_ == true`、dense、`n >= 16`。dual-indices 额外要求 indices 合法且两侧等长；correspondence 额外要求 query/match 合法；source-indexed 额外要求 indices 合法和 32-bit byte offset gather 边界。所有 out-of-scope gate 失败时自然 fallback 到原 `ConstCloudIterator` 标量路径。

默认下一阶段不再是新的 row-source phase，而是 `doc/phases/060-dual-indices-correspondences-production-integration/result.zh.md` 之后的 topic-local 文档、evaluation、roadmap、matrix 和长期 `doc-rvv` 收尾。`ready_for_review` 现在对四条 row source 的 production 范围都成立。
