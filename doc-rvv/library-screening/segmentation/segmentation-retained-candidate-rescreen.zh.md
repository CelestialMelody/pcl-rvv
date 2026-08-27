# segmentation 模块 RVV 保留候选复筛

本文档对 `doc-rvv/library-screening/segmentation/segmentation-function-evaluation-queue.zh.md`
中第 7.2 节的 13 个“保留实施的候选文件”执行 retained-candidate-rescreen。复筛目的不是重跑第一轮文件筛选，也不是重跑第二轮函数评估队列，而是用已完成的
`approximate_progressive_morphological_filter` 与 `extract_polygonal_prism_data` 主题的真实测试、反汇编、板卡、Evidence Doctor 和采纳 / 回退结果，重新排序剩余保留候选。

本轮不修改 production 源码，不建立新的 `test-rvv` topic，不跑板卡 bench，不提交。

## 1. 输入依据与复筛原因

### 1.1 输入文档

- 筛选 skill：`.agents/skills/rvv-screening/SKILL.md`
- 筛选标准：`.agents/skills/rvv-screening/references/screening-criteria.md`
- 阶段与队列规则：`.agents/skills/rvv-screening/references/stage-and-queue-policy.md`
- 证据边界：`.agents/skills/rvv-screening/references/evidence-boundaries.md`
- 保留候选复筛模板：`.agents/skills/rvv-screening/references/templates/retained-candidate-rescreen-template.md`
- 函数评估队列：`doc-rvv/library-screening/segmentation/segmentation-function-evaluation-queue.zh.md`
- 已完成主题文档：`doc-rvv/segmentation/approximate_progressive_morphological_filter-RVV.zh.md`
- 已完成主题文档：`doc-rvv/segmentation/extract_polygonal_prism_data-RVV.zh.md`
- 已完成主题统计：`tmp/rvv-topic-stats/2/segmentation-approximate-progressive-morphological-filter-stats.zh.md`
- 已完成主题统计：`tmp/rvv-topic-stats/2/segmentation-extract-polygonal-prism-data-stats.zh.md`

### 1.2 复筛输入边界

本轮默认只复筛第二轮函数评估队列第 7.2 节的 13 个保留候选文件：

| 顺序 | 候选文件 | 第二轮主题 |
| ---: | --- | --- |
| 1 | `impl/grabcut_segmentation.hpp` | GrabCut staging / n-link |
| 2 | `src/grabcut_segmentation.cpp` | GrabCut GMM / max-flow boundary |
| 3 | `impl/organized_connected_component_segmentation.hpp` | organized connected component |
| 4 | `impl/organized_multi_plane_segmentation.hpp` | organized multi-plane |
| 5 | `impl/progressive_morphological_filter.hpp` | progressive PMF tail |
| 6 | `impl/cpc_segmentation.hpp` | CPC weighted RANSAC score |
| 7 | `impl/crf_segmentation.hpp` | CRF staging / unary |
| 8 | `impl/lccp_segmentation.hpp` | LCCP convexity |
| 9 | `impl/min_cut_segmentation.hpp` | min-cut potentials |
| 10 | `impl/random_walker.hpp` | random walker row operations |
| 11 | `impl/segment_differences.hpp` | segment differences tail |
| 12 | `impl/supervoxel_clustering.hpp` | supervoxel local math |
| 13 | `impl/unary_classifier.hpp` | unary classifier staging |

本轮没有补入队列外文件。当前源码阅读没有发现函数评估队列结论因文件删除、入口迁移或新增可复用模式而失效；已完成主题暴露出的强模式也都能在上述 13 个保留文件内完成映射，不需要扩展成全模块重筛。

### 1.3 复筛原因

第二轮建议队列中的两个主题均已关闭并采纳有界 production RVV：APMF 采纳 grid z-min / height threshold production 路径，polygonal prism 采纳 single / nested polygon dense / indexed production 路径以及常见 `<=32` byte PointXYZ-like 点型。与此同时，两个主题也给出了明确负向边界：APMF window-open 不接入，APMF 050 tail 剩余比较 / 输出压缩探针 neutral 并回退，polygonal prism `PointXYZINormal` wide-stride 回退标量。这些结果足以改变原 7.2 保留队列的排序口径。

## 2. 筛选统计

| 项目 | 数量 | 说明 |
| --- | ---: | --- |
| 保留实施候选输入文件 | 13 | 完整覆盖函数评估队列第 7.2 节。 |
| 复筛主题数 | 12 | `impl/grabcut_segmentation.hpp` 与 `src/grabcut_segmentation.cpp` 合并为同一个 GrabCut 函数级评估主题。 |
| 建议启动函数级评估的主题 | 3 | min-cut potentials、GrabCut staging / n-link / GMM、organized multi-plane prepass / projection。 |
| 建议启动函数级评估覆盖文件 | 4 | 含 GrabCut 两个伴随文件。 |
| 暂缓 / 不单独实施的主题 | 9 | tail-only、search/solver-only、graph/state-heavy、tool/profile prerequisite 类保留项降级。 |
| 暂缓 / 不单独实施覆盖文件 | 9 | 每个文件独立交代暂缓条件。 |
| 新增补充候选 | 0 | 没有当前源码变化、profile 或已完成主题证据指向队列外文件。 |
| 合并主题 | 1 | GrabCut 头文件和源文件属于同一 public entry / graph-cut 主题，不拆成两个独立 topic。 |
| 删除或源码冲突 | 0 | 未发现候选文件缺失或源码结论冲突。 |

`建议启动函数级评估` 的默认评估路径分布：

| 默认评估路径 | 主题数 | 主题 |
| --- | ---: | --- |
| `component ablation` | 2 | min-cut potentials；organized multi-plane prepass / projection |
| `production-shaped diagnostic` | 1 | GrabCut staging / n-link / GMM |
| `production-value evaluation` | 0 | 当前没有候选可直接按已完成两主题那样进入 production direct 判断。 |
| `profile prerequisite` | 0 | profile prerequisite 项本轮全部暂缓，不作为下一批自动 topic。 |
| `no-production confirmation` | 0 | random walker 等 no-production diagnostic 价值不足以优先启动。 |

## 3. 已完成主题经验总结

### 3.1 已完成主题结果

| 主题 | 主文件 | 函数评估队列原始定位 | 实际覆盖范围 | 目标硬件结论 | 正确性证据 | 反汇编证据 | 生产接入状态 | 回退 / 暂缓原因 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| approximate PMF grid/open | `impl/approximate_progressive_morphological_filter.hpp` | 建议队列第 1 条；`extract` 中 grid z-min、window open、height threshold tail | 已采纳 grid z-min 与 threshold tail 的 production hybrid path；window-open 保持标量；tail 最终 `Zf` lookup / compare / push 保持标量 | Milkv-Jupiter production public 5-run：`PointXYZ` dense 1.60x、non-dense 1.35x；点型扩展 `PointXYZ` 1.48x / 1.28x，`PointXYZI` 1.52x / 1.42x，`PointXYZRGB` 1.50x / 1.43x，`PointXYZRGBA` 1.51x / 1.43x | `make -C test-rvv/segmentation/approximate_progressive_morphological_filter run_test_compare`：Std/RVV 各 14/14 passed；覆盖 dense、non-dense、小规模 fallback 和点型扩展 | `check_production_rvv_asm` passed；production bench RVV binary 可定位 `apmfExtractRVV` 和关键 RVV 指令 | adopted production behavior；topic closed；topic commit `ea2ba420a`，evidence commit `7932089a` | window-open component evidence 中性到负向；050 tail-vector-filter-compress 相对 040 RVV baseline 整体 neutral，已回退；OpenMP 和更多点型属于新 scope |
| polygonal prism predicate | `impl/extract_polygonal_prism_data.hpp` | 建议队列第 2 条；`segment` 中 height mask、point-to-plane、polygon predicate、保序输出 | 已采纳投影后 polygon scan production path：height mask、signed distance、single / nested polygon parity / XOR、dense / indexed row source、保序 `output.indices` 压缩 | Milkv-Jupiter production public 5-run：single dense / indexed 均 1.75x；nested dense 2.18x、nested indexed 2.13x；`PointXYZI` 1.85x、`PointXYZRGB` 1.83x、`PointXYZRGBA` 1.84x；size 32 confirm5 1.19x | `make -C test-rvv/segmentation/extract_polygonal_prism_data run_test_compare`：Std 2/2、RVV 16/16 passed；`run_board_test`：16/16 passed | `dump_bench_rvv` 后可定位 `segmentRvv`，命中 `vlse32.v`、`vluxseg3ei32.v`、`vfmacc.vf`、`vmxor.mm`、`vcompress.vm` | adopted production behavior；topic closed；topic commit `6baf3b978`，evidence commit `eed4f441c` | `projectPoints` 保持标量；`PointXYZINormal` wide-stride 接入前 20-run 不稳定，post-gate fallback 1.00x；`Scalar=double`、自定义点型、低于 32 点不外推 |

### 3.2 可复用模式与失败边界

| 模式标签 | 来自哪些已完成主题 | 成立条件 | 失败 / 回退边界 | 对后续候选的影响 |
| --- | --- | --- | --- | --- |
| public-entry bulk scan adopted | APMF；polygonal prism | RVV 覆盖公开入口内随 `input_->size()` / `indices_->size()` 增长的大批量扫描；correctness、asm、board 和 Evidence Doctor 能闭环 | 只覆盖局部 helper、初始化边角或外部 solver 前后很薄的片段时，不直接推为 production | 后续优先考虑能把 component evidence 包进 public-shaped bench 的候选；graph/search/solver 主导项必须先证明占比 |
| xyz AoS float layout gate | APMF；polygonal prism | 只读 `x/y/z` 或常见小点型字段；`RVVXYZAoSFloatLayout<PointT>`、32-bit byte offset 和规模 gate 可守住 fallback | `PointXYZINormal` 等 wide-stride 点型在板卡上不稳定；不能把常见点型收益外推到所有 `PCL_XYZ_POINT_TYPES` | min-cut 的 `PointXYZ` potential、organized multi-plane 的 point/normal dot 可以借鉴；supervoxel / unary classifier 的宽结构和复杂 feature 不自动升优先级 |
| indexed gather can win if arithmetic is enough | polygonal prism；APMF tail gather/staging | indexed row source 后有 signed distance、polygon parity、row/col 计算或 min/update staging 等足够工作量，并且输出语义可保序 | gather 之后只有一次阈值 / copy / append 时收益容易被吞掉 | min-cut unary foreground min-distance 可作为 component ablation；segment differences 的 one-NN 后阈值 / copy 降级 |
| mask + vcompress works for main selection | polygonal prism | 输出是按扫描顺序保留 indices，mask 生成本身属于主成本，且有 dense / indexed correctness 对拍 | APMF 050 证明“剩余 compare / compress”相对已优化基线整体 neutral；tail-only 不能单独升优先级 | progressive PMF tail、segment differences tail 暂缓；只有完整主扫描被 RVV 覆盖时再考虑压缩接入 |
| reduction / staging should keep conflict state scalar | APMF grid z-min | RVV 负责 row/col/z staging 或局部 min，冲突敏感 cell update 保留标量，正确性易对拍 | 随机写、union-find merge、graph edge mutation 或 label 改写不可直接套用 | min-cut unary min reduction 值得做 isolating evidence；organized CCL label merge、Boost graph edge 更新不作为首个 RVV 目标 |
| organized dense loops are not automatically positive | APMF window-open | organized grid / row-col 循环即使规则，也可能因窗口访存、边界、寄存器压力或已有标量局部性而中性 | 仅凭二维循环或 stencil 形态不能升优先级 | organized connected component 和 organized multi-plane 只能挑 `plane_d` / projection 等简单片段先消融，主 CCL/refine 不直接承诺 |
| solver/search/state dominates should demote | APMF 050；polygonal prism wide-stride fallback | 若主成本是 KNN/search、Boost graph、Eigen sparse solver、RANSAC random、queue/BFS、map/set/list mutation 或外部 DenseCRF，局部 RVV 必须先证明占比 | 不能只因内部有 dot、exp、histogram 或 copy loop 就启动 production 优化 | CPC、LCCP、supervoxel、CRF、random walker、unary classifier 大多暂缓或仅保留重新考虑条件 |

## 4. 筛选口径修正 / 复筛变化理由

1. 原 7.2 顺序中的下一项是 GrabCut，但已完成主题表明“无现成 direct test + graph/max-flow 主导 + `std::exp` / GMM 数值边界”的组合不适合直接作为下一条最高优先级。GrabCut 仍建议启动函数级评估，但排在 min-cut 之后，并且首阶段必须是 production-shaped diagnostic。
2. min-cut 从原保留队列提升为下一条建议启动主题。原因不是“graph 算法数学多”，而是 `test/segmentation/test_segmentation.cpp` 已有 `MinCutSegmentationTest`，`buildGraph` 中 unary potential 是明确的 `indices_ x foreground_points_` 距离 min reduction，binary potential 是 KNN 后的三维距离和 `exp`，可以在一个 topic 内先回答 solver/search 稀释比例。
3. organized multi-plane 保留为建议启动但不是下一条。`plane_d[i] = input dot normal` 与 boundary projection loop 匹配已验证的 bulk xyz / dot 模式；但上游 direct test 缺失，且后续 CCL、region covariance、`eigen33`、refine label growth 会稀释收益，所以只建议 component ablation。
4. progressive PMF tail 与 segment differences tail 明确降级。APMF 050 已证明对已优化后剩余 compare / compress 的进一步 RVV 化为 neutral；segment differences 还叠加 `nearestKSearch` 主导，不应单独启动。
5. random walker、CRF、unary classifier 虽有局部矩阵 / unary / histogram 片段，但主成本分别落在 Eigen sparse solver、DenseCRF 外部推理、FPFH / KMeans / FLANN，且缺少能直接支持 production public speedup 的入口证据，本轮暂缓。
6. CPC、LCCP、supervoxel 的局部 dot / distance / convexity 公式保留 profile 价值，但图结构、octree / KdTree、set/list/map mutation 和 RANSAC random state 与已完成主题的成功模式距离较远，本轮不单独实施。

## 5. 保留实施候选逐项复筛

### 5.1 建议启动函数级评估

| 主题 | 主文件 | 关键入口 | 主成本覆盖类型 | 默认评估路径 / 首阶段证据问题 | 匹配的已验证模式 | 主要风险 | 推荐理由 | 证据来源 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| min-cut potentials | `impl/min_cut_segmentation.hpp` | `MinCutSegmentation<PointT>::extract`、`buildGraph`、`calculateUnaryPotential`、`calculateBinaryPotential` | `diagnostic` | `component ablation`：先隔离 unary foreground min-distance reduction 与 KNN 后 binary potential，量化它们相对 `nearestKSearch`、Boost graph mutation 和 `boykov_kolmogorov_max_flow` 的占比；首阶段只回答 potential loop 是否值得继续，不承诺接入 solver | APMF 的 reduction/staging 模式；polygonal prism 的 indexed row source + 算术密度模式；已完成主题对 graph/search 稀释的降级规则 | `buildGraph` 仍包含 KNN、Boost graph add_edge、edge_marker set 和 max-flow；foreground 点数小于阈值时 unary reduction 不足；`std::exp` 与 double / float 数值边界需对拍 | 在保留项中同时具备现成上游 test、公开入口、可隔离 loop、可构造 board component timing。相比 GrabCut，不需要先补完整算法测试；相比 organized multi-plane，测试入口更清楚；相比 random walker，核心 solver 外的 potential 片段更容易独立量化 | `segmentation/include/pcl/segmentation/impl/min_cut_segmentation.hpp` 中 `buildGraph`、`calculateUnaryPotential`、`calculateBinaryPotential`、`assembleLabels`；`test/segmentation/test_segmentation.cpp` 中 `MinCutSegmentationTest`；APMF / polygonal prism 主题文档和 stats |
| GrabCut staging / n-link / GMM | `impl/grabcut_segmentation.hpp` | `GrabCut<PointT>::initCompute`、`fitGMMs`、`refineOnce`、`initGraph`、`computeBetaOrganized`、`computeNLinksOrganized`、`computeBetaNonOrganized`、`computeNLinksNonOrganized`；伴随 `src/grabcut_segmentation.cpp` 的 `buildGMMs`、`learnGMMs`、`GMM::probabilityDensity` | `partial-preprocess` | `production-shaped diagnostic`：先构造 organized RGB/RGBA synthetic cloud、trimap 和 public `extract` / `refineOnce` 形态，分离 color staging、organized beta/n-link、GMM probability 与 `graph_.solve()` 占比；首阶段问题是 n-link/GMM 是否接近公开入口总成本 | polygonal prism 的 color-capable small point-type gate；APMF 的 staging + scalar conflict update；已完成主题对 solver/state 主导路径的证据优先规则 | `BoykovKolmogorov::solve` 是 active set、parent/orphan、map/deque 状态机；non-organized path 被 KNN search 稀释；`std::exp` / `std::log` / GMM covariance 数值边界复杂；当前未发现 direct unit test | 仍值得启动，因为 organized path 中 `computeBetaOrganized` / `computeNLinksOrganized` 是 `width * height` 规则扫描，`Image` / `Color` staging 与 n-link 权重是可测 component；但它不能再凭原队列顺位排在第一 | `segmentation/include/pcl/segmentation/impl/grabcut_segmentation.hpp`；`segmentation/src/grabcut_segmentation.cpp`；函数评估队列第 5 / 7.2 节；已完成主题失败边界 |
| organized multi-plane prepass / projection | `impl/organized_multi_plane_segmentation.hpp` | `OrganizedMultiPlaneSegmentation::segment` 中 `plane_d[i]` dot prepass；`segment` / `segmentAndRefine` 中 boundary gather；`projectToPlaneFromViewpoint` | `partial-preprocess` | `component ablation`：先只评估 `plane_d` point-normal dot、boundary cloud gather 和 optional projection loop 的 component 上界；首阶段问题是这些片段能否覆盖 CCL、per-region covariance / `eigen33` 和 refine 之外的可见成本 | APMF / polygonal prism 的 xyz AoS bulk scan；polygonal prism 的 dot / FMA / mask 证据；wide-stride fallback 对 normal-heavy 点型的边界提醒 | direct test 缺失；`OrganizedConnectedComponentSegmentation::segment`、`computeMeanAndCovarianceMatrix`、`eigen33` 和 refine label growth 稀释；point + normal 双输入可能扩大 layout gate；projection 只覆盖 boundary 后处理 | 建议启动但排在 min-cut、GrabCut 后。它是保留队列里最接近已验证 dense scan 模式的 organized 候选，但证据路径应先小而硬，不进入完整 plane segmentation production 改造 | `segmentation/include/pcl/segmentation/impl/organized_multi_plane_segmentation.hpp` 中 `plane_d`、boundary gather、`projectToPlaneFromViewpoint`；`organized_connected_component_segmentation.hpp` 调用关系；已完成主题文档和 stats |

### 5.2 暂缓 / 不单独实施

| 主题 | 主文件 | 主成本覆盖类型 | 暂缓原因 | 重新考虑条件 | 证据来源 |
| --- | --- | --- | --- | --- | --- |
| organized connected component | `impl/organized_connected_component_segmentation.hpp` | `diagnostic` | 主 label pass 依赖 left/up label、`findRoot` union、虚调用 `compare_->compare` 和 label merge 顺序；可批量部分主要是 finite mask、final label remap 和 comparator staging，尚不能覆盖主状态机 | organized multi-plane 或真实 organized segmentation profile 证明 comparator predicate / label remap 占比高；或者先完成 comparator stub correctness 后再作为 companion phase 重开 | `OrganizedConnectedComponentSegmentation::segment` 中 first row、row pass、union-find remap、`label_indices` fill；函数评估队列第 5 节；已完成主题对 organized dense loop 和状态机边界的模式总结 |
| progressive PMF tail | `impl/progressive_morphological_filter.hpp` | `tail-compress` | 本文件可 RVV 化部分主要是 `applyMorphologicalOperator` 后的 height threshold 和保序压缩；APMF 050 已证明剩余 tail compare / compress 相对优化基线 neutral，普通 PMF 还会被 filters 模块 open 和 `copyPointCloud` 稀释 | 有 component timing 证明普通 PMF tail 占公开入口大头，或 filters 模块 morphological operator 另有 RVV 证据可共同覆盖主成本 | `ProgressiveMorphologicalFilter<PointT>::extract`；`tools/progressive_morphological_filter.cpp`；APMF 正式文档与 stats 的 050 rejected probe |
| CPC weighted RANSAC score | `impl/cpc_segmentation.hpp` | `diagnostic` | `applyCuttingPlane` 以 Boost graph edge mutation、递归切割、RANSAC random sampling 和 sample_consensus model 为主；weighted score dot accumulation 是局部片段，缺少 direct test 和 profile | CPC example-shaped profile 明确指向 `WeightedRandomSampleConsensus::computeModel` 的 inlier score loop，且能固定随机种子建立 scalar/RVV 对拍 | `CPCSegmentation::applyCuttingPlane`、`WeightedRandomSampleConsensus::computeModel`；`examples/segmentation/example_cpc_segmentation.cpp`；solver/search/state 降级模式 |
| CRF staging / unary | `impl/crf_segmentation.hpp` | `partial-preprocess` | `createUnaryPotentials` 有 `N * n_labels` fill，但 `segmentPoints` 的核心仍是 voxel grid、外部 DenseCRF 推理、debug output 和 old/new CRF 对齐；当前缺少 direct unit test | 真实工具场景 profile 显示 unary / data vector staging 接近总成本，且先补 small labeled voxel correctness；否则不进入 production 改造 | `createDataVectorFromVoxelGrid`、`createUnaryPotentials`、`segmentPoints`；`tools/crf_segmentation.cpp`；已完成主题对外部 solver 稀释的边界 |
| LCCP convexity | `impl/lccp_segmentation.hpp` | `diagnostic` | `connIsConvex` 有 centroid/normal dot、cross、angle、`exp` 等局部公式，但整体由 Boost graph、supervoxel adjacency、set/map mutation、segment merge 和 recursive growing 主导 | supervoxel/LCCP example profile 证明 `connIsConvex` 或 edge scan 是热点，并能把 supervoxel 输入固定成可复核测试资产 | `prepareSegmentation`、`doGrouping`、`mergeSmallSegments`、`applyKconvexity`、`connIsConvex`；`examples/segmentation/example_lccp_segmentation.cpp` |
| random walker row operations | `impl/random_walker.hpp` | `diagnostic` | 有 `test_random_walker.cpp`，但核心成本是 Eigen `SimplicialCholesky` sparse solve 与 Boost graph / bimap traversal；`assignColors` row max 和 `getPotentials` copy 更像 no-production diagnostic，不足以优先启动 | profile 或 test matrix 显示 `assignColors` / potentials extraction 在大 graph 中占比异常高；或用户明确需要用 no-production diagnostic 关闭该候选 | `RandomWalker::buildLinearSystem`、`solveLinearSystem`、`assignColors`、`getPotentials`；`test/segmentation/test_random_walker.cpp` |
| segment differences tail | `impl/segment_differences.hpp` | `tail-compress` | 每个 source 点先执行 `tree->nearestKSearch`，RVV 只覆盖 finite mask、distance threshold 和 `copyPointCloud` 前的 indices 收集；这与 APMF 050 tail neutral 和 search 主导降级模式一致 | profile 证明 target search 被替换或很轻，后处理阈值 / copy 成为瓶颈；或者 search 模块已有 RVV / batch-search 证据后联合复筛 | `getPointCloudDifference`、`SegmentDifferences::segment`；`test/segmentation/test_segmentation.cpp` 中 `SegmentDifferences`；APMF 050 rejected probe |
| supervoxel local math | `impl/supervoxel_clustering.hpp` | `diagnostic` | `voxelDataDistance`、centroid reduction 和 normal/RGB/XYZ accumulation 有局部数学价值，但 octree leaves、KdTree search、set/list mutation、owner stealing 和 adjacency graph 是主风险；数据布局不连续 | example-shaped profile 指向 `voxelDataDistance` 或 centroid update；先固定 synthetic voxel cloud 和 supervoxel helper state，再做 component ablation | `SupervoxelClustering::extract`、`selectInitialSupervoxelSeeds`、`computeVoxelData`、`SupervoxelHelper::expand`、`refineNormals`、`updateCentroid`、`voxelDataDistance`；`examples/segmentation/example_supervoxels.cpp` |
| unary classifier staging | `impl/unary_classifier.hpp` | `partial-preprocess` | 33-bin histogram staging、FLANN query input copy 和 `assignLabels` 存在批量片段，但主成本是 FPFH、KMeans、FLANN `knnSearch` 和 per-query allocation；工具入口存在但无 unit test | `tools/unary_classifier_segment.cpp` / `tools/train_unary_classifier.cpp` profile 显示 staging 或 `assignLabels` 占比高，并先建立 scalar/RVV correctness oracle | `convertCloud`、`findClusters`、`kmeansClustering`、`queryFeatureDistances`、`assignLabels`、`train`、`segment`；unary classifier tools；已完成主题对 wide feature / external search 的边界 |

## 6. 新的执行清单 / 状态表

### 6.1 建议启动清单

下表按候选文件展开；GrabCut 仍属于同一函数级评估主题，但把模板入口层和非模板后端层拆成两行，避免把“3 个主题，覆盖 4 个文件”读成只有 3 个文件。

| 顺序 | 主题 | 主文件 | 推荐入口 / 第一 RVV 目标 | 依据模式 / 证据来源 | 状态 | 当前结论 / 下一步条件 |
| ---: | --- | --- | --- | --- | --- | --- |
| 1 | min-cut potentials | `impl/min_cut_segmentation.hpp` | `calculateUnaryPotential` foreground min-distance reduction；`calculateBinaryPotential` KNN 后 distance / `exp` component | 现成 `MinCutSegmentationTest`；APMF reduction/staging；polygonal prism indexed gather + arithmetic；graph/search 降级边界 | completed no-production | 已完成 topic-local component ablation 和 production-shaped timing。component 为 positive，但 buildGraph-shaped repeated 为 neutral 且 Evidence Doctor 报退化频率 Error；当前 potential batch 不建议接 production。 |
| 2 | GrabCut staging / n-link / GMM | `impl/grabcut_segmentation.hpp` | `GrabCut<PointT>::initCompute`、`initGraph`、`computeBetaOrganized`、`computeNLinksOrganized`、`computeBetaNonOrganized`、`computeNLinksNonOrganized` | 模板入口层；organized dense scan；Color staging；但缺 direct test 且受后端 max-flow 稀释 | 建议启动函数级评估 | 与下一行同属 GrabCut 主题。先补 public-shaped diagnostic，确认 n-link / color staging 占比。 |
| 3 | GrabCut GMM / max-flow boundary | `src/grabcut_segmentation.cpp` | `buildGMMs`、`learnGMMs`、`GMM::probabilityDensity`；`BoykovKolmogorov::solve` 只作为边界记录 | 非模板后端层；GMM probability 有局部公式；max-flow 是 state-heavy 主风险 | 建议启动函数级评估 | 与上一行同属 GrabCut 主题。不先动 `BoykovKolmogorov::solve`；只有 GMM / n-link 占比清楚才进入 production-value evaluation。 |
| 4 | organized multi-plane prepass / projection | `impl/organized_multi_plane_segmentation.hpp` | `plane_d` point-normal dot prepass、boundary gather、`projectToPlaneFromViewpoint` | xyz bulk scan / dot 模式；organized path；但 direct test 缺失，CCL/eigen/refine 稀释 | 建议启动函数级评估 | 先做 component 上界，确认 prepass / projection 是否可见；主 CCL 和 refine label growth 不作为首阶段目标。 |

### 6.2 暂缓 / 不单独实施清单

| 顺序 | 主题 | 主文件 | 推荐入口 / 第一 RVV 目标 | 依据模式 / 证据来源 | 状态 | 当前结论 / 下一步条件 |
| ---: | --- | --- | --- | --- | --- | --- |
| 5 | organized connected component | `impl/organized_connected_component_segmentation.hpp` | comparator staging / label remap only if companion phase | union-find / label state；organized loops 不自动 positive | 暂缓 / 不单独实施 | 随 organized multi-plane 或 comparator profile 重开。 |
| 6 | progressive PMF tail | `impl/progressive_morphological_filter.hpp` | `applyMorphologicalOperator` 后 threshold compress | APMF 050 tail compare / compress neutral；tail-only 降级 | 暂缓 / 不单独实施 | 需证明普通 PMF tail 占比，或与 filters morphological operator 联合评估。 |
| 7 | CPC weighted RANSAC score | `impl/cpc_segmentation.hpp` | RANSAC weighted score dot accumulation | graph mutation / random sampling / sample_consensus 主导 | 暂缓 / 不单独实施 | 需 CPC example profile 指向 score loop。 |
| 8 | CRF staging / unary | `impl/crf_segmentation.hpp` | `createUnaryPotentials` / data vector staging | DenseCRF 外部 solver 和工具场景缺 test | 暂缓 / 不单独实施 | 需真实工具 profile 与 small labeled voxel oracle。 |
| 9 | LCCP convexity | `impl/lccp_segmentation.hpp` | `connIsConvex` / edge scan | Boost graph / supervoxel adjacency / merge state 主导 | 暂缓 / 不单独实施 | 需 LCCP profile 证明 convexity loop 热。 |
| 10 | random walker row operations | `impl/random_walker.hpp` | `assignColors` row max / `getPotentials` copy | Eigen sparse solve 主导；no-production diagnostic 优先级低 | 暂缓 / 不单独实施 | 需 profile 证明 row ops 占比异常高。 |
| 11 | segment differences tail | `impl/segment_differences.hpp` | one-NN 后 threshold / output copy | `nearestKSearch` 主导；tail-only 被 APMF 050 降级 | 暂缓 / 不单独实施 | search 被替换或后处理成热点时再复筛。 |
| 12 | supervoxel local math | `impl/supervoxel_clustering.hpp` | `voxelDataDistance` / centroid reduction | octree / KdTree / set-list mutation / adjacency 主导 | 暂缓 / 不单独实施 | 需 supervoxel example-shaped profile。 |
| 13 | unary classifier staging | `impl/unary_classifier.hpp` | 33-bin histogram staging / `assignLabels` | FPFH / KMeans / FLANN 主导；无 unit test | 暂缓 / 不单独实施 | 需 unary tools profile 和 correctness oracle。 |

### 6.3 当前推荐下一主题

`min-cut potentials` 曾是本复筛文档推荐的下一主题，原因是它有现成上游测试入口，且 `calculateUnaryPotential` / `calculateBinaryPotential` 可以先做 component ablation，不需要一开始改 max-flow。

当前该主题已完成 no-production closeout。topic-local 证据显示 unary / binary component 分别为 positive，但 buildGraph-shaped repeated benchmark 只有 median `1.02x`，且 5 run 中 2 run 低于 1.0；Evidence Doctor 报 `ba_degradation_frequency` Error。因此 min-cut potential batch 不再是待启动主题，也不建议接入 production。

下一条建议启动主题顺延为 `GrabCut staging / n-link / GMM`。它仍需要新 topic 单独建立 phase plan，并先用 production-shaped diagnostic 量化 n-link / color staging / GMM 与 max-flow state machine 的比例。organized multi-plane 仍为第三优先；其它 9 个主题当前均不建议单独启动。

## 7. Closeout

- 输出文档：`doc-rvv/library-screening/segmentation/segmentation-retained-candidate-rescreen.zh.md`
- 复筛输入：函数评估队列第 7.2 节 13 个保留实施候选文件。
- 补入候选：0。
- 合并主题：1，GrabCut 头文件与源文件合并为一个函数级评估主题。
- 复筛结果：建议启动函数级评估 3 个主题，覆盖 4 个文件；暂缓 / 不单独实施 9 个主题，覆盖 9 个文件。
- `min-cut potentials` 已完成 no-production closeout：主文件 `impl/min_cut_segmentation.hpp`，topic-local 证据位于 `test-rvv/segmentation/min_cut_segmentation/`。下一条建议启动主题顺延为 `GrabCut staging / n-link / GMM`，但需要新 topic 另行建立 phase plan。
- 本轮没有发现需要补充到 `rvv-screening`、`rvv-test`、`rvv-workflow`、implementation 或 documentation skill 的通用规则。
