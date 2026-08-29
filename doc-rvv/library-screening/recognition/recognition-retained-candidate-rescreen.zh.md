# recognition 模块 RVV 第三轮保留候选复筛

本文复筛第二轮函数评估队列中的 `保留实施的候选文件`。复筛轮本身不重做第一轮，不新增全模块候选，不修改 production 源码，不创建 `test-rvv` topic，不运行板卡 bench，不提交 commit。后续单 topic 执行状态在第 6 节持续更新，并以对应 topic 的 production diff、`test-rvv`、`doc-rvv` 和板卡证据为准。

## 1. 输入依据与复筛原因

- 第一轮文件候选筛选文档：`doc-rvv/library-screening/modules/recognition-file-candidate-screening.zh.md`
- 第二轮函数评估队列：`doc-rvv/library-screening/recognition/recognition-function-evaluation-queue.zh.md`
- 已完成主题文档：`doc-rvv/recognition/linemod_template_scoring-RVV.zh.md`、`doc-rvv/recognition/color_gradient_modality-RVV.zh.md`、`doc-rvv/recognition/surface_normal_modality-RVV.zh.md`、`doc-rvv/recognition/dotmod_template_matching-RVV.zh.md`、`doc-rvv/recognition/color_gradient_dot_modality-RVV.zh.md`、`doc-rvv/recognition/occlusion_reasoning-RVV.zh.md`、`doc-rvv/recognition/quantizable_modality-RVV.zh.md`、`doc-rvv/recognition/color_modality-RVV.zh.md`

复筛原因：第二轮建议队列中的 7 个 recognition 主题均已进入 `已采纳 / topic closeout` 状态，下一步不应回到第一轮全量筛选，而应使用已完成主题的真实性能、回退边界和可复用模式，复筛第二轮保留队列中的 9 个文件。

## 2. 筛选统计

| 项目 | 数量 | 说明 |
| --- | ---: | --- |
| 保留实施候选输入总数 | 9 | 仅来自第二轮函数评估队列第 5 节。 |
| 建议启动函数级评估 | 5 | 可以进入单 topic 的 S1-S2 评估，不表示可直接修改 production。 |
| 暂缓 / 不单独实施 | 4 | 当前不建议作为独立 topic 启动。 |
| 新增补入候选 | 0 | 未发现需要从 `low` 或其它文件补入的明确漏判。 |
| 重新纳入 / 合并 / 删除 / 源码冲突 | 0 | 9 个原保留候选均逐项交代去向。 |

建议启动函数级评估的默认路径分布：`production-value evaluation` 2 个，`production-shaped diagnostic` 1 个，`component ablation` 1 个，`profile prerequisite` 1 个。

## 3. 已完成主题经验总结

| 主题 | 主文件 | 函数评估队列原始定位 | 实际覆盖范围 | 目标硬件结论 | 正确性证据 | 反汇编证据 | 生产接入状态 | 回退 / 暂缓原因 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| LINEMOD template scoring | `recognition/src/linemod.cpp` | high，建议队列顺序 1 | `matchTemplates`、`detectTemplates`、`detectTemplatesSemiScaleInvariant` 中 EnergyMaps 到 LinearizedMaps copy loop | Phase 070 public entry median `1.138x` / `1.129x`；Phase 080 semi-scale median `1.136x` | production public correctness 通过，checksum 一致 | RVV helper 指令归属明确 | adopted | NMS、averaged detection、score accumulation 和 threshold scan 未覆盖，需另起 phase。 |
| LINEMOD color gradient modality | `recognition/include/pcl/recognition/color_gradient_modality.h` | high，建议队列顺序 2 | Sobel + quantize + filter，并接入 `processInputData()` | Phase 050 production direct median `1.620x` / `1.590x` | Std/RVV 对拍通过 | RVV helper 路径命中 | adopted | `extractFeatures()` 未证明仍是热点。 |
| LINEMOD surface normal modality | `recognition/include/pcl/recognition/surface_normal_modality.h` | high，建议队列顺序 3 | depth-to-normal / quantize、5x5 filter、surface-normal 专用 spread | Phase 030 production direct median `1.620x` / `1.640x` | Std/RVV 对拍通过 | RVV helper 路径命中 | adopted | 公共 `QuantizedMap::spreadQuantizedMap()` 需另起跨 modality topic。 |
| DOTMOD template matching | `recognition/src/dotmod.cpp` | high，建议队列顺序 4 | `detectTemplates()` 直接窗口读取并计数 | production-public median `3.298x`，0/5 退化 | production direct 对拍通过，checksum 一致 | `dotmodScoreWindowDirectRVV` 及 RVV 指令可见 | adopted | standalone `QuantizedMap::getSubMap()`、threshold 输出和真实 RGB-D 分布未覆盖。 |
| DOTMOD color gradient modality | `recognition/include/pcl/recognition/color_gradient_dot_modality.h` | high，建议队列顺序 5 | `processInputData()` 中 `computeMaxColorGradients` RVV，dominant scan 保持标量 | production direct median `2.600x` / `3.510x` | Std/RVV 对拍通过，checksum 一致 | RVV helper 指令归属明确 | adopted | `computeInvariantQuantizedMap()` 因状态恢复风险暂缓。 |
| Occlusion inline filter | `recognition/include/pcl/recognition/hv/occlusion_reasoning.h` | mid，建议队列顺序 6 | public inline `filter()` / `getOccludedCloud()` projection + keep indices | Phase 020 median `1.250x`，0/5 退化 | 公开入口对拍通过 | RVV helper 路径命中 | adopted | 仅覆盖 `PointXYZ` 风格 AoS float。 |
| ZBuffering occlusion filter | `recognition/include/pcl/recognition/impl/hv/occlusion_reasoning.hpp` | mid，建议队列顺序 7 | `ZBuffering::filter(model, indices)` projection + depth compare | Phase 010 median `1.270x`，0/5 退化 | Std/RVV 对拍通过 | RVV helper 路径命中 | adopted | `filter(model, filtered)` 的 `copyPointCloud` wrapper 未单独计时。 |

| 模式标签 | 来自哪些已完成主题 | 成立条件 | 失败 / 回退边界 | 对后续候选的影响 |
| --- | --- | --- | --- | --- |
| organized pixel preprocessing | color gradient modality、surface normal modality、ColorGradientDOTModality | 主循环为 `width * height`，布局稳定，能直接挂到 `processInputData()` | 后续 list / sort / feature selection 可能稀释尾段收益 | `color_modality.h` 可从保留候选升为函数级评估。 |
| direct public entry | LINEMOD scoring、DOTMOD matching、occlusion reasoning | RVV 路径覆盖真实公开入口或主 wrapper | 只剩支撑拷贝、序列化、尾段压缩时不应单独实施 | 有 public entry 的候选优先于 companion header。 |
| projection + mask + compress | occlusion reasoning | 投影、bounds mask、压缩暂存和标量尾段边界清楚 | depth gather、复杂状态机和输出顺序仍需保守 | 规则几何谓词可做 component ablation。 |
| byte-map / window helper | DOTMOD matching、surface normal default spread | 连续 byte map、固定窗口、按位 OR / compare 可表达 | 纯 submap copy 或容器视图构造收益不稳 | `spreadQuantizedMap` 值得评估；`getSubMap` 不宜单独实施。 |
| scatter / search / training heavy | Hough、GC、ISM、face detection 类候选 | 只有局部子核可向量化，必须先隔离证据 | accumulator scatter、RANSAC、search tree、Eigen / KMeans、random training 稀释收益 | 这类候选默认走 diagnostic、component ablation 或 profile prerequisite。 |

## 4. 筛选口径修正 / 复筛变化理由

第二轮的 9 个保留候选并不是同一类风险。本轮把它们拆开处理：

1. 与已完成 modality 主题同构、且有真实 `processInputData()` 或共享 map helper 的，提升为函数级评估候选。
2. 有大 trip count 几何谓词或 vote-generation 子核，但主路径含 scatter / RANSAC / search 的，允许先做 diagnostic 或 component ablation。
3. 训练态、纯容器拷贝、实现配套文件、或被 IO / histogram scatter / random path 主导的，暂缓独立实施。

## 5. 保留实施候选逐项复筛

### 建议启动函数级评估

| 主题 | 文件 | 关键入口 | 函数 / loop | trip count | RVV 适配点 | 主成本覆盖类型 | 默认评估路径 / 首阶段证据问题 | 主要风险 | 推荐理由 | 证据来源 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Color modality follow-on | `recognition/include/pcl/recognition/color_modality.h` | `ColorModality<PointInT>::processInputData()` | `quantizeColors`、`filterQuantizedColors`、`computeDistanceMap`、`extractFeatures` | organized RGB `width * height`、8 个 color bin、feature 候选数 | RGB extrema 分类、3x3 histogram、byte map 初始化、shared spread | `partial-preprocess` | `production-value evaluation`；先确认 quantize / filter / spread 是否覆盖入口主成本 | `computeDistanceMap` 是前后双 pass 顺序依赖；`extractFeatures` 有 list / sort 和距离约束 | 与已完成的 color / surface modality 同类，public entry 清楚，适合作为下一批 production-value 评估 | 第二轮第 5 节；源码中 `processInputData()` 串接 quantize/filter/spread；已完成 modality 主题证据 |
| Geometric consistency ablation | `recognition/include/pcl/recognition/impl/cg/geometric_consistency.hpp` | `GeometricConsistencyGrouping::recognize()` -> `clusterCorrespondences()` | sorted correspondences 上的 pairwise distance consistency 与 consensus 增长 | `model_scene_corrs_->size()`，近似 `O(n^2)`，受 consensus 长度影响 | model / scene point gather、距离差、threshold mask、consensus count | `diagnostic` | `component ablation`；先隔离几何谓词是否在入口中接近主成本 | `std::sort`、`taken_corresps`、consensus 顺序、RANSAC rejector、输出 transformation 顺序 | 算术密度高且边界清楚，适合以子核证据决定是否继续 | 第二轮保留候选；源码 `clusterCorrespondences()` 中嵌套 correspondence loop；occlusion 的 mask/compress 模式可借鉴但不能直接外推 |
| Hough vote-generation diagnostic | `recognition/include/pcl/recognition/impl/cg/hough_3d.hpp` | `Hough3DGrouping::recognize()` -> `houghVoting()` | model vote train、scene vote 生成、`d_min/d_max` reduction、distance weight | model 点数、correspondence 数 `n_matches`、reference frame 数 | 3x3 RF 线性组合、scene vote 坐标、min/max reduction、weight 计算 | `diagnostic` | `production-shaped diagnostic`；先把 vote generation 与 accumulator scatter 分离 | `vote()` / `voteInt()` 是 accumulator scatter，插值写多个 bin，maxima 输出顺序敏感 | 前半段 vote generation 同构且入口明确，值得先做生产形态诊断 | 第二轮保留候选；源码 `houghVoting()` 先计算 `scene_votes` 后写 HoughSpace；DOTMOD direct-window 经验提示应避开中间拷贝/状态 |
| ISM profile-gated evaluation | `recognition/include/pcl/recognition/impl/implicit_shape_model.hpp` | `ImplicitShapeModelEstimation::trainISM()`、`findObjects()` | descriptor distance、`calculateSigmas`、`calculateWeights`、`ISMVoteList::shiftMean()` / `getDensityAtPoint()` | keypoints、`FeatureSize`、cluster 数、training objects / points / votes、radius search 结果 | descriptor batch distance、pairwise point distance、Gaussian 权重公式、weighted sum reduction | `diagnostic` | `profile prerequisite`；先要求真实 workload/profile 指向 descriptor 或 sigma/weight 子核 | feature estimator、KMeans、Eigen dynamic matrix、random centers、`radiusSearch` 和 vote list 状态稀释收益 | 子核很清楚，但主成本不清；进入函数级评估的第一件事应是证明 profile 条件 | 第二轮保留候选；源码 `calculateSigmas()`、`calculateWeights()`、`shiftMean()`；已完成主题显示局部收益不能替代入口收益 |
| Quantized map spread helper | `recognition/src/quantizable_modality.cpp` | `QuantizedMap::spreadQuantizedMap()` | 横向 OR window 后纵向 OR window | map `width * height`、`spreading_size` | byte unit-stride load、bitwise OR reduction、两阶段临时 map | `non-standalone` | `production-value evaluation`；先确认该 helper 是否是多个 modality 链路中的共享热点 | 边界写入、window size、临时 map 初始化、caller 耦合 | 已完成 surface-normal topic 已证明 default spread 有正向价值，公共 helper 值得单独评估但仍需 caller 覆盖证据 | 第二轮保留候选；源码 `spreadQuantizedMap()`；surface normal 和 color modality 已完成/待评估主题 |

### 暂缓 / 不单独实施

| 主题 | 文件 | 关键入口 | 函数 / loop | trip count | RVV 适配点 | 主成本覆盖类型 | 主要风险 / 暂缓原因 | 推荐理由 / 重新考虑条件 | 证据来源 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Face detection RF utils | `recognition/include/pcl/recognition/face_detection/rf_face_utils.h` | random forest training / evaluation helper | `evaluateFeature` batch overload、`computeMeanAndCovarianceOffset`、`computeMeanAndCovarianceAngles`、`computeInformationGain` | training examples、positive examples、branch 数、feature 数 | integral image feature evaluation、translation / rotation covariance 9 项规约、branch 分类 | `diagnostic` | training path 价值不清；随机 fallback、determinant / log / Eigen 小矩阵和 branch state 主导 | 当前不建议单独启动；只有真实 face detector training/evaluation profile 指向这些规约时再考虑 | 第二轮保留候选；源码 batch evaluation 和 covariance loop |
| LineRGBD companion | `recognition/include/pcl/recognition/impl/linemod/line_rgbd.hpp` | `LineRGBD::loadTemplates()`、`detect()` / `detectSemiScaleInvariant()` 后处理、`refineDetectionsAlongDepth()` | template centroid、ROI depth min/max、depth histogram、integral bins、template point translation | template cloud 点数、detection 数、ROI `width * height`、固定 `nr_bins = 1000` | finite mask、min/max/sum reduction、point translation、histogram 前置筛选 | `partial-preprocess` | IO/TAR/PCD、LINEMOD 调度、detection clustering、histogram scatter、RANSAC depth ICP 主导 | 作为 LINEMOD 伴随文件保留；若真实 profile 证明 depth refinement 是公开入口热点，再单独评估 | 第二轮保留候选；源码 `refineDetectionsAlongDepth()` 和 `computeTransformedTemplatePoints()` |
| QuantizedMap submap | `recognition/include/pcl/recognition/quantized_map.h` | `QuantizedMap::getSubMap()` | submap 二维拷贝 loop；serialize / deserialize 元素 loop | submap `width * height`、map 元素数 | byte unit-stride / row-stride load-store，或由 caller 直接窗口读取 | `non-standalone` | 文件是容器；单独 RVV 化 copy 类 helper 价值低；DOTMOD 已用 direct window 避免该拷贝 | 不建议单独实施；只有新 caller profile 证明 repeated submap 构造是真热点时再考虑，且优先评估 caller 访问 | 第二轮保留候选；DOTMOD template matching 已完成主题明确未覆盖 standalone `getSubMap()` |
| Hough accumulator support | `recognition/src/cg/hough_3d.cpp` | `HoughSpace3D::vote()`、`voteInt()`、`findMaxima()` | bin index 计算、interpolated vote 27 邻域写入、maxima scan | vote 数、Hough bin 总数 `total_bins_count_`、邻域 bin 数 | maxima threshold scan、neighbor compare、vote 坐标量化前置 | `diagnostic` | accumulator scatter 写 `hough_space_` 与 `voter_ids_`，maxima 输出顺序敏感 | 不单独启动；若 `impl/cg/hough_3d.hpp` 的 vote-generation diagnostic 正向，再决定是否只评估 maxima scan | 第二轮保留候选；源码 `voteInt()` 和 `findMaxima()` |

## 6. 新的执行清单 / 状态表

| 顺序 | 主题 | 主文件 | 推荐入口 / 第一 RVV 目标 | 依据模式 / 证据来源 | 状态 | 当前结论 / 下一步条件 |
| ---: | --- | --- | --- | --- | --- | --- |
| 1 | Color modality follow-on | `recognition/include/pcl/recognition/color_modality.h` | `processInputData()` 中 `quantizeColors` / `filterQuantizedColors` / shared spread | organized pixel preprocessing；已完成 modality 主题 | 已采纳 / topic closeout | 已接入 `ColorModality<PointXYZRGB>::processInputData()` 的 RGB extrema quantize + 3x3 dominant filter production RVV；Phase 040 board summary median `2.490x` / `2.410x`，`0/5` 退化，checksum 一致。正式文档为 `doc-rvv/recognition/color_modality-RVV.zh.md`；本地统计记录为 `tmp/rvv-topic-stats/3/recognition-color-modality-stats.zh.md`。 |
| 2 | Quantized map spread helper | `recognition/src/quantizable_modality.cpp` | `QuantizedMap::spreadQuantizedMap()` 横向 / 纵向 OR window | byte-map / window helper；surface-normal default spread 已正向 | 已采纳 / topic closeout | 已接入公共 helper production path：`__RVV10__` 构建下默认 `spreading_size == 8` 且尺寸足够时走 `spreadQuantizedMapRVV()`，其它情况回退 `spreadQuantizedMapStd()`。5-run board summary 中 `shared_spread_320x240` median `4.670x`、`shared_spread_641x481_tail` median `4.770x`，均 `0/5` 退化，checksum 一致，Evidence Doctor `Errors=0 / Warnings=0 / Suggestions=4`。正式文档为 `doc-rvv/recognition/quantizable_modality-RVV.zh.md`；caller 端到端收益需另开 public-entry topic。 |
| 3 | Geometric consistency ablation | `recognition/include/pcl/recognition/impl/cg/geometric_consistency.hpp` | `clusterCorrespondences()` pairwise consistency predicate | projection/mask 类规则几何经验；源码 O(n^2) 子核 | 待启动 | 先做 component ablation，不直接承诺 production。 |
| 4 | Hough vote-generation diagnostic | `recognition/include/pcl/recognition/impl/cg/hough_3d.hpp` | `houghVoting()` vote generation + min/max reduction | scatter / diagnostic 边界；源码中 vote generation 与 accumulator 可拆 | 已完成 / topic closeout | 已完成 `vote generation`、`production direct`、`no-interpolation` 和 `default distance weight` 四个 phase 的收口；当前 production patch 保留为 attempted、未采纳，`doc-rvv/recognition/hough_3d-RVV.zh.md` 未创建；本地统计记录为 `tmp/rvv-topic-stats/3/recognition-hough_3d-stats.zh.md`。 |
| 5 | ISM profile-gated evaluation | `recognition/include/pcl/recognition/impl/implicit_shape_model.hpp` | `findObjects()` descriptor nearest cluster assignment；`calculateSigmas` / density 子核保留后续候选 | profile prerequisite -> production direct narrow adoption | 已采纳 / topic closeout | 已接入 `findObjects()` 公开入口中的 descriptor-to-cluster 最近邻分配：`__RVV10__` 构建下走 RVV helper。Phase 020 production direct board summary 中 `public_find_objects_descriptor_assignment` median `1.060x`、min `1.040x`、max `1.070x`、`0/5` 退化，checksum `semantic:public_votes=494:votes_match=True:peak_density_match=True:peak_fingerprint_match=True`，Evidence Doctor `Errors=0 / Warnings=0 / Suggestions=2`。正式文档为 `doc-rvv/recognition/implicit_shape_model-RVV.zh.md`；`trainISM()`、sigma、density、其它 `FeatureSize` / 点型扩展需另建 phase。 |

暂缓文件不进入执行清单：`face_detection/rf_face_utils.h`、`impl/linemod/line_rgbd.hpp`、`quantized_map.h`、`src/cg/hough_3d.cpp`。它们仍可作为后续 topic 的背景或支撑文件，但不静默合并为其它文件的优化任务。

## 7. Closeout

- 筛选文档位置：`doc-rvv/library-screening/recognition/recognition-retained-candidate-rescreen.zh.md`
- 保留候选复筛结果：`建议启动函数级评估 5`，`暂缓 / 不单独实施 4`
- 第一条未完成主题：`Geometric consistency ablation`
- Hough vote-generation diagnostic 已完成 no-production closeout；当前 production patch 保留，未创建长期 `doc-rvv` 文档，本地统计记录为 `tmp/rvv-topic-stats/3/recognition-hough_3d-stats.zh.md`。
- 后续执行更新：`Color modality follow-on` 已完成 production RVV 接入和 topic closeout；当前 production 源码、测试资产、长期文档和板卡证据分别位于 `recognition/include/pcl/recognition/color_modality.h`、`test-rvv/recognition/color_modality/`、`doc-rvv/recognition/color_modality-RVV.zh.md` 和 `test-rvv/recognition/color_modality/log/board/repeated_phase040_quantize_filter_rvv/summary.md`；本地统计记录为 `tmp/rvv-topic-stats/3/recognition-color-modality-stats.zh.md`。
- 后续执行更新：`Quantized map spread helper` 已完成 production-detail RVV 接入和 topic closeout；当前 production 源码、测试资产、长期文档和板卡证据分别位于 `recognition/src/quantizable_modality.cpp`、`test-rvv/recognition/quantizable_modality/`、`doc-rvv/recognition/quantizable_modality-RVV.zh.md` 和 `test-rvv/recognition/quantizable_modality/log/board/repeated_phase000_shared_spread_rvv/summary.md`；本地统计记录为 `tmp/rvv-topic-stats/3/recognition-quantizable-modality-stats.zh.md`。
- 后续执行更新：`ISM profile-gated evaluation` 已完成 `findObjects()` descriptor assignment 的窄范围 production 接入和 topic closeout；当前 production 源码、测试资产、长期文档和板卡证据分别位于 `recognition/include/pcl/recognition/impl/implicit_shape_model.hpp`、`test-rvv/recognition/implicit_shape_model/`、`doc-rvv/recognition/implicit_shape_model-RVV.zh.md` 和 `test-rvv/recognition/implicit_shape_model/log/board/repeated_phase020_public_entry_findobjects_production_direct/summary.md`。
- 复筛轮本身没有修改 production 源码，没有创建 `test-rvv` topic，没有运行板卡 bench，没有提交 commit；单 topic 执行状态以后续 topic 文档为准。
- 未发现需要补充到 `rvv-workflow`、`rvv-test`、`rvv-implementation` 或 `rvv-documentation` skill 的通用规则
