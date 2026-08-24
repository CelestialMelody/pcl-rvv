# registration 保留候选复筛

## 1. 输入依据与复筛原因

本文件是 registration 模块函数评估队列的保留候选复筛，只复筛
`doc-rvv/library-screening/registration/registration-function-evaluation-queue.zh.md`
中 `3.2 保留实施的候选文件` 的 22 个条目；不重新扩大到全 registration 模块，不补入
3.2 之外的新候选。

本轮遵守 `rvv-screening` 的保留候选复筛口径：筛选阶段只做源码阅读、静态分析、轻量入口确认和筛选文档；
不修改 production 源码，不建立新的 `test-rvv` topic，不运行板卡 bench，不提交。筛选队列只授权后续进入
函数级评估，不能替代后续 `rvv-workflow` / `rvv-test` 的 correctness、QEMU、反汇编、board 和
production integration 证据闭环。

复筛原因是 registration 已完成多个 RVV topic，真实板卡结果、回退原因和 production 接入边界已经改变
保留候选的排序价值。特别是：

- row-source / normal-equation / Umeyama 前置累加类主题已经有强 production direct 正向证据。
- search、tree、RANSAC、hash、外部 solver、LM / BFGS 控制流和薄尾段 helper 已多次显示会稀释局部 RVV 收益。
- 局部 diagnostic 或 weak-positive 不能替代完整 public entry 的 production direct 证据。
- `transformation_estimation_2D` 当前仍有未提交 topic 改动和 pending / in-progress 边界，本文件只把它作为临时观察，不纳入已完成主题证据包。

## 2. 筛选统计

| 统计项 | 数量 / 结论 |
| --- | ---: |
| 3.2 保留实施候选输入总数 | 22 |
| 建议启动函数级评估 | 1 |
| 已完成函数级评估 / no-production | 2 |
| 暂缓 / 不单独实施 | 19 |
| 重新纳入 3.2 之外候选 | 0 |
| 合并、删除或源码冲突项 | 0 |

当前建议启动和已完成路径分布：

| 默认评估路径 | 数量 | 说明 |
| --- | ---: | --- |
| `production-value evaluation` | 1 | `transformation_estimation_svd_scale`，直接复用 SVD-family 已验证的 row-source 累加模式。 |
| `completed no-production` | 2 | `gicp` 已回退生产补丁并提交 topic closeout；`ndt` 已完成函数级评估且不建议接入 production。 |

## 3. 已完成主题经验总结

### 3.1 已完成主题证据包

| 主题 | 主文件 | 函数评估队列原始定位 | 实际覆盖范围 | 目标硬件结论 | 正确性证据 | 反汇编证据 | 生产接入状态 | 回退 / 暂缓原因 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `transformation_validation_euclidean` | `transformation_validation_euclidean.hpp` | transform staging + KdTree validation | test-rvv diagnostic；production-shaped full validation | transform staging `2.73x`-`3.92x`，fresh / prebuilt full validation 约 `1.01x` | Std/RVV 7 tests，通过 full validation score 对拍 | `vlsseg3e32.v`、`vfmacc.vf`、`vssseg3e32.v` 归属 staging | no production | KdTree setup / `nearestKSearch` 主导，局部 staging 占 full 约 1%-2%。 |
| `correspondence_estimation_organized_projection` | `correspondence_estimation_organized_projection.hpp` | organized projection correspondence | production source transform、projection-pixel、target predicate；append 标量 | identity fake / explicit `1.64x` / `1.65x`，non-identity `2.36x` | QEMU Std 39 tests；RVV 39 tests；board smoke 39 tests | source / target gather、projection FMA、`vcompress`、`vcpop` 等 | production-ready | `Correspondence` append 和 stored distance 保持标量，避免可变输出和 bit pattern 风险。 |
| `transformation_estimation_point_to_plane_lls` | `transformation_estimation_point_to_plane_lls.hpp` | point-to-plane LLS | full-cloud f32 AoS layout-gated normal-equation block | 代表点型 64K / 256K 约 `2.80x`-`3.15x` | production direct tests 覆盖 public matrix、normal equation、invalid lane、fallback | strided load、finite mask、`vfmacc`、`vcpop`、`vfredosum` | production-candidate / production adopted in current tree | indexed、correspondences、weighted、double 不外推。 |
| `transformation_estimation_point_to_plane_lls_weighted` | `transformation_estimation_point_to_plane_lls_weighted.hpp` | weighted point-to-plane LLS | full-cloud block-reduction；source-indexed staged-gather default | full-cloud约 `2.76x`-`3.00x`；source-indexed staged 约 `2.22x`-`2.78x` | full/source-indexed correctness、input semantics、fallback tests | full/source-indexed asm 和 production default trace | production-adopted | public Std/RVV 正向不能证明某个 RVV family 胜出；block-fused probe 相对 staged mixed / negative。 |
| `transformation_estimation_symmetric_point_to_plane_lls` | `transformation_estimation_symmetric_point_to_plane_lls.hpp` | symmetric LLS | full-cloud 与 source-indexed generic normal production；dual / correspondence diagnostic | full-cloud `2.48x`-`2.71x`；source-indexed `1.87x`-`2.11x`；dual `0.80x/0.63x`，correspondence `0.77x/0.86x` | Std/RVV 25 tests；board smoke 25 tests | full stride、source gather、dual gather 指令归属 | production-ready for full/source-indexed | 单侧 gather 可成立，双侧 gather和 correspondence 当前负向，不能继承。 |
| `icp_transform_cloud` | `icp.hpp` | ICP `transformCloud` | production direct transformCloud full scan | `PointXYZ` 64K / 256K `5.68x` / `5.30x`；`PointNormal` `3.76x` / `3.93x` | QEMU 和 board 12 tests | production `transformCloud` 符号内 RVV 指令 | production direct positive | 只证明 isolated `transformCloud`，不声明 ICP end-to-end 加速。 |
| `correspondence_types` | `correspondence_types.hpp` | query / match index extraction、distance stats | diagnostic + 临时 production probe | diagnostic medians `0.959/0.960/0.987`；production probe `0.983/0.966/0.877`，均 negative | QEMU / board correctness 8 tests | bench 级 asm；production attribution 未抵消负向 | rollback / no-production | 语义清楚的 strided field extraction 仍太小 / memory-bound，分流成本高于收益。 |
| `correspondence_rejection_poly` | `correspondence_rejection_poly.hpp` | polygon edge predicate / acceptance | production-shaped diagnostic + production direct replay | edge gather staging historical weak-positive；production direct 2048 `0.904x`、8192 `0.977x`，两组 5/5 degradation | QEMU / board 8 tests | replay full asm 可见 Standard / RVV helper | rollback / no-production | random sampling、histogram / Otsu、输出过滤和完整入口控制流稀释局部 edge formula。 |
| `transformation_estimation_svd` | `transformation_estimation_svd.hpp` | Umeyama / SVD row-source | ordered、source-indexed、dual-indices、correspondence 四条 public overload | ordered `14.372x/24.471x/23.841x`；source-indexed `9.634x/12.217x/11.558x`；dual `6.805x/6.404x/5.964x`；correspondence `8.649x/8.644x/7.872x` | QEMU Std/RVV 22/22；board RVV 22/22 | 四条 public overload 均有 load/gather/FMA/reduction 归属 | production-ready / adopted | 只覆盖 `Scalar=float`、dense、layout-gated xyz AoS、`use_umeyama_==true` 和合法 row source。 |
| `transformation_estimation_dual_quaternion` | `transformation_estimation_dual_quaternion.hpp` | dual quaternion C1/C2 | ordered、source-indexed、dual-indexed production；correspondence scalar | ordered `3.232x/3.644x/3.656x`；source-indexed `2.540x/2.631x/2.586x`；dual `2.015x/1.808x/2.021x`；correspondence probe negative | QEMU Std 28/28，RVV 32/32 | retained bench binary 中 load/gather/widen/reduction 指令 | production-adopted for three row sources | correspondence 64K / 256K negative，已移除 production dispatch。 |
| `bfgs` | `bfgs.h` | optimizer direction update | test-only diagnostic | direction-update vector6 `0.648x`，vector128 `0.740x`，均 negative | QEMU Std/RVV 6 tests | bench binary filtered asm；无 production hotspot 归属 | diagnostic stop / no-production | GICP 6 维状态太小，line search / functor 回调 / 控制流主导。 |

### 3.2 可复用模式与失败边界

| 模式标签 | 来自哪些已完成主题 | 成立条件 | 失败 / 回退边界 | 对后续候选的影响 |
| --- | --- | --- | --- | --- |
| `row-source fused accumulation` | SVD、dual quaternion、LLS、weighted LLS、symmetric LLS | public overload 主成本是按点对 / row source 扫描，RVV 能替代动态矩阵填充、sum/cross-sum、C1/C2 或 normal-equation 构造 | `Scalar=double`、非 dense、未验证点型、非法 index、solver tail 单独小规模 | 直接提升 SVD-family 的 `transformation_estimation_svd_scale`。 |
| `organized projection direct pipeline` | CEOP | 避免 KdTree，projection / predicate / mask / compress 形成连续生产链，append 标量边界清晰 | 可变输出结构体 scatter、stored distance bit pattern、target append 顺序 | 只支持 organized projection 类候选；不能提升普通 KNN correspondence。 |
| `isolated transform full scan` | ICP `transformCloud` | 公开 helper 主成本就是大规模 4x4 transform / normal rotation，AoS offset gate 清晰 | end-to-end ICP 仍可能由 correspondence/search/estimator 主导 | 支持把 transformCloud 当独立已完成能力，不再用它升级 search-dominated 候选。 |
| `search dilution` | transformation validation euclidean、普通 correspondence、GICP / NDT 源码审计 | RVV 只覆盖 search 前后少量 staging 或 threshold tail | KdTree / voxel search / radius search / tree setup 占入口主成本 | 普通 KNN correspondence、GICP、NDT 默认不能写成 production 优化；需 profile 或 component ablation。 |
| `simple strided helper negative` | correspondence_types、BFGS | loop 语义清楚但每元素算术太少，或状态向量过小 | 分流、`vsetvli`、load/store、模板维护成本超过收益 | index extraction、固定小公式、单纯 field copy / tail-compress 默认暂缓。 |
| `local weak-positive cannot adopt production` | correspondence_rejection_poly、TVE | 局部 formula 或 staging 有板卡信号 | 完整 public entry 负向或接近 1.0，不能归因到局部 RVV | RANSAC / sampling / histogram / graph / solver 类候选必须先做完整入口证据。 |
| `row-source variants are independent` | SVD、symmetric LLS、dual quaternion、weighted LLS | ordered、source-indexed、dual-indexed、correspondence 的取数和展开成本各自闭合 | 单侧 gather 正向不能推出双侧 gather或 correspondence 正向 | 后续候选必须按 row source 拆证据，不用一个正向路径覆盖所有 overload。 |

## 4. in-progress 主题的临时观察边界

`transformation_estimation_2D` 当前只作为 in-progress / provisional observation。当前 worktree 中该 topic 仍有大量
未提交的 production、`test-rvv`、doc 和 log 变更；Phase 093 结果也把 dual-indexed narrow patch 写成
`pending user confirmation` / bounded production candidate。因此本文件不把它纳入第 3 节已完成主题证据包，
也不使用它直接升级或降级 3.2 的任何候选。

允许记录的临时观察仅用于后续 agent 规则和风险提示：

- direct gather、materialize-to-ordered 和 public scalar baseline 之间容易发生 comparison-boundary mismatch；
  family selection 需要同一 production boundary 内的 RVV-vs-RVV A/B。
- source-indexed、dual-indexed、correspondence 三类 row source 必须独立判断；单侧 gather 的证据不能覆盖双侧或 correspondence。
- generic point type gate 即使多数 case 正向，也可能存在单个 point-type / size negative bucket；未逐类型上板时必须保留代表性边界。
- in-progress 文档中出现 `adopted` 或 board 正向数字时，若 topic 仍 dirty、仍 pending confirmation 或未完成 closeout，
  只能写作风险 / 结构观察，不能作为二轮复筛排序证据。

## 5. 筛选口径修正 / 复筛变化理由

本轮对 3.2 保留候选作如下口径修正：

1. 对 SVD-family row-source 累加候选上调：普通 SVD 已证明 fused sum / cross-sum 可以替代动态矩阵填充并覆盖四类 public row source，
   `transformation_estimation_svd_scale` 是当前 retained 队列中最直接的可复用项。
2. 对 search-dominated 候选降级：只覆盖 KdTree / radiusSearch 前后处理、K 个候选内小公式或 threshold append 的文件，
   不再默认作为独立生产价值主题；只有真实 profile 或 component ablation 显示局部片段接近主成本时才恢复。
3. 对 optimizer / solver 主导候选降级：BFGS direction-update 在板卡负向，LM / numerical diff / Eigen solver 控制流仍未显示可被局部 residual RVV 覆盖。
4. 对 RANSAC、FPCS、sample consensus 和 correspondence rejector 降级：随机采样、feature KNN、model estimation、candidate sorting、histogram / output flow
   会稀释局部 residual 或 inlier scan。
5. 对简单 field extraction、小固定规模公式和 graph orchestration 降级：`correspondence_types` 和 `bfgs` 已显示语义清楚不等于硬件收益。
6. `diagnostic`、`production-shaped diagnostic`、`component ablation` 和 `profile prerequisite` 只作为后续 topic 内的首阶段证据路径；
   已完成 topic 单独归入 `已完成函数级评估 / no-production`，未启动项仍分为 `建议启动函数级评估` 与 `暂缓 / 不单独实施`。

## 6. 保留实施候选逐项复筛

### 6.1 建议启动函数级评估

| 主题 | 关键入口 | 主成本覆盖类型 | 默认评估路径 / 首阶段证据问题 | 匹配的已验证模式 | 主要风险 | 推荐理由 | 证据来源 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `transformation_estimation_svd_scale` | `TransformationEstimationSVDScale::getTransformationFromCorrelation`；后续可扩到 SVD-scale public overload | `partial-preprocess` -> 可验证为 production-value | `production-value evaluation`：能否把 scale 所需 `sum_ss/sum_tt` 与 SVD 已验证的 source / target sum、cross-sum 统一累加，避免 `R4 * cloud_src_demean` 和额外动态矩阵 pass，并保持 scale 数值语义 | SVD 四 row-source fused accumulation 强正向；row-source legal-index / dense / layout gate 可复用 | scale 分母为 0、`float scale` 与 `double sum` 语义、3x3 SVD tail、`use_umeyama_` / dense gate 差异 | 与普通 SVD 数据流同源，是 retained 队列中最明确的下一未完成主题；首 topic 可以快速回答是否继承 SVD production 价值 | `transformation_estimation_svd_scale.hpp`；`transformation_estimation_svd-RVV.zh.md` |

### 6.1A 已完成函数级评估 / no-production

| 主题 | 已尝试入口 | 目标硬件结论 | 当前状态 | 后续恢复条件 | 证据来源 |
| --- | --- | --- | --- | --- | --- |
| `gicp` | residual / Mahalanobis diagnostic、covariance post-KNN、`OptimizationFunctorWithIndices::operator()` cost-only production probe、`OptimizationFunctorWithIndices::dfddf()` / `dfddfLoopRVV()` production probe、Phase 004 gather-width 微调 | cost-only public median `1.019x/1.011x` 为 neutral；clean `dfddfLoopRVV()` public median `1.080x/1.058x` 为 weak-positive；gather32 median `1.073x` 未改善 | rollback / no-production；生产源码零 diff，topic closeout 已提交 `4638abfd3` | 只有出现新的高收益候选、或 profile 证明 residual / covariance 能明显穿透完整 public entry 维护成本时才重开 | `test-rvv/registration/gicp/README.zh.md`；`test-rvv/registration/gicp/doc/gicp-evaluation.zh.md` |
| `ndt` | `computeDerivatives` / `updateDerivatives` per-point neighborhood accumulation | NDT phase 000/010/020 evidence；double `exp` 消融负向 | 已完成函数级评估；no-production | 只能从 fused formula / 减少 staging 的新形态开始，并重新上板卡 | `test-rvv/registration/ndt` |

### 6.2 暂缓 / 不单独实施

| 主题 | 主成本覆盖类型 | 暂缓原因 | 重新考虑条件 | 证据来源 |
| --- | --- | --- | --- | --- |
| `ndt_2d` | `partial-preprocess` | 2D grid 构建、normal distribution estimation、每轮 `target_ndt.test`、3x3 Newton / eigen solve 和 dataset 形态混在一起；相比 3D NDT 缺少更强通用价值 | 3D NDT component ablation 后证明 derivative / grid score 是主成本，或出现稳定 2D dataset / upstream case | `ndt_2d.hpp` |
| `joint_icp` | `non-standalone` | 多 cloud correspondence、rejector 和 transform estimation orchestration；真实 RVV 点在 ICP transformCloud、correspondence 或 estimator 子主题 | 子主题完成后 profile 显示本文件仍有独立热点 | `joint_icp.hpp`；`icp-RVV.zh.md` |
| `correspondence_estimation` | `tail-compress` | 每个 source 点主成本是 `tree_->nearestKSearch`；threshold 和 append tail 太薄 | 有 search 后处理占比 profile，或上游提供已物化 nearest-neighbor 结果的独立 helper | `correspondence_estimation.hpp`；TVE search dilution |
| `correspondence_estimation_normal_shooting` | `diagnostic` | 每点先 KNN，再在 K 个候选中做点到 normal line 距离；局部 k-loop 不能代表入口 | `k_` 很大且 profile 证明 distance-min loop 主导，或 search 可复用预计算结果 | `correspondence_estimation_normal_shooting.hpp` |
| `correspondence_estimation_backprojection` | `diagnostic` | KNN / reciprocal KNN 主导，normal dot / weighted distance score loop 只是候选内小段 | 大 K workload 或 profile 证明 score loop 接近主成本 | `correspondence_estimation_backprojection.hpp` |
| `elch` | `non-standalone` | loop closure graph / registration orchestration，不承载独立可批量 RVV 主循环 | 下游 registration 子主题全部完成后仍有 graph-level profile 热点 | `elch.hpp` |
| `pyramid_feature_matching` | `diagnostic` | histogram intersection 是 unit-stride min+sum，但使用场景和 bin 规模不清；`correspondence_types` 说明简单循环不自动收益 | 上游 workload / dataset 显示 `comparePyramidFeatureHistograms` 是热点，且 hist size 足够大 | `pyramid_feature_matching.hpp`；`correspondence_types-evaluation.zh.md` |
| `ppf_registration` | `diagnostic` | nested scene reference、radiusSearch、pair feature hash、accumulator voting、pose clustering 和 sorting 主导 | 固定 model/scene case profile 证明 pair feature formula 独立占比高 | `ppf_registration.hpp` |
| `lum` | `partial-preprocess` | graph edge accumulation 后接 linear system / QR / inverse 等 solver；graph / solver 主导 | profile 指向 edge matrix accumulation，并能构造 production-shaped ablation | `lum.hpp` |
| `transformation_estimation_point_to_plane_weighted` | `diagnostic` | LM residual functor 可批处理，但 Eigen `NumericalDiff` / LevenbergMarquardt 多次调用和 solver 控制流主导；weighted LLS 正向不能外推到 LM | LM workload profile 证明 residual functor 占主成本，并与 weighted LLS 公式边界区分 | `transformation_estimation_point_to_plane_weighted.hpp`；weighted LLS production docs |
| `transformation_estimation_lm` | `diagnostic` | 同 LM / numerical diff 控制流；warp + residual 局部公式不代表 full estimate | 真实 LM caller profile 和 production-shaped residual diagnostic 均成立 | `transformation_estimation_lm.hpp`；BFGS negative |
| `sample_consensus_prerejective` | `diagnostic` | random sample selection、feature KNN、transform、fitness / inlier scan 和 early termination 共同主导 | profile 证明 fitness scan 独立热点，且 search / random sampling 已隔离 | `sample_consensus_prerejective.hpp` |
| `ia_ransac` | `diagnostic` | SAC-IA 随机采样、feature correspondence KNN、transform 和 nearest-neighbor error metric 主导 | 已有 deterministic workload + profile 显示 error reduction 是主成本 | `ia_ransac.hpp` |
| `ia_fpcs` | `diagnostic` | FPCS base selection、radiusSearch、pair matching、candidate state 和 nearest search 主导 | profile 证明 validate residual loop 独立主导，且候选排序 / search 可隔离 | `ia_fpcs.hpp` |
| `ia_kfpcs` | `diagnostic` | KFPCS candidate filtering、sort、nearest search 和 residual score 混杂 | 同上，需真实 workload profile 和 component ablation | `ia_kfpcs.hpp` |
| `correspondence_rejection_sample_consensus` | `diagnostic` | 委托 `pcl::RandomSampleConsensus` 和 SAC model；inlier scan / unordered_map 只是后段 | RANSAC 模型已外部固定且 profile 指向 inlier map / scan | `correspondence_rejection_sample_consensus.hpp` |
| `correspondence_rejection_sample_consensus_2d` | `diagnostic` | 与 3D SAC rejector 相同且适用面更窄 | 稳定 2D production case 和 profile 指向后段 scan | `correspondence_rejection_sample_consensus_2d.hpp` |
| `transformation_estimation_3point` | `diagnostic` | 输入固定 3 点，公式小且每次调用规模太小 | 只有作为其它大外层 topic 的 inline helper，不单独实施 | `transformation_estimation_3point.hpp`；BFGS small-vector negative |
| `pairwise_graph_registration` | `non-standalone` | pairwise graph registration orchestration，不承载独立可批量主循环 | 子 registration / graph solver 主题完成后仍有独立 profile 热点 | `pairwise_graph_registration.hpp` |

## 7. 新的执行清单 / 状态表

| 顺序 | 主题 | 主文件 | 推荐入口 / 第一 RVV 目标 | 依据模式 / 证据来源 | 状态 | 当前结论 / 下一步条件 |
| ---: | --- | --- | --- | --- | --- | --- |
| 1 | `transformation_estimation_svd_scale` | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` | `getTransformationFromCorrelation` 中 scale-aware fused accumulation；先覆盖 ordered / dense / `Scalar=float` | SVD production-ready 四 row-source；源码中 `sum_ss/sum_tt` 和 `R4 * cloud_src_demean` 可被同源累加问题验证 | 建议启动函数级评估 | 第一条建议启动的未完成主题。S2 首问：scale 项能否与 SVD fused sums 合并并给出 correctness / numerical budget。 |
| 2 | `gicp` | `registration/include/pcl/registration/impl/gicp.hpp` | 已尝试 residual / Mahalanobis diagnostic、covariance post-KNN、`dfddfLoopRVV()` production probe 和 gather-width 微调 | topic-local GICP evidence；cost-only public median `1.019x/1.011x` neutral；clean `dfddfLoopRVV()` public median `1.080x/1.058x` weak-positive；Phase 004 gather32 `1.073x` no-improvement | 已完成函数级评估；rollback / no-production | 不建议接入 production。生产源码已回到零 diff，topic closeout 已提交为 `4638abfd3`；若未来重开，需新的高收益候选或 profile 证明收益能覆盖维护成本。 |
| 3 | `ndt` | `registration/include/pcl/registration/impl/ndt.hpp` | `computeDerivatives` / `updateDerivatives` per-point neighborhood accumulation | NDT phase 000/010/020 evidence；double `exp` 消融负向 | 已完成函数级评估；no-production | 不建议接入 production；若未来重开，只能从 fused formula / 减少 staging 的新形态开始，并重新上板卡。 |
| 4 | retained remainder | 其余 19 个 3.2 文件 | 无默认独立 topic | search / RANSAC / LM / graph / small-loop failure boundaries | 暂缓 / 不单独实施 | 等 profile、dataset、子主题完成或用户明确要求恢复。 |

本轮发现两条可补充到 agent 指令的通用规则：

1. 保留候选复筛中，任何 in-progress、dirty worktree、pending user confirmation 或未完成 closeout 的 topic，
   必须放入单独的 provisional section；可以记录风险和结构观察，但不能作为已完成主题证据影响排序。
2. public Std/RVV 正向只证明该 public 边界上的 RVV 相对标量收益；当后续要选择两个 RVV 实现族时，
   必须补同一 production boundary 内的 RVV-vs-RVV A/B，或把 family 选择写成 caveat / pending。
