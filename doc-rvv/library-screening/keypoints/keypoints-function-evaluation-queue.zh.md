# keypoints 模块 RVV 第二轮函数评估队列

本文档基于 `doc-rvv/library-screening/modules/keypoints-file-candidate-screening.zh.md` 的第一轮 `high/mid` 文件候选，执行第二轮函数级下钻筛选。第二轮不重做第一轮，只复核第一轮交接的 `12` 个文件，并形成后续函数级评估队列；不修改 production 源码，不建立 `test-rvv` topic，不运行板卡 bench，也不提交。

## 1. 输入依据

- 指令与模板：`.agents/skills/rvv-screening/SKILL.md`、`screening-criteria.md`、`stage-and-queue-policy.md`、`evidence-boundaries.md`、`function-evaluation-queue-template.md`。
- 第一轮基线：`keypoints-file-candidate-screening.zh.md`，统计为 `36` 个源码文件、`5 high / 7 mid / 24 low`。
- 源码范围：`keypoints/include/pcl/keypoints/**`、`keypoints/src/**`。
- 第二轮必查文件：`impl/harris_2d.hpp`、`impl/trajkovic_2d.hpp`、`impl/trajkovic_3d.hpp`、`src/brisk_2d.cpp`、`src/narf_keypoint.cpp`、`src/agast_2d.cpp`、`impl/harris_3d.hpp`、`impl/harris_6d.hpp`、`impl/iss_3d.hpp`、`impl/sift_keypoint.hpp`、`impl/smoothed_surfaces_keypoint.hpp`、`impl/susan.hpp`。
- 测试入口：`test/keypoints/CMakeLists.txt` 只登记 `keypoints_general` 和 `keypoints_iss_3d`；`test/keypoints/test_keypoints.cpp` 覆盖 SIFT，`test/keypoints/test_iss_3d.cpp` 覆盖 ISS。当前未发现 keypoints 模块专用 benchmark 入口，也未发现既有 `test-rvv/keypoints` 主题目录。
- 已有同类证据：`features` 模块中 organized image / normal / descriptor 等主题已显示，能覆盖公开入口主路径且验证形态清楚的局部 RVV 才适合进入建议队列；search、Eigen solver、histogram scatter、tail-compress 或 helper-only 成本通常需要先做 diagnostic 或 profile prerequisite。

## 2. 二轮筛选统计

| 项目 | 数量 | 说明 |
| ---- | ---: | ---- |
| 第一轮源码文件总数 | 36 | `keypoints/**` 文件级覆盖数 |
| 第一轮 high | 5 | 第二轮必查 |
| 第一轮 mid | 7 | 第二轮必查 |
| 第一轮 low | 24 | 默认不进入第二轮，除非发现明确漏判 |
| 第二轮初始候选基线 | 12 | `high + mid` 去重后数量 |
| 新增补充候选 | 0 | 未发现需要从 `low` 补入的明确源码漏判 |
| 第二轮候选总数 | 12 | 初始基线 `12` + 补入 `0` |
| 建议进行 RVV 优化的文件 | 4 | 覆盖公开入口主路径或直接补齐 RISC-V 可执行 SIMD 路径 |
| 保留实施的候选文件 | 6 | 有明确 RVV 片段，但需先回答 search/Eigen/状态机/测试形态导致的主成本问题 |
| 暂缓或不推荐考虑 RVV 优化的文件 | 2 | 当前证据不支持独立 RVV 主题 |
| 源码冲突、删除或需要修正第一轮路径 | 0 | 未发现文件删除或路径冲突 |
| 第一轮 high/mid 降级为暂缓或不推荐 | 2 | `src/narf_keypoint.cpp`、`impl/smoothed_surfaces_keypoint.hpp` |

## 3. 文件级变化理由

第二轮没有补入 `low` 文件。第一轮 `low` 中的公开 `.h` 文件、显式实例化 `.cpp`、base class 和薄 wrapper 与当前源码一致，真实循环已由对应 `impl/*.hpp` 或 `src/*.cpp` 的 `high/mid` 行覆盖。

主题归并只用于共享判断，不改变后续文件级推进粒度：

| 主题簇 | 共享判断 |
| ------ | -------- |
| organized Harris / Trajkovic | `impl/harris_2d.hpp`、`impl/trajkovic_2d.hpp`、`impl/trajkovic_3d.hpp` 都要求 organized 或等尺寸输入，主响应图是行列 stencil / fixed-neighbor 公式，trip count 来自 `width * height`，适合 VLA strip-mining、mask 和尾部处理。非极大值抑制中的 `std::sort`、occupancy map、critical `push_back` 不应作为第一 RVV 目标。 |
| BRISK / AGAST image detector | `src/brisk_2d.cpp` 有 `halfsample` / `twothirdsample` 的 SSSE3 分支，文件本身给出手写 SIMD 证据，RISC-V 当前 fallback 直接报错，建议优先评估 RVV 等价路径。`src/agast_2d.cpp` 承载 BRISK 依赖的 OAST/AGAST detector，但主 detector 是生成式深分支 / goto 决策树，只保留做 score/NMS 或 row-scan component ablation。 |
| search / Eigen 3D keypoints | `impl/harris_3d.hpp`、`impl/harris_6d.hpp`、`impl/iss_3d.hpp`、`impl/sift_keypoint.hpp`、`impl/susan.hpp` 均有邻域规约、协方差、阈值或输出压缩片段；但公开入口主成本高度受 `radiusSearch` / `nearestKSearch`、normal / gradient estimator、Eigen solver、动态 vector 和输出顺序支配。当前更适合保留为 diagnostic 或 production-shaped diagnostic。 |
| range image / multiscale state | `src/narf_keypoint.cpp` 和 `impl/smoothed_surfaces_keypoint.hpp` 虽有 `width * height` 或 `scale * points` 级循环，但主流程包含 region growing、动态队列、polynomial approximation、sort、跨尺度 KdTree 搜索和状态复用。局部 RVV 片段不足以支撑独立建议队列，暂缓。 |

逐项去向如下：

| 第一轮文件 | 第一轮等级 | 第二轮去向 | 变化理由 |
| ---------- | ---------- | ---------- | -------- |
| `impl/harris_2d.hpp` | high | 建议进行 RVV 优化 | 保持 high 的主因是 derivatives、`computeSecondMomentMatrix` 与四类 response loop 直接覆盖 organized 公开入口主路径；NMS 阶段另作风险边界。 |
| `impl/trajkovic_2d.hpp` | high | 建议进行 RVV 优化 | 保持 high，4/8-corner response grid 是固定邻域、连续 organized 访问和简单阈值 mask，适合先做 response-only production-value evaluation。 |
| `impl/trajkovic_3d.hpp` | high | 建议进行 RVV 优化 | 保持 high，normal stencil response 与 2D Trajkovic 同构，且 `IntegralImageNormalEstimation` 可作为输入前置；第一目标限于 response map，不覆盖 normal 生成或 NMS。 |
| `src/brisk_2d.cpp` | high | 建议进行 RVV 优化 | 保持 high，`halfsample` / `twothirdsample` 已有 SSSE3 手写 SIMD，且非 x86 fallback 当前只报错；RVV 可直接补齐 RISC-V scale-space 下采样路径。 |
| `src/narf_keypoint.cpp` | high | 暂缓或不推荐考虑 RVV 优化 | 从 high 降级；虽有 range image 批量循环，但主成本和语义受 region growing、动态邻居队列、18-bin 状态、polynomial approximation、sort 与距离去重支配。 |
| `src/agast_2d.cpp` | mid | 保留实施 | 保持 mid；批量 image scan 和 corner score 可诊断，但生成式深分支 detector 与顺序输出 `push_back` 不适合直接作为 production RVV 建议。 |
| `impl/harris_3d.hpp` | mid | 保留实施 | 保持 mid；`calculateNormalCovar` 有 SSE 先例和邻域规约价值，但公开入口每点 `radiusSearch`、normal estimation、Tomasi eigen33、refine solver 与 NMS 会稀释收益。 |
| `impl/harris_6d.hpp` | mid | 保留实施 | 保持 mid；RGB intensity staging、gradient normalization、21 项 covariance 可诊断，但 6x6 `SelfAdjointEigenSolver`、IntensityGradientEstimation 和 critical output 是硬风险。 |
| `impl/iss_3d.hpp` | mid | 保留实施 | 保持 mid；`getScatterMatrix`、eigen ratio 和 mask/output pass 有局部价值，现有 `test_iss_3d` 可验证，但 search、BoundaryEstimation 和 Eigen solver 需先隔离。 |
| `impl/sift_keypoint.hpp` | mid | 保留实施 | 保持 mid；DoG scale-space 中 Gaussian 权重和 extrema min/max 有 RVV 片段，现有 SIFT test 可触达，但 VoxelGrid、KdTree、`std::exp`、early break 和输出顺序使其先走 component ablation。 |
| `impl/smoothed_surfaces_keypoint.hpp` | mid | 暂缓或不推荐考虑 RVV 优化 | 降级；scale diff dot loop 明确但只是前置预处理，后续多 scale `radiusSearch`、跨尺度 extrema、动态输出和缺少专门测试入口使独立实施价值不足。 |
| `impl/susan.hpp` | mid | 保留实施 | 保持 mid；USAN 相似区域、centroid 和 minima 判断可做 diagnostic，但每点 `radiusSearch`、动态 `usan` vector、几何验证和缺少 keypoints 专项 test 限制直接建议。 |

## 4. 建议进行 RVV 优化的文件

| 文件路径 | 关键入口 / 函数族 | 所属主题簇 | 主成本覆盖类型 | trip count / 数据规模来源 | RVV 适配点 | 主要风险 | 测试/bench 可行性 | 第二轮去向 | 建议实施顺序 |
| -------- | ---------------- | ---------- | -------------- | ------------------------- | ---------- | -------- | ---------------- | ---------- | ------------ |
| `keypoints/include/pcl/keypoints/impl/harris_2d.hpp` | `HarrisKeypoint2D::detectKeypoints`、`computeSecondMomentMatrix`、`responseHarris/Noble/Lowe/Tomasi` | organized Harris 2D | `direct-main-path` | derivatives loop 为 `input_->width * input_->height`；response loop 为 `input_->size()`；second-moment 内层为 `window_width * window_height` | 行列 derivatives、窗口内 `ix*ix / ix*iy / iy*iy` 规约、det/trace/Tomasi 响应公式、finite/threshold mask；可用 VLA strip-mining 先覆盖 response map | `IntensityT` accessor 对点类型布局的泛型成本；Tomasi `sqrt` 与 NaN/Inf 传播；NMS 的 `std::sort`、occupancy map、critical output 不纳入第一目标 | 无 keypoints 专项 Harris test；可构造 organized synthetic cloud 做 scalar/RVV response 对拍，再补 production-shaped `compute()` smoke。无现成 benchmark，需后续 topic 内新增专项 bench | 建议进行 RVV 优化 | 1 |
| `keypoints/include/pcl/keypoints/impl/trajkovic_2d.hpp` | `TrajkovicKeypoint2D::detectKeypoints` 的 FOUR_CORNERS / EIGHT_CORNERS response grid | organized Trajkovic 2D | `direct-main-path` | 内部像素区域 `(width - window_size) * (height - window_size)`；NMS pass 为 `indices_->size()` | 固定 4/8 邻域强度 load、center 差分、平方和、min/max、`B/A` 响应、threshold mask；适合 row-wise RVV 计算 response_ | eight-corners 路径每点临时 `std::vector<float>` 标量写法需保持等价；NMS sort/occupancy/output 顺序不能改变；阈值边界和除零分支需精确对拍 | 未发现专用上游 test；可构造 organized grayscale 点云，覆盖 4/8 corners、边界、阈值等价和 NMS-off / NMS-on smoke | 建议进行 RVV 优化 | 2 |
| `keypoints/include/pcl/keypoints/impl/trajkovic_3d.hpp` | `TrajkovicKeypoint3D::detectKeypoints` 的 normal FOUR_CORNERS / EIGHT_CORNERS response grid | organized Trajkovic 3D | `direct-main-path` | 内部像素区域 `(width - window_size) * (height - window_size)`；normal cloud 与 input 等尺寸；NMS pass 为 `indices_->size()` | fixed-neighbor normal finite gate、`squaredNormalsDiff` / `normalsDiff`、sqrt、小公式响应和 threshold mask；可复用 Trajkovic 2D 的 row-wise response-map 评估形态 | normal 若未提供会由 `IntegralImageNormalEstimation` 生成，前置成本可能稀释收益；finite gate 多；NMS sort/occupancy/output 顺序风险同 2D | 未发现专用上游 test；可构造 organized point+normal cloud 对拍 response-only，并用 precomputed normals 避免把 normal estimation 归入本文件 | 建议进行 RVV 优化 | 3 |
| `keypoints/src/brisk_2d.cpp` | `ScaleSpace::constructPyramid`、`Layer::halfsample`、`Layer::twothirdsample`、`Layer::getValue`、`Layer::getAgastPoints` | BRISK scale-space image | `direct-main-path` | pyramid 每层 image `width * height`；downsample loops 为 source rows/cols；score fill 为 layer keypoint 数；interpolation窗口由 scale 决定 | `uint8_t` 连续 image load、2x2 halfsample 平均、3x3 到 2x2 twothirdsample 加权平均、row tail、saturating/narrow store；已有 SSSE3 分支可作为等价 SIMD 参照 | 当前 `#if defined(__SSSE3__)` 下非 x86 fallback 直接 `PCL_ERROR`，RISC-V 上需确认构建/运行现状；AGAST score 和 scale refinement 仍为深分支；整数平均的舍入/饱和语义需逐字节对拍 | 没有 keypoints benchmark；可先建 halfsample/twothirdsample helper 对拍和 RISC-V smoke，再接 BRISK `compute()` production-shaped diagnostic | 建议进行 RVV 优化 | 4 |

## 5. 保留实施的候选文件

| 文件路径 | 关键入口 / 函数族 | 所属主题簇 | 主成本覆盖类型 | trip count / 数据规模来源 | RVV 适配点 | 主要风险 | 测试/bench 可行性 | 第二轮去向 | 建议实施顺序 |
| -------- | ---------------- | ---------- | -------------- | ------------------------- | ---------- | -------- | ---------------- | ---------- | ------------ |
| `keypoints/src/agast_2d.cpp` | `AbstractAgastDetector::detectKeypoints`、`computeCornerScores`、`applyNonMaxSuppression`、`AgastDetector*_detect`、`OastDetector9_16` | AGAST/OAST image detector | `diagnostic` | detector row scan 为 `image width * height`；corner score 为 detected corners 数；NMS 为 corner 数 | intensity staging、ring-offset threshold comparisons、corner score 批量计算、score sort 前的 copy / select，可做 row-scan 或 score-only component ablation | 主 detector 函数包含大量 generated branch / `goto` 决策树，per-pixel 控制流差异大；输出 `corners.push_back` 顺序和 NMS 连通块语义要保持；BRISK 也调用 OAST detector | 可借 BRISK 或 AgastKeypoint2D 公开入口构造 image-shaped diagnostic；当前 keypoints tests 未直接覆盖 AGAST，需后续 topic 内补 correctness oracle | 保留实施 | 5 |
| `keypoints/include/pcl/keypoints/impl/harris_3d.hpp` | `HarrisKeypoint3D::detectKeypoints`、`calculateNormalCovar`、`responseHarris/Noble/Lowe/Tomasi/Curvature`、`refineCorners` | 3D Harris covariance | `diagnostic` | `input_->size()`；每点 `radiusSearch` 返回的邻域数；refine 为 corners 数乘最多 10 次迭代 | 邻域 normal covariance 累加、det/trace 响应公式、curvature copy、已有 `__SSE__` helper 说明局部 SIMD 价值 | 每点 `tree_->radiusSearch` 是主成本候选；Tomasi 走 `eigen33`，refine 走 `LDLT`；非极大值抑制再次 search 并 critical push；浮点规约顺序和 neighbor gather 需验证 | 无专用上游 Harris 3D test；可先做 `calculateNormalCovar` component ablation，再做 NMS-off production-shaped diagnostic | 保留实施 | 6 |
| `keypoints/include/pcl/keypoints/impl/harris_6d.hpp` | `HarrisKeypoint6D::detectKeypoints`、RGB intensity staging、gradient normalization、`calculateCombinedCovar`、`responseTomasi` | 6D Harris covariance | `diagnostic` | `surface_->size()`、`intensity_gradients_->size()`、每点 `radiusSearch` 邻域数 | RGB-to-intensity 转换、gradient length normalize/zero mask、21 项 normal+gradient covariance 累加 | `NormalEstimation` / `IntegralImageNormalEstimation` 和 `IntensityGradientEstimation` 是前置重成本；6x6 `SelfAdjointEigenSolver` 标量主导；`responseTomasi` 在 OpenMP critical 中 `push_back`，输出顺序风险高 | 无专用上游 test；需先构造 precomputed normals / gradients 的 component ablation，不能直接承诺 production path | 保留实施 | 7 |
| `keypoints/include/pcl/keypoints/impl/iss_3d.hpp` | `ISSKeypoint3D::detectKeypoints`、`getBoundaryPoints`、`getScatterMatrix`、eigen ratio / non-max / output pass | ISS 3D salient points | `diagnostic` | `input_->size()`；每点 salient / nonmax / border radius search 邻域数；3x3 covariance entries | scatter matrix 累加、border/feature bool mask、eigen ratio threshold、final feat_max 压缩式输出 | `searchForNeighbors`、BoundaryEstimation、NormalEstimation、Eigen 3x3 solver 和多阶段临时数组主导；output push 顺序和 threshold/tie 语义敏感 | `test/keypoints/test_iss_3d.cpp` 覆盖 with/without boundary estimation，可作为 correctness 基线；后续先做 scatter-matrix component ablation 与 production-shaped profile | 保留实施 | 8 |
| `keypoints/include/pcl/keypoints/impl/sift_keypoint.hpp` | `SIFTKeypoint::detectKeypoints`、`detectKeypointsForOctave`、`computeScaleSpace`、`findScaleSpaceExtrema` | SIFT scale-space 3D | `diagnostic` | octave 数；每 octave downsampled `input.size()`；scale 数 `nr_scales_per_octave + 3`；`radiusSearch` / fixed `nearestKSearch(25)` 邻域数 | Gaussian weight `exp` 前后公式、numerator / denominator 规约、DoG matrix 写入、scale extrema min/max over neighbors | `VoxelGrid` downsample、KdTree search、`std::exp`、radius early break、Eigen `MatrixXf` layout 和 keypoint output order 复杂；public test 比较 exact count/coordinates | `test/keypoints/test_keypoints.cpp` 覆盖 SIFT；可做 `computeScaleSpace` component ablation，首阶段回答 search 后 math 是否足够重 | 保留实施 | 9 |
| `keypoints/include/pcl/keypoints/impl/susan.hpp` | `SUSANKeypoint::detectKeypoints`、USAN construction、centroid / area、geometric validation、NMS minima | SUSAN 3D/2D keypoint | `diagnostic` | `input_->size()`；每点 `radiusSearch` 邻域数；USAN size；response size | intensity threshold、normal dot threshold、centroid sum、distance/norm validation、minima mask | 每点 search 和动态 `usan` vector 主导；geometry validation 有 cross/norm/dot helper；NMS 对 response index / normals index 的语义需先核对；当前缺少专用 keypoints test | 未发现上游 SUSAN test；需先新增 focused oracle 和 component diagnostic，当前只保留不排入建议队列 | 保留实施 | 10 |

## 6. 暂缓或不推荐考虑 RVV 优化的文件

| 文件路径 | 关键入口 / 函数族 | 所属主题簇 | 主成本覆盖类型 | trip count / 数据规模来源 | RVV 适配点 | 主要风险 | 测试/bench 可行性 | 第二轮去向 | 建议实施顺序 |
| -------- | ---------------- | ---------- | -------------- | ------------------------- | ---------- | -------- | ---------------- | ---------- | ------------ |
| `keypoints/src/narf_keypoint.cpp` | `NarfKeypoint::calculateCompleteInterestImage`、`calculateSparseInterestImage`、`calculateInterestPoints`、`detectKeypoints` | NARF range image keypoint | `diagnostic` | range image `width * height`；scale-space 层数；region-growing neighbors；18-bin angle histogram；interest point candidate 数 | interest image init、valid/border mask、surface-change threshold、distance/weight formula、angle histogram局部更新、final interest-point image scan | 主路径包含 region growing queue、`was_touched` 状态、dynamic vectors、angle bin element lists、polynomial approximation、sort 和 min-distance suppression；输出为 image index 顺序，局部 RVV 容易只覆盖碎片 | 当前 keypoints tests 未覆盖 NARF；features 模块有 NARF descriptor test 但不等价 keypoint 入口。需真实 range-image workload / profile 后再重开 | 暂缓；只有 profile 证明 interest-image 局部公式是热点，或有稳定 NARF keypoint oracle 时再考虑 | 不排期 |
| `keypoints/include/pcl/keypoints/impl/smoothed_surfaces_keypoint.hpp` | `SmoothedSurfacesKeypoint::detectKeypoints` 的 scale diff dot loop、input scale extrema、cross-scale extrema | smoothed surfaces multiscale | `partial-preprocess` | scale 数乘 `input_->size()`；后续每点、每 scale `radiusSearch` 邻域数 | per-scale normal dot displacement、diffs 数组 min/max 比较、候选 mask | 第一 RVV 片段只覆盖前置 diff 计算；主路径随后执行 input-tree 和 multi-scale radius searches、动态邻域 extrema 与 `push_back` 输出；`PCL_INFO`、cloud/normal 尺寸对齐和 scale sort 状态也增加测试负担 | 未发现专用上游 test 或 benchmark；需要先有代表多尺度输入和 profile | 暂缓；只有 profile 指向 diff dot 预处理占比高，或其它 keypoints 主题形成可复用 multiscale oracle 时再考虑 | 不排期 |

## 7. 执行清单 / 状态表

执行清单只列入第二轮建议队列中可直接启动函数级评估的文件。保留候选与暂缓候选已在第 5/6 节逐文件列出；后续复筛从对应表读取，不在状态表中重复做主题汇总。

| 顺序 | 主题 | 主文件 | 推荐入口 / 第一 RVV 目标 | 状态 | 当前结论 / 下一步条件 |
| ----: | ---- | ------ | ------------------------ | ---- | -------------------- |
| 1 | Harris 2D organized response | `impl/harris_2d.hpp` | `detectKeypoints` 中 derivatives + `responseHarris/Noble/Lowe/Tomasi` response map，先不碰 NMS sort/occupancy | 已采纳窄范围 production RVV | `PointXYZI -> PointXYZI`、`Scalar=float`、organized dense public `compute()`、`nonmax=false` 已接入 `responseRVV()`；phase020 production direct board 三组 case median 1.070x / 1.090x / 1.180x，Evidence Doctor 无 Error/Warning。泛型 `IntensityT`、NMS enabled 和真实 workload 保持后续 phase 范围。长期文档见 `doc-rvv/keypoints/harris_2d-RVV.zh.md`。 |
| 2 | Trajkovic 2D response grid | `impl/trajkovic_2d.hpp` | FOUR_CORNERS / EIGHT_CORNERS response grid 的 fixed-neighbor stencil | 已采纳窄范围 production RVV | EIGHT_CORNERS `PointXYZI` / 默认 accessor / 3x3 public `compute()` 已接入 production RVV；FOUR_CORNERS、泛型点类型、RGB accessor、非 3x3 window 和 NMS 保持标量或另开后续 phase。长期文档见 `doc-rvv/keypoints/trajkovic_2d_response_grid-RVV.zh.md`。 |
| 3 | Trajkovic 3D normal response grid | `impl/trajkovic_3d.hpp` | precomputed normals 条件下的 normal stencil response map | 已采纳窄范围 production RVV | `FOUR_CORNERS`、3x3、organized dense public `compute()` 路径已接入 RVV；production board `public_four_corners_320x240` median 1.769x，Evidence Doctor 无 Error/Warning。`EIGHT_CORNERS`、non-dense、normal estimation、NMS 和更宽点类型保持标量或进入后续 phase。长期文档见 `doc-rvv/keypoints/trajkovic_3d-RVV.zh.md`。 |
| 4 | BRISK scale-space downsample | `src/brisk_2d.cpp` | `Layer::halfsample` / `Layer::twothirdsample` 的 RISC-V RVV 等价路径 | 已采纳 / topic closeout | RISC-V `__RVV10__` 路径已接入 `halfsample` / `twothirdsample`，非 RVV / 非 SSSE3 使用 portable scalar fallback；Milkv-Jupiter Phase 010 5-run repeated board 显示 `constructPyramid` median 1.136x、`twothirdsample` median 1.108x，Evidence Doctor 无 Error/Warning。synthetic `BriskKeypoint2D::compute()` public case median 1.017x、0/5 反向，说明公开入口影响接近中性；topic closeout 统计见 `tmp/rvv-topic-stats/3/keypoints-brisk_2d-stats.zh.md`，长期文档见 `doc-rvv/keypoints/brisk_2d-RVV.zh.md`。 |

## 8. Closeout

- 输出文档位置：`doc-rvv/library-screening/keypoints/keypoints-function-evaluation-queue.zh.md`。
- 第一轮文件候选筛选统计：`5 high / 7 mid / 24 low`，第二轮必查基线 `12`。
- 第二轮队列统计：建议 `4`，保留 `6`，暂缓或不推荐 `2`，补入 `low` 为 `0`。
- 第一条未完成建议主题：当前建议队列中的 Harris 2D、Trajkovic 2D、Trajkovic 3D 和 BRISK 均已有窄范围 production RVV 采纳，其中 BRISK 已收口并补入 stats 记录；后续若继续 keypoints，应从第 5 节保留实施候选中按 profile / diagnostic prerequisite 选择，例如 `src/agast_2d.cpp` 或 3D Harris 系列。
- 通用规则反哺：keypoints 模块的 2D organized response 与 BRISK downsample 可以优先进入函数级评估；search/Eigen/region-growing/state-machine 主导文件即使存在局部大循环，也应先走 component ablation、production-shaped diagnostic 或 profile prerequisite，不能凭第一轮 high/mid 直接承诺 production RVV 分流。
