# sample_consensus 模块 RVV 第一轮文件级筛选报告

本文档记录 `sample_consensus` 模块的第一轮 RVV 文件级粗筛结果。第一轮只回答“文件中是否存在值得第二轮下钻复核的可 SIMD/RVV 片段”，不直接决定最终 RVV 实施队列。

## 1. 输入依据与范围

- 源码范围：`sample_consensus/**`
- 源码后缀：`.h`、`.hpp`、`.c`、`.cc`、`.cpp`、`.cu`
- 覆盖结果：总文件 `70`，已判定 `70`（`70/70` 全覆盖）。
- 目录拆分：`include` `54`，`src` `16`。
- 排除口径：第三方实现、生成文件、测试、文档和非主库路径只登记或排除，不纳入候选主线；本模块本轮未发现 `3rdparty/**` 源码文件。
- 重做口径：既有同路径文档只作为 previous baseline 和覆盖检查参考；本轮不沿用旧文档中的循环数量或数学项数量作为充分判断依据。
- 路径显示：候选表和覆盖表中的 `include` 文件省略公共前缀 `sample_consensus/include/pcl/sample_consensus/`，`src` 文件以 `src/` 开头显示。

## 2. 第一轮筛选口径

- 第一轮是文件级粗筛，判断标准是文件内是否存在可向量化循环、数学密集片段、批量字段访问、规约、mask/压缩、图像式 organized 遍历或可诊断的局部 SIMD/RVV 点。
- `high/mid` 是第二轮必须复核并交代去向的初始候选基线，不是最终实施全集。
- `low` 表示本轮未发现足以进入二轮基线的证据，不是永久排除；如果第二轮源码下钻发现明显漏判，可以补入并说明证据。
- 第一轮不承诺 RVV 覆盖公开入口主成本；主成本覆盖、测试可行性、fallback 条件和维护风险由第二轮筛选继续判断。
- 循环数量、数学项数量或关键词命中只能作为扫描线索，不能单独支撑 `high` / `mid`。候选必须定位到可复核的入口、loop/helper、trip count、RVV 适配点和主要风险。

评估维度：

| 维度       | 第一轮判断口径                                                                 |
| ---------- | ------------------------------------------------------------------------------ |
| 循环规模   | 是否随点数、像素数、邻域数、correspondence 数、bin 数或文件大小线性或更高增长 |
| 算术密度   | 是否包含乘加、距离、统计、几何谓词、权重、规约、矩阵/向量小公式或数学函数     |
| 访存模式   | 是否存在连续、固定 stride、AoS 字段、organized 行列、indices gather 等访问     |
| 分支复杂度 | 是否主要是简单谓词/mask，还是深分支、状态机、随机采样、map/hash 或搜索结构     |
| 语义风险   | 是否涉及非结合规约、阈值边界、NaN/Inf、输出顺序、indices 或用户自定义点类型    |
| 可验证性   | 是否能构造 std/RVV 对拍、边界 case、fallback case、QEMU 和板卡验证             |

优先级含义：

| 优先级 | 含义                                                                                 |
| ------ | ------------------------------------------------------------------------------------ |
| high   | 文件内存在明显批量循环或数学密集片段，且粗看具备较强 SIMD/RVV 评估价值              |
| mid    | 文件内存在可向量化片段，但主成本、数据布局、语义风险或测试入口需要第二轮继续确认    |
| low    | 以声明、薄 wrapper、调度、类型、构建胶水、小规模固定计算或明显不规则状态路径为主     |

## 3. 第一轮筛选统计

| 项目             | 数量 | 说明 |
| ---------------- | ---: | ---- |
| 源码文件总数     | 70 | `sample_consensus/**` 源码后缀文件；测试、文档和 benchmark 不纳入本表。 |
| 已判定文件数     | 70 | `70/70` |
| high             | 8 | 二轮必查 |
| mid              | 10 | 二轮必查 |
| low              | 52 | 已覆盖但不进入二轮初始基线 |
| high + mid       | 18 | 第一轮候选基线 |
| 候选占比         | 18/70 = 25.7% | high + mid / 源码文件总数 |

## 4. high 候选

| 文件 | 关键入口 / loop | trip count 来源 | RVV 适配点 | 主要风险 | 第一轮判定理由 |
| ---- | --------------- | --------------- | ------------ | -------- | ---------------- |
| `impl/sac_model_plane.hpp` | `getDistancesToModel`、`selectWithinDistance`、`countWithinDistanceStandard/RVV` 中按 `indices_` 逐点计算 `|ax + by + cz + d|`；已有 SSE/AVX/RVV 分支集中在 `countWithinDistance` | `indices_->size()`，通常等于输入云或用户索引子集规模 | AoS 点坐标经 `indices_` gather 后做三乘加、绝对值、阈值 mask 与计数规约；已有 `distRVV_f32m2` 可作为二轮入口锚点 | `indices_` gather 成本、float/double 距离语义、阈值边界、已有 SSE/AVX 路径和点类型 layout fallback | 公开模型最基础的平面距离/内点计数路径，循环迭代独立，已有 RVV guard 与测试资产痕迹，适合二轮优先复核主成本覆盖与现有 RVV 行为。 |
| `impl/sac_model_normal_plane.hpp` | `selectWithinDistanceStandard/RVV`、`countWithinDistanceStandard/RVV`、`getDistancesToModelStandard/RVV` 混合平面距离、法线夹角和曲率权重 | `indices_->size()`；每个索引访问点云与 normals | 点坐标、法线、曲率多字段 gather，距离核、`getAcuteAngle3D` 近似、权重融合、mask/compress 输出 inliers 与 error distances | 近似 `acos`、float vs double、曲率权重、mask 压缩顺序、点类型/法线 layout 条件 | 该文件已含多入口 RVV 实现和标量 fallback，覆盖 normal-plane 公开内点选择、计数和距离输出，二轮必须核对 correctness、fallback 与主成本边界。 |
| `impl/sac_model_sphere.hpp` | `getDistancesToModel`、`selectWithinDistance`、`countWithinDistanceStandard/RVV` 中逐点计算球心距离、内外半径阈值和计数 | `indices_->size()` | x/y/z gather 后计算平方距离、双阈值 mask、计数规约；已有 `sqr_distRVV_f32m2` 与 `countWithinDistanceRVV` | sqrt 仅在部分路径出现，阈值包含关系与 SSE 注释差异、float/double 误差、已有 SIMD/RVV 路径一致性 | 球模型的批量距离/计数路径规则、算术密度高且已有 RVV 分支，是二轮应优先确认的直接主路径候选。 |
| `impl/sac_model_circle.hpp` | `getDistancesToModel`、`selectWithinDistance`、`countWithinDistanceStandard/RVV` 中按索引计算 2D 圆距离和内外半径阈值 | `indices_->size()` | x/y gather、平方距离、双阈值 mask、计数规约；已有 `sqr_distRVV_f32m2` 与 `countWithinDistanceRVV` | 只使用 x/y 字段，阈值边界、sqrt 路径、点类型 field layout fallback | 圆模型存在清晰逐点距离核，并已有 RVV count 路径；二轮需确认是否只扩展 count 还是连带 select/getDistances。 |
| `impl/sac_model_line.hpp` | `getDistancesToModel`、`selectWithinDistance`、`countWithinDistance` 逐点计算点到线的 cross3 距离，`optimizeModelCoefficients` 调用 centroid/covariance | `indices_->size()` 或 `inliers.size()` | x/y/z gather 后做向量差、cross3、平方范数、阈值 mask/计数；可诊断 select/count/getDistances 的同构距离核 | Eigen `Vector4f` map、cross3 展开成本、浮点阈值、输出 inliers 顺序、优化阶段依赖 common centroid/covariance | 点到线距离循环是独立大循环，算术足够集中；虽然优化函数部分依赖 common 模块，距离/内点路径本文件内可独立复核。 |
| `impl/sac_model_stick.hpp` | `getDistancesToModel`、`selectWithinDistance`、`countWithinDistance` 按索引计算点到线段/棍模型距离和分层阈值 | `indices_->size()` | x/y/z gather、方向归一化后逐点 cross3、阈值 mask、计数和可压缩 inliers 输出 | 与 line 模型相近但包含半径上下限、双计数 `nr_i/nr_o` 和 outlier 惩罚距离，语义边界需对拍 | 文件包含明显逐点几何距离核，且与 line 模型可共享二轮分析模式；建议作为 high 候选下钻。 |
| `impl/sac_model_cylinder.hpp` | `getDistancesToModel`、`selectWithinDistance`、`countWithinDistance` 逐点计算到轴线距离、法线夹角、曲率权重；`optimizeModelCoefficients` 准备 inlier arrays | `indices_->size()` 和 `inliers.size()` | 点/法线 gather、dot、norm、`getAngle3D`、权重融合、early mask、计数/压缩；inlier x/y/z staging 可单独评估 | 法线依赖、sqrt/acos、early continue 分支、半径约束、LM 优化阶段由 Eigen solver 主导 | cylinder 模型内点判断是点云规模批量核，算术密度高；二轮应拆分距离/角度核和优化前 staging，避免把 Eigen solver 本体误列为 RVV 目标。 |
| `impl/sac_model_normal_sphere.hpp` | `selectWithinDistance`、`countWithinDistance`、`getDistancesToModel` 混合球面欧氏距离和法线角距离 | `indices_->size()` | 点/法线 gather、norm、`getAngle3D`、normal-distance 权重、阈值 mask 和计数 | early continue、法线方向锐角折叠、float/double 差异、缺少现成 RVV 分支 | 逐点 normal-sphere 距离核和 normal-plane/cylinder 形态相近，具备二轮复用价值；因无现成 RVV 分支，需优先核对语义风险。 |

## 5. mid 候选

| 文件 | 关键入口 / loop | trip count 来源 | RVV 适配点 | 主要风险 | 第一轮判定理由 |
| ---- | --------------- | --------------- | ------------ | -------- | ---------------- |
| `impl/sac_model_circle3d.hpp` | `getDistancesToModel`、`selectWithinDistance`、`countWithinDistance` 对每个点投影到圆平面并计算到圆的距离；另有 `optimizeModelCoefficients` inlier staging | `indices_->size()`、`inliers.size()` | x/y/z gather、dot、投影、normalize、norm、阈值 mask；inlier staging 可批量加载 | 每 lane normalize/divide 较重，double 计算较多，阈值和退化法线边界敏感 | 有明确逐点批量几何核，但 3D 圆投影语义比 plane/sphere/line 更复杂，先列 `mid` 给二轮判断是否适合作为独立主题。 |
| `impl/sac_model_ellipse3d.hpp` | `getDistancesToModel`、`selectWithinDistance`、`countWithinDistance` 每点变换到局部坐标并调用 `internal::dvec2ellipse`；`projectPoints` 逐 inlier 投影 | `indices_->size()`、`inliers.size()`、`projected_points.size()` | 局部坐标矩阵乘、距离阈值、inlier mask；部分 field copy 与投影写回可诊断 | `dvec2ellipse` 每点可能含迭代/分支，compute 阶段由 Eigen solver 主导，字段复制和输出语义复杂 | 文件中有大循环，但核心距离 helper 不够简单，且模型拟合使用 Eigen 特征求解；保留为二轮 `mid`，需先拆清可 RVV 的纯局部核。 |
| `impl/sac_model_cone.hpp` | `selectWithinDistance`、`countWithinDistance`、`getDistancesToModel` 逐点计算到 cone axis 的距离、实际半径和法线角 | `indices_->size()` | 点/法线 gather、dot/cross/norm、`tan` 相关半径、`getAngle3D`、mask 计数 | 角函数、法线依赖、几何退化、float/double 误差、model coefficient 语义复杂 | 有批量算术核，但 cone 几何比 cylinder 更复杂且缺少现成 RVV 分支；二轮应确认是否只做 component ablation。 |
| `impl/sac_model_torus.hpp` | `selectWithinDistance`、`countWithinDistance`、`getDistancesToModel` 按索引计算 torus 距离；`computeModelCoefficients` 小样本求解 | `indices_->size()`；compute 阶段只随固定 sample size 小规模变化 | 点/法线 gather、局部几何距离、阈值 mask；可能拆出每点距离子核 | torus 距离公式复杂，模型系数求解含小矩阵/SVD，法线和半径语义边界多 | 存在逐点距离循环，但主计算复杂度和验证难度高于常规几何模型，第一轮保留为 `mid`。 |
| `impl/sac_model_registration.hpp` | `getDistancesToModel`、`selectWithinDistance`、`countWithinDistance` 对 source/target correspondence 做 4x4 transform 和距离 | `indices_->size()`，并要求 `indices_` 与 `indices_tgt_` 同长 | source/target 双 gather、4x4 乘法、差值平方范数、阈值 mask/计数 | 双索引 gather、correspondence 顺序、SVD 求变换不在本文件可 RVV 化主核内、与 registration 模块已有主题重叠 | 逐 correspondence 距离判断是规则批量核，但入口与 registration 使用场景强耦合，二轮需判断是否归属 sample_consensus 还是 registration 主题。 |
| `impl/sac_model_registration_2d.hpp` | `getDistancesToModel`、`selectWithinDistance`、`countWithinDistance` 做 3D transform、相机投影、2D target 距离 | `indices_->size()`，并要求 source/target indices 同长 | source gather、矩阵乘、投影除法、2D 阈值 mask | uv[2] 负值分支、投影除法、target `u/v` 字段、双 indices 语义和 registration 主题边界 | 有逐 correspondence 计算，但控制流和数据布局更复杂，列为 `mid` 等二轮确认。 |
| `impl/msac.hpp` | `computeModel` 中每次迭代调用模型距离，然后对 `distances` 做 penalty sum 和最终 inlier 压缩 | RANSAC 迭代数 × `distances.size()` | 对连续 `std::vector<double>` 做 min/sum、threshold mask、indices 压缩；模型距离由其它文件提供 | 外层随机采样/早停状态机主导，浮点规约顺序，距离计算通常才是主成本 | 本文件自身有可 SIMD 的后处理循环，但不一定覆盖主成本；保留为 `mid`，二轮需先评估是否被模型距离核覆盖。 |
| `impl/mlesac.hpp` | `computeModel` 中 EM 循环计算 inlier probability、gamma 规约、log likelihood；另有 `computeMedianAbsoluteDeviation` 和 `getMinMax` 扫描点云 | `iterations_EM_ * indices_size`、`indices->size()` | 连续 probability 数组 exp/log、sum 规约、min/max 扫描点坐标 | `exp/log` 成本与近似误差、非结合规约、外层 RANSAC 状态、median 函数外部实现 | 文件内有连续数组数学循环和点云 min/max，但与随机迭代控制耦合；列 `mid` 供二轮判断是否做诊断型子核。 |
| `impl/rmsac.hpp` | `computeModel` 使用 `distances` 做阈值 inlier 计数、penalty 累加和最终 inlier 生成 | RANSAC 迭代数 × `distances.size()` | 连续 double distances 的 mask、sum/count、indices 压缩 | 外层随机采样和模型距离主导，浮点累加顺序，收益可能被虚调用/模型核稀释 | 有后处理 SIMD 形态，但不是独立几何主核，列 `mid` 供二轮复核。 |
| `sac.h` | `refineModel` 循环反复调用 `optimizeModelCoefficients`、`selectWithinDistance`，并比较新旧 inlier 序列 | `max_iterations`、`inliers_.size()` | 新旧 inlier 等值比较可批量化；真正距离/优化工作下沉到模型文件 | loop-carried 收敛状态、模型虚调用、阈值更新和 variance 依赖，独立 RVV 价值偏弱 | 基类 refine path 有规模相关循环，但主成本多在派生模型；列为 `mid` 仅要求二轮解释是否非独立实施。 |

## 6. 全量文件覆盖表

| 文件 | 优先级 | 是否候选 | 判断依据 | 关联实现 |
| ---- | ------ | -------- | -------- | -------- |
| `impl/lmeds.hpp` | low | 否 | `computeModel` 以随机采样、模型虚调用、距离排序/median 和控制状态为主；本文件未形成比派生模型距离核更清楚的独立 RVV 入口。 | `lmeds.h` |
| `impl/mlesac.hpp` | mid | 是 | EM probability、gamma 规约、log likelihood、MAD 和 min/max 扫描存在规模相关循环，但外层 RANSAC 状态和数学函数近似风险需要二轮确认。 | `mlesac.h` |
| `impl/msac.hpp` | mid | 是 | 对 `distances` 做 penalty sum、threshold inlier 计数和最终 inlier 压缩；模型距离计算由派生模型承担，独立主成本待确认。 | `msac.h` |
| `impl/prosac.hpp` | low | 否 | 主体是 PROSAC 采样池扩张、排序、binomial quantile 和 RANSAC 状态更新；可 SIMD 片段不清晰且控制流不规则。 | `prosac.h` |
| `impl/ransac.hpp` | low | 否 | `computeModel` 由随机采样、OpenMP 临界区/原子、模型虚调用和早停状态机主导；批量工作下沉到 `countWithinDistance`。 | `ransac.h` |
| `impl/rmsac.hpp` | mid | 是 | 对连续 distances 做阈值计数、penalty 累加和 inlier 生成；但主成本可能仍在模型距离核和 RANSAC 控制。 | `rmsac.h` |
| `impl/rransac.hpp` | low | 否 | Randomized RANSAC 控制路径以采样、局部投票和模型调用为主，未发现适合文件级二轮下钻的稳定批量核。 | `rransac.h` |
| `impl/sac_model_circle.hpp` | high | 是 | `getDistancesToModel`、`selectWithinDistance`、`countWithinDistance` 存在按 `indices_` 的 2D 圆距离/双阈值批量核，已有 RVV count helper。 | `sac_model_circle.h` |
| `impl/sac_model_circle3d.hpp` | mid | 是 | 3D 圆逐点投影和距离计算随 `indices_` 增长，但每 lane normalize/divide、double 语义和退化边界较重。 | `sac_model_circle3d.h` |
| `impl/sac_model_cone.hpp` | mid | 是 | cone 模型逐点距离、法线角和权重判断可批处理，但几何公式、角函数和法线依赖风险较高。 | `sac_model_cone.h` |
| `impl/sac_model_cylinder.hpp` | high | 是 | cylinder 内点/距离循环按 `indices_` 访问点和 normals，包含 dot/norm/angle/权重/阈值，具备明确二轮下钻价值。 | `sac_model_cylinder.h` |
| `impl/sac_model_ellipse3d.hpp` | mid | 是 | ellipse 逐点距离和投影循环存在，但 `dvec2ellipse`、Eigen solver、字段复制和投影语义复杂，需先拆分可 RVV 子核。 | `sac_model_ellipse3d.h` |
| `impl/sac_model_line.hpp` | high | 是 | line 距离、select、count 均按 `indices_` 做 cross3/squaredNorm/阈值，循环独立且主核清楚。 | `sac_model_line.h` |
| `impl/sac_model_normal_parallel_plane.hpp` | low | 否 | 主要继承 normal-plane 的距离/内点实现，文件自身只补模型有效性检查，不形成独立批量核。 | `sac_model_normal_parallel_plane.h` |
| `impl/sac_model_normal_plane.hpp` | high | 是 | normal-plane 已含 select/count/getDistances 的 RVV 与标量路径，按点、法线、曲率批量 gather 并做距离/角度/权重融合。 | `sac_model_normal_plane.h` |
| `impl/sac_model_normal_sphere.hpp` | high | 是 | normal-sphere 按 `indices_` 逐点计算欧氏距离、法线角和权重，形态接近 normal-plane/cylinder，适合二轮复核。 | `sac_model_normal_sphere.h` |
| `impl/sac_model_parallel_line.hpp` | low | 否 | 主要复用 line 模型距离/count/select，只补 axis 约束有效性；独立 RVV 点应归入 `impl/sac_model_line.hpp`。 | `sac_model_parallel_line.h` |
| `impl/sac_model_parallel_plane.hpp` | low | 否 | 主要转调 plane 的 select/count/getDistances 并做角度约束检查；批量核属于 `impl/sac_model_plane.hpp`。 | `sac_model_parallel_plane.h` |
| `impl/sac_model_perpendicular_plane.hpp` | low | 否 | 主要转调 plane 批量路径，文件自身角度有效性检查为小规模模型约束。 | `sac_model_perpendicular_plane.h` |
| `impl/sac_model_plane.hpp` | high | 是 | plane 距离/count/select 是按 `indices_` 的点云主循环，已有 SSE/AVX/RVV 路径和纯距离 RVV helper。 | `sac_model_plane.h` |
| `impl/sac_model_registration.hpp` | mid | 是 | 逐 correspondence 做 source transform、target 距离和阈值判断；双索引 gather 与 registration 归属需二轮确认。 | `sac_model_registration.h` |
| `impl/sac_model_registration_2d.hpp` | mid | 是 | 逐 correspondence 做 3D transform、相机投影和 2D 距离；投影分支和 target layout 风险较高。 | `sac_model_registration_2d.h` |
| `impl/sac_model_sphere.hpp` | high | 是 | sphere 距离/select/count 按 `indices_` 计算平方距离和双阈值，已有 RVV count helper。 | `sac_model_sphere.h` |
| `impl/sac_model_stick.hpp` | high | 是 | stick 距离/select/count 使用按点 cross3、阈值和双计数，和 line 模型共享清晰几何核形态。 | `sac_model_stick.h` |
| `impl/sac_model_torus.hpp` | mid | 是 | torus 有逐点距离/内点循环，但几何公式、法线和小矩阵求解复杂，先保留二轮复核。 | `sac_model_torus.h` |
| `lmeds.h` | low | 否 | 公开声明和参数接口，实际实现位于 `impl/lmeds.hpp`；不单独作为文件候选。 | `impl/lmeds.hpp` |
| `method_types.h` | low | 否 | 枚举/类型定义，无规模相关循环或批量算术。 | `-` |
| `mlesac.h` | low | 否 | 公开声明和参数成员，批量 EM/MAD/minmax 实现位于 `impl/mlesac.hpp`。 | `impl/mlesac.hpp` |
| `model_types.h` | low | 否 | 模型类型枚举/常量，无可 RVV 循环。 | `-` |
| `msac.h` | low | 否 | 公开声明，后处理循环实现在 `impl/msac.hpp`。 | `impl/msac.hpp` |
| `prosac.h` | low | 否 | 公开声明和参数配置，控制实现位于 `impl/prosac.hpp`，本身无批量核。 | `impl/prosac.hpp` |
| `ransac.h` | low | 否 | 公开声明，`computeModel` 控制实现位于 `impl/ransac.hpp`；批量模型距离不在该头内。 | `impl/ransac.hpp` |
| `rmsac.h` | low | 否 | 公开声明，后处理循环实现在 `impl/rmsac.hpp`。 | `impl/rmsac.hpp` |
| `rransac.h` | low | 否 | 公开声明，随机采样控制实现位于 `impl/rransac.hpp`。 | `impl/rransac.hpp` |
| `sac.h` | mid | 是 | `refineModel` 存在迭代优化、重新选内点和 inlier 序列比较，但主成本下沉到派生模型。 | `-` |
| `sac_model.h` | low | 否 | 抽象基类、采样、索引初始化和虚接口为主；随机采样/search 半径路径不适合第一轮 RVV 候选。 | `-` |
| `sac_model_circle.h` | low | 否 | 声明、SIMD/RVV helper 原型和 functor 定义；主要批量实现位于 `impl/sac_model_circle.hpp`。 | `impl/sac_model_circle.hpp` |
| `sac_model_circle3d.h` | low | 否 | 类声明和约束接口为主，逐点距离/投影实现在 `impl/sac_model_circle3d.hpp`。 | `impl/sac_model_circle3d.hpp` |
| `sac_model_cone.h` | low | 否 | 类声明、参数和接口为主，逐点距离/角度实现在 `impl/sac_model_cone.hpp`。 | `impl/sac_model_cone.hpp` |
| `sac_model_cylinder.h` | low | 否 | 类声明、参数和接口为主，逐点距离/角度实现在 `impl/sac_model_cylinder.hpp`。 | `impl/sac_model_cylinder.hpp` |
| `sac_model_ellipse3d.h` | low | 否 | 类声明与 helper 声明为主，复杂拟合和逐点距离实现在 `impl/sac_model_ellipse3d.hpp`。 | `impl/sac_model_ellipse3d.hpp` |
| `sac_model_line.h` | low | 否 | 类声明为主，距离/select/count 实现在 `impl/sac_model_line.hpp`。 | `impl/sac_model_line.hpp` |
| `sac_model_normal_parallel_plane.h` | low | 否 | 类声明和参数接口，主要实现为 normal-plane 派生约束。 | `impl/sac_model_normal_parallel_plane.hpp` |
| `sac_model_normal_plane.h` | low | 否 | 声明 normal-plane RVV/standard/SSE/AVX 入口和 layout 条件；实现在 `impl/sac_model_normal_plane.hpp`。 | `impl/sac_model_normal_plane.hpp` |
| `sac_model_normal_sphere.h` | low | 否 | 类声明、normal 权重接口和实例化宏，批量实现位于 `impl/sac_model_normal_sphere.hpp`。 | `impl/sac_model_normal_sphere.hpp` |
| `sac_model_parallel_line.h` | low | 否 | 类声明和 axis 约束，实际批量路径复用 line 实现。 | `impl/sac_model_parallel_line.hpp` |
| `sac_model_parallel_plane.h` | low | 否 | 类声明和角度约束接口，批量路径复用 plane 实现。 | `impl/sac_model_parallel_plane.hpp` |
| `sac_model_perpendicular_plane.h` | low | 否 | 类声明和 axis/angle 约束接口，批量路径复用 plane 实现。 | `impl/sac_model_perpendicular_plane.hpp` |
| `sac_model_plane.h` | low | 否 | 声明 SSE/AVX/RVV helper 原型和接口，实现在 `impl/sac_model_plane.hpp`；不单独作为文件候选。 | `impl/sac_model_plane.hpp` |
| `sac_model_registration.h` | low | 否 | 类声明、correspondence map 和接口，距离/count/select 实现在 `impl/sac_model_registration.hpp`。 | `impl/sac_model_registration.hpp` |
| `sac_model_registration_2d.h` | low | 否 | 类声明和投影矩阵接口，逐 correspondence 投影实现在 `impl/sac_model_registration_2d.hpp`。 | `impl/sac_model_registration_2d.hpp` |
| `sac_model_sphere.h` | low | 否 | 声明 SIMD/RVV helper 原型和接口，实现在 `impl/sac_model_sphere.hpp`。 | `impl/sac_model_sphere.hpp` |
| `sac_model_stick.h` | low | 否 | 类声明和半径接口，距离/select/count 实现在 `impl/sac_model_stick.hpp`。 | `impl/sac_model_stick.hpp` |
| `sac_model_torus.h` | low | 否 | 类声明、normal 输入和参数接口，逐点距离/拟合实现在 `impl/sac_model_torus.hpp`。 | `impl/sac_model_torus.hpp` |
| `src/sac.cpp` | low | 否 | 只包含 SAC 方法头并触发预编译实例化/注册，未承载独立批量算法。 | `sac.h` |
| `src/sac_model_circle.cpp` | low | 否 | 包含 `impl/sac_model_circle.hpp` 并做 `PCL_INSTANTIATE`，无独立循环。 | `impl/sac_model_circle.hpp` |
| `src/sac_model_circle3d.cpp` | low | 否 | 包含 `impl/sac_model_circle3d.hpp` 并做显式实例化，批量实现不在本文件。 | `impl/sac_model_circle3d.hpp` |
| `src/sac_model_cone.cpp` | low | 否 | 除包含 impl 外主要是 `optimizeModelCoefficientsCone` 的 Eigen LM functor 和显式实例化；优化求解由 Eigen 主导，不列文件级候选。 | `impl/sac_model_cone.hpp` |
| `src/sac_model_cylinder.cpp` | low | 否 | 除包含 impl 外主要是 `optimizeModelCoefficientsCylinder` 的 Eigen LM functor 和显式实例化；可 SIMD 的 array 表达由 Eigen 管理，暂不作为本轮候选。 | `impl/sac_model_cylinder.hpp` |
| `src/sac_model_ellipse3d.cpp` | low | 否 | 包含 `impl/sac_model_ellipse3d.hpp` 并实例化，未承载额外批量核。 | `impl/sac_model_ellipse3d.hpp` |
| `src/sac_model_line.cpp` | low | 否 | 包含 `impl/sac_model_line.hpp` 并实例化，未承载额外批量核。 | `impl/sac_model_line.hpp` |
| `src/sac_model_normal_parallel_plane.cpp` | low | 否 | 包含相关 impl 并实例化，实际批量路径归属 normal-plane/plane impl。 | `impl/sac_model_normal_parallel_plane.hpp` |
| `src/sac_model_normal_plane.cpp` | low | 否 | 包含 `impl/sac_model_normal_plane.hpp` 并实例化，RVV/标量实现不在本文件。 | `impl/sac_model_normal_plane.hpp` |
| `src/sac_model_normal_sphere.cpp` | low | 否 | 包含 `impl/sac_model_normal_sphere.hpp` 并实例化，未承载额外批量核。 | `impl/sac_model_normal_sphere.hpp` |
| `src/sac_model_parallel_line.cpp` | low | 否 | 包含 `impl/sac_model_parallel_line.hpp` 并实例化，实际批量路径复用 line。 | `impl/sac_model_parallel_line.hpp` |
| `src/sac_model_plane.cpp` | low | 否 | 包含 plane/perpendicular/parallel impl 并实例化，未承载独立算法循环。 | `impl/sac_model_plane.hpp` |
| `src/sac_model_registration.cpp` | low | 否 | 包含 `impl/sac_model_registration.hpp` 并实例化，实际 correspondence 距离循环在 impl。 | `impl/sac_model_registration.hpp` |
| `src/sac_model_sphere.cpp` | low | 否 | 包含 sphere impl，另有 `optimizeModelCoefficientsSphere` Eigen LM functor；array 计算由 Eigen solver 调用，不列文件级候选。 | `impl/sac_model_sphere.hpp` |
| `src/sac_model_stick.cpp` | low | 否 | 包含 `impl/sac_model_stick.hpp` 并实例化，未承载额外批量核。 | `impl/sac_model_stick.hpp` |
| `src/sac_model_torus.cpp` | low | 否 | 包含 `impl/sac_model_torus.hpp` 并实例化，未承载额外批量核。 | `impl/sac_model_torus.hpp` |

## 7. 二轮交接说明

- 第二轮筛选应读取本文件，并把所有 `high/mid` 文件作为初始候选基线逐项交代去向。
- 第二轮可以将 `high/mid` 降级为暂缓、不推荐或不单独实施，但必须说明源码证据和主成本覆盖原因。
- 第二轮可以补入 `low` 或初筛遗漏文件，但必须说明补入来源、证据和为什么没有扩展成重新全模块/全库筛选。
- 第二轮输出应使用 `doc-rvv/library-screening/sample_consensus/sample_consensus-function-evaluation-queue.zh.md`，并按“建议进行 RVV 优化的文件 / 保留实施的候选文件 / 暂缓或不推荐考虑 RVV 优化的文件”三类组织。
- 本轮相对旧稿的主要口径变化：旧稿把头声明、显式实例化 `src/*.cpp` 和循环/数学项关键词命中广泛列入 `mid`；本轮按当前模板要求，只把能定位到入口、trip count、RVV 适配点和风险的 `impl/*.hpp`/基类路径保留为 `high/mid`，其余降为 `low` 或关联实现说明。
