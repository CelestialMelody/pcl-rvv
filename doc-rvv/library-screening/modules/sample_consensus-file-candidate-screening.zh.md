# sample_consensus 模块 RVV 第一轮文件级筛选报告

本文档记录 `sample_consensus` 模块的第一轮 RVV 文件级粗筛结果。第一轮只回答“文件中是否存在值得第二轮下钻复核的可 SIMD/RVV 片段”，不直接决定最终 RVV 实施队列。

## 1. 输入依据与范围

- 覆盖范围：`sample_consensus/**` 下源码后缀文件（`.h/.hpp/.c/.cc/.cpp/.cu`）。
- 覆盖结果：总文件 `70`，已判定 `70`（`70/70` 全覆盖）。
- 目录拆分：`include` `54`，`src` `16`。
- 第三方口径：`3rdparty/**` 文件 `0`，候选 `0`（仅登记，不纳入优化队列）。
- 路径显示：候选表和覆盖表中的 `include` 文件省略公共前缀 `sample_consensus/include/pcl/sample_consensus/`，`src` 文件以 `src/` 开头显示。

## 2. 第一轮筛选口径

- 第一轮是文件级粗筛，判断标准是文件内是否存在可向量化循环、数学密集片段、批量字段访问、规约、mask/压缩、图像式 organized 遍历或可诊断的局部 SIMD/RVV 点。
- `high/mid` 是第二轮必须复核并交代去向的初始候选基线，不是最终实施全集。
- `low` 表示本轮未发现足以进入二轮基线的证据，不是永久排除；如果第二轮源码下钻发现明显漏判，可以补入并说明证据。
- 第一轮不承诺 RVV 覆盖公开入口主成本；主成本覆盖、测试可行性、fallback 条件和维护风险由第二轮筛选继续判断。

优先级含义：

| 优先级 | 含义 |
| --- | --- |
| `high` | 文件内存在明显批量循环或数学密集片段，且粗看具备较强 SIMD/RVV 评估价值 |
| `mid` | 文件内存在可向量化片段，但主成本、数据布局、语义风险或测试入口需要第二轮继续确认 |
| `low` | 以声明、薄 wrapper、调度、类型、构建胶水、小规模固定计算或明显不规则状态路径为主 |

## 3. 第一轮筛选统计

| 项目 | 数量 | 说明 |
| --- | ---: | --- |
| 源码文件总数 | 70 | `sample_consensus/**` 源码文件，第三方实现仅登记覆盖，不纳入候选主线。 |
| 已判定文件数 | 70 | `70/70` |
| high | 7 | 二轮必查 |
| mid | 44 | 二轮必查 |
| low | 19 | 已覆盖但不进入二轮初始基线 |
| high + mid | 51 | 第一轮候选基线 |
| 候选占比 | 51/70 = 72.9% | high + mid / 源码文件总数 |

## 4. high 候选（7）

| 文件 | 第一轮证据 |
| --- | --- |
| `impl/sac_model_ellipse3d.hpp` | 循环10处，模型拟合计算密集，建议优先优化 |
| `impl/mlesac.hpp` | 循环10处，模型拟合计算密集，建议优先优化 |
| `impl/sac_model_cone.hpp` | 循环9处，模型拟合计算密集，建议优先优化 |
| `impl/sac_model_torus.hpp` | 循环9处，模型拟合计算密集，建议优先优化 |
| `impl/sac_model_circle3d.hpp` | 循环9处，模型拟合计算密集，建议优先优化 |
| `impl/sac_model_line.hpp` | 循环8处，模型拟合计算密集，建议优先优化 |
| `impl/sac_model_normal_plane.hpp` | 循环8处，模型拟合计算密集，建议优先优化 |

## 5. mid 候选（44）

| 文件 | 第一轮证据 |
| --- | --- |
| `impl/sac_model_sphere.hpp` | 存在可向量化路径（循环17，数学项95），建议次优先 |
| `impl/sac_model_plane.hpp` | 存在可向量化路径（循环11，数学项97），建议次优先 |
| `impl/sac_model_circle.hpp` | 存在可向量化路径（循环11，数学项67），建议次优先 |
| `impl/sac_model_cylinder.hpp` | 存在可向量化路径（循环9，数学项130），建议次优先 |
| `impl/sac_model_stick.hpp` | 存在可向量化路径（循环8，数学项87），建议次优先 |
| `sac_model.h` | 存在可向量化路径（循环6，数学项119），建议次优先 |
| `impl/sac_model_registration.hpp` | 存在可向量化路径（循环6，数学项37），建议次优先 |
| `sac_model_registration.h` | 存在可向量化路径（循环5，数学项48），建议次优先 |
| `sac_model_registration_2d.h` | 存在可向量化路径（循环4，数学项38），建议次优先 |
| `impl/msac.hpp` | 存在可向量化路径（循环4，数学项21），建议次优先 |
| `impl/rmsac.hpp` | 存在可向量化路径（循环4，数学项21），建议次优先 |
| `impl/prosac.hpp` | 存在可向量化路径（循环4，数学项13），建议次优先 |
| `sac.h` | 存在可向量化路径（循环3，数学项74），建议次优先 |
| `impl/sac_model_normal_sphere.hpp` | 存在可向量化路径（循环3，数学项44），建议次优先 |
| `impl/sac_model_registration_2d.hpp` | 存在可向量化路径（循环3，数学项29），建议次优先 |
| `impl/lmeds.hpp` | 存在可向量化路径（循环2，数学项17），建议次优先 |
| `sac_model_ellipse3d.h` | 存在可向量化路径（循环1，数学项104），建议次优先 |
| `sac_model_torus.h` | 存在可向量化路径（循环1，数学项81），建议次优先 |
| `sac_model_circle3d.h` | 存在可向量化路径（循环1，数学项61），建议次优先 |
| `sac_model_circle.h` | 存在可向量化路径（循环1，数学项53），建议次优先 |
| `impl/ransac.hpp` | 存在可向量化路径（循环1，数学项19），建议次优先 |
| `impl/rransac.hpp` | 存在可向量化路径（循环1，数学项17），建议次优先 |
| `sac_model_cone.h` | 存在可向量化路径（循环0，数学项95），建议次优先 |
| `sac_model_cylinder.h` | 存在可向量化路径（循环0，数学项95），建议次优先 |
| `sac_model_plane.h` | 存在可向量化路径（循环0，数学项94），建议次优先 |
| `sac_model_sphere.h` | 存在可向量化路径（循环0，数学项70），建议次优先 |
| `src/sac_model_cylinder.cpp` | 存在可向量化路径（循环0，数学项70），建议次优先 |
| `sac_model_stick.h` | 存在可向量化路径（循环0，数学项58），建议次优先 |
| `sac_model_normal_parallel_plane.h` | 存在可向量化路径（循环0，数学项48），建议次优先 |
| `sac_model_perpendicular_plane.h` | 存在可向量化路径（循环0，数学项45），建议次优先 |
| `sac_model_parallel_plane.h` | 存在可向量化路径（循环0，数学项41），建议次优先 |
| `sac_model_line.h` | 存在可向量化路径（循环0，数学项38），建议次优先 |
| `sac_model_normal_plane.h` | 存在可向量化路径（循环0，数学项36），建议次优先 |
| `ransac.h` | 存在可向量化路径（循环0，数学项33），建议次优先 |
| `src/sac_model_cone.cpp` | 存在可向量化路径（循环0，数学项31），建议次优先 |
| `msac.h` | 存在可向量化路径（循环0，数学项29），建议次优先 |
| `rransac.h` | 存在可向量化路径（循环0，数学项27），建议次优先 |
| `sac_model_normal_sphere.h` | 存在可向量化路径（循环0，数学项27），建议次优先 |
| `mlesac.h` | 存在可向量化路径（循环0，数学项25），建议次优先 |
| `sac_model_parallel_line.h` | 存在可向量化路径（循环0，数学项25），建议次优先 |
| `src/sac_model_sphere.cpp` | 存在可向量化路径（循环0，数学项25），建议次优先 |
| `prosac.h` | 存在可向量化路径（循环0，数学项23），建议次优先 |
| `lmeds.h` | 存在可向量化路径（循环0，数学项22），建议次优先 |
| `rmsac.h` | 存在可向量化路径（循环0，数学项22），建议次优先 |

## 6. 全量文件覆盖表（70/70）

| 文件 | 优先级 | 是否候选 | 判断依据 | 关联实现 |
| --- | --- | --- | --- | --- |
| `impl/lmeds.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项17），建议次优先 | `-` |
| `impl/mlesac.hpp` | `high` | 是 | 循环10处，模型拟合计算密集，建议优先优化 | `-` |
| `impl/msac.hpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项21），建议次优先 | `-` |
| `impl/prosac.hpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项13），建议次优先 | `-` |
| `impl/ransac.hpp` | `mid` | 是 | 存在可向量化路径（循环1，数学项19），建议次优先 | `-` |
| `impl/rmsac.hpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项21），建议次优先 | `-` |
| `impl/rransac.hpp` | `mid` | 是 | 存在可向量化路径（循环1，数学项17），建议次优先 | `-` |
| `impl/sac_model_circle.hpp` | `mid` | 是 | 存在可向量化路径（循环11，数学项67），建议次优先 | `-` |
| `impl/sac_model_circle3d.hpp` | `high` | 是 | 循环9处，模型拟合计算密集，建议优先优化 | `-` |
| `impl/sac_model_cone.hpp` | `high` | 是 | 循环9处，模型拟合计算密集，建议优先优化 | `-` |
| `impl/sac_model_cylinder.hpp` | `mid` | 是 | 存在可向量化路径（循环9，数学项130），建议次优先 | `-` |
| `impl/sac_model_ellipse3d.hpp` | `high` | 是 | 循环10处，模型拟合计算密集，建议优先优化 | `-` |
| `impl/sac_model_line.hpp` | `high` | 是 | 循环8处，模型拟合计算密集，建议优先优化 | `-` |
| `impl/sac_model_normal_parallel_plane.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `impl/sac_model_normal_plane.hpp` | `high` | 是 | 循环8处，模型拟合计算密集，建议优先优化 | `-` |
| `impl/sac_model_normal_sphere.hpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项44），建议次优先 | `-` |
| `impl/sac_model_parallel_line.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `impl/sac_model_parallel_plane.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `impl/sac_model_perpendicular_plane.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `impl/sac_model_plane.hpp` | `mid` | 是 | 存在可向量化路径（循环11，数学项97），建议次优先 | `-` |
| `impl/sac_model_registration.hpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项37），建议次优先 | `-` |
| `impl/sac_model_registration_2d.hpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项29），建议次优先 | `-` |
| `impl/sac_model_sphere.hpp` | `mid` | 是 | 存在可向量化路径（循环17，数学项95），建议次优先 | `-` |
| `impl/sac_model_stick.hpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项87），建议次优先 | `-` |
| `impl/sac_model_torus.hpp` | `high` | 是 | 循环9处，模型拟合计算密集，建议优先优化 | `-` |
| `lmeds.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项22），建议次优先 | `impl/lmeds.hpp` |
| `method_types.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `mlesac.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项25），建议次优先 | `impl/mlesac.hpp` |
| `model_types.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `msac.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项29），建议次优先 | `impl/msac.hpp` |
| `prosac.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项23），建议次优先 | `impl/prosac.hpp` |
| `ransac.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项33），建议次优先 | `impl/ransac.hpp` |
| `rmsac.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项22），建议次优先 | `impl/rmsac.hpp` |
| `rransac.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项27），建议次优先 | `impl/rransac.hpp` |
| `sac.h` | `mid` | 是 | 存在可向量化路径（循环3，数学项74），建议次优先 | `-` |
| `sac_model.h` | `mid` | 是 | 存在可向量化路径（循环6，数学项119），建议次优先 | `-` |
| `sac_model_circle.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项53），建议次优先 | `impl/sac_model_circle.hpp` |
| `sac_model_circle3d.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项61），建议次优先 | `impl/sac_model_circle3d.hpp` |
| `sac_model_cone.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项95），建议次优先 | `impl/sac_model_cone.hpp` |
| `sac_model_cylinder.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项95），建议次优先 | `impl/sac_model_cylinder.hpp` |
| `sac_model_ellipse3d.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项104），建议次优先 | `impl/sac_model_ellipse3d.hpp` |
| `sac_model_line.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项38），建议次优先 | `impl/sac_model_line.hpp` |
| `sac_model_normal_parallel_plane.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项48），建议次优先 | `impl/sac_model_normal_parallel_plane.hpp` |
| `sac_model_normal_plane.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项36），建议次优先 | `impl/sac_model_normal_plane.hpp` |
| `sac_model_normal_sphere.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项27），建议次优先 | `impl/sac_model_normal_sphere.hpp` |
| `sac_model_parallel_line.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项25），建议次优先 | `impl/sac_model_parallel_line.hpp` |
| `sac_model_parallel_plane.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项41），建议次优先 | `impl/sac_model_parallel_plane.hpp` |
| `sac_model_perpendicular_plane.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项45），建议次优先 | `impl/sac_model_perpendicular_plane.hpp` |
| `sac_model_plane.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项94），建议次优先 | `impl/sac_model_plane.hpp` |
| `sac_model_registration.h` | `mid` | 是 | 存在可向量化路径（循环5，数学项48），建议次优先 | `impl/sac_model_registration.hpp` |
| `sac_model_registration_2d.h` | `mid` | 是 | 存在可向量化路径（循环4，数学项38），建议次优先 | `impl/sac_model_registration_2d.hpp` |
| `sac_model_sphere.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项70），建议次优先 | `impl/sac_model_sphere.hpp` |
| `sac_model_stick.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项58），建议次优先 | `impl/sac_model_stick.hpp` |
| `sac_model_torus.h` | `mid` | 是 | 存在可向量化路径（循环1，数学项81），建议次优先 | `impl/sac_model_torus.hpp` |
| `src/sac.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/sac_model_circle.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/sac_model_circle3d.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/sac_model_cone.cpp` | `mid` | 是 | 存在可向量化路径（循环0，数学项31），建议次优先 | `-` |
| `src/sac_model_cylinder.cpp` | `mid` | 是 | 存在可向量化路径（循环0，数学项70），建议次优先 | `-` |
| `src/sac_model_ellipse3d.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/sac_model_line.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/sac_model_normal_parallel_plane.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/sac_model_normal_plane.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/sac_model_normal_sphere.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/sac_model_parallel_line.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/sac_model_plane.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/sac_model_registration.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/sac_model_sphere.cpp` | `mid` | 是 | 存在可向量化路径（循环0，数学项25），建议次优先 | `-` |
| `src/sac_model_stick.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/sac_model_torus.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |

## 7. 二轮交接说明

- 第二轮筛选应读取本文件，并把所有 `high/mid` 文件作为初始候选基线逐项交代去向。
- 第二轮可以将 `high/mid` 降级为暂缓、不推荐或不单独实施，但必须说明源码证据和主成本覆盖原因。
- 第二轮可以补入 `low` 或初筛遗漏文件，但必须说明补入来源、证据和为什么没有扩展成重新全模块/全库筛选。
- 第二轮输出应使用 `doc-rvv/library-screening/sample_consensus/sample_consensus-function-evaluation-queue.zh.md`，并按“建议进行 RVV 优化的文件 / 保留实施的候选文件 / 暂缓或不推荐考虑 RVV 优化的文件”三类组织。
