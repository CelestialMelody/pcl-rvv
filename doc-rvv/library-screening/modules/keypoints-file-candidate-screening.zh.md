# keypoints 模块 RVV 第一轮文件级筛选报告

本文档记录 `keypoints` 模块的第一轮 RVV 文件级粗筛结果。第一轮只回答“文件中是否存在值得第二轮下钻复核的可 SIMD/RVV 片段”，不直接决定最终 RVV 实施队列。

## 1. 输入依据与范围

- 源码范围：`keypoints/**`
- 源码后缀：`.h`、`.hpp`、`.c`、`.cc`、`.cpp`、`.cu`
- 覆盖结果：总文件 `36`，已判定 `36`（`36/36` 全覆盖）。
- 目录拆分：`include` `25`，`src` `11`。
- 第三方口径：`3rdparty/**` 文件 `0`，候选 `0`（仅登记，不纳入优化队列）。
- 指令依据：`.agents/skills/rvv-screening/SKILL.md`、`screening-criteria.md`、`stage-and-queue-policy.md`、`evidence-boundaries.md`、`file-candidate-screening-template.md`。
- 重做口径：旧同路径文档只作为 previous baseline（历史基线）和覆盖检查参考；本次判定以当前源码中的入口、loop/helper、trip count、RVV 适配点和风险为依据，不沿用旧文档的循环数量或数学项数量。
- 路径显示：候选表和覆盖表中的 `include` 文件省略公共前缀 `keypoints/include/pcl/keypoints/`，`src` 文件以 `src/` 开头显示。

## 2. 第一轮筛选口径

- 第一轮是文件级粗筛，判断标准是文件内是否存在可向量化循环、数学密集片段、批量字段访问、规约、mask/压缩、图像式 organized 遍历或可诊断的局部 SIMD/RVV 点。
- `high/mid` 是第二轮必须复核并交代去向的初始候选基线，不是最终实施全集。
- `low` 表示本轮未发现足以进入二轮基线的证据，不是永久排除；如果第二轮源码下钻发现明显漏判，可以补入并说明证据。
- 第一轮不承诺 RVV 覆盖公开入口主成本；主成本覆盖、测试可行性、fallback（回退路径）条件和维护风险由第二轮筛选继续判断。
- 公开 `.h` 文件如果只承载类声明、参数访问器、类型别名和 `impl` 包含，不因关联实现存在批量循环而单独列为候选；真实循环归入对应 `impl/*.hpp` 或 `src/*.cpp`。
- 显式实例化 `.cpp` 文件如果只包含实现头和 `PCL_INSTANTIATE` / commented instantiation（注释掉的显式实例化），不单独列为候选。

评估维度：

| 维度 | 第一轮判断口径 |
| --- | --- |
| 循环规模 | 是否随点数、像素数、尺度层数、邻域数、候选 keypoint 数或 range image 大小线性或更高增长 |
| 算术密度 | 是否包含强度差分、Harris / Trajkovic 响应、协方差、特征值前处理、距离、dot、histogram、阈值比较或局部规约 |
| 访存模式 | 是否存在连续 image buffer、organized 行列、AoS 字段、邻域 indices gather 或固定小窗口访问 |
| 分支复杂度 | 是否主要是简单谓词/mask，还是生成式决策树、search、sort、region growing、Eigen solver 或状态机主导 |
| 语义风险 | 是否涉及非极大值抑制顺序、阈值边界、NaN/Inf、输出 keypoints 顺序、indices 和用户自定义点类型 |
| 可验证性 | 是否有上游 test/example，可构造 std/RVV 对拍、边界 case、fallback case、QEMU 和板卡验证 |

优先级含义：

| 优先级 | 含义 |
| --- | --- |
| `high` | 文件内至少有一个强批量 loop 或函数族，满足生产价值、并行合法性、RVV 访存匹配中的两个以上强信号，且没有未解释的语义或测试硬伤 |
| `mid` | 存在可 SIMD/RVV 片段，但主成本、数据布局、indices/gather 成本、浮点语义、入口覆盖或测试可行性仍需函数评估队列判定 |
| `low` | 以声明、薄 wrapper、调度、类型胶水、小规模固定计算、不规则容器/search/solver/状态机或非热点路径为主；除非有 profile、源码变化或已完成主题证据，否则不进入函数评估队列初始候选 |

## 3. 第一轮筛选统计

| 项目 | 数量 | 说明 |
| --- | ---: | --- |
| 源码文件总数 | 36 | `keypoints/**` 源码文件，第三方实现仅登记覆盖，不纳入候选主线。 |
| 已判定文件数 | 36 | `36/36` |
| high | 5 | 二轮必查 |
| mid | 7 | 二轮必查 |
| low | 24 | 已覆盖但不进入二轮初始基线 |
| high + mid | 12 | 第一轮候选基线 |
| 候选占比 | 12/36 = 33.3% | high + mid / 源码文件总数 |

相对旧文档的主要变化：旧文档把大量公开头声明和显式实例化文件按关键词命中列为 `mid`，并把若干 search / solver 主导实现列为 `high`。本次重做后，候选基线收敛到真实承载算法循环的 `impl/*.hpp` 和少数非模板 `.cpp` 文件；`harris_3d`、`harris_6d`、`iss_3d`、`sift_keypoint`、`susan` 因邻域搜索、Eigen 求解、动态集合或输出顺序风险降为 `mid`。

## 4. high 候选

| 文件 | 关键入口 / loop | trip count 来源 | RVV 适配点 | 主要风险 | 第一轮判定理由 |
| --- | --- | --- | --- | --- | --- |
| `impl/harris_2d.hpp` | `HarrisKeypoint2D::detectKeypoints` 的 derivatives 行列扫描、`responseHarris/Noble/Lowe/Tomasi`、`computeSecondMomentMatrix` | organized 输入的 `width * height`、窗口 `window_width * window_height`、`response_->size()` | 连续/organized 强度读取，行列导数，窗口内 `ix*ix/ix*iy/iy*iy` 累加，响应公式和阈值 mask | 非极大值抑制包含 `std::sort`、occupancy map 和 critical 输出；`IntensityT` 字段访问和 NaN 传播需对拍 | 公开入口要求 organized cloud 且不支持 subset indices；响应计算覆盖主批量路径，访存和公式比 3D search 类候选更规整。 |
| `impl/trajkovic_2d.hpp` | `TrajkovicKeypoint2D::detectKeypoints` 的 4/8-corner response grid 与非极大值抑制 | organized 输入内部区域 `(width - window) * (height - window)`、`indices.size()` | 固定 4/8 邻域强度差、平方和、min/max、小公式和阈值 mask；响应图可按行列分块 | 8-corner 路径每点分配小 `std::vector`，非极大值抑制需要排序和 occupancy 写入；输出顺序需保持 | 主响应是图像式 stencil（固定邻域模板）循环，数据布局和公式适合第二轮优先下钻。 |
| `impl/trajkovic_3d.hpp` | `TrajkovicKeypoint3D::detectKeypoints` 的 normal stencil response 与非极大值抑制 | organized 输入内部区域 `(width - window) * (height - window)`、`indices.size()` | 固定邻域 normal dot / squared diff，小公式响应，threshold mask；和 2D Trajkovic 可共享 organized stencil 评估思路 | normal 生成依赖 `IntegralImageNormalEstimation`，有限值 gate 多；非极大值抑制仍有排序、occupancy 和 critical 输出 | 虽然输入 normal 预处理不在本文件内，核心 response loop 仍是规整 organized 访问，具备较强第一轮 RVV 下钻价值。 |
| `src/brisk_2d.cpp` | `ScaleSpace::constructPyramid/getKeypoints`、`Layer::halfsample/twothirdsample/getValue/getAgastPoints`、subpixel/scale refinement helpers | 图像 `width * height`、pyramid layer 数、layer keypoint 数、patch/interpolation 窗口 | 连续 `unsigned char` image buffer 下采样、面积加权采样、score image 填充、局部 2D/scale 插值；文件已有 SSSE3 x86 SIMD 分支可作为手写 SIMD 形态线索 | 与 AGAST score 决策树耦合，`#if defined(__SSSE3__)` 下非 x86 fallback 直接报错；边界、饱和/舍入和 layer scale 语义复杂 | 这是 BRISK 非模板主体实现，承载 scale-space 的批量 image 操作；已有平台 SIMD 说明该文件值得二轮评估 RVV 替代或补齐路径。 |
| `src/narf_keypoint.cpp` | `calculateCompleteInterestImage`、`calculateSparseInterestImage`、`calculateInterestPoints`、`detectKeypoints` | range image `width * height`、scale-space 层数、region-growing 邻居数、18-bin angle histogram、候选 interest points | range image 批量有效性判断、surface-change score、角度 histogram、min/max、距离/权重公式、最终 interest point mask 输出 | region growing、动态 vector、sort、polynomial approximation、range-image border 状态和 OpenMP private state 复杂；局部 SIMD 需分阶段验证 | 文件是 NARF keypoint 的真实实现，包含多个随 range image 和邻域规模增长的批量热点；虽然状态复杂，仍是 keypoints 模块少数值得 high 级下钻的 `.cpp`。 |

## 5. mid 候选

| 文件 | 关键入口 / loop | trip count 来源 | RVV 适配点 | 主要风险 | 第一轮判定理由 |
| --- | --- | --- | --- | --- | --- |
| `src/agast_2d.cpp` | `AbstractAgastDetector::detectKeypoints/applyNonMaxSuppression/computeCornerScores`、`AgastDetector*_detect`、`*_computeCornerScore` | image `width * height`、detected corners 数、binary-search score 迭代 | 连续 image buffer、环形 offset 比较、corner score 批处理、NMS score 选择 | 主 detector 是生成式深分支/goto 决策树，per-pixel 控制流差异大；输出 push_back 和 NMS 顺序需保持 | 有清楚的批量图像扫描和 score 循环，但第一 RVV 目标很可能只适合 component ablation（组件消融）或 score/NMS 子路径，故列 `mid`。 |
| `impl/harris_3d.hpp` | `responseHarris/Noble/Lowe/Tomasi/Curvature`、`calculateNormalCovar`、`refineCorners` | `input_->size()`、每点 `radiusSearch` 邻域数、corners 数 | normal covariance 累加、Harris/Noble/Lowe 3x3 determinant/trace 公式、curvature copy、已有 SSE helper 可作为局部 SIMD 线索 | `radiusSearch`、normal estimation、Tomasi eigen33、refine `LDLT` solver 和非极大值抑制主导；邻域 gather 和浮点规约语义需验证 | 文件有明确逐点/邻域数学核，但 search/Eigen/critical 输出可能稀释收益；第二轮需先判断 covariance 子核是否接近主成本。 |
| `impl/harris_6d.hpp` | `detectKeypoints` 的 RGB intensity staging、gradient normalization、`responseTomasi`、`calculateCombinedCovar` | `surface_->size()`、`intensity_gradients_->size()`、每点邻域数 | RGB 到 intensity 批量转换，gradient length threshold，21 项 combined covariance 累加 | Normal/IntensityGradient estimator 和 6x6 `SelfAdjointEigenSolver` 主导；`responseTomasi` 内 critical push_back 和 search gather 风险高 | 存在算术密集局部核，但公开入口主成本很可能落在特征估计器和 Eigen solver，先列 `mid`。 |
| `impl/iss_3d.hpp` | `getBoundaryPoints`、`getScatterMatrix`、`detectKeypoints` 的 eigen ratio、non-max 和 output pass | `input_->size()`、`radiusSearch` 邻域数、3x3 covariance entries、non-max 邻域数 | scatter matrix 累加、eigen ratio threshold、border/feature mask、最终 keypoint 压缩式输出 | Octree/search、BoundaryEstimation、NormalEstimation、3x3 Eigen solver 和多阶段临时数组主导；顺序和边界点语义复杂 | 上游有 `test/keypoints/test_iss_3d.cpp` 可验证，但第一轮只能确认局部 RVV 子核价值，不能把 ISS 直接列 high。 |
| `impl/sift_keypoint.hpp` | `detectKeypointsForOctave`、`computeScaleSpace`、`findScaleSpaceExtrema` | octave 数、downsampled `input.size()`、scale 数、`radiusSearch`/`nearestKSearch` 邻域数 | Gaussian weight `exp`、numerator/denominator 规约、DoG matrix 写入、scale extrema min/max | `VoxelGrid`、KdTree search、`std::exp`、early break、Eigen `MatrixXf` 布局和多尺度输出顺序复杂 | 有上游 `test/keypoints/test_keypoints.cpp`，但主成本和 RVV 目标需要拆成 scale-space 组件评估，列 `mid`。 |
| `impl/smoothed_surfaces_keypoint.hpp` | `detectKeypoints` 的 scale diff dot loop、input scale extrema search、跨尺度 minima/maxima 判断 | scale 数、`input_->size()`、每点 `radiusSearch` 邻域数 | normals dot displacement 批量计算，diffs 数组 min/max 比较，输出 keypoints mask | 多 scale KdTree、radiusSearch 和跨尺度动态邻域主导；`PCL_INFO`、输入 cloud 对齐假设和输出顺序需检查 | 文件有直接逐点 dot 预处理和后续 extrema 判断，但邻域搜索占比不明，列 `mid`。 |
| `impl/susan.hpp` | `SUSANKeypoint::detectKeypoints` 的 per-point USAN（相似区域）构造、centroid/area、非极大值抑制 | `input_->size()`、每点 `radiusSearch` 邻域数、USAN size、response size | intensity threshold、normal dot threshold、centroid 累加、distance/geometric validation、minima mask | radiusSearch、动态 `usan` vector、几何验证、normal estimation 和 response/output 顺序复杂；当前没有 keypoints 专项上游 test | 有批量邻域算术和输出筛选，但 search 与动态集合较重，适合作为二轮保留/诊断候选而非 high。 |

## 6. 全量文件覆盖表

| 文件 | 优先级 | 是否候选 | 判断依据 | 关联实现 |
| --- | --- | --- | --- | --- |
| `agast_2d.h` | low | 否 | 公开 detector / keypoint 类声明、参数访问器和内部适配结构为主；真实 image scan 和 score 决策树在 `src/agast_2d.cpp`。 | `impl/agast_2d.hpp`、`src/agast_2d.cpp` |
| `brisk_2d.h` | low | 否 | 公开类、Layer/ScaleSpace 声明和访问器为主；真实 pyramid、sampling 和 AGAST score 实现在 `src/brisk_2d.cpp`。 | `impl/brisk_2d.hpp`、`src/brisk_2d.cpp` |
| `harris_2d.h` | low | 否 | 公开类声明、response method、参数接口和 `impl` include；批量 derivatives/response loop 在 `impl/harris_2d.hpp`。 | `impl/harris_2d.hpp` |
| `harris_3d.h` | low | 否 | 公开类声明、参数接口和 response 函数声明；实际 search/covariance/response loop 在 `impl/harris_3d.hpp`。 | `impl/harris_3d.hpp` |
| `harris_6d.h` | low | 否 | 公开类声明、参数接口和成员状态；实际 combined covariance 和 Tomasi response 在 `impl/harris_6d.hpp`。 | `impl/harris_6d.hpp` |
| `impl/agast_2d.hpp` | low | 否 | 模板入口只把 organized cloud intensity 拷贝到 `image_data` 并分发到 detector；独立 RVV 价值不足，主算法在 `src/agast_2d.cpp`。 | `src/agast_2d.cpp` |
| `impl/brisk_2d.hpp` | low | 否 | 模板入口执行 intensity staging、调用 `ScaleSpace`、可选 invalid 3D keypoint 清理；主 scale-space 批量核在 `src/brisk_2d.cpp`。 | `src/brisk_2d.cpp` |
| `impl/harris_2d.hpp` | high | 是 | Organized derivatives、second-moment matrix 和 Harris/Noble/Lowe/Tomasi response 是规整 image/grid 批量路径。 | `harris_2d.h` |
| `impl/harris_3d.hpp` | mid | 是 | 逐点 response 和 normal covariance 有 SIMD 片段，但每点 radius search、Eigen/Tomasi、refine solver 和非极大值抑制会稀释主成本。 | `harris_3d.h` |
| `impl/harris_6d.hpp` | mid | 是 | RGB intensity staging、gradient normalization 和 21 项 covariance 可下钻，但 6x6 Eigen solver 与 feature estimator 主导风险高。 | `harris_6d.h` |
| `impl/iss_3d.hpp` | mid | 是 | Scatter matrix、eigen ratio、border/feature mask 有局部 RVV 点；search、BoundaryEstimation 和 Eigen solver 需要二轮拆分。 | `iss_3d.h` |
| `impl/keypoint.hpp` | low | 否 | base `Keypoint::initCompute/compute` 负责 search method 绑定、调度和输出前后处理，不承载可独立 RVV 的算法批量核。 | `keypoint.h` |
| `impl/sift_keypoint.hpp` | mid | 是 | Scale-space Gaussian response、DoG matrix、extrema min/max 有批量算术；VoxelGrid、KdTree 和 `exp` 规约使主成本待确认。 | `sift_keypoint.h` |
| `impl/smoothed_surfaces_keypoint.hpp` | mid | 是 | 跨 scale dot-diff 预处理和 extrema 判断存在批量 loop；多 KdTree radius search 与动态邻域主导风险需二轮确认。 | `smoothed_surfaces_keypoint.h` |
| `impl/susan.hpp` | mid | 是 | USAN 邻域 intensity/normal 判断、centroid/area 和 response 筛选可局部 SIMD；radius search 和动态集合复杂。 | `susan.h` |
| `impl/trajkovic_2d.hpp` | high | 是 | Organized 4/8-corner intensity stencil response 是直接批量主路径，适合行列分块和 mask 化评估。 | `trajkovic_2d.h` |
| `impl/trajkovic_3d.hpp` | high | 是 | Organized normal stencil response 与 Trajkovic 2D 类似，批量公式规整；normal 预处理和 non-max 由二轮继续审计。 | `trajkovic_3d.h` |
| `iss_3d.h` | low | 否 | 公开类声明、半径/阈值参数和 octree/search 类型别名；真实 loop 在 `impl/iss_3d.hpp`。 | `impl/iss_3d.hpp` |
| `keypoint.h` | low | 否 | 抽象基类、search 方法声明、getter/setter 和纯虚 `detectKeypoints`；实现只是调度。 | `impl/keypoint.hpp` |
| `narf_keypoint.h` | low | 否 | NARF 参数、公开接口和 stream 输出；真实 range image interest 计算在 `src/narf_keypoint.cpp`。 | `src/narf_keypoint.cpp` |
| `sift_keypoint.h` | low | 否 | Field selector 和 SIFT 类声明为主；RGB/intensity 小 helper 固定开销，scale-space loop 在 `impl/sift_keypoint.hpp`。 | `impl/sift_keypoint.hpp` |
| `smoothed_surfaces_keypoint.h` | low | 否 | 公开类声明、参数和 cloud 管理接口；批量 diff/extrema loop 在 `impl/smoothed_surfaces_keypoint.hpp`。 | `impl/smoothed_surfaces_keypoint.hpp` |
| `src/agast_2d.cpp` | mid | 是 | 非模板 AGAST/OAST detector 实现包含 image scan、corner score、NMS score loop；生成式深分支使其先列 `mid`。 | `agast_2d.h` |
| `src/brisk_2d.cpp` | high | 是 | BRISK scale-space、sampling、score image 和已有 SSSE3 downsampling 是明确批量 image 主体实现。 | `brisk_2d.h` |
| `src/harris_3d.cpp` | low | 否 | 只包含 `sift_keypoint.h`、`impl/harris_3d.hpp` 和注释掉的 `PCL_INSTANTIATE_PRODUCT`；无独立批量算法。 | `impl/harris_3d.hpp` |
| `src/harris_6d.cpp` | low | 否 | 只包含 `impl/harris_6d.hpp` 和注释掉的显式实例化；真实 loop 在 impl。 | `impl/harris_6d.hpp` |
| `src/iss_3d.cpp` | low | 否 | 只包含 `impl/iss_3d.hpp` 和注释掉的显式实例化；真实 loop 在 impl。 | `impl/iss_3d.hpp` |
| `src/narf_keypoint.cpp` | high | 是 | NARF range image interest、sparse/complete image、histogram 和 interest point 输出包含多个规模相关批量 loop。 | `narf_keypoint.h` |
| `src/sift_keypoint.cpp` | low | 否 | 只包含 `impl/sift_keypoint.hpp` 和注释掉的显式实例化；真实 scale-space loop 在 impl。 | `impl/sift_keypoint.hpp` |
| `src/smoothed_surfaces_keypoint.cpp` | low | 否 | 主要是 `SmoothedSurfacesKeypoint` 显式实例化，批量实现位于 `impl/smoothed_surfaces_keypoint.hpp`。 | `impl/smoothed_surfaces_keypoint.hpp` |
| `src/susan.cpp` | low | 否 | 只包含 `impl/susan.hpp` 和注释掉的显式实例化；真实 USAN loop 在 impl。 | `impl/susan.hpp` |
| `src/trajkovic_2d.cpp` | low | 否 | 只包含 `trajkovic_2d.h` 和注释掉的显式实例化；真实 response loop 在 `impl/trajkovic_2d.hpp`。 | `impl/trajkovic_2d.hpp` |
| `src/trajkovic_3d.cpp` | low | 否 | 只包含 `trajkovic_3d.h` 和注释掉的显式实例化；真实 response loop 在 `impl/trajkovic_3d.hpp`。 | `impl/trajkovic_3d.hpp` |
| `susan.h` | low | 否 | 公开类声明、参数接口和 intensity/normal 类型；真实 USAN 实现在 `impl/susan.hpp`。 | `impl/susan.hpp` |
| `trajkovic_2d.h` | low | 否 | 公开类声明、阈值/窗口参数和 comparator；批量 response loop 在 `impl/trajkovic_2d.hpp`。 | `impl/trajkovic_2d.hpp` |
| `trajkovic_3d.h` | low | 否 | 公开类声明和少量 inline normal helper；规模相关 organized response loop 在 `impl/trajkovic_3d.hpp`。 | `impl/trajkovic_3d.hpp` |

## 7. 二轮交接说明

- 第二轮筛选应读取本文件，并把所有 `high/mid` 文件作为初始候选基线逐项交代去向。
- 第二轮可以将 `high/mid` 降级为暂缓、不推荐或不单独实施，但必须说明源码证据和主成本覆盖原因。
- 第二轮可以补入 `low` 或初筛遗漏文件，但必须说明补入来源、证据和为什么没有扩展成重新全模块/全库筛选。
- 建议二轮优先顺序：先看 organized image/stencil 形态的 `impl/harris_2d.hpp`、`impl/trajkovic_2d.hpp`、`impl/trajkovic_3d.hpp` 和 `src/brisk_2d.cpp`；再评估 `src/narf_keypoint.cpp` 是否能拆出局部 component ablation（组件消融）；最后复核 search/Eigen 主导的 `mid` 候选是否有足够 production value（生产价值）。
- 第二轮输出应使用 `doc-rvv/library-screening/keypoints/keypoints-function-evaluation-queue.zh.md`，并按“建议进行 RVV 优化的文件 / 保留实施的候选文件 / 暂缓或不推荐考虑 RVV 优化的文件”三类组织。
- 本次只重做筛选文档；未修改 production 源码，未建立 `test-rvv` topic，未运行 bench（性能测试），未提交。
