# filters 模块 RVV 二轮筛选报告

`filters` 是全库 RVV 优化中候选密度较高的模块。它包含大量逐点扫描、字段判断、几何筛选、organized 卷积、min/max 规约、mask 输出等形态，理论上与 RVV 比较匹配。

但第一次筛选只是文件级粗筛。它判断“这个文件是否值得进入候选池”，不等于判断“这个文件一定适合生产 RVV”。例如一些文件虽然有循环和数学操作，但实际主成本可能在 search、sort、map、Eigen solver、随机采样、邻域不规则访问或整点复制上。

本文档记录 `filters` 模块从全库初筛结果转为模块实施队列的第二轮筛选结论。输入依据为 `doc-rvv/library-screening/modules/filters-function-triage.zh.md`，并结合当前源码、上游测试和 benchmark 覆盖进行修正。

## 1. 输入依据

- 原始模块 triage：`doc-rvv/library-screening/modules/filters-function-triage.zh.md`
- 源码范围：`filters/include/pcl/filters/**`、`filters/src/**`
- 上游测试：`test/filters/*.cpp`
- 上游 benchmark：`benchmarks/filters/voxel_grid.cpp`、`benchmarks/filters/radius_outlier_removal.cpp`
- 既有 RVV 工作流：`tmp/02ChatLogs/01-rvv-workflow-prompt-skill/prompt-v1.md`、`tmp/02ChatLogs/01-rvv-workflow-prompt-skill/skill-v1.md`

原始 triage 对 `filters` 做了全覆盖登记：总文件 `109`，其中 high `6`、mid `70`、low `33`。第二轮不重复全库筛选，只把候选文件转成可执行队列。

## 1.1 二轮筛选统计

- 第一轮目录统计：`include` `75`、`src` `34`，合计 `109`。
- 第一轮模块统计：high `6`，mid `70`，low `33`。
- 第一轮模块候选占比：`76/109 = 69.7%`。
- 第二轮候选全集：仍以第一轮 high/mid 的 `76` 个文件为输入，不额外删除候选全集。
- 第二轮候选文件：`35` 个，其中 `impl/*.hpp` `34` 个，`src/voxel_grid_label.cpp` `1` 个。
- 第二轮合并为主题关联文件：`41` 个，其中公开声明头 `31` 个、`src` 伴随实现 / 显式实例化 / 薄封装 `10` 个。
- 第二轮首批函数级筛选：`4` 个。
- 第二轮第二批靠前：`5` 个。
- 第二轮后续保留候选：`26` 个，暂不作为首批实现，但仍保留在模块队列中。
- 第二轮新增外部文件：`0` 个；本轮只有优先级提升、合并和暂缓，没有从 `76` 个候选之外新增文件。

## 1.2 二轮筛选出的候选文件

### 1.2.1 高优先级，首批进入函数级筛选（2）

| file_path                                            | 说明                                                                                                                  |
| ---------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------- |
| `filters/include/pcl/filters/impl/voxel_grid.hpp`  | 常用下采样入口；优先评估 `getMinMax3D` dense、float、非 indexed 路径，以及 `applyFilter` 前置扫描是否值得局部 RVV |
| `filters/include/pcl/filters/impl/convolution.hpp` | organized 行列卷积；优先评估 dense 行方向卷积，暂缓 non-dense、RGB packed 和列方向 stride 路径                        |

### 1.2.2 中高优先级，首批进入函数级筛选（2）

| file_path                                               | 说明                                                                    |
| ------------------------------------------------------- | ----------------------------------------------------------------------- |
| `filters/include/pcl/filters/impl/filter.hpp`         | 通用 NaN / normal 清理入口；适合评估标准字段 non-dense 线性扫描         |
| `filters/include/pcl/filters/impl/filter_indices.hpp` | indices-only NaN 清理入口；语义清晰，适合作为 filters 中低风险 RVV 样本 |

### 1.2.3 中优先级，第二批靠前（5）

| file_path                                                      | 说明                                                                     |
| -------------------------------------------------------------- | ------------------------------------------------------------------------ |
| `filters/include/pcl/filters/impl/passthrough.hpp`           | 常用字段区间过滤；需先评估 mask 压缩写、removed_indices 和 negative 语义 |
| `filters/include/pcl/filters/impl/crop_box.hpp`              | 空间区间裁剪；identity transform 路径较规整，带变换路径后置              |
| `filters/include/pcl/filters/impl/voxel_grid_covariance.hpp` | 前置扫描有潜在空间，但 covariance、eigen 和 searchable leaf 状态风险较高 |
| `filters/include/pcl/filters/impl/fast_bilateral.hpp`        | organized 图像式滤波，循环规模大；buffer 和 range 维度复杂，后续专项处理 |
| `filters/include/pcl/filters/impl/fast_bilateral_omp.hpp`    | OMP 版本 fast bilateral；需单独评估 RVV 与线程并行边界                   |

### 1.2.4 后续保留实施的候选文件（26）

这些文件不是被删除，而是因为测试成本、访存形态、算法不规则性、外部依赖或收益不确定，排在首批和第二批靠前文件之后。

| file_path                                                                | 二轮去向            | 主要原因                                                                  |
| ------------------------------------------------------------------------ | ------------------- | ------------------------------------------------------------------------- |
| `filters/include/pcl/filters/impl/approximate_voxel_grid.hpp`          | 后续保留            | voxel 类路径，需先完成标准 `voxel_grid` 后再比较数据流与收益            |
| `filters/include/pcl/filters/impl/bilateral.hpp`                       | 后续保留            | 滤波计算存在空间，但邻域/权重组织比 NaN 清理和 min/max 规约复杂           |
| `filters/include/pcl/filters/impl/box_clipper3D.hpp`                   | 后续保留            | 几何裁剪，适合后续与 `crop_box` 一起评估                                |
| `filters/include/pcl/filters/impl/conditional_removal.hpp`             | 后续保留            | 条件组合和字段比较多，mask 逻辑复杂，先不作为首批样本                     |
| `filters/include/pcl/filters/impl/convolution_3d.hpp`                  | 后续保留            | 三维邻域/搜索关系更复杂，先完成二维/organized convolution 样本            |
| `filters/include/pcl/filters/impl/covariance_sampling.hpp`             | 后续保留            | 统计采样与矩阵相关路径，数值语义和测试成本高于首批                        |
| `filters/include/pcl/filters/impl/crop_hull.hpp`                       | 后续保留 / 暂缓靠后 | 多边形/多面体判定控制流不规则，SIMD 适配度较弱                            |
| `filters/include/pcl/filters/impl/extract_indices.hpp`                 | 后续保留            | 以索引拷贝/提取为主，可能受内存带宽和 copy 行为主导                       |
| `filters/include/pcl/filters/impl/farthest_point_sampling.hpp`         | 后续保留            | 采样状态依赖较强，不适合作为 filters 首批 RVV 样本                        |
| `filters/include/pcl/filters/impl/frustum_culling.hpp`                 | 后续保留            | 几何判定较多，需单独拆分平面测试和输出压缩                                |
| `filters/include/pcl/filters/impl/grid_minimum.hpp`                    | 后续保留            | 网格最小值路径可评估，但热点和上游测试优先级低于首批                      |
| `filters/include/pcl/filters/impl/local_maximum.hpp`                   | 后续保留            | 局部邻域比较，访存不如线性扫描规整                                        |
| `filters/include/pcl/filters/impl/median_filter.hpp`                   | 后续保留            | median/排序类局部操作不适合直接作为首批 RVV                               |
| `filters/include/pcl/filters/impl/model_outlier_removal.hpp`           | 后续保留            | 模型距离判定可能可向量化，但依赖模型类型，需单独评估                      |
| `filters/include/pcl/filters/impl/morphological_filter.hpp`            | 后续保留            | 形态学邻域操作，需独立处理邻域访问和边界                                  |
| `filters/include/pcl/filters/impl/normal_space.hpp`                    | 后续保留            | normal 空间分桶和索引状态较多，先不做首批                                 |
| `filters/include/pcl/filters/impl/plane_clipper3D.hpp`                 | 后续保留            | 平面裁剪可向量化，但属于几何裁剪专项，排在 `crop_box` 后                |
| `filters/include/pcl/filters/impl/project_inliers.hpp`                 | 后续保留 / 暂缓靠后 | 当前文件主要分派到 sample_consensus 模型，实际热点不在 filters 文件本身   |
| `filters/include/pcl/filters/impl/pyramid.hpp`                         | 后续保留            | pyramid 数据流可能有收益，但需与 `filters/src/pyramid.cpp` 共同梳理     |
| `filters/include/pcl/filters/impl/radius_outlier_removal.hpp`          | 后续保留            | 依赖邻域搜索，RVV 覆盖点通常不在搜索主成本上                              |
| `filters/include/pcl/filters/impl/sampling_surface_normal.hpp`         | 后续保留            | normal / surface sampling 逻辑复杂，测试和收益边界需后续拆分              |
| `filters/include/pcl/filters/impl/shadowpoints.hpp`                    | 后续保留            | 几何关系判定为主，需单独评估                                              |
| `filters/include/pcl/filters/impl/statistical_outlier_removal.hpp`     | 后续保留            | 依赖邻域距离统计和搜索，RVV 覆盖面需进一步证明                            |
| `filters/include/pcl/filters/impl/uniform_sampling.hpp`                | 后续保留            | 采样 / voxel 类路径，排在标准 voxel grid 之后                             |
| `filters/include/pcl/filters/impl/voxel_grid_occlusion_estimation.hpp` | 后续保留            | occlusion estimation 几何状态较多，先不作为首批                           |
| `filters/src/voxel_grid_label.cpp`                                     | 后续保留 / 暂缓靠后 | 固定 `PointXYZRGBL`，含 sort、label 直方图和 `std::map`，只能局部 RVV |

### 1.2.5 合并到主题中、不单独作为 RVV 修改点的文件（41）

这些文件来自第一轮 `76` 个候选，不是简单删除；二轮将其合并到对应实现主题，或判定为伴随文件。若后续修改对应主题，需要在测试和文档中同时考虑这些公开入口或实例化文件。

| file_path                                                         | 二轮去向                                                | 理由                                                                   |
| ----------------------------------------------------------------- | ------------------------------------------------------- | ---------------------------------------------------------------------- |
| `filters/include/pcl/filters/approximate_voxel_grid.h`          | 合并到 `impl/approximate_voxel_grid.hpp`              | 公开声明头，真实循环在 impl                                            |
| `filters/include/pcl/filters/bilateral.h`                       | 合并到 `impl/bilateral.hpp`                           | 公开声明头，真实循环在 impl                                            |
| `filters/include/pcl/filters/box_clipper3D.h`                   | 合并到 `impl/box_clipper3D.hpp`                       | 公开声明头，真实循环在 impl                                            |
| `filters/include/pcl/filters/conditional_removal.h`             | 合并到 `impl/conditional_removal.hpp`                 | 公开声明头，真实循环在 impl                                            |
| `filters/include/pcl/filters/convolution.h`                     | 合并到 `impl/convolution.hpp`                         | 公开声明头，首批主题已覆盖 impl                                        |
| `filters/include/pcl/filters/convolution_3d.h`                  | 合并到 `impl/convolution_3d.hpp`                      | 公开声明头，真实循环在 impl                                            |
| `filters/include/pcl/filters/covariance_sampling.h`             | 合并到 `impl/covariance_sampling.hpp`                 | 公开声明头，真实循环在 impl                                            |
| `filters/include/pcl/filters/crop_box.h`                        | 合并到 `impl/crop_box.hpp`                            | 公开声明头，第二批靠前主题已覆盖 impl                                  |
| `filters/include/pcl/filters/crop_hull.h`                       | 合并到 `impl/crop_hull.hpp`                           | 公开声明头，真实循环在 impl                                            |
| `filters/include/pcl/filters/extract_indices.h`                 | 合并到 `impl/extract_indices.hpp`                     | 公开声明头，真实循环在 impl                                            |
| `filters/include/pcl/filters/fast_bilateral.h`                  | 合并到 `impl/fast_bilateral.hpp`                      | 公开声明头，第二批靠前主题已覆盖 impl                                  |
| `filters/include/pcl/filters/fast_bilateral_omp.h`              | 合并到 `impl/fast_bilateral_omp.hpp`                  | 公开声明头，第二批靠前主题已覆盖 impl                                  |
| `filters/include/pcl/filters/filter.h`                          | 合并到 `impl/filter.hpp`                              | 公开声明头，首批主题已覆盖 impl                                        |
| `filters/include/pcl/filters/filter_indices.h`                  | 合并到 `impl/filter_indices.hpp`                      | 公开声明头，首批主题已覆盖 impl                                        |
| `filters/include/pcl/filters/frustum_culling.h`                 | 合并到 `impl/frustum_culling.hpp`                     | 公开声明头，真实循环在 impl                                            |
| `filters/include/pcl/filters/grid_minimum.h`                    | 合并到 `impl/grid_minimum.hpp`                        | 公开声明头，真实循环在 impl                                            |
| `filters/include/pcl/filters/median_filter.h`                   | 合并到 `impl/median_filter.hpp`                       | 公开声明头，真实循环在 impl                                            |
| `filters/include/pcl/filters/model_outlier_removal.h`           | 合并到 `impl/model_outlier_removal.hpp`               | 公开声明头，真实循环在 impl                                            |
| `filters/include/pcl/filters/normal_refinement.h`               | 合并到 `impl/normal_refinement.hpp`                   | 公开声明头；impl 在第一轮为 low，二轮不单独提前                        |
| `filters/include/pcl/filters/normal_space.h`                    | 合并到 `impl/normal_space.hpp`                        | 公开声明头，真实循环在 impl                                            |
| `filters/include/pcl/filters/passthrough.h`                     | 合并到 `impl/passthrough.hpp`                         | 公开声明头，第二批靠前主题已覆盖 impl                                  |
| `filters/include/pcl/filters/plane_clipper3D.h`                 | 合并到 `impl/plane_clipper3D.hpp`                     | 公开声明头，真实循环在 impl                                            |
| `filters/include/pcl/filters/project_inliers.h`                 | 合并到 `impl/project_inliers.hpp`                     | 公开声明头，真实循环/分派在 impl                                       |
| `filters/include/pcl/filters/pyramid.h`                         | 合并到 `impl/pyramid.hpp` 和 `src/pyramid.cpp` 主题 | 公开声明头，需与 impl/src 共同看                                       |
| `filters/include/pcl/filters/radius_outlier_removal.h`          | 合并到 `impl/radius_outlier_removal.hpp`              | 公开声明头，真实循环在 impl                                            |
| `filters/include/pcl/filters/sampling_surface_normal.h`         | 合并到 `impl/sampling_surface_normal.hpp`             | 公开声明头，真实循环在 impl                                            |
| `filters/include/pcl/filters/statistical_outlier_removal.h`     | 合并到 `impl/statistical_outlier_removal.hpp`         | 公开声明头，真实循环在 impl                                            |
| `filters/include/pcl/filters/uniform_sampling.h`                | 合并到 `impl/uniform_sampling.hpp`                    | 公开声明头，真实循环在 impl                                            |
| `filters/include/pcl/filters/voxel_grid.h`                      | 合并到 `impl/voxel_grid.hpp`                          | 公开声明头，首批主题已覆盖 impl                                        |
| `filters/include/pcl/filters/voxel_grid_covariance.h`           | 合并到 `impl/voxel_grid_covariance.hpp`               | 公开声明头，第二批靠前主题已覆盖 impl                                  |
| `filters/include/pcl/filters/voxel_grid_occlusion_estimation.h` | 合并到 `impl/voxel_grid_occlusion_estimation.hpp`     | 公开声明头，真实循环在 impl                                            |
| `filters/src/convolution.cpp`                                   | 合并到 `convolution` 主题                             | 主要是特化/显式实例化伴随文件，真实模板主体在 impl                     |
| `filters/src/crop_box.cpp`                                      | 合并到 `crop_box` 主题                                | 伴随实例化 / 非主实现文件，优先看 impl                                 |
| `filters/src/extract_indices.cpp`                               | 合并到 `extract_indices` 主题                         | 伴随实例化 / 非主实现文件，优先看 impl                                 |
| `filters/src/passthrough.cpp`                                   | 合并到 `passthrough` 主题                             | 伴随实例化 / 非主实现文件，优先看 impl                                 |
| `filters/src/project_inliers.cpp`                               | 合并到 `project_inliers` 主题                         | 伴随实例化 / 分派路径，优先看 impl 和 sample_consensus                 |
| `filters/src/pyramid.cpp`                                       | 合并到 `pyramid` 主题                                 | 需要与 `impl/pyramid.hpp` 共同筛选，不单独作为首批                   |
| `filters/src/radius_outlier_removal.cpp`                        | 合并到 `radius_outlier_removal` 主题                  | 伴随实现，核心收益受 search/neighbor 影响                              |
| `filters/src/random_sample.cpp`                                 | 合并到 random sampling 主题 / 后置                      | 随机采样状态主导，暂不单独作为 RVV 首批                                |
| `filters/src/statistical_outlier_removal.cpp`                   | 合并到 statistical outlier 主题                         | 伴随实现，核心收益受 search/neighbor 影响                              |
| `filters/src/voxel_grid.cpp`                                    | 合并到 `voxel_grid` 主题                              | PCLPointCloud2 / 实例化伴随文件，需随 `impl/voxel_grid.hpp` 一起验证 |

## 1.3 filters RVV 主题执行清单

| 执行顺序 | 主题                            | 主实现文件                                                                                               | 函数级评估文档                                                                    | 主题文档                                            | 当前状态 | 下一步动作                                                                                                                                                                                                                            |
| -------- | ------------------------------- | -------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------- | --------------------------------------------------- | -------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 1        | `voxel_grid`                  | `filters/include/pcl/filters/impl/voxel_grid.hpp`                                                      | `test-rvv/filters/voxel_grid/voxel_grid-evaluation.zh.md`                       | `doc-rvv/filters/voxel_grid-RVV.zh.md`            | 已完成   | 不再重复实施；仅在回归或板卡日志更新时同步文档                                                                                                                                                                                        |
| 2        | `convolution`                 | `filters/include/pcl/filters/impl/convolution.hpp`                                                     | `test-rvv/filters/convolution/convolution-evaluation.zh.md`                     | `doc-rvv/filters/convolution-RVV.zh.md`           | 已完成   | dense organized `PointXYZI` ignore / duplicate / mirror 行列方向 RVV 已完成；列方向旧 `0.36x` 问题已修正，新版板卡 ignore 列约 `3.60x`、duplicate/mirror 列约 `3.85x` / `3.83x`，上游 `test_convolution` std/RVV 对拍通过 |
| 3        | `filter_indices` / `filter` | `filters/include/pcl/filters/impl/filter_indices.hpp`、`filters/include/pcl/filters/impl/filter.hpp` | `test-rvv/filters/filter_indices/filter_indices-evaluation.zh.md`               | `doc-rvv/filters/filter_indices-RVV.zh.md`        | 已完成   | non-dense 标准 `float x/y/z` indices-only 与 cloud-out RVV 已完成；板卡约 `2.35x` / `2.21x` / `1.62x`，normals 已尝试但因退化回退 Std                                                                                         |
| 4        | `passthrough`                 | `filters/include/pcl/filters/impl/passthrough.hpp`                                                     | `test-rvv/filters/passthrough/passthrough-evaluation.zh.md`                     | `doc-rvv/filters/passthrough-RVV.zh.md`           | 已完成   | 已实现 `PointT` identity indices + FLOAT32 字段区间过滤 RVV；`PCLPointCloud2` 与显式 subset indices 暂缓并记录原因                                                                                                                |
| 5        | `crop_box`                    | `filters/include/pcl/filters/impl/crop_box.hpp`                                                        | `test-rvv/filters/crop_box/crop_box-evaluation.zh.md`                           | `doc-rvv/filters/crop_box-RVV.zh.md`              | 已完成   | dense identity transform 的 xyz 区间裁剪已实现；板卡主路径约 `2.09x`~`3.08x`，fallback case 约 `1.00x`                                                                                                                          |
| 6        | `voxel_grid_covariance`       | `filters/include/pcl/filters/impl/voxel_grid_covariance.hpp`                                           | `test-rvv/filters/voxel_grid_covariance/voxel_grid_covariance-evaluation.zh.md` | `doc-rvv/filters/voxel_grid_covariance-RVV.zh.md` | 已完成   | dense 标准 float xyz 的 first-pass leaf id 预计算 RVV 已完成；covariance、eigen、searchable leaf 状态保持标量；板卡主路径约 `1.46x`~`1.52x`，fallback case 保持语义一致                                                           |
| 7        | `fast_bilateral`              | `filters/include/pcl/filters/impl/fast_bilateral.hpp`                                                  | `test-rvv/filters/fast_bilateral/fast_bilateral-evaluation.zh.md`               | `doc-rvv/filters/fast_bilateral-RVV.zh.md`        | 已完成   | organized `PointXYZ` depth z 预处理 RVV 已完成；finite z min/max 与 non-finite z 替换命中 RVV；放大到 320x240 后板卡主路径约 `1.04x`~`1.10x`；data/buffer blur bench-diagnosis RVV 实验为 `0.95x`，生产 blur 保持标量         |
| 8        | `fast_bilateral_omp`          | `filters/include/pcl/filters/impl/fast_bilateral_omp.hpp`                                              | `test-rvv/filters/fast_bilateral_omp/fast_bilateral_omp-evaluation.zh.md`       | `doc-rvv/filters/fast_bilateral_omp-RVV.zh.md`    | 已完成   | 复用 `fast_bilateral` z 预处理 RVV helper；finite z min/max 与 non-finite z 替换命中 RVV；OpenMP lattice 主体保持标量，板卡主路径约 `1.05x`~`1.16x`                                                                             |

## 1.4 首批已完成/待完成状态

| 主题                            | 函数级评估 | RVV 实现 | 专项测试 | bench | QEMU 对拍 | 反汇编 | 板卡闭环 | 主题文档 | 工作日志 |
| ------------------------------- | ---------- | -------- | -------- | ----- | --------- | ------ | -------- | -------- | -------- |
| `voxel_grid`                  | 完成       | 完成     | 完成     | 完成  | 完成      | 完成   | 完成     | 完成     | 完成     |
| `convolution`                 | 完成       | 完成     | 完成     | 完成  | 完成      | 完成   | 完成     | 完成     | 完成     |
| `filter_indices` / `filter` | 完成       | 完成     | 完成     | 完成  | 完成      | 完成   | 完成     | 完成     | 完成     |
| `passthrough`                 | 完成       | 完成     | 完成     | 完成  | 完成      | 完成   | 完成     | 完成     | 完成     |
| `crop_box`                    | 完成       | 完成     | 完成     | 完成  | 完成      | 完成   | 完成     | 完成     | 完成     |
| `voxel_grid_covariance`       | 完成       | 完成     | 完成     | 完成  | 完成      | 完成   | 完成     | 完成     | 完成     |
| `fast_bilateral`              | 完成       | 完成     | 完成     | 完成  | 完成      | 完成   | 完成     | 完成     | 完成     |
| `fast_bilateral_omp`          | 完成       | 完成     | 完成     | 完成  | 完成      | 完成   | 完成     | 完成     | 完成     |

## 2. 原始 `76` 个候选的二轮去向说明

第一轮的 `76` 个候选没有被随意丢弃，而是被分成四类：

1. 首批进入函数级筛选：`4` 个。
2. 第二批靠前：`5` 个。
3. 后续保留候选：`26` 个。
4. 合并到主题或伴随文件：`41` 个。

这四类加起来仍然是 `76` 个文件，没有新增外部文件，也没有遗漏第一轮候选。

### 2.1 被放入首批和第二批靠前的原因

- `voxel_grid.hpp`、`convolution.hpp`：存在明确的大循环或大规模线性扫描，且有现成测试 / benchmark 支撑，适合先做函数级拆分。
- `filter.hpp`、`filter_indices.hpp`：线性 NaN / normal 清理，语义清晰，风险低，适合作为 filters 的低风险样本。
- `passthrough.hpp`、`crop_box.hpp`：是常用入口，但需要先确认 mask 压缩、removed_indices、transform 分支等行为，故排在首批之后。
- `voxel_grid_covariance.hpp`、`fast_bilateral.hpp`、`fast_bilateral_omp.hpp`：有向量化空间，但数据流和边界更复杂，先保留为第二批靠前。

### 2.2 被合并到主题或伴随文件的原因

这些文件不是因为“不需要”，而是因为它们本身不是最合适的首个 RVV 实施点：

- 公开 `.h` 头：主要是 API 声明，真正的实现逻辑在 `impl/*.hpp`。
- 伴随 `.cpp` 文件：很多只是显式实例化、薄封装或与主实现协同，不适合作为独立 RVV 目标。
- 主题伴随文件：例如 `pyramid.cpp`、`voxel_grid.cpp`、`convolution.cpp`，要和对应 `impl` 一起看，不能单独作为首轮实施文件。

### 2.3 被后续保留的原因

这部分文件仍然是候选，只是不作为当前首轮实施点。常见原因包括：

- 依赖 sort、map、搜索或 eigen 分解，RVV 覆盖面有限；
- 邻域搜索 / 几何判定 / 采样状态复杂；
- 访存不规整，收益需要更强证据；
- 测试和验证成本高于首批样本。

### 2.4 本轮没有新增外部文件的说明

二轮筛选没有从 `76` 个候选之外新增文件。所谓“新增”只体现在优先级变化和主题合并方式上，不是额外扩大候选池。

## 3. 二轮筛选口径

| 维度       | 判断口径                                                                                   |
| ---------- | ------------------------------------------------------------------------------------------ |
| 数据布局   | 连续 PointCloud、AoS 点字段、PCLPointCloud2 字节步进、organized 图像式布局、indices gather |
| 热点可能性 | 是否是常用滤波入口、是否已有 benchmark、是否在典型点云管线中反复调用                       |
| RVV 适配度 | 是否有大规模线性扫描、规整 float 字段、可条带化 load/store、可用 mask 表达分支             |
| 测试可行性 | 是否已有上游单测，是否容易构造 std/RVV 对拍与 bench                                        |
| 风险       | 是否依赖 sort/map/search/eigen 分解、是否涉及公开模板头、是否改变 NaN/Inf 或字段拷贝语义   |

上游原始测试不是每个 filters RVV 主题的强制项：若目标函数没有直接对应的上游测试，或上游测试覆盖范围远大于当前函数，可以在函数级评估中说明不强制新增。若上游测试源码或 CMake 已有运行参数要求，应优先复用仓库 `test/` 中已有 PCD / txt / xml 等数据，通过专项 Makefile 的 `UPSTREAM_TEST_ARGS` 指向原路径，不复制测试数据到每个 `test-rvv` 专项目录；若测试需要输出文件或临时目录，Makefile 应提供可覆盖变量并确保目录存在。缺少运行参数、裸 `tee` 日志文件或当前 Makefile 未补齐库路径，不应直接写成环境阻塞。

## 3. 相对原始 triage 的增删改列表

### 3.1 合并处理

| 原始条目                                                                                                                                                             | 二轮结论                                                              | 理由                                                                                                                              |
| -------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------- |
| `filters/include/pcl/filters/voxel_grid.h` + `filters/include/pcl/filters/impl/voxel_grid.hpp` + `filters/src/voxel_grid.cpp`                                  | 合并为 `voxel_grid` 主题，执行文件以 `impl/voxel_grid.hpp` 为主   | `.h` 是公开声明，`src/voxel_grid.cpp` 主要实例化，真实热点循环在 `impl/voxel_grid.hpp`；实现 RVV 时必须保持公开 API 不变    |
| `filters/include/pcl/filters/voxel_grid_covariance.h` + `filters/include/pcl/filters/impl/voxel_grid_covariance.hpp` + `filters/src/voxel_grid_covariance.cpp` | 合并为 `voxel_grid_covariance` 主题，暂列后续队列                   | `.h` 是声明，`.cpp` 偏实例化；核心在 `impl`，但涉及 covariance、eigen solver、searchable leaf 结构，风险高于普通 voxel grid |
| `filters/include/pcl/filters/convolution.h` + `filters/include/pcl/filters/impl/convolution.hpp` + `filters/src/convolution.cpp`                               | 合并为 `convolution` 主题，执行文件以 `impl/convolution.hpp` 为主 | `src/convolution.cpp` 主要是特化/显式实例化补充；批量行列卷积路径在 `impl`                                                    |

### 3.2 删除 / 降级出首批执行队列

| 文件                                                                                 | 原始结论 | 二轮结论           | 理由                                                                                                                                                              |
| ------------------------------------------------------------------------------------ | -------- | ------------------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `filters/src/voxel_grid_label.cpp`                                                 | high     | 暂缓               | 固定 `PointXYZRGBL` 实现、含 sort 和 label 直方图/`std::map`，RVV 只能覆盖部分前置线性扫描；测试入口不如 `VoxelGrid<PointXYZ>` 直接，收益和风险不如首批队列 |
| `filters/include/pcl/filters/voxel_grid.h`                                         | high     | 不单独作为实现文件 | 公开声明头，实际 RVV 修改应落在 `impl/voxel_grid.hpp` 或内部 helper；单独优化该文件没有意义                                                                     |
| `filters/include/pcl/filters/voxel_grid_covariance.h`                              | high     | 不单独作为实现文件 | 公开声明头，实际热点在 `impl/voxel_grid_covariance.hpp`                                                                                                         |
| `filters/include/pcl/filters/impl/voxel_grid_covariance.hpp`                       | high     | 第二批             | 第一遍扫描和 per-leaf 累加有潜在空间，但后半段包含 covariance、inverse covariance、eigen 分解和 searchable leaf 状态，函数级拆分后再决定是否只覆盖前置扫描        |
| `filters/include/pcl/filters/impl/fast_bilateral.hpp` / `fast_bilateral_omp.hpp` | mid      | 后续专项           | organized 图像式滤波有大规模循环，但三维 buffer、range 分层和 OMP 分支较复杂；更适合作为 convolution/voxel_grid 之后的专项                                        |
| `filters/include/pcl/filters/impl/crop_hull.hpp`                                   | mid      | 暂缓               | 几何多边形/多面体判定，分支和不规则控制流多；SIMD 适配度不如简单线性过滤                                                                                          |
| `filters/include/pcl/filters/impl/project_inliers.hpp`                             | mid      | 暂缓               | 当前文件主要分派到 sample_consensus 模型，实际投影热点不在 filters 文件本身                                                                                       |

### 3.3 新增到首批执行队列

| 文件                                                    | 原始结论 | 二轮结论       | 理由                                                                                                                                                            |
| ------------------------------------------------------- | -------- | -------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `filters/include/pcl/filters/impl/filter_indices.hpp` | mid      | 第一批         | `removeNaNFromPointCloud` 的 indices-only 路径是线性扫描，语义清晰，可作为 filters 中低风险 RVV 样本；已有上游测试覆盖基础行为                                |
| `filters/include/pcl/filters/impl/filter.hpp`         | mid      | 第一批         | `removeNaNFromPointCloud` / `removeNaNNormalsFromPointCloud` 是通用前处理入口，循环形态清晰；可先覆盖 `PointXYZ` / normal 标准字段、大规模 non-dense 路径 |
| `filters/include/pcl/filters/impl/passthrough.hpp`    | mid      | 第一批候选补充 | 常用过滤入口，按指定 float 字段线性筛选；输出为 indices，适合 mask 压缩方向，但需要评估压缩写和 removed_indices 语义后再实现                                    |
| `filters/include/pcl/filters/impl/crop_box.hpp`       | mid      | 第二批靠前     | 线性点筛选，identity transform 分支可规整化；已有 `test_clipper.cpp`，但变换矩阵分支和 removed_indices 语义需要单独拆分                                       |

## 4. 文件级变化理由

### 4.1 `filters/include/pcl/filters/impl/voxel_grid.hpp`

- 数据布局：`PointCloud<PointT>` 是 AoS；`PCLPointCloud2` 的 `getMinMax3D` 是字节步进字段访问；`applyFilter` 使用 `indices_`，有 gather 可能。
- 热点可能性：VoxelGrid 是常用下采样入口，且已有 `benchmarks/filters/voxel_grid.cpp`。
- RVV 适配度：`getMinMax3D` 的 dense、非 indexed、float x/y/z 路径适合条带规约；`applyFilter` 的 voxel index 生成可部分条带化，但后续 sort 和 per-voxel 聚合不适合一次性整体 RVV。
- 测试可行性：`test/filters/test_filters.cpp` 覆盖 VoxelGrid；可单独构造大规模点云和复用上游 benchmark。
- 风险：公开模板头；NaN/Inf、distance field、indices、min_points_per_voxel、save_leaf_layout、downsample_all_data 均需保持原语义。
- 二轮结论：第一批，高优先；先做函数级评估，第一轮实现只考虑 `getMinMax3D` / dense 标准字段 / 非 indexed 或低风险子路径。

### 4.2 `filters/include/pcl/filters/impl/convolution.hpp`

- 数据布局：organized PointCloud，按行/列访问；行方向相对连续，列方向为大 stride；点类型可能是 `PointXYZI`、`RGB`、`PointXYZRGB` 等。
- 热点可能性：典型图像式滤波/平滑路径；上游有 `test/filters/test_convolution.cpp` 覆盖行列卷积。
- RVV 适配度：dense 行卷积适合滑窗/条带化；列卷积 stride 较大，需要先评估访存收益；RGB 专门特化需要谨慎。
- 测试可行性：已有确定性卷积结果测试，可迁移到 `test-rvv/filters/convolution`。
- 风险：边界策略 `ignore/duplicate/mirror`、non-dense 距离阈值、颜色字段语义和 OMP 并行路径都增加维护成本。
- 二轮结论：第一批，高优先；先做函数级评估，第一轮只考虑 dense、float-like 字段、行方向或小范围确定路径。

### 4.3 `filters/include/pcl/filters/impl/filter.hpp` 与 `filter_indices.hpp`

- 数据布局：AoS 点云，主要读取 xyz 或 normal_xyz，输出点云和/或 indices。
- 热点可能性：通用预处理入口，常在滤波前后清理 NaN。
- RVV 适配度：non-dense 扫描的 `isfinite(x/y/z)` 或 `isfinite(normal_x/y/z)` 可用 mask 生成；dense 路径主要是拷贝和顺序填充 indices，RVV 收益需要 bench 验证。
- 测试可行性：`test/filters/test_filters.cpp` 已覆盖基础行为，容易做专项 std/RVV 对拍。
- 风险：输出是压缩后的点云/indices，RVV mask 压缩写需要谨慎处理顺序、in-place 和 `is_dense` 标记。
- 二轮结论：第一批，中高优先；作为 filters 模块的低风险函数级评估样本。

### 4.4 `filters/include/pcl/filters/impl/passthrough.hpp`

- 数据布局：AoS，按 `filter_field_name_` 的 float offset 读取字段，输出 indices 和 removed_indices。
- 热点可能性：PassThrough 是常用裁剪过滤入口。
- RVV 适配度：字段读取 + 区间判断可向量化；但输出需要保持 inlier/removed 顺序，mask 压缩写是关键。
- 测试可行性：上游 `test/filters/test_filters.cpp` 有基础 PassThrough 用例，可补充大规模点云和 negative/extract_removed_indices 组合。
- 风险：字段名不存在、非 FLOAT32、`rgb` 警告、NaN/Inf 清理、negative 语义都必须保持。
- 二轮结论：第一批候选补充；函数级评估后再决定是否优先实现。

### 4.5 `filters/include/pcl/filters/impl/crop_box.hpp`

- 数据布局：AoS 点云，输出 indices。
- 热点可能性：常用空间裁剪入口。
- RVV 适配度：identity transform + dense 路径可做 xyz 区间 mask；带 transform/rotation 时每点矩阵变换类似 common transforms，但输出仍为压缩 indices。
- 测试可行性：`test/filters/test_clipper.cpp` 覆盖 CropBox。
- 风险：negative、removed_indices、transform/translation/rotation 组合较多。
- 二轮结论：第二批靠前，先不进入本轮首批函数级实现。

### 4.6 `filters/include/pcl/filters/impl/voxel_grid_covariance.hpp`

- 数据布局：AoS 点云，per-leaf 维护 mean/cov/centroid/eigen 数据。
- 热点可能性：NDT/协方差体素相关路径中可能有热点。
- RVV 适配度：前置 min/max 与 voxel index 生成可部分适配；per-leaf map 累加、协方差矩阵和特征分解不适合直接向量化。
- 测试可行性：`test/filters/test_filters.cpp` 有相关覆盖，但专项验证复杂度高。
- 风险：数值顺序、最小点数、协方差正定性、inverse covariance 和 searchable leaf 状态都较敏感。
- 二轮结论：第二批，先不作为 filters 首个实现对象。

## 5. 新的执行队列与优先级

| 队列 | 优先级 | 主题 / 文件                                                                                      | 当前动作 | 首轮 RVV 覆盖建议                                                                                                                               |
| ---- | ------ | ------------------------------------------------------------------------------------------------ | -------- | ----------------------------------------------------------------------------------------------------------------------------------------------- |
| 1    | 高     | `voxel_grid`：`filters/include/pcl/filters/impl/voxel_grid.hpp`                              | 已完成   | `getMinMax3D` dense、float x/y/z、非 indexed；`applyFilter` 仅评估 voxel index 生成，不直接承诺实现                                         |
| 2    | 高     | `convolution`：`filters/include/pcl/filters/impl/convolution.hpp`                            | 已完成   | dense organized 行卷积优先；列卷积和 non-dense 暂缓                                                                                             |
| 3    | 中高   | `filter_indices`：`filters/include/pcl/filters/impl/filter.hpp`、`impl/filter_indices.hpp` | 已完成   | `removeNaNFromPointCloud` 标准字段 non-dense RVV 已完成；`removeNaNNormalsFromPointCloud` 已评估后回退 Std                                  |
| 4    | 中     | `passthrough`：`filters/include/pcl/filters/impl/passthrough.hpp`                            | 已完成   | `PointT` identity indices + FLOAT32 字段区间判断 + 顺序压缩输出已实现；显式 subset indices 与 `PCLPointCloud2` 路径暂缓                     |
| 5    | 中     | `crop_box`：`filters/include/pcl/filters/impl/crop_box.hpp`                                  | 已完成   | dense identity transform 的 xyz 区间判断已实现；带 transform、non-dense、显式 subset 和 `PCLPointCloud2` 路径保持标量回退                     |
| 6    | 中     | `voxel_grid_covariance`：`filters/include/pcl/filters/impl/voxel_grid_covariance.hpp`        | 已完成   | dense 标准 float xyz 的 first-pass leaf id 预计算 RVV 已完成；cov/eigen 保持标量；板卡主路径约 `1.46x`~`1.52x`                              |
| 7    | 中     | `fast_bilateral` / `fast_bilateral_omp`                                                      | 已完成   | organized depth z 预处理 RVV 已完成；非 OMP blur bench-diagnosis 实验未达收益，生产 blur/lattice 保持标量；OMP 版本复用 z helper 并保持线程边界 |
| 8    | 低     | `project_inliers`、`crop_hull`、`voxel_grid_label` 等                                      | 暂缓     | 当前文件内 RVV 覆盖面有限或风险高                                                                                                               |

## 6. 后续候选复筛入口

截至第一实施波次 closeout，`1.2.1` 到 `1.2.3` 的靠前主题均已完成。后续保留候选不再直接按本二轮报告旧顺序进入实现，应先读取：

- `doc-rvv/library-screening/filters/filters-module-followup-rescreen.zh.md`

该复筛文档基于已完成主题的板卡真实性能、弱收益路径、bench-diagnosis 回退和暂缓原因，重新下钻 `1.2.4` 的 26 个后续保留候选，并给出下一阶段执行队列。
