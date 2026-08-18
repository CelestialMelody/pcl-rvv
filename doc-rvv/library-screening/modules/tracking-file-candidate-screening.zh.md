# tracking 模块 RVV 第一轮文件级筛选报告

本文档记录 `tracking` 模块的第一轮 RVV 文件级粗筛结果。第一轮只回答“文件中是否存在值得第二轮下钻复核的可 SIMD/RVV 片段”，不直接决定最终 RVV 实施队列。

## 1. 输入依据与范围

- 覆盖范围：`tracking/**` 下源码后缀文件（`.h/.hpp/.c/.cc/.cpp/.cu`）。
- 覆盖结果：总文件 `30`，已判定 `30`（`30/30` 全覆盖）。
- 目录拆分：`include` `26`，`src` `4`。
- 第三方口径：`3rdparty/**` 文件 `0`，候选 `0`（仅登记，不纳入优化队列）。
- 路径显示：候选表和覆盖表中的 `include` 文件省略公共前缀 `tracking/include/pcl/tracking/`，`src` 文件以 `src/` 开头显示。

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
| 源码文件总数 | 30 | `tracking/**` 源码文件，第三方实现仅登记覆盖，不纳入候选主线。 |
| 已判定文件数 | 30 | `30/30` |
| high | 2 | 二轮必查 |
| mid | 20 | 二轮必查 |
| low | 8 | 已覆盖但不进入二轮初始基线 |
| high + mid | 22 | 第一轮候选基线 |
| 候选占比 | 22/30 = 73.3% | high + mid / 源码文件总数 |

## 4. high 候选（2）

| 文件 | 第一轮证据 |
| --- | --- |
| `impl/pyramidal_klt.hpp` | 循环27处，跟踪状态估计计算密集，建议优先优化 |
| `impl/particle_filter.hpp` | 循环21处，跟踪状态估计计算密集，建议优先优化 |

## 5. mid 候选（20）

| 文件 | 第一轮证据 |
| --- | --- |
| `impl/kld_adaptive_particle_filter_omp.hpp` | 存在可向量化路径（循环6，数学项4），建议次优先 |
| `impl/particle_filter_omp.hpp` | 存在可向量化路径（循环6，数学项4），建议次优先 |
| `impl/tracking.hpp` | 存在可向量化路径（循环5，数学项74），建议次优先 |
| `impl/kld_adaptive_particle_filter.hpp` | 存在可向量化路径（循环4，数学项5），建议次优先 |
| `kld_adaptive_particle_filter.h` | 存在可向量化路径（循环3，数学项20），建议次优先 |
| `impl/nearest_pair_point_cloud_coherence.hpp` | 存在可向量化路径（循环3，数学项7），建议次优先 |
| `impl/approx_nearest_pair_point_cloud_coherence.hpp` | 存在可向量化路径（循环3，数学项6），建议次优先 |
| `impl/coherence.hpp` | 存在可向量化路径（循环1，数学项8），建议次优先 |
| `particle_filter.h` | 存在可向量化路径（循环0，数学项64），建议次优先 |
| `pyramidal_klt.h` | 存在可向量化路径（循环0，数学项42），建议次优先 |
| `hsv_color_coherence.h` | 存在可向量化路径（循环0，数学项22），建议次优先 |
| `tracking.h` | 存在可向量化路径（循环0，数学项22），建议次优先 |
| `coherence.h` | 存在可向量化路径（循环0，数学项20），建议次优先 |
| `kld_adaptive_particle_filter_omp.h` | 存在可向量化路径（循环0，数学项18），建议次优先 |
| `particle_filter_omp.h` | 存在可向量化路径（循环0，数学项18），建议次优先 |
| `distance_coherence.h` | 存在可向量化路径（循环0，数学项14），建议次优先 |
| `normal_coherence.h` | 存在可向量化路径（循环0，数学项14），建议次优先 |
| `nearest_pair_point_cloud_coherence.h` | 存在可向量化路径（循环0，数学项13），建议次优先 |
| `tracker.h` | 存在可向量化路径（循环0，数学项9），建议次优先 |
| `approx_nearest_pair_point_cloud_coherence.h` | 存在可向量化路径（循环0，数学项7），建议次优先 |

## 6. 全量文件覆盖表（30/30）

| 文件 | 优先级 | 是否候选 | 判断依据 | 关联实现 |
| --- | --- | --- | --- | --- |
| `approx_nearest_pair_point_cloud_coherence.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项7），建议次优先 | `impl/approx_nearest_pair_point_cloud_coherence.hpp` |
| `coherence.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项20），建议次优先 | `impl/coherence.hpp` |
| `distance_coherence.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项14），建议次优先 | `impl/distance_coherence.hpp` |
| `hsv_color_coherence.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项22），建议次优先 | `impl/hsv_color_coherence.hpp` |
| `impl/approx_nearest_pair_point_cloud_coherence.hpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项6），建议次优先 | `-` |
| `impl/coherence.hpp` | `mid` | 是 | 存在可向量化路径（循环1，数学项8），建议次优先 | `-` |
| `impl/distance_coherence.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `impl/hsv_color_coherence.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `impl/kld_adaptive_particle_filter.hpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项5），建议次优先 | `-` |
| `impl/kld_adaptive_particle_filter_omp.hpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项4），建议次优先 | `-` |
| `impl/nearest_pair_point_cloud_coherence.hpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项7），建议次优先 | `-` |
| `impl/normal_coherence.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `impl/particle_filter.hpp` | `high` | 是 | 循环21处，跟踪状态估计计算密集，建议优先优化 | `-` |
| `impl/particle_filter_omp.hpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项4），建议次优先 | `-` |
| `impl/pyramidal_klt.hpp` | `high` | 是 | 循环27处，跟踪状态估计计算密集，建议优先优化 | `-` |
| `impl/tracker.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `impl/tracking.hpp` | `mid` | 是 | 存在可向量化路径（循环5，数学项74），建议次优先 | `-` |
| `kld_adaptive_particle_filter.h` | `mid` | 是 | 存在可向量化路径（循环3，数学项20），建议次优先 | `impl/kld_adaptive_particle_filter.hpp` |
| `kld_adaptive_particle_filter_omp.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项18），建议次优先 | `impl/kld_adaptive_particle_filter_omp.hpp` |
| `nearest_pair_point_cloud_coherence.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项13），建议次优先 | `impl/nearest_pair_point_cloud_coherence.hpp` |
| `normal_coherence.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项14），建议次优先 | `impl/normal_coherence.hpp` |
| `particle_filter.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项64），建议次优先 | `impl/particle_filter.hpp` |
| `particle_filter_omp.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项18），建议次优先 | `impl/particle_filter_omp.hpp` |
| `pyramidal_klt.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项42），建议次优先 | `impl/pyramidal_klt.hpp` |
| `tracker.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项9），建议次优先 | `impl/tracker.hpp` |
| `tracking.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项22），建议次优先 | `impl/tracking.hpp` |
| `src/coherence.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/kld_adaptive_particle_filter.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/particle_filter.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/tracking.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |

## 7. 二轮交接说明

- 第二轮筛选应读取本文件，并把所有 `high/mid` 文件作为初始候选基线逐项交代去向。
- 第二轮可以将 `high/mid` 降级为暂缓、不推荐或不单独实施，但必须说明源码证据和主成本覆盖原因。
- 第二轮可以补入 `low` 或初筛遗漏文件，但必须说明补入来源、证据和为什么没有扩展成重新全模块/全库筛选。
- 第二轮输出应使用 `doc-rvv/library-screening/tracking/tracking-function-evaluation-queue.zh.md`，并按“建议进行 RVV 优化的文件 / 保留实施的候选文件 / 暂缓或不推荐考虑 RVV 优化的文件”三类组织。
