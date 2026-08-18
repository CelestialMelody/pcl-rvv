# filters 模块 RVV 二轮筛选报告

本文档记录 `filters` 模块从文件级初筛结果转为 RVV 模块实施队列的第二轮筛选结论。

`filters` 是全库 RVV 优化中候选密度较高的模块。它包含大量逐点扫描、字段判断、几何筛选、organized 卷积、min/max 规约、mask 输出等形态，理论上与 RVV 比较匹配。但第一轮筛选只是文件级粗筛，主要回答“这个文件是否存在值得继续看的可向量化循环”，不等于判断“这个文件一定适合生产 RVV”。第二轮需要继续下钻到公开入口、函数族、主成本覆盖类型、测试可闭环性和语义风险。

本文档使用三类结论，不再使用旧分批术语：

- `建议进行 RVV 优化的文件`：公开入口主成本较清晰，或者可以建立 production / full diagnostic 闭环。
- `保留实施的候选文件`：有局部 RVV 点，但需要后续复筛、bench 诊断或已完成主题经验来证明收益。
- `暂缓或不推荐考虑 RVV 优化的文件`：公开声明头、薄 wrapper、显式实例化、伴随 src，或真实循环在其它主题中，不单独作为 RVV 修改点。

## 1. 输入依据

- 原始文件候选筛选：`doc-rvv/library-screening/modules/filters-file-candidate-screening.zh.md`
- 源码范围：`filters/include/pcl/filters/**`、`filters/src/**`
- 上游测试：`test/filters/*.cpp`
- 上游 benchmark：`benchmarks/filters/voxel_grid.cpp`、`benchmarks/filters/radius_outlier_removal.cpp`
- 表格文件路径说明：候选表中的文件名省略公共前缀 `filters/include/pcl/filters/`；`src/*` 文件保留 `src/` 前缀。

## 2. 二轮筛选统计

第一轮对 `filters` 做了全覆盖登记：总文件 `109`，其中 high `6`、mid `70`、low `33`。第二轮不把 high/mid 机械等同为实施对象，而是把 high/mid 的 `76` 个文件作为必查基线，并按源码下钻结果重新归类。本轮未从 high/mid 之外新增候选。

| 项目                            | 数量 | 说明                                               |
| ------------------------------- | ---: | -------------------------------------------------- |
| 第一轮 include 文件             |   75 | `filters/include/pcl/filters/**`                 |
| 第一轮 src 文件                 |   34 | `filters/src/**`                                 |
| 第一轮总文件                    |  109 | include + src                                     |
| 第一轮 high                     |    6 | 全部进入第二轮必查基线                            |
| 第一轮 mid                      |   70 | 全部进入第二轮必查基线                            |
| 第一轮 low                      |   33 | 本轮未发现必须补入的明显漏筛项                    |
| 第二轮初始候选基线              |   76 | high + mid 去重后数量                             |
| 新增补充候选                    |    0 | 本轮未从 high/mid 之外新增文件                    |
| 第二轮候选总数                  |   76 | 三分类合计仍为 76                                 |
| 建议进行 RVV 优化的文件         |    9 | 公开入口主成本较清晰，或已适合进入函数级评估      |
| 保留实施的候选文件              |   26 | 有局部 RVV 点，但需要复筛或 bench 诊断证明收益    |
| 暂缓或不推荐考虑 RVV 优化的文件 |   41 | 公开声明头、伴随实现、显式实例化或不单独实施文件  |
| 暂缓 / 删除 / 源码冲突          |    0 | 未删除 high/mid 候选，未发现源码冲突              |

## 3. 文件级变化理由

本节合并原先分散在“候选去向说明”“增删改列表”“文件级变化理由”中的内容。第二轮的核心变化不是扩大候选池，而是把第一轮的文件级粗筛结果转成可执行主题：真实循环位于 `impl/*.hpp` 的公开 `.h` 头并入对应实现主题；伴随 `.cpp`、显式实例化和薄封装不单独实施；有 RVV 点但主成本可能被 search、sort、map、Eigen、邻域访问或整点复制稀释的文件保留给后续复筛。

### 3.1 建议进行 RVV 优化的文件

这些文件是本轮认为可进入函数级评估和专项验证的建议优化队列。它们不再拆成不同实施组别；执行清单中的顺序只用于后续挑选下一项工作。

| 文件                               | 关键入口 / 函数族                                           | 主成本覆盖类型         | 二轮变化理由                                                                                                             |
| ---------------------------------- | ------------------------------------------------------------ | ---------------------- | ------------------------------------------------------------------------------------------------------------------------ |
| `impl/voxel_grid.hpp`              | `VoxelGrid::applyFilter`、`getMinMax3D`                    | `direct-main-path`     | 常用下采样入口；dense、float、非 indexed 的 min/max 和字段扫描路径规整，且已有上游 test / benchmark 支撑。               |
| `impl/convolution.hpp`             | organized row / column convolution                           | `direct-main-path`     | organized 图像式卷积，dense 行列内区有连续访存和固定边界策略；适合先限定 float-like 点类型和确定性边界策略。            |
| `impl/filter.hpp`                  | `removeNaNFromPointCloud`、`removeNaNNormalsFromPointCloud` | `direct-main-path`     | 通用 NaN / normal 清理入口；non-dense 标准字段线性扫描清晰，适合作为 filters 低风险样本。                               |
| `impl/filter_indices.hpp`          | `removeNaNFromPointCloud` indices-only                       | `direct-main-path`     | indices-only NaN 清理语义清晰，mask + 保序压缩输出可验证，是低风险 RVV 样本。                                            |
| `impl/passthrough.hpp`             | `PassThrough::applyFilter`                                  | `direct-main-path`     | 常用字段区间过滤入口；字段 load、区间比较、inlier / removed 双路保序输出适合 RVV，但需验证 negative 和 removed_indices。 |
| `impl/crop_box.hpp`                | `CropBox::applyFilter`                                      | `direct-main-path`     | 常用空间裁剪入口；identity transform、dense、标准 xyz 路径可形成多字段 mask + `vcompress` 主路径。                       |
| `impl/voxel_grid_covariance.hpp`   | leaf id 预计算、covariance voxel staging                     | `partial-preprocess`   | 前置扫描和 leaf id 预计算有空间，但 covariance、Eigen solver 和 searchable leaf 状态会稀释收益，需限定生产覆盖点。       |
| `impl/fast_bilateral.hpp`          | organized depth preprocessing / bilateral lattice            | `partial-preprocess`   | organized 图像式数据规模大，但可安全覆盖点主要是 finite z min/max 与替换；lattice 主体复杂，需用 full bench 判断收益。   |
| `impl/fast_bilateral_omp.hpp`      | OMP organized depth preprocessing / bilateral lattice        | `partial-preprocess`   | 与非 OMP 版本共享 z 预处理机会；OpenMP 与 RVV 边界需明确，lattice 主体不应直接承诺生产分流。                             |

### 3.2 保留实施的候选文件

这些文件仍在候选池内，但当前证据不足以直接进入建议优化队列。保留原因主要包括：主成本可能被 search、sort、map、Eigen solver、邻域不规则访问、采样状态或整点复制稀释；或者需要等待已完成主题经验后再复筛。

| 文件                                             | 主成本覆盖类型       | 保留原因 / 后续验证问题                                                                 |
| ------------------------------------------------ | -------------------- | ---------------------------------------------------------------------------------------- |
| `impl/approximate_voxel_grid.hpp`                | `partial-preprocess` | voxel 类路径，需先与标准 `voxel_grid` 的数据流和收益对比。                              |
| `impl/bilateral.hpp`                             | `diagnostic`    | 邻域权重和 intensity 计算有空间，但 search / 邻域组织和 `exp` 成本需要专项诊断。         |
| `impl/box_clipper3D.hpp`                         | `direct-main-path`   | 几何裁剪入口较规整，适合后续与 `crop_box`、`plane_clipper3D` 一起评估。                  |
| `impl/conditional_removal.hpp`                   | `direct-main-path`   | 简单字段条件可能成立，但通用条件树、多态比较、字段复制和 keep_organized 语义复杂。       |
| `impl/convolution_3d.hpp`                        | `diagnostic`    | 三维邻域和 search 关系复杂，需先证明 kernel loop 能代表真实入口主成本。                  |
| `impl/covariance_sampling.hpp`                   | `partial-preprocess` | 统计采样、矩阵构造和 Eigen 相关路径复杂，需隔离 solver / 采样状态成本。                  |
| `impl/crop_hull.hpp`                             | `diagnostic`    | 多边形 / 多面体判定控制流不规则，需限定 hull 形态并证明局部判定占主成本。                |
| `impl/extract_indices.hpp`                       | `diagnostic`    | 索引提取和整点复制占比高，需诊断 bitmap / set-difference 是否能带动 full 入口。           |
| `impl/farthest_point_sampling.hpp`               | `diagnostic`    | 采样状态和每轮依赖强，不适合作为直接生产主题；需证明距离更新和 max 查找是瓶颈。          |
| `impl/frustum_culling.hpp`                       | `direct-main-path`   | 6 平面几何筛选适合 RVV，但需单独处理 camera plane、far plane、negative 和输出保序语义。   |
| `impl/grid_minimum.hpp`                          | `partial-preprocess` | grid id / floor 预计算可评估，但 sort 和 per-cell min z 可能稀释整体收益。               |
| `impl/local_maximum.hpp`                         | `diagnostic`    | 局部邻域比较和 search / visited 状态主导，访存不如线性扫描规整。                         |
| `impl/median_filter.hpp`                         | `diagnostic`    | window gather 与 finite mask 有局部空间，但 median / `nth_element` 类操作主导。           |
| `impl/model_outlier_removal.hpp`                 | `tail-compress`      | threshold + compress 可诊断，但 `getDistancesToModel` 和模型多态通常是主成本。           |
| `impl/morphological_filter.hpp`                  | `diagnostic`    | octree / boxSearch 与邻域结果 min/max 交织，需证明 search-result 处理占比。               |
| `impl/normal_space.hpp`                          | `partial-preprocess` | normal bin id 可预计算，但 list/bin/random sampling 状态主导。                            |
| `impl/plane_clipper3D.hpp`                       | `direct-main-path`   | 平面裁剪是直接几何筛选，适合后续与 frustum / box clipper 一起复筛。                       |
| `impl/project_inliers.hpp`                       | `non-standalone`     | 主要分派到 sample_consensus 模型，实际投影热点不在 filters 文件本身。                    |
| `impl/pyramid.hpp`                               | `direct-main-path`   | organized 小 kernel 下采样有空间，需与 `src/pyramid.cpp` 和点类型分支共同评估。           |
| `impl/radius_outlier_removal.hpp`                | `tail-compress`      | nearestK / radius search 主导，尾段 `to_keep` 压缩只能给出成本上界。                      |
| `impl/sampling_surface_normal.hpp`               | `partial-preprocess` | min/max 和小分区 covariance 有空间，但 recursive partition、random、Eigen plane solve 主导。 |
| `impl/shadowpoints.hpp`                          | `direct-main-path`   | point + normal 几何判定可做直接主路径评估，但双输入 stride 和 cloud-out 语义需验证。       |
| `impl/statistical_outlier_removal.hpp`           | `tail-compress`      | KNN search 主导，统计和 threshold 尾段需证明占比。                                        |
| `impl/uniform_sampling.hpp`                      | `partial-preprocess` | leaf id 和 voxel center distance 有空间，但 map / per-leaf conflict update 主导。          |
| `impl/voxel_grid_occlusion_estimation.hpp`       | `diagnostic`    | ray traversal 状态机和 occupancy 访问不规则，需构造可归因诊断 case。                     |
| `src/voxel_grid_label.cpp`                       | `partial-preprocess` | 固定 `PointXYZRGBL`，但 sort、label histogram、`std::map` 和字段聚合主导。                |

### 3.3 暂缓或不推荐考虑 RVV 优化的文件

这些文件不是从记录中删除，而是不建议作为独立 RVV 主题。若后续推进对应主主题，需要在测试和文档中同时复核这些公开入口或伴随文件。

| 文件                                         | 合并到主题或原始去向                         | 暂缓、不推荐或不单独实施原因                                      |
| -------------------------------------------- | -------------------------------------------- | ----------------------------------------------------------------- |
| `approximate_voxel_grid.h`                   | `impl/approximate_voxel_grid.hpp`            | 公开声明头，真实循环在 impl。                                     |
| `bilateral.h`                                | `impl/bilateral.hpp`                         | 公开声明头，真实循环在 impl。                                     |
| `box_clipper3D.h`                            | `impl/box_clipper3D.hpp`                     | 公开声明头，真实循环在 impl。                                     |
| `conditional_removal.h`                      | `impl/conditional_removal.hpp`               | 公开声明头，真实循环在 impl。                                     |
| `convolution.h`                              | `impl/convolution.hpp`                       | 公开声明头，真实循环在 impl。                                     |
| `convolution_3d.h`                           | `impl/convolution_3d.hpp`                    | 公开声明头，真实循环在 impl。                                     |
| `covariance_sampling.h`                      | `impl/covariance_sampling.hpp`               | 公开声明头，真实循环在 impl。                                     |
| `crop_box.h`                                 | `impl/crop_box.hpp`                          | 公开声明头，真实循环在 impl。                                     |
| `crop_hull.h`                                | `impl/crop_hull.hpp`                         | 公开声明头，真实循环在 impl。                                     |
| `extract_indices.h`                          | `impl/extract_indices.hpp`                   | 公开声明头，真实循环在 impl。                                     |
| `fast_bilateral.h`                           | `impl/fast_bilateral.hpp`                    | 公开声明头，真实循环在 impl。                                     |
| `fast_bilateral_omp.h`                       | `impl/fast_bilateral_omp.hpp`                | 公开声明头，真实循环在 impl。                                     |
| `filter.h`                                   | `impl/filter.hpp`                            | 公开声明头，真实循环在 impl。                                     |
| `filter_indices.h`                           | `impl/filter_indices.hpp`                    | 公开声明头，真实循环在 impl。                                     |
| `frustum_culling.h`                          | `impl/frustum_culling.hpp`                   | 公开声明头，真实循环在 impl。                                     |
| `grid_minimum.h`                             | `impl/grid_minimum.hpp`                      | 公开声明头，真实循环在 impl。                                     |
| `median_filter.h`                            | `impl/median_filter.hpp`                     | 公开声明头，真实循环在 impl。                                     |
| `model_outlier_removal.h`                    | `impl/model_outlier_removal.hpp`             | 公开声明头，真实循环在 impl。                                     |
| `normal_refinement.h`                        | `impl/normal_refinement.hpp`                 | 公开声明头；impl 在第一轮为 low，本轮未发现需要补入的主路径证据。  |
| `normal_space.h`                             | `impl/normal_space.hpp`                      | 公开声明头，真实循环在 impl。                                     |
| `passthrough.h`                              | `impl/passthrough.hpp`                       | 公开声明头，真实循环在 impl。                                     |
| `plane_clipper3D.h`                          | `impl/plane_clipper3D.hpp`                   | 公开声明头，真实循环在 impl。                                     |
| `project_inliers.h`                          | `impl/project_inliers.hpp`                   | 公开声明头，真实循环 / 分派在 impl。                              |
| `pyramid.h`                                  | `impl/pyramid.hpp`、`src/pyramid.cpp`        | 公开声明头，需与 impl/src 共同评估。                              |
| `radius_outlier_removal.h`                   | `impl/radius_outlier_removal.hpp`            | 公开声明头，真实循环在 impl。                                     |
| `sampling_surface_normal.h`                  | `impl/sampling_surface_normal.hpp`           | 公开声明头，真实循环在 impl。                                     |
| `statistical_outlier_removal.h`              | `impl/statistical_outlier_removal.hpp`       | 公开声明头，真实循环在 impl。                                     |
| `uniform_sampling.h`                         | `impl/uniform_sampling.hpp`                  | 公开声明头，真实循环在 impl。                                     |
| `voxel_grid.h`                               | `impl/voxel_grid.hpp`                        | 公开声明头，真实循环在 impl。                                     |
| `voxel_grid_covariance.h`                    | `impl/voxel_grid_covariance.hpp`             | 公开声明头，真实循环在 impl。                                     |
| `voxel_grid_occlusion_estimation.h`          | `impl/voxel_grid_occlusion_estimation.hpp`   | 公开声明头，真实循环在 impl。                                     |
| `src/convolution.cpp`                        | `convolution` 主题                           | 主要是特化 / 显式实例化伴随文件，真实模板主体在 impl。             |
| `src/crop_box.cpp`                           | `crop_box` 主题                              | 伴随实例化 / 非主实现文件，优先看 impl。                           |
| `src/extract_indices.cpp`                    | `extract_indices` 主题                       | 伴随实例化 / 非主实现文件，优先看 impl。                           |
| `src/passthrough.cpp`                        | `passthrough` 主题                           | 伴随实例化 / 非主实现文件，优先看 impl。                           |
| `src/project_inliers.cpp`                    | `project_inliers` 主题                       | 伴随实例化 / 分派路径，优先看 impl 和 sample_consensus。            |
| `src/pyramid.cpp`                            | `pyramid` 主题                               | 需要与 `impl/pyramid.hpp` 共同筛选，不单独实施。                   |
| `src/radius_outlier_removal.cpp`             | `radius_outlier_removal` 主题                | 伴随实现，核心收益受 search / neighbor 影响。                       |
| `src/random_sample.cpp`                      | random sampling 主题                         | 随机采样状态主导，暂不单独作为 filters RVV 主题。                   |
| `src/statistical_outlier_removal.cpp`        | statistical outlier 主题                     | 伴随实现，核心收益受 search / neighbor 影响。                       |
| `src/voxel_grid.cpp`                         | `voxel_grid` 主题                            | PCLPointCloud2 / 实例化伴随文件，需随 `impl/voxel_grid.hpp` 复核。  |

### 3.4 相对第一轮筛选的主要调整

| 调整类型 | 文件或主题 | 变化理由 |
| -------- | ---------- | -------- |
| 公开声明头合并 | `voxel_grid.h`、`convolution.h`、`filter.h`、`passthrough.h`、`crop_box.h` 等 | `.h` 主要承载公开 API 声明，真实循环通常在对应 `impl/*.hpp`；RVV 修改应落在主实现文件或 helper 中。 |
| 伴随 src 合并 | `src/voxel_grid.cpp`、`src/convolution.cpp`、`src/passthrough.cpp`、`src/crop_box.cpp` 等 | 多数是显式实例化、薄封装或 PCLPointCloud2 伴随路径，不适合脱离主主题单独实施。 |
| high 候选保留但降为复筛 | `impl/voxel_grid_covariance.hpp`、`src/voxel_grid_label.cpp` | 有前置扫描或 leaf id 空间，但后续 covariance / Eigen / sort / map / label histogram 可能稀释收益。 |
| mid 候选提升为建议优化 | `impl/filter.hpp`、`impl/filter_indices.hpp`、`impl/passthrough.hpp`、`impl/crop_box.hpp` | 公开入口常用，循环形态清晰，输出顺序、indices / removed_indices、negative、NaN/Inf 等语义可通过专项测试闭环。 |
| 大循环但谨慎处理 | `impl/fast_bilateral.hpp`、`impl/fast_bilateral_omp.hpp` | organized 循环规模大，但安全生产覆盖点主要是 z 预处理；lattice、blur、插值和 OMP 主体需用 full bench 验证。 |
| 暂不新增外部文件 | 无 | 本轮没有从 76 个 high/mid 候选之外新增文件；low 文件若后续被 profile 或源码证据证明漏筛，可在复筛文档中补入。 |

## 4. 执行清单 / 状态表

### 4.1 建议进行 RVV 优化的文件

| 顺序 | 主题                         | 主文件                               | 当前状态 | 当前结论 / 下一步条件 |
| ---: | ---------------------------- | ------------------------------------ | -------- | --------------------- |
|    1 | `voxel_grid`                 | `impl/voxel_grid.hpp`                | 已完成   | 不再重复实施；仅在回归、板卡日志或文档口径变化时同步。 |
|    2 | `convolution`                | `impl/convolution.hpp`               | 已完成   | dense organized `PointXYZI` ignore / duplicate / mirror 行列方向 RVV 已完成。 |
|    3 | `filter_indices` / `filter`  | `impl/filter_indices.hpp`、`impl/filter.hpp` | 已完成   | non-dense 标准 `float x/y/z` indices-only 与 cloud-out RVV 已完成。 |
|    4 | `passthrough`                | `impl/passthrough.hpp`               | 已完成   | `PointT` identity indices + FLOAT32 字段区间过滤 RVV 已完成；subset / PCLPointCloud2 暂缓。 |
|    5 | `crop_box`                   | `impl/crop_box.hpp`                  | 已完成   | dense identity transform 的 xyz 区间裁剪已完成。 |
|    6 | `voxel_grid_covariance`      | `impl/voxel_grid_covariance.hpp`     | 已完成   | dense 标准 float xyz 的文件候选筛选 leaf id 预计算 RVV 已完成；covariance / Eigen 保持标量。 |
|    7 | `fast_bilateral`             | `impl/fast_bilateral.hpp`            | 已完成   | organized `PointXYZ` depth z 预处理 RVV 已完成；blur bench 诊断不接入生产。 |
|    8 | `fast_bilateral_omp`         | `impl/fast_bilateral_omp.hpp`        | 已完成   | 复用 `fast_bilateral` z 预处理 helper；OpenMP lattice 主体保持标量。 |

### 4.2 建议优化文件状态矩阵

| 主题                         | 函数级评估 | RVV 实现 | 专项测试 | bench | QEMU 对拍 | 反汇编 | 板卡闭环 | 主题文档 | 工作日志 |
| ---------------------------- | ---------- | -------- | -------- | ----- | --------- | ------ | -------- | -------- | -------- |
| `voxel_grid`                 | 完成       | 完成     | 完成     | 完成  | 完成      | 完成   | 完成     | 完成     | 完成     |
| `convolution`                | 完成       | 完成     | 完成     | 完成  | 完成      | 完成   | 完成     | 完成     | 完成     |
| `filter_indices` / `filter`  | 完成       | 完成     | 完成     | 完成  | 完成      | 完成   | 完成     | 完成     | 完成     |
| `passthrough`                | 完成       | 完成     | 完成     | 完成  | 完成      | 完成   | 完成     | 完成     | 完成     |
| `crop_box`                   | 完成       | 完成     | 完成     | 完成  | 完成      | 完成   | 完成     | 完成     | 完成     |
| `voxel_grid_covariance`      | 完成       | 完成     | 完成     | 完成  | 完成      | 完成   | 完成     | 完成     | 完成     |
| `fast_bilateral`             | 完成       | 完成     | 完成     | 完成  | 完成      | 完成   | 完成     | 完成     | 完成     |
| `fast_bilateral_omp`         | 完成       | 完成     | 完成     | 完成  | 完成      | 完成   | 完成     | 完成     | 完成     |

## 5. 与后续复筛文档的关系

本文档给出 `filters` 第二轮的三分类基线。其中 `保留实施的候选文件` 的 26 个文件已在 `doc-rvv/library-screening/filters/filters-retained-candidate-rescreen.zh.md` 中进一步复筛。后续保留候选复筛基于已完成主题的板卡结果和回退原因，将候选分为 `建议启动函数级评估` 与 `暂缓 / 不单独实施`；diagnostic / bench-only 是后续 topic 内证据路径，不是模块复筛固定队列。

因此，阅读顺序建议为：

1. 先读本文档，理解第一轮 high/mid 候选如何被整理成三类；
2. 再读保留候选复筛文档，理解 26 个保留候选在已完成主题经验之后如何继续分流；
3. 最后进入具体主题文档和 `test-rvv/filters/*/*-evaluation.zh.md`，核对函数级评估、测试、bench、反汇编和板卡闭环证据。
