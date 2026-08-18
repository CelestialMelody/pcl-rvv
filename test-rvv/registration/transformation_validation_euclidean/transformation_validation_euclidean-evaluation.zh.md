# registration/transformation_validation_euclidean 函数级 RVV 评估

## 范围

- 主题：`transformation_validation_euclidean`
- 主文件：`registration/include/pcl/registration/impl/transformation_validation_euclidean.hpp`
- 公开入口：`pcl::registration::TransformationValidationEuclidean<PointSource, PointTarget, Scalar>::validateTransformation`
- 专项目录：`test-rvv/registration/transformation_validation_euclidean/`
- 模块依据：`doc-rvv/library-screening/registration/registration-function-evaluation-queue.zh.md` 的建议优化队列第一项。

## 函数级结论

`TransformationValidationEuclidean` 用于给一个候选 4x4 变换打分：先把 source cloud 按变换矩阵生成临时 `input_transformed`，再对每个 transformed point 在 target cloud 上执行 `nearestKSearch(point, 1, ...)`，把不超过 `max_range_` 的 squared distance 累加并除以有效匹配数。`isValid` 只是调用本入口并与 `threshold_` 比较。

可 RVV 化的直接片段是前置 `PointSource -> PointTarget` 4x4 xyz transform staging：

```text
tx = m00*x + m01*y + m02*z + m03
ty = m10*x + m11*y + m12*z + m13
tz = m20*x + m21*y + m22*z + m23
```

但公开入口的后半段每点调用 KdTree nearest neighbor search。该 search 是不规则树遍历，当前 RVV 方案只覆盖 search 之前的 staging，不能覆盖入口主成本。因此本轮先作为 bench-diagnosis 主题推进：建立 `PointXYZ -> PointXYZ` 诊断 helper、专项 test、transform-staging microbench 和 full KdTree validation bench；只有 full diagnostic 在板卡上证明稳定明显收益时，后续才考虑把 helper 迁入生产分流。

当前不修改上游 `transformation_validation_euclidean.hpp`，不改变公开 API 和默认生产行为。

## 函数族评估

| 函数 / 路径                                             | 优先级 | RVV 决策                          | 覆盖 / 回退                                                                                             |
| ------------------------------------------------------- | ------ | --------------------------------- | ------------------------------------------------------------------------------------------------------- |
| `validateTransformation` 的前置整云 transform staging | 中     | 已建立 bench-diagnosis RVV helper | 诊断 helper 覆盖 `PointXYZ -> PointXYZ`、`Scalar=float`、点数 `>=64`；小规模或非 RVV 编译回退标量 |
| `validateTransformation` 的 full validation           | 中     | 只做 full diagnostic，不接生产    | transform staging 可选 RVV，KdTree search、`max_range` 过滤和 score 累加保持标量                      |
| 泛型 `PointSource` / `PointTarget`                  | 中     | 暂缓                              | PCL 模板允许不同点类型；生产接入前需要字段布局、类型转换和目标点构造边界                                |
| `Scalar=double`                                       | 低     | 回退标量                          | 原入口按 `Scalar` 矩阵计算后 cast 到 float；double RVV 对收益和语义都需要单独评估                     |
| KdTree search                                           | 低     | 不属于本主题 RVV 范围             | 搜索树遍历和 FLANN 内部结构不是规整 VL chunk 线性算子                                                   |

## RVV 诊断设计

新增 `transformation_validation_euclidean_diag.hpp`，只位于 `test-rvv` 专项目录：

- `transformPointXYZStd`：保留标量 staging 公式；
- `transformPointXYZRVV`：`__RVV10__` 下用公共 `pcl::rvv_load::strided_load3_f32m2` / `pcl::rvv_store::strided_store3_f32m2` 对 `PointXYZ` AoS 做 VL chunk 变换；
- `transformPointXYZCandidate`：RVV 构建且 `n >= 64` 时走 RVV，否则走 Std；
- `validateTransformationStd` / `validateTransformationCandidate`：full diagnostic，前置 staging 分别走 Std / Candidate，后续 KdTree search、`max_range` 判断和 score 累加保持同一标量流程。

诊断 helper 没有使用 `_rm` intrinsic，也不修改 FRM/FCSR。

## test_support_split_decision

当前决策：`deferred`（暂缓拆分）。

`transformation_validation_euclidean_diag.hpp` 当前为 227 行，已经混合四类职责：

- scalar reference（标量参考）：`transformPointXYZStd`、`validateTransformationStd*`；
- RVV math（RVV 数学链路）：`transformPointXYZRVV` 和 `transformPointXYZCandidate`；
- KdTree/search diagnostic（搜索树诊断）：`setupTargetTree`、`scoreTransformedWithTree`、fresh-tree 和 prebuilt-tree full validation；
- component ablation support（组件消融支撑）：供 bench 拆分 transform、tree setup、search-only negative control 和 full validation。

按 `rvv-test` 的长 test support 文件拆分规则，若后续继续扩展，建议把内部实现拆成相邻 `test_support/` 子目录，例如 `transform_staging.hpp`、`kdtree_validation.hpp` 和 `ablation_support.hpp`，当前 `transformation_validation_euclidean_diag.hpp` 保持稳定 aggregator header（聚合头文件）。本轮暂不拆分，原因是本次 closeout 修正只允许修改审查性文档 / work log，不允许再改 test-rvv 代码；而且当前 227 行仍可由文件头和函数级注释审查。若未来执行拆分，即使逻辑保持不变，也应至少重跑 `run_test_compare`、`run_bench_compare` 和 `dump_bench_rvv`；若移动代码可能影响内联或符号归属，还需要补板卡 summary-only evidence，避免 reviewer 无法区分纯机械拆分和行为变化。

## 风险和处理

| 风险                                      | 处理                                                                                                             |
| ----------------------------------------- | ---------------------------------------------------------------------------------------------------------------- |
| search 稀释 transform staging 收益        | bench 同时提供 transform microbench 和 full validation case；生产判断只看 full diagnostic / production-like case |
| `PointSource` 与 `PointTarget` 可不同 | 诊断限定 `PointXYZ -> PointXYZ`；生产接入前必须补泛型布局和类型转换边界                                        |
| 小规模点云启动成本                        | `n < 64` 回退标量                                                                                              |
| `max_range_` 语义                       | full diagnostic 保持原逻辑：`nearestKSearch` 返回 squared distance，超过 `max_range` 的点跳过                |
| KdTree 重建成本                           | 诊断函数每次 full validation 与原入口一致地构建并设置 tree；bench 用同一 target 和同一变换比较 std/RVV 构建      |

## 测试计划

专项测试：`test-rvv/registration/transformation_validation_euclidean/test_transformation_validation_euclidean.cpp`

- 大规模 transform candidate 与 scalar 对拍；
- 小规模 fallback 对拍；
- full validation score 对拍；
- prebuilt KdTree / tree reuse score 对拍；
- search-only tail 与 full validation 尾段对拍；
- 真实公开类 `setSearchMethodTarget(..., force_no_recompute=true)` 与默认 fresh tree 对拍；
- `max_range` 全拒绝时返回 `double::max()`。

上游测试：当前上游 `test/registration/test_registration.cpp` 只是间接包含 `transformation_validation_euclidean.h`，没有直接覆盖本类的独立 gtest。因为本轮不修改上游生产源码，暂不强制新增上游测试目标；若后续升级生产分流，应补定向上游或专项公开类测试。

## Bench 计划

专项 bench：`test-rvv/registration/transformation_validation_euclidean/bench_transformation_validation_euclidean.cpp`

输出保持可解析：

- `Dataset: synthetic PointXYZ clouds; transform staging microbench and full KdTree validation diagnostic`
- `Iterations: 20`
- 每个 case 一行 `<name> : <avg> ms/iter`，详情行包含 `Total Time` 和 checksum。

| case                                    | 入口                                | 规模 / 参数                                                   | 路径含义                                           |
| --------------------------------------- | ----------------------------------- | ------------------------------------------------------------- | -------------------------------------------------- |
| `tve transform-staging pointxyz 64K`  | `transformPointXYZCandidate`      | 64K `PointXYZ`，固定 4x4 transform                          | 局部片段诊断，不直接代表生产收益                   |
| `tve transform-staging pointxyz 256K` | `transformPointXYZCandidate`      | 256K `PointXYZ`，固定 4x4 transform                         | 局部片段规模放大诊断                               |
| `tve kdtree-setup pointxyz 64K` | `setupTargetTree` | 64K target | 只测 target tree setup，用于解释默认 fresh-tree 入口成本 |
| `tve kdtree-setup pointxyz 256K` | `setupTargetTree` | 256K target | 放大规模后 tree setup 组件消融 |
| `tve search-only negative-control pointxyz 64K` | `scoreTransformedWithTree` | 64K transformed source，target tree 预建 | 只测 `nearestKSearch`、`max_range` 和 score tail |
| `tve search-only negative-control pointxyz 256K` | `scoreTransformedWithTree` | 256K transformed source，target tree 预建 | 搜索主成本负向对照 |
| `tve full-validation fresh-tree pointxyz 64K` | `validateTransformationCandidateDetailed` | 64K source，target 为同一 transform 后点云，`max_range=1.0` | 接近 production 默认形态，包含 tree setup / search |
| `tve full-validation fresh-tree pointxyz 256K` | `validateTransformationCandidateDetailed` | 256K source / target，同上 | 判断默认 full 形态是否有生产价值 |
| `tve full-validation prebuilt-tree pointxyz 64K` | `validateTransformationCandidateWithTree` | 64K source，target tree 预建 | 对应 tree reuse / `force_no_recompute` 生产形态诊断 |
| `tve full-validation prebuilt-tree pointxyz 256K` | `validateTransformationCandidateWithTree` | 256K source，target tree 预建 | 判断 tree reuse 后 full 是否仍被 search 稀释 |

## 当前状态

- 函数级评估：已完成。
- RVV 实现：仅在 `test-rvv` 中建立 bench-diagnosis helper；上游生产入口未修改。
- 专项 test / bench / Makefile / board.mk：已建立并在本轮 rerun 中扩展到 tree reuse、`force_no_recompute`、search-only negative control 和 component ablation。
- QEMU：专项 test、bench compare 通过。
- 反汇编：已确认 `vlsseg3e32.v`、`vfmacc.vf`、`vssseg3e32.v`、`vsetvli e32,m2` 路径仍归属 transform staging；`scoreTransformedWithTree` 作为标量 search tail 单独存在。
- 板卡：本轮 rerun 已完成 smoke；transform-staging microbench 约 `2.73x`~`3.92x`，但 fresh-tree 和 prebuilt-tree full validation 都只有约 `1.01x`，因此仍不接生产分流。

## 验证结果

- QEMU 专项测试：`make -C test-rvv/registration/transformation_validation_euclidean run_test_compare` 通过。本轮 rerun 中 std/RVV 两套构建均通过 7 个专项测试；历史首轮为 4 个专项测试。
- QEMU bench：`make -C test-rvv/registration/transformation_validation_euclidean run_bench_compare` 通过，`output/qemu/analyze_bench_compare.log` 无 `未解析`、`n/a`、`Total Time 不计算`。QEMU 只作为构建、checksum、日志格式和指令路径证据。
- 反汇编：`make -C test-rvv/registration/transformation_validation_euclidean dump_bench_rvv` 生成 `build/asm/riscv/bench_transformation_validation_euclidean_rvv.full.asm`；`output/qemu/rvv_asm_check.log` 确认 `vlsseg3e32.v`、`vfmacc.vf`、`vssseg3e32.v`、`vsetvli ... e32,m2`。
- 板卡验证：`make -C test-rvv/registration/transformation_validation_euclidean board_smoke` 通过，日志在 `output/board/`。

Milkv-Jupiter 历史基线结果（首轮 4-case 表，仅作 rerun 前对照）：

| case                                    | Std ms/iter | RVV ms/iter | speedup | 结论                                                      |
| --------------------------------------- | ----------: | ----------: | ------: | --------------------------------------------------------- |
| `tve transform-staging pointxyz 64K`  |      3.2105 |      0.8258 |   3.89x | 只测 4x4 transform staging，局部片段收益成立              |
| `tve transform-staging pointxyz 256K` |     12.1858 |      4.2220 |   2.89x | 局部片段在更大规模仍有收益                                |
| `tve full-validation pointxyz 64K`    |    269.2100 |    267.4663 |   1.01x | full 入口包含 KdTree setup/search，局部收益被 search 稀释 |
| `tve full-validation pointxyz 256K`   |   1255.8260 |   1219.2291 |   1.03x | full 入口仍是弱收益，不足以支撑生产接入                   |

## 本轮 rerun：tree reuse 与组件消融

本轮在不修改 production 源码的前提下，把 test-only 诊断拆成四个可计时边界：

| case | 层级 | 计时边界 | 证明点 | 不能证明什么 |
| --- | --- | --- | --- | --- |
| `tve transform-staging pointxyz ...` | local fragment（局部片段） | 只测 `PointXYZ` 4x4 transform staging | 证明前置 affine transform 可被 RVV 加速 | 不代表完整 `validateTransformation` |
| `tve kdtree-setup pointxyz ...` | component ablation（组件消融） | 只测 target `KdTree::setInputCloud` | 估算默认入口每次重建 target tree 的成本 | 不证明 search 或 RVV staging |
| `tve search-only negative-control pointxyz ...` | negative control（负向对照） | transformed cloud 和 target tree 都预建，计时内只做 `nearestKSearch`、`max_range` 和 score tail | 证明 std/RVV 构建在搜索主成本上应接近 `1x` | 不证明 production dispatch 或 transform staging 收益 |
| `tve full-validation fresh-tree pointxyz ...` | production-shaped diagnostic（生产形态诊断） | transform staging + target tree setup + search + score | 对应 production 默认 `force_no_recompute_=false` 形态 | 仍不是 production direct，因为 RVV 只在 test-only helper 中 |
| `tve full-validation prebuilt-tree pointxyz ...` | production-shaped diagnostic | 预建 target tree，计时内 transform staging + search + score | 对应 `setSearchMethodTarget(..., force_no_recompute=true)` / tree reuse 场景 | 不证明真实 production RVV dispatch |

新增专项测试：

| 测试 | 层级 | 作用 |
| --- | --- | --- |
| `PrebuiltTreeScoreMatchesFreshTree` | production-shaped diagnostic correctness | 预建 KdTree 后，std 与 RVV candidate 的 score 和 accepted point 数一致。 |
| `SearchOnlyScoreMatchesFullValidationTail` | component correctness | transformed cloud 直接进入 search tail 时，分数等于 fresh full validation 的尾段结果。 |
| `ProductionForceNoRecomputeMatchesFreshTree` | public-entry-shaped correctness | 真实 `TransformationValidationEuclidean` 使用预建 tree 和 `force_no_recompute=true` 时，分数等于默认 fresh tree 形态。它只证明公开入口语义，不证明 RVV production dispatch。 |

QEMU 结果：

- `make -C test-rvv/registration/transformation_validation_euclidean run_test_compare`：std/RVV 各 7 个专项测试通过。
- `make -C test-rvv/registration/transformation_validation_euclidean run_bench_compare`：10 个 bench case 均可解析，checksum 对齐或符合预期；QEMU timing 不作为性能结论。
- `make -C test-rvv/registration/transformation_validation_euclidean dump_bench_rvv`：当前 asm 中可见 `vsetvli e32,m2`、`vlsseg3e32.v`、`vfmacc.vf` 和 `vssseg3e32.v`；`scoreTransformedWithTree` 是独立标量 search tail。

Milkv-Jupiter rerun 结果：

| case | Std ms/iter | RVV ms/iter | speedup | 结论 |
| --- | ---: | ---: | ---: | --- |
| `tve transform-staging pointxyz 64K` | 3.2473 | 0.8280 | 3.92x | 局部 4x4 staging 仍有明确收益。 |
| `tve transform-staging pointxyz 256K` | 12.0023 | 4.3998 | 2.73x | 放大规模后局部 staging 仍有收益。 |
| `tve kdtree-setup pointxyz 64K` | 86.3713 | 86.3952 | 1.00x | target tree setup 不受 RVV staging 影响。 |
| `tve kdtree-setup pointxyz 256K` | 454.2784 | 467.1076 | 0.97x | setup 成本很大且非 RVV 覆盖段。 |
| `tve search-only negative-control pointxyz 64K` | 176.3510 | 174.8610 | 1.01x | 搜索尾段基本同速，说明 full 小收益有运行波动 / 非 staging 成分。 |
| `tve search-only negative-control pointxyz 256K` | 776.1762 | 769.9378 | 1.01x | 搜索主成本支配 prebuilt-tree 形态。 |
| `tve full-validation fresh-tree pointxyz 64K` | 262.6956 | 259.4601 | 1.01x | 默认 production-shaped full 仍只有弱收益。 |
| `tve full-validation fresh-tree pointxyz 256K` | 1267.0592 | 1255.5100 | 1.01x | 放大规模后仍没有稳定明显收益。 |
| `tve full-validation prebuilt-tree pointxyz 64K` | 179.0340 | 176.9088 | 1.01x | tree reuse 后 full 仍被 search tail 主导。 |
| `tve full-validation prebuilt-tree pointxyz 256K` | 785.7998 | 776.7294 | 1.01x | `force_no_recompute` 形态仍不足以进入 production-candidate。 |

归因：

- 64K fresh-tree 中，标量 transform staging 为 `3.2473 ms`，fresh full 为 `262.6956 ms`，staging 只占约 `1.24%`。即使只看 prebuilt-tree full，staging 也只占 `1.81%`。
- 256K fresh-tree 中，标量 transform staging 为 `12.0023 ms`，fresh full 为 `1267.0592 ms`，staging 只占约 `0.95%`。prebuilt-tree 中 staging 占约 `1.53%`。
- tree setup 本身占 fresh full 的约 `32.9%`（64K）和 `35.9%`（256K），但 RVV transform staging 不覆盖这部分。
- search-only negative control 与 full validation 的 speedup 同为约 `1.01x`，说明 full validation 里的微弱差异不能可靠归因到 RVV transform；它也可能来自 KdTree/search 运行波动或坐标 FMA 微差导致的搜索路径细微变化。

EvidenceDecision：`bench-only/no-production`。本轮补齐 tree reuse、`force_no_recompute`、full validation production-shaped diagnostic、search-dominated negative control 和 component ablation 后，仍没有发现稳定明显的 full validation 收益。若后续要重新评估，必须先有能减少 / 批量化 `nearestKSearch` 或复用 target tree 后仍显著提高 staging 占比的生产形态证据。

transform-staging microbench 的 std/RVV checksum 不完全相同，原因是 RVV helper 使用 FMA 组合，标量表达式按编译器生成的浮点求值顺序执行，逐点结果存在约 `1e-7` 量级差异。专项测试使用 `1e-6f` 容差验证坐标一致；full validation 的 score 对拍通过。该差异不涉及 FRM/FCSR，helper 未使用 `_rm` intrinsic，也未修改浮点舍入环境。

## full validation 收益被稀释的原因

直接原因是 full validation 的主成本不是 4x4 transform staging，而是 staging 之后的 KdTree 流程，尤其是逐点：

```cpp
tree.nearestKSearch(point, 1, nn_indices, nn_dists);
```

以及每次 validation 前的 target tree 设置 / 构建成本。RVV helper 只替换 source 点的前置坐标变换，不改变 tree 构建、tree traversal、`max_range` 判断和 score 累加。

本轮 rerun 的板卡时间可以解释这个判断：

- 64K：标量 transform staging 为 `3.2473 ms`，fresh-tree full validation 为 `262.6956 ms`，staging 只占 fresh full 的约 `1.24%`；prebuilt-tree full validation 为 `179.0340 ms`，staging 也只占约 `1.81%`。
- 256K：标量 transform staging 为 `12.0023 ms`，fresh-tree full validation 为 `1267.0592 ms`，staging 只占 fresh full 的约 `0.95%`；prebuilt-tree full validation 为 `785.7998 ms`，staging 也只占约 `1.53%`。
- search-only negative control 为 `1.01x` / `1.01x`，与 fresh-tree 和 prebuilt-tree full validation 的 speedup 接近。因此 full validation 的微弱差异不能可靠归因到 RVV transform staging，也不能作为生产收益依据。

结论是：`nearestKSearch` 及其相关 KdTree 成本确实是 full validation 只剩弱收益的主要原因。这个主题只能证明 transform staging 局部可 RVV 化，不能证明公开入口主成本可被 RVV 覆盖。

### 代码链路复核

该调用点在源码中是逐点 search 入口：

```cpp
// registration/include/pcl/registration/impl/transformation_validation_euclidean.hpp
for (const auto& point : input_transformed) {
  tree_->nearestKSearch(point, 1, nn_indices, nn_dists);
  if (nn_dists[0] > max_range_)
    continue;
  fitness_score += nn_dists[0];
  ++nr;
}
```

`tree_` 的公开 `nearestKSearch` 在 `search/include/pcl/search/impl/kdtree.hpp` 中只是单点转发：

```cpp
return (tree_->nearestKSearch (point, k, k_indices, k_sqr_distances));
```

真正实现位于 `kdtree/include/pcl/kdtree/impl/kdtree_flann.hpp`。该实现先把单个 query 点展开为 `std::vector<float> query(dim_)`，随后交给 FLANN 的 `knn_search`：

```cpp
point_representation_->vectorize (static_cast<PointT> (point), query);
knn_search(*flann_index_,
           ::flann::Matrix<float>(query.data(), 1, dim_),
           k_indices,
           k_distances_mat,
           k,
           param_k_);
```

这条链路说明，`transformation_validation_euclidean` 里真正的热点不是 query 的 `vectorize`，而是 `knn_search` 背后的树遍历、候选维护和回溯。前者只有 3 个坐标的展开，后者是强分支、强数据依赖、不规整控制流；当前 RVV 只能作用于前置 transform staging，不能直接覆盖 `knn_search` 主体。

## 结论

`TransformationValidationEuclidean` 的 4x4 transform staging 可以 RVV 化并保持函数级语义，本轮板卡 rerun 的片段收益为 `2.73x` 到 `3.92x`。但 fresh-tree 和 prebuilt-tree full validation 都只有约 `1.01x`，且主要成本来自 KdTree setup / `nearestKSearch`，当前 RVV 方案没有覆盖入口主成本。

因此本主题继续收敛为 bench-diagnosis，不接入上游生产分流。后续若重新评估，需要先证明 search 之外的 staging 在真实调用场景中占比显著提高，或提出能减少 / 批量化 KdTree 查询成本的方案；否则单独优化 transform staging 不值得增加生产模板分流和泛型点类型维护边界。
