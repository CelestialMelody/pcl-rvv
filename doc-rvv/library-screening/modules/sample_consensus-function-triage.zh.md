# sample_consensus 模块文件级筛查清单（全覆盖版，重评）

本版按当前统一标准重评：全文件覆盖、实现优先、允许推翻旧结论；`3rdparty/**` 仅登记覆盖，不纳入候选。

## 1. 覆盖范围与口径

- 覆盖范围：`sample_consensus/**` 下源码后缀文件（`.h/.hpp/.c/.cc/.cpp/.cu`）。
- 覆盖结果：总文件 `70`，已判定 `70`（`70/70` 全覆盖）。
- 目录拆分：`include` `54`，`src` `16`。
- 第三方口径：`3rdparty/**` 文件 `0`，候选 `0`（仅登记，不纳入优化队列）。
- 候选定义：仅 `high/mid` 计入候选。

## 2. 筛选出的文件清单（候选）

### 2.1 high 候选（7）

| file_path | 说明 |
| --- | --- |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_ellipse3d.hpp` | 循环10处，模型拟合计算密集，建议优先优化 |
| `sample_consensus/include/pcl/sample_consensus/impl/mlesac.hpp` | 循环10处，模型拟合计算密集，建议优先优化 |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_cone.hpp` | 循环9处，模型拟合计算密集，建议优先优化 |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_torus.hpp` | 循环9处，模型拟合计算密集，建议优先优化 |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle3d.hpp` | 循环9处，模型拟合计算密集，建议优先优化 |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_line.hpp` | 循环8处，模型拟合计算密集，建议优先优化 |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_plane.hpp` | 循环8处，模型拟合计算密集，建议优先优化 |

### 2.2 mid 候选（44）

| file_path | 说明 |
| --- | --- |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_sphere.hpp` | 存在可向量化路径（循环17，数学项95），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_plane.hpp` | 存在可向量化路径（循环11，数学项97），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle.hpp` | 存在可向量化路径（循环11，数学项67），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_cylinder.hpp` | 存在可向量化路径（循环9，数学项130），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_stick.hpp` | 存在可向量化路径（循环8，数学项87），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/sac_model.h` | 存在可向量化路径（循环6，数学项119），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_registration.hpp` | 存在可向量化路径（循环6，数学项37），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/sac_model_registration.h` | 存在可向量化路径（循环5，数学项48），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/sac_model_registration_2d.h` | 存在可向量化路径（循环4，数学项38），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/impl/msac.hpp` | 存在可向量化路径（循环4，数学项21），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/impl/rmsac.hpp` | 存在可向量化路径（循环4，数学项21），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/impl/prosac.hpp` | 存在可向量化路径（循环4，数学项13），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/sac.h` | 存在可向量化路径（循环3，数学项74），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_sphere.hpp` | 存在可向量化路径（循环3，数学项44），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_registration_2d.hpp` | 存在可向量化路径（循环3，数学项29），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/impl/lmeds.hpp` | 存在可向量化路径（循环2，数学项17），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/sac_model_ellipse3d.h` | 存在可向量化路径（循环1，数学项104），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/sac_model_torus.h` | 存在可向量化路径（循环1，数学项81），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/sac_model_circle3d.h` | 存在可向量化路径（循环1，数学项61），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/sac_model_circle.h` | 存在可向量化路径（循环1，数学项53），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/impl/ransac.hpp` | 存在可向量化路径（循环1，数学项19），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/impl/rransac.hpp` | 存在可向量化路径（循环1，数学项17），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/sac_model_cone.h` | 存在可向量化路径（循环0，数学项95），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/sac_model_cylinder.h` | 存在可向量化路径（循环0，数学项95），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/sac_model_plane.h` | 存在可向量化路径（循环0，数学项94），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/sac_model_sphere.h` | 存在可向量化路径（循环0，数学项70），建议次优先 |
| `sample_consensus/src/sac_model_cylinder.cpp` | 存在可向量化路径（循环0，数学项70），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/sac_model_stick.h` | 存在可向量化路径（循环0，数学项58），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/sac_model_normal_parallel_plane.h` | 存在可向量化路径（循环0，数学项48），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/sac_model_perpendicular_plane.h` | 存在可向量化路径（循环0，数学项45），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/sac_model_parallel_plane.h` | 存在可向量化路径（循环0，数学项41），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/sac_model_line.h` | 存在可向量化路径（循环0，数学项38），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/sac_model_normal_plane.h` | 存在可向量化路径（循环0，数学项36），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/ransac.h` | 存在可向量化路径（循环0，数学项33），建议次优先 |
| `sample_consensus/src/sac_model_cone.cpp` | 存在可向量化路径（循环0，数学项31），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/msac.h` | 存在可向量化路径（循环0，数学项29），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/rransac.h` | 存在可向量化路径（循环0，数学项27），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/sac_model_normal_sphere.h` | 存在可向量化路径（循环0，数学项27），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/mlesac.h` | 存在可向量化路径（循环0，数学项25），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/sac_model_parallel_line.h` | 存在可向量化路径（循环0，数学项25），建议次优先 |
| `sample_consensus/src/sac_model_sphere.cpp` | 存在可向量化路径（循环0，数学项25），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/prosac.h` | 存在可向量化路径（循环0，数学项23），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/lmeds.h` | 存在可向量化路径（循环0，数学项22），建议次优先 |
| `sample_consensus/include/pcl/sample_consensus/rmsac.h` | 存在可向量化路径（循环0，数学项22），建议次优先 |

## 3. 全量文件覆盖表（70/70）

| file_path | file_priority | 是否候选 | 判断依据 | impl关联文件 |
| --- | --- | --- | --- | --- |
| `sample_consensus/include/pcl/sample_consensus/impl/lmeds.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项17），建议次优先 | `-` |
| `sample_consensus/include/pcl/sample_consensus/impl/mlesac.hpp` | `high` | 是 | 循环10处，模型拟合计算密集，建议优先优化 | `-` |
| `sample_consensus/include/pcl/sample_consensus/impl/msac.hpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项21），建议次优先 | `-` |
| `sample_consensus/include/pcl/sample_consensus/impl/prosac.hpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项13），建议次优先 | `-` |
| `sample_consensus/include/pcl/sample_consensus/impl/ransac.hpp` | `mid` | 是 | 存在可向量化路径（循环1，数学项19），建议次优先 | `-` |
| `sample_consensus/include/pcl/sample_consensus/impl/rmsac.hpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项21），建议次优先 | `-` |
| `sample_consensus/include/pcl/sample_consensus/impl/rransac.hpp` | `mid` | 是 | 存在可向量化路径（循环1，数学项17），建议次优先 | `-` |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle.hpp` | `mid` | 是 | 存在可向量化路径（循环11，数学项67），建议次优先 | `-` |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle3d.hpp` | `high` | 是 | 循环9处，模型拟合计算密集，建议优先优化 | `-` |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_cone.hpp` | `high` | 是 | 循环9处，模型拟合计算密集，建议优先优化 | `-` |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_cylinder.hpp` | `mid` | 是 | 存在可向量化路径（循环9，数学项130），建议次优先 | `-` |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_ellipse3d.hpp` | `high` | 是 | 循环10处，模型拟合计算密集，建议优先优化 | `-` |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_line.hpp` | `high` | 是 | 循环8处，模型拟合计算密集，建议优先优化 | `-` |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_parallel_plane.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_plane.hpp` | `high` | 是 | 循环8处，模型拟合计算密集，建议优先优化 | `-` |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_sphere.hpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项44），建议次优先 | `-` |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_parallel_line.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_parallel_plane.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_perpendicular_plane.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_plane.hpp` | `mid` | 是 | 存在可向量化路径（循环11，数学项97），建议次优先 | `-` |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_registration.hpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项37），建议次优先 | `-` |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_registration_2d.hpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项29），建议次优先 | `-` |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_sphere.hpp` | `mid` | 是 | 存在可向量化路径（循环17，数学项95），建议次优先 | `-` |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_stick.hpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项87），建议次优先 | `-` |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_torus.hpp` | `high` | 是 | 循环9处，模型拟合计算密集，建议优先优化 | `-` |
| `sample_consensus/include/pcl/sample_consensus/lmeds.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项22），建议次优先 | `sample_consensus/include/pcl/sample_consensus/impl/lmeds.hpp` |
| `sample_consensus/include/pcl/sample_consensus/method_types.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `sample_consensus/include/pcl/sample_consensus/mlesac.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项25），建议次优先 | `sample_consensus/include/pcl/sample_consensus/impl/mlesac.hpp` |
| `sample_consensus/include/pcl/sample_consensus/model_types.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `sample_consensus/include/pcl/sample_consensus/msac.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项29），建议次优先 | `sample_consensus/include/pcl/sample_consensus/impl/msac.hpp` |
| `sample_consensus/include/pcl/sample_consensus/prosac.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项23），建议次优先 | `sample_consensus/include/pcl/sample_consensus/impl/prosac.hpp` |
| `sample_consensus/include/pcl/sample_consensus/ransac.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项33），建议次优先 | `sample_consensus/include/pcl/sample_consensus/impl/ransac.hpp` |
| `sample_consensus/include/pcl/sample_consensus/rmsac.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项22），建议次优先 | `sample_consensus/include/pcl/sample_consensus/impl/rmsac.hpp` |
| `sample_consensus/include/pcl/sample_consensus/rransac.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项27），建议次优先 | `sample_consensus/include/pcl/sample_consensus/impl/rransac.hpp` |
| `sample_consensus/include/pcl/sample_consensus/sac.h` | `mid` | 是 | 存在可向量化路径（循环3，数学项74），建议次优先 | `-` |
| `sample_consensus/include/pcl/sample_consensus/sac_model.h` | `mid` | 是 | 存在可向量化路径（循环6，数学项119），建议次优先 | `-` |
| `sample_consensus/include/pcl/sample_consensus/sac_model_circle.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项53），建议次优先 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle.hpp` |
| `sample_consensus/include/pcl/sample_consensus/sac_model_circle3d.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项61），建议次优先 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle3d.hpp` |
| `sample_consensus/include/pcl/sample_consensus/sac_model_cone.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项95），建议次优先 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_cone.hpp` |
| `sample_consensus/include/pcl/sample_consensus/sac_model_cylinder.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项95），建议次优先 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_cylinder.hpp` |
| `sample_consensus/include/pcl/sample_consensus/sac_model_ellipse3d.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项104），建议次优先 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_ellipse3d.hpp` |
| `sample_consensus/include/pcl/sample_consensus/sac_model_line.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项38），建议次优先 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_line.hpp` |
| `sample_consensus/include/pcl/sample_consensus/sac_model_normal_parallel_plane.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项48），建议次优先 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_parallel_plane.hpp` |
| `sample_consensus/include/pcl/sample_consensus/sac_model_normal_plane.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项36），建议次优先 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_plane.hpp` |
| `sample_consensus/include/pcl/sample_consensus/sac_model_normal_sphere.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项27），建议次优先 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_sphere.hpp` |
| `sample_consensus/include/pcl/sample_consensus/sac_model_parallel_line.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项25），建议次优先 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_parallel_line.hpp` |
| `sample_consensus/include/pcl/sample_consensus/sac_model_parallel_plane.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项41），建议次优先 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_parallel_plane.hpp` |
| `sample_consensus/include/pcl/sample_consensus/sac_model_perpendicular_plane.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项45），建议次优先 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_perpendicular_plane.hpp` |
| `sample_consensus/include/pcl/sample_consensus/sac_model_plane.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项94），建议次优先 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_plane.hpp` |
| `sample_consensus/include/pcl/sample_consensus/sac_model_registration.h` | `mid` | 是 | 存在可向量化路径（循环5，数学项48），建议次优先 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_registration.hpp` |
| `sample_consensus/include/pcl/sample_consensus/sac_model_registration_2d.h` | `mid` | 是 | 存在可向量化路径（循环4，数学项38），建议次优先 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_registration_2d.hpp` |
| `sample_consensus/include/pcl/sample_consensus/sac_model_sphere.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项70），建议次优先 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_sphere.hpp` |
| `sample_consensus/include/pcl/sample_consensus/sac_model_stick.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项58），建议次优先 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_stick.hpp` |
| `sample_consensus/include/pcl/sample_consensus/sac_model_torus.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项81），建议次优先 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_torus.hpp` |
| `sample_consensus/src/sac.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `sample_consensus/src/sac_model_circle.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `sample_consensus/src/sac_model_circle3d.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `sample_consensus/src/sac_model_cone.cpp` | `mid` | 是 | 存在可向量化路径（循环0，数学项31），建议次优先 | `-` |
| `sample_consensus/src/sac_model_cylinder.cpp` | `mid` | 是 | 存在可向量化路径（循环0，数学项70），建议次优先 | `-` |
| `sample_consensus/src/sac_model_ellipse3d.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `sample_consensus/src/sac_model_line.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `sample_consensus/src/sac_model_normal_parallel_plane.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `sample_consensus/src/sac_model_normal_plane.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `sample_consensus/src/sac_model_normal_sphere.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `sample_consensus/src/sac_model_parallel_line.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `sample_consensus/src/sac_model_plane.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `sample_consensus/src/sac_model_registration.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `sample_consensus/src/sac_model_sphere.cpp` | `mid` | 是 | 存在可向量化路径（循环0，数学项25），建议次优先 | `-` |
| `sample_consensus/src/sac_model_stick.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `sample_consensus/src/sac_model_torus.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |

## 4. 简要统计

- 模块统计：high `7`，mid `44`，low `19`。
- 候选占比：`51/70 = 72.9%`。
- include 口径：候选 `48/54`。
- src 口径：候选 `3/16`。
- thirdparty 口径：`0` 文件已登记，候选 `0`。
