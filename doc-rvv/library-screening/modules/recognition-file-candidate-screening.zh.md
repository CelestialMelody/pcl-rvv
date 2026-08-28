# recognition 模块 RVV 第一轮文件候选筛选报告

本文档重做 `recognition` 模块第一轮文件候选筛选。第一轮只回答“文件中是否存在值得第二轮继续下钻的 SIMD/RVV（单指令多数据 / RISC-V Vector）片段”，形成 `high/mid/low` 文件级基线；不进入实现、不建立 topic（单个 RVV 工作主题）测试资产、不运行 benchmark / bench（性能测试）。

## 1. 输入依据与范围

- 源码范围：`recognition/**` 下后缀为 `.h/.hpp/.c/.cc/.cpp/.cu` 的源码文件。
- 覆盖结果：总文件 `70`，已判定 `70`，覆盖率 `70/70`。
- 目录拆分：`include` 文件 `53`，`src` 文件 `17`。
- 路径显示：表中 `include` 文件省略公共前缀 `recognition/include/pcl/recognition/`，`src` 文件以 `src/` 开头显示。
- 第三方口径：`recognition/include/pcl/recognition/3rdparty/**` 下存在 `metslib` 头文件和说明文件，共 `11` 个文件；其中 `.hh` 不属于本轮计数后缀，因此第三方目录仅登记为依赖上下文，不纳入 70 个候选文件主线。
- 旧文档口径：同路径旧文档只作为 previous baseline（上一版基线）和覆盖检查参考；本轮判断依据改为当前源码中的入口、loop/helper（循环 / 辅助函数）、trip count（循环次数来源）、访存形态、语义风险和二轮可复核性。

## 2. 第一轮筛选口径

- `high`：文件内至少存在一个强批量 loop 或函数族，能定位到公开入口或主要调用路径，并同时具备较清楚的 trip count、RVV 访存适配点和可隔离风险。
- `mid`：存在可 SIMD/RVV 下钻片段，但主成本可能被 search/sort/tree/map/hash（搜索 / 排序 / 树 / 映射表 / 哈希表）、RANSAC、ICP、Eigen、IO、随机采样或容器状态稀释；需要第二轮判断是否只是 component ablation（组件消融，只验证局部片段）或保留候选。
- `low`：以声明、薄 wrapper、显式实例化、序列化、容器胶水、小规模固定计算、不规则状态机或第三方 / 外部算法调度为主；本轮不进入二轮初始候选基线。
- `high/mid` 是第二轮必须逐项交代去向的初始基线，不等于“建议优化”或最终实施队列。
- `fallback（回退路径）`、输出顺序、浮点边界、NaN/Inf、饱和 / 舍入、scatter（离散写入）冲突和测试 oracle（正确性参照）都留给第二轮和后续 topic 证据链确认。

## 3. 第一轮筛选统计

| 项目 | 数量 | 说明 |
| --- | ---: | --- |
| 源码文件总数 | 70 | `recognition/**` 的 `.h/.hpp/.c/.cc/.cpp/.cu` 文件。 |
| 已判定文件数 | 70 | 全量覆盖表一文件一行。 |
| high | 5 | 图像 / 模板匹配主路径或 modality（模态，输入特征类型）批量预处理主循环清楚。 |
| mid | 20 | 有可下钻片段，但主成本、状态、search/sort/tree（搜索 / 排序 / 树结构）或测试入口仍需第二轮压实。 |
| low | 45 | 本轮未发现足以进入二轮初始基线的独立 RVV 文件候选。 |
| high + mid | 25 | 第二轮必须读取并交代去向的文件候选基线。 |
| 候选占比 | 25/70 = 35.7% | 较旧文档 `32/70 = 45.7%` 收窄。 |

与旧文档相比，数量从 `high/mid/low = 5/27/38` 调整为 `5/20/45`。主要变化是：不再把 loop 数、数学项数或文件名热度作为充分依据；`impl/implicit_shape_model.hpp`、`src/face_detection/rf_face_detector_trainer.cpp` 从旧 high 降为 mid，`src/dotmod.cpp` 和 `color_gradient_dot_modality.h` 因可定位到直接模板匹配 / 图像 bin 主路径升为 high。

## 4. high 候选（5）

| 文件 | 关键入口 / loop | trip count 来源 | RVV 适配点 | 主要风险 | 第一轮判定理由 |
| --- | --- | --- | --- | --- | --- |
| `src/linemod.cpp` | `LINEMOD::matchTemplates` / `detectTemplates` 中 energy map、linearized map 和模板得分扫描；已有 `__SSE2__` 分支。 | `mem_size = (width / 8) * (height / 8)`、模板数、feature 数和 modality 数。 | 连续 `unsigned char` map 读取、按模板 feature 做字节加和、阈值比较和候选输出可用 mask / widening accumulation（扩宽累加）表达。 | 必须保持 SSE2 与标量路径分数语义、溢出 / 饱和行为、copy-back、local maximum 和 detection 输出顺序。 | 公开 LINEMOD 匹配主路径内已有手写 SIMD（单指令多数据）证据，RVV 下钻入口、trip count 和 fallback 边界都清楚。 |
| `color_gradient_modality.h` | `processInputData` 调 `computeMaxColorGradientsSobel`、`quantizeColorGradients`、`filterQuantizedColorGradients`、`erode`、`extractFeatures`。 | organized RGB 图像 `width * height`，Sobel 3x3 邻域，候选特征数。 | 行列连续图像访问、RGB 差分、sqrt / atan2 前后的梯度强度和方向量化、阈值 mask、3x3 邻域 filter。 | 浮点 sqrt/atan2 近阈值边界、边框处理、feature list sort / 去近邻会稀释尾段收益。 | modality 预处理覆盖 LINEMOD 模板生成和匹配输入主数据，像素级批量 loop 明确，二轮值得直接下钻。 |
| `surface_normal_modality.h` | `processInputData` 调 `computeAndQuantizeSurfaceNormals2`、`filterQuantizedSurfaceNormals`、`QuantizedMap::spreadQuantizedMap`。 | organized depth 图像 `width * height`，每像素 8 个邻域 offset。 | 深度转 `unsigned short`、局部 normal 计算、点积 / 叉积 / norm、方向量化和 map spread 可分段批量处理。 | 无效深度、边界像素、magic focal length、sqrt/atan2 量化边界、NaN/Inf 行为需要同链路对拍。 | 深度图主预处理 loop 大且同构，是 recognition 模板匹配链路里最清楚的 RVV 图像候选之一。 |
| `color_gradient_dot_modality.h` | DOTMOD color gradient 的 `computeMaxColorGradients`、`computeDominantQuantizedGradients`、`computeInvariantQuantizedMap`。 | binned image 的 `width * height`、量化 bin 数和 dominant gradient 轮数。 | 连续像素梯度、bin 直方图、mask 选择、invariant quantized map 的按位 OR / 最大值选择。 | `computeInvariantQuantizedMap` 会临时改写并恢复 `color_gradients_`，dominant 选择顺序、局部最大和状态恢复必须保持。 | DOTMOD 入口的图像预处理主循环可定位，虽然状态风险高，但比普通 support 文件更接近直接匹配输入主路径。 |
| `src/dotmod.cpp` | `DOTMOD::detectTemplates` / `matchTemplates` 对位置、模板、modality 和 bin 做响应累加。 | 图像扫描位置数、模板数、每模板 feature/bin 数、modality 数。 | `QuantizedMap` byte 数据按区域读取，bitwise AND / equality、response 累加、threshold mask 和 detection 压缩。 | `getSubMap` 拷贝成本、模板嵌套层级、输出 detection 顺序、阈值等价和分配行为需要拆分验证。 | 文件承载 DOTMOD 直接模板匹配主路径，批量比较 / 累加形态清楚，值得作为 high 进入第二轮。 |

## 5. mid 候选（20）

| 文件 | 关键入口 / loop | trip count 来源 | RVV 适配点 | 主要风险 | 第一轮判定理由 |
| --- | --- | --- | --- | --- | --- |
| `color_modality.h` | `quantizeColors`、`filterQuantizedColors`、`computeDistanceMap`、`extractFeatures`。 | organized RGB 图像 `width * height`、8 个 color bin、候选 feature 数。 | RGB 到 8 类 extrema 距离、3x3 histogram、distance map 初始化和双 pass min 更新。 | distance transform 有前后向依赖，`extractFeatures` 使用 list/sort 和距离约束选择。 | 有像素级同构算术，但主路径含顺序 DP 和容器选择，先列 mid。 |
| `crh_alignment.h` | `computeRollAngle` 中频域乘法归一化、FFT 后 peak 扫描。 | CRH histogram bin 数 `nbins_`、padding 后 `180` 个 bin、peak 数。 | complex multiply、magnitude / normalization、threshold peak scan 有局部 RVV 片段。 | `kiss_fft` 外部库和 sort 主导，bin 数固定偏小，峰值去重有顺序状态。 | 存在可 SIMD 片段，但不是清楚的大规模主成本，只保留二轮确认。 |
| `face_detection/rf_face_utils.h` | random forest feature handler 和 stats estimator 的 `evaluateFeature`、均值 / 协方差 / information gain / branch index 计算。 | training examples 数、node 样本数、feature 数。 | 样本级积分图 feature 评估、label mask、均值 / 协方差规约和 branch 分类。 | random feature fallback、log / determinant / Eigen 小矩阵、训练路径和树状态主导。 | 可下钻训练 / 评估子核明确，但生产价值和测试 oracle 需要二轮确认。 |
| `hv/hv_go.h` | GO hypothesis verification 的 cue 状态、octree / occupancy grid 参数和部分 inline 访问路径。 | recognition model 数、scene 点数、visible / explained index 数。 | explained / unexplained 标记、occupancy grid index 和 cue 累加可与实现文件联动评估。 | 公开头主要承载状态和参数，真实 loop 分散在 `impl/hv/hv_go.hpp`。 | 作为 `impl/hv/hv_go.hpp` 的伴随候选保留，非独立 high。 |
| `hv/occlusion_reasoning.h` | `filter` / occlusion reasoning 公开入口及 inline 投影相关路径。 | input cloud 点数、organized depth map 尺寸、indices 数。 | 点投影、深度比较和 keep mask 具备批量化形态。 | 头文件本身主要是接口，真实实现与深度图状态在 `impl/hv/occlusion_reasoning.hpp`。 | 与实现文件共同形成遮挡过滤候选，二轮需判断是否独立实施。 |
| `impl/cg/geometric_consistency.hpp` | `GeometricConsistencyGrouping::cluster` 中 correspondence 两两一致性和 consensus 计数。 | correspondence 数的 `O(n^2)` pair、每个 seed 的 consensus 数。 | 距离差、阈值比较、consensus mask 和局部计数可做 component ablation。 | sort、taken state、早停、RANSAC rejector 和输出聚类顺序主导。 | 有大 trip count 几何谓词，但状态和后续聚类风险使其列 mid。 |
| `impl/cg/hough_3d.hpp` | `train` vote 生成、`houghVoting` 场景 vote 计算和 min/max 扫描。 | model 点数、correspondence 数、reference frame 数。 | centroid 差、RF 变换、min/max、scene vote 写入前的连续/索引数组计算。 | Hough accumulator scatter、reference frame 查找、RANSAC 和动态容器主导。 | 可拆出 vote 生成子核，但主成本覆盖不清，列 mid。 |
| `impl/hv/greedy_verification.hpp` | model-wise voxel grid、radius search 后 explained / outlier indices 收集，`verify` 遍历模型。 | recognition model 数、model 点数、scene 点数、search result 数。 | per-point transform 后距离阈值、explained flags、sort/unique 前的 mask 生成。 | radiusSearch 和容器排序主导，输出 index 去重顺序敏感。 | 有局部批量判断，但 search 稀释明显，列 mid。 |
| `impl/hv/hv_go.hpp` | `initialize`、`computeClutterCue`、`updateExplainedVector`、`updateUnexplainedVector`、cue 累加。 | scene 点数、model 数、visible point 数、occupancy grid cell 数。 | dot/acos 前的 normal 比较、occupancy grid 索引、explained/unexplained 信息量规约。 | kd-tree search、map、OpenMP、simulated annealing 和多阶段状态主导。 | 局部 RVV 片段多，但必须先做主成本与状态边界评估。 |
| `impl/hv/hv_papazov.hpp` | Papazov HV 中模型解释点、冲突图和 support/penalty 计算。 | model 数、model 点数、scene 点数、explained_by_RM 列表。 | 点距离、阈值 mask、support/penalty 计数和 conflict edge 前置计算。 | radiusSearch、图结构、排序去重和模型间状态冲突主导。 | 可下钻但不宜第一轮 high，列 mid。 |
| `impl/hv/occlusion_reasoning.hpp` | `filter` 中点投影到 depth map、深度阈值和 keep indices 输出。 | input 点数、organized depth map 尺寸、smoothing window。 | `x/z`、`y/z` 投影、bounds mask、深度比较、indices 压缩。 | 除法、NaN/Inf、z-buffer scatter / min、输出顺序和 `copyPointCloud` 行为。 | 遮挡过滤入口具备批量 mask 形态，二轮可评估是否独立候选。 |
| `impl/implicit_shape_model.hpp` | `findObjects`、`calculateSigmas`、`calculateWeights`、`ISMVoteList::shiftMean` / `getDensityAtPoint`。 | cloud 点数、descriptor 数、cluster 数、vote 数、radius search 结果数。 | descriptor 距离、Gaussian 权重、pairwise distance、vote weighted sum 有规约形态。 | KMeans、search tree、feature estimator、random center、Eigen 和 `nth_element` 主导。 | 旧 high 降级；可 SIMD 子核存在，但主成本和复现性风险大。 |
| `impl/linemod/line_rgbd.hpp` | template load/add 时 bounding box、centroid、点云中心化和 LINEMOD template 创建。 | template cloud 点数、模板数、tar chunk 数。 | min/max/sum 规约、有限值 mask、点坐标平移可批量化。 | IO/TAR/PCD 读取、模板创建调用 `linemod_`、初始化路径非匹配主热区。 | 适合作为 LINEMOD 伴随候选，不单独列 high。 |
| `quantized_map.h` | `QuantizedMap::getSubMap`、`serialize` / `deserialize` 元素 loop，`spreadQuantizedMap` 声明。 | submap `width * height`、map 元素数。 | byte map 连续复制、submap 读取、spread 支撑可与 `src/quantizable_modality.cpp` 联动。 | 多数函数是容器/序列化，单独收益可能低；真正 spread 实现在 `.cpp`。 | LINEMOD/DOTMOD 共享支撑文件，列 mid 供二轮关联处理。 |
| `ransac_based/trimmed_icp.h` | `TrimmedICP::align` 中 source 点 transform、nearestKSearch、排序后 trimmed energy 累加。 | source 点数、trimmed overlap 点数。 | 点变换、距离数组填充、trimmed sum 规约。 | nearestKSearch、sort 和 rigid transform estimation 主导，最近邻 tie-break 敏感。 | 有每点同构计算，但主成本被搜索和排序稀释，列 mid。 |
| `src/cg/hough_3d.cpp` | `HoughSpace3D::vote` / `voteInt` / `findMaxima`。 | vote 数、Hough bin 数、邻域 bin 数。 | bin 扫描、neighbor max、threshold mask 和 vote 坐标量化。 | accumulator scatter 冲突、边界 bin、neighbor 遍历和 maxima 输出顺序。 | 与 `impl/cg/hough_3d.hpp` 共同构成 Hough 候选，列 mid。 |
| `src/face_detection/rf_face_detector_trainer.cpp` | `detectFaces` sliding window、`faceVotesClustering` vote 聚类和 heat map 写入。 | sliding window 数、forest leaves 数、vote 数、cluster 数。 | patch 有效性 mask、leaf 结果筛选、window 权重累加和 vote 坐标规约。 | forest evaluator、ICP、normal estimation、随机 / 训练路径和聚类状态主导。 | 旧 high 降级；可批量片段存在，但二轮必须先确认是否有生产入口价值。 |
| `src/quantizable_modality.cpp` | `QuantizedMap::spreadQuantizedMap` 横向 / 纵向 OR window。 | map `width * height`、spreading size。 | byte map unit-stride、bitwise OR、固定窗口传播。 | 边界复制、窗口大小、与 modality 主路径耦合，单独优化价值取决于调用频率。 | 共享支撑核清楚但非独立入口，列 mid。 |
| `src/ransac_based/model_library.cpp` | model add 时 oriented point pair signature 和 hash table 插入。 | full leaves 数、intersected leaves 数、point-pair 组合数。 | 点对距离 / 角度 signature、min/max、固定公式可批量化。 | hash table 插入、octree leaf 结构、采样策略和内存布局不规则。 | 有重型预处理子核，但 hash/tree 主导，列 mid。 |
| `src/ransac_based/obj_rec_ransac.cpp` | hypothesis test / normal-based test 中 full model leaves 变换、projection、normal dot 和评分。 | model leaves 数、scene octree 命中数、hypothesis 数。 | 点变换、距离 / normal dot、threshold mask、inlier 计数。 | RANSAC 随机采样、hash / octree 查询、graph / set 插入和输出顺序主导。 | 可 SIMD 片段明确但算法不规则，列 mid。 |

## 6. 全量文件覆盖表（70/70）

| 文件 | 优先级 | 是否候选 | 判断依据 | 二轮关注 |
| --- | --- | --- | --- | --- |
| `cg/correspondence_grouping.h` | `low` | 否 | correspondence grouping 公共接口声明，真实 loop 在实现头。 | 如二轮从实现头补入，只作为声明伴随文件。 |
| `cg/geometric_consistency.h` | `low` | 否 | geometric consistency 公共参数 / typedef / 声明为主。 | 关联 `impl/cg/geometric_consistency.hpp`。 |
| `cg/hough_3d.h` | `low` | 否 | Hough grouping 公共接口与状态声明为主。 | 关联 `impl/cg/hough_3d.hpp` 和 `src/cg/hough_3d.cpp`。 |
| `color_gradient_dot_modality.h` | `high` | 是 | DOTMOD 图像梯度、dominant bin 和 invariant map loop 明确。 | 状态恢复、量化边界和输出一致性。 |
| `color_gradient_modality.h` | `high` | 是 | LINEMOD RGB 梯度 Sobel / quantize / filter / feature extract 主预处理。 | sqrt/atan2 边界、feature 选择顺序。 |
| `color_modality.h` | `mid` | 是 | RGB extrema 量化、distance map 和 feature 选择含可下钻 loop。 | distance transform 依赖与 list/sort 稀释。 |
| `crh_alignment.h` | `mid` | 是 | CRH histogram 复数乘法、归一化和 peak scan 有局部 SIMD 点。 | FFT / sort 主导且 bin 数偏小。 |
| `dense_quantized_multi_mod_template.h` | `low` | 否 | 模板容器和序列化结构为主，缺少独立批量主循环。 | 如 LINEMOD/DOTMOD 下钻需要可作数据结构背景。 |
| `distance_map.h` | `low` | 否 | `DistanceMap` 数据容器、resize 和访问器为主。 | 真实计算在 modality 文件内。 |
| `dot_modality.h` | `low` | 否 | DOT modality 抽象接口为主。 | 关联 `color_gradient_dot_modality.h`。 |
| `dotmod.h` | `low` | 否 | DOTMOD 类声明、参数和 API 为主。 | 关联 `src/dotmod.cpp`。 |
| `face_detection/face_common.h` | `low` | 否 | training example / feature / tree node 结构和序列化为主。 | 若二轮评估 face detector，可作为状态结构背景。 |
| `face_detection/face_detector_data_provider.h` | `low` | 否 | data provider 接口声明和配置状态为主。 | 训练数据生成 loop 在 `.cpp`，但本轮未列候选。 |
| `face_detection/rf_face_detector_trainer.h` | `low` | 否 | trainer 参数、getter/setter 和少量 vote 拷贝 helper 为主。 | 关联 `src/face_detection/rf_face_detector_trainer.cpp`。 |
| `face_detection/rf_face_utils.h` | `mid` | 是 | random forest feature / stats estimator 有样本级评估和规约。 | 训练路径、随机 fallback、Eigen 统计语义。 |
| `hv/greedy_verification.h` | `low` | 否 | Greedy HV 公共接口、参数和状态声明为主。 | 关联 `impl/hv/greedy_verification.hpp`。 |
| `hv/hv_go.h` | `mid` | 是 | GO HV 的 cue / occupancy / explained 状态与实现头紧耦合。 | 是否只是实现头伴随文件。 |
| `hv/hv_papazov.h` | `low` | 否 | Papazov HV 公共接口和状态声明为主。 | 关联 `impl/hv/hv_papazov.hpp`。 |
| `hv/hypotheses_verification.h` | `low` | 否 | hypothesis verification 基类、参数和调度接口为主。 | 子类实现下钻时作为背景。 |
| `hv/occlusion_reasoning.h` | `mid` | 是 | 遮挡推理公开入口与投影 / depth map 状态直接相关。 | 与实现头合并评估独立价值。 |
| `impl/cg/correspondence_grouping.hpp` | `low` | 否 | 模板实现主要是调度 / clusterer 外壳，缺少独立批量核。 | 真实几何一致性和 Hough 文件另列。 |
| `impl/cg/geometric_consistency.hpp` | `mid` | 是 | correspondence 两两距离一致性和 consensus 计数。 | O(n^2) 谓词可消融，状态 / sort 风险需确认。 |
| `impl/cg/hough_3d.hpp` | `mid` | 是 | vote 生成、RF 变换和 scene vote 计算。 | accumulator scatter 和 search 稀释。 |
| `impl/hv/greedy_verification.hpp` | `mid` | 是 | model / scene 点解释、outlier 收集和 mask 判断。 | radiusSearch 与 sort/unique 主导。 |
| `impl/hv/hv_go.hpp` | `mid` | 是 | cue 计算、occupancy grid、explained/unexplained 统计。 | kd-tree、map、OpenMP、退火状态。 |
| `impl/hv/hv_papazov.hpp` | `mid` | 是 | support/penalty、模型冲突和 explained 点统计。 | search、冲突图和去重状态。 |
| `impl/hv/occlusion_reasoning.hpp` | `mid` | 是 | 点投影 depth map、bounds mask、深度阈值和 keep indices。 | 除法 / NaN、scatter 和输出顺序。 |
| `impl/implicit_shape_model.hpp` | `mid` | 是 | descriptor 距离、Gaussian 权重、vote 均值和 sigma 计算。 | KMeans、search、random 和 Eigen 稀释。 |
| `impl/linemod/line_rgbd.hpp` | `mid` | 是 | template cloud bounding box / centroid / 中心化 loop。 | IO / 模板加载路径是否热点。 |
| `impl/ransac_based/simple_octree.hpp` | `low` | 否 | octree insert / query 指针结构和递归式空间划分为主。 | 除非 profile 指向，不作初始候选。 |
| `impl/ransac_based/voxel_structure.hpp` | `low` | 否 | voxel 数据结构、hash / 容器管理为主。 | RANSAC 文件下钻时作为背景。 |
| `implicit_shape_model.h` | `low` | 否 | ISM 公共接口声明和参数为主。 | 关联 `impl/implicit_shape_model.hpp`。 |
| `linemod.h` | `low` | 否 | LINEMOD 公共 API、类型和模板容器声明为主。 | 关联 `src/linemod.cpp`。 |
| `linemod/line_rgbd.h` | `low` | 否 | LineRGBD 接口声明和组合状态为主。 | 关联 `impl/linemod/line_rgbd.hpp`。 |
| `mask_map.h` | `low` | 否 | `MaskMap` 容器、访问器和简单 map 存储为主。 | 计算实现看 `src/mask_map.cpp`。 |
| `point_types.h` | `low` | 否 | 点类型注册和字段声明。 | 无独立 RVV 入口。 |
| `quantizable_modality.h` | `low` | 否 | modality 抽象基类声明为主。 | 真实 spread 实现在 `src/quantizable_modality.cpp`。 |
| `quantized_map.h` | `mid` | 是 | `getSubMap` / serialize / deserialize 有 map 元素 loop，且共享支撑 high 候选。 | 是否合并进 LINEMOD/DOTMOD 主题。 |
| `ransac_based/auxiliary.h` | `low` | 否 | 固定小规模 3D helper、几何标量公式和工具类为主。 | 外层大循环在 `.cpp` / octree 文件。 |
| `ransac_based/bvh.h` | `low` | 否 | BVH tree、sort、递归 / pointer traversal 为主。 | 不作为第一轮 RVV 候选。 |
| `ransac_based/hypothesis.h` | `low` | 否 | hypothesis 数据结构和比较 / 序列化为主。 | RANSAC 主路径另列。 |
| `ransac_based/model_library.h` | `low` | 否 | model library 接口声明和状态为主。 | 关联 `src/ransac_based/model_library.cpp`。 |
| `ransac_based/obj_rec_ransac.h` | `low` | 否 | object recognition RANSAC 公共接口和参数为主。 | 关联 `src/ransac_based/obj_rec_ransac.cpp`。 |
| `ransac_based/orr_graph.h` | `low` | 否 | graph / set / edge 管理为主，访存不规则。 | 不作初始候选。 |
| `ransac_based/orr_octree.h` | `low` | 否 | octree class 声明和节点结构为主。 | `src/ransac_based/orr_octree.cpp` 未列二轮基线。 |
| `ransac_based/orr_octree_zprojection.h` | `low` | 否 | z-projection 类声明和集合状态为主。 | `.cpp` 作为支持路径暂不列基线。 |
| `ransac_based/rigid_transform_space.h` | `low` | 否 | transform space / entry 数据结构和 map-like 状态为主。 | RANSAC 下钻时背景文件。 |
| `ransac_based/simple_octree.h` | `low` | 否 | simple octree 接口和节点结构为主。 | 实现头也因 pointer traversal 降 low。 |
| `ransac_based/trimmed_icp.h` | `mid` | 是 | source 点 transform、nearest distance 填充和 trimmed energy。 | nearest search / sort 主导。 |
| `ransac_based/voxel_structure.h` | `low` | 否 | voxel structure 声明、hash key 和容器状态为主。 | 不作初始候选。 |
| `region_xy.h` | `low` | 否 | 矩形区域数据结构和访问器。 | 无独立批量核。 |
| `sparse_quantized_multi_mod_template.h` | `low` | 否 | sparse template feature 容器与序列化为主。 | LINEMOD/DOTMOD 下钻时作为数据结构背景。 |
| `surface_normal_modality.h` | `high` | 是 | organized depth 上 normal / quantize / filter / spread 主预处理。 | 深度无效值、量化边界和 border。 |
| `src/cg/geometric_consistency.cpp` | `low` | 否 | 显式实例化 / 编译单元胶水。 | 真实实现见 `impl/cg/geometric_consistency.hpp`。 |
| `src/cg/hough_3d.cpp` | `mid` | 是 | Hough bin vote、neighbor scan 和 maxima 查找。 | scatter、bin 边界和输出顺序。 |
| `src/dotmod.cpp` | `high` | 是 | DOTMOD 直接模板匹配响应累加。 | submap 拷贝、阈值和 detection 顺序。 |
| `src/face_detection/face_detector_data_provider.cpp` | `low` | 否 | 训练数据 IO、shuffle、patch 采样和 label 构造为主。 | 如 profile 指向训练数据生成，可二轮补入。 |
| `src/face_detection/rf_face_detector_trainer.cpp` | `mid` | 是 | sliding window detection、leaf 筛选、vote 聚类和 heat map。 | forest evaluator、ICP 和聚类状态主导。 |
| `src/hv/greedy_verification.cpp` | `low` | 否 | 显式实例化 / 编译单元胶水。 | 真实实现见 `impl/hv/greedy_verification.hpp`。 |
| `src/hv/hv_go.cpp` | `low` | 否 | 显式实例化 / 编译单元胶水。 | 真实实现见 `impl/hv/hv_go.hpp`。 |
| `src/hv/hv_papazov.cpp` | `low` | 否 | 显式实例化 / 编译单元胶水。 | 真实实现见 `impl/hv/hv_papazov.hpp`。 |
| `src/hv/occlusion_reasoning.cpp` | `low` | 否 | 显式实例化 / 编译单元胶水。 | 真实实现见 `impl/hv/occlusion_reasoning.hpp`。 |
| `src/implicit_shape_model.cpp` | `low` | 否 | 显式实例化 / 编译单元胶水。 | 真实实现见 `impl/implicit_shape_model.hpp`。 |
| `src/linemod.cpp` | `high` | 是 | LINEMOD 直接模板匹配主路径，已有 SSE2 分支。 | RVV / SSE2 / 标量分数等价和输出顺序。 |
| `src/mask_map.cpp` | `low` | 否 | `std::transform` mask difference 和小型 erode helper，主成本通常在 modality。 | 可随 modality 主题复查，不列独立基线。 |
| `src/quantizable_modality.cpp` | `mid` | 是 | `spreadQuantizedMap` 横纵向 byte OR window。 | 与 modality 主题合并还是独立评估。 |
| `src/ransac_based/model_library.cpp` | `mid` | 是 | oriented point-pair signature、距离 / 角度公式和 hash 前置计算。 | hash/tree 插入主导。 |
| `src/ransac_based/obj_rec_ransac.cpp` | `mid` | 是 | hypothesis test 中点变换、projection、normal dot 和 inlier 计数。 | RANSAC、octree/hash、graph/set 状态。 |
| `src/ransac_based/orr_octree.cpp` | `low` | 否 | octree build / traverse 指针追踪、节点分裂和空间查询为主。 | 只在 profile 指向 leaf 统计时重新考虑。 |
| `src/ransac_based/orr_octree_zprojection.cpp` | `low` | 否 | z-projection 使用 set / interval / pointer leaf 遍历，规则算术不足以独立支撑。 | 可作为 RANSAC 支持文件随主候选复查。 |

## 7. 二轮交接说明

- 第二轮函数评估队列应读取本文件，并把全部 `high` 与 `mid` 文件逐项交代去向。
- 第二轮应优先压实 `src/linemod.cpp`、`color_gradient_modality.h`、`surface_normal_modality.h`、`color_gradient_dot_modality.h`、`src/dotmod.cpp` 的公开入口、第一 RVV 目标、fallback 条件和 correctness oracle。
- 对 `mid` 文件，第二轮需要明确是 `direct-main-path（直接主路径）`、`partial-preprocess（部分预处理）`、`component ablation`、`non-standalone（非独立文件）` 还是暂缓。
- 第二轮可以补入本轮 `low` 文件，但必须写明补入来源、源码证据和为什么不扩展成重新全模块筛选。
- 建议下一份文档：`doc-rvv/library-screening/recognition/recognition-function-evaluation-queue.zh.md`，使用“建议进行 RVV 优化的文件 / 保留实施的候选文件 / 暂缓或不推荐考虑 RVV 优化的文件”三类队列。
- 本轮没有修改 production（生产）源码，没有建立 `test-rvv` topic，没有运行 bench，也没有提交。
