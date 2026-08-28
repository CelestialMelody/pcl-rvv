# sample_consensus 模块 RVV 保留候选复筛

## 1. 输入依据与复筛原因

本文件是 `sample_consensus` 模块函数评估队列完成后的保留候选复筛，只复筛
`doc-rvv/library-screening/sample_consensus/sample_consensus-function-evaluation-queue.zh.md`
中 `5. 保留实施的候选文件` 的 6 个条目；不重新扩大到全模块，不重跑第一轮文件候选筛选，也不补入保留候选之外的新文件。

本文件最初只做源码阅读、静态分析、轻量入口确认和筛选文档；筛选结论只授权后续进入函数级评估，
不能替代 `rvv-workflow` / `rvv-test` 的 correctness（正确性）、QEMU（仿真器）、反汇编、board bench
（板卡性能测试）和 production integration（生产接入）证据闭环。当前执行状态已追加到本文：
`sac_model_cylinder.hpp` 和 `sac_model_normal_sphere.hpp` 已完成三入口 production adoption（生产采纳）；`sac_model_circle3d.hpp`
已完成当前候选的 production probe（生产探针），接入后证据不支持采纳。

复筛原因是建议队列中的 normal-plane、plane、sphere、circle2d、line、stick 主题已经形成较完整的正向和负向证据：

- 基础几何模型的 `countWithinDistance` / `selectWithinDistance` / `getDistancesToModel` 公开入口，若 RVV 能覆盖 indexed gather、同构几何公式、mask / `vcompress` 和 dense double store，通常有明确生产价值。
- normal-plane 证明点云 + 法线 AoS gather、normal angle helper 和 `vcompress` 写回可以成立，但只覆盖当前 normal-plane 三入口及其 source / normal layout gate，不外推到 cylinder、cone 或 normal-sphere。
- sphere 证明 shell count/select 可采纳，但 `getDistancesToModel` 的 squared-distance + scalar sqrt/store 形态为负向；circle、line、stick 又证明 full-RVV `vfsqrt + vfwcvt + vse64` dense / compressed 写回可以成为正向模式。
- identity-index strided load 在 circle、line 等主题中已经被同边界 A/B 拒绝或仅窄范围采纳；后续候选不因 indices 看似连续而自动升级。
- SAC 方法后处理和注册类候选若主成本在模型距离、随机采样、map/SVD 或外部模块，应先走 profile prerequisite 或 component ablation，而不是直接写成 production 优化。

## 2. 筛选统计

| 统计项 | 数量 / 结论 |
| --- | ---: |
| 保留实施候选输入总数 | 6 |
| 建议启动 / 已推进函数级评估 | 3 |
| 暂缓 / 不单独实施 | 3 |
| 重新纳入保留队列之外候选 | 0 |
| 合并、删除或源码冲突项 | 0 |
| 已完成 production-adopted 保留候选 | 2 |

建议启动项的默认评估路径分布：

| 默认评估路径 | 数量 | 候选 |
| --- | ---: | --- |
| `production-shaped diagnostic` | 0 | 当前无。 |
| `component ablation` | 1 | `sac_model_circle3d.hpp` |
| `profile prerequisite` | 0 | 本轮不把缺少真实热度的 registration / MLESAC 放入默认启动队列。 |
| `production-adopted` | 2 | `sac_model_cylinder.hpp`、`sac_model_normal_sphere.hpp` |

## 3. 已完成主题经验总结

### 3.1 已完成主题证据包

| 主题 | 主文件 | 函数评估队列原始定位 | 实际覆盖范围 | 目标硬件结论 | 正确性证据 | 反汇编证据 | 生产接入状态 | 回退 / 暂缓原因 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| normal-plane | `impl/sac_model_normal_plane.hpp` | normal-plane 三入口已有 RVV 资产需收敛 | `selectWithinDistance`、`countWithinDistance`、`getDistancesToModel` production helper；source / normal layout gate；代表 source × normal correctness | `PointXYZ + Normal` helper 5-run median：select `9.64x`、count `12.72x`、getDistances `12.02x`；代表 source 点型仍 positive-stable | Std/RVV QEMU、public dispatch / fallback、source AoS、normal layout 和交叉点型 correctness | 三入口 helper 均有 RVV 指令归属 | production-adopted；commit `80112eb5d` | 不外推到泛型 normal-like 点型全集、公开入口 repeated performance、`Scalar=double` 或其它法线几何模型。 |
| plane | `impl/sac_model_plane.hpp` | 基础 plane 三入口 | 三入口 production RVV；select/count 窄范围 identity strided load；getDistances 保持 gather-only | Phase 000 三入口 positive；Phase 010 select/count identity 正向，getDistances identity 拒绝 | QEMU Std/RVV 7 tests、代表 AoS 点型和空 indices correctness | production asm / helper 归属闭合 | production-adopted | identity load 只窄采纳 select/count；不能外推到其它模型或 getDistances。 |
| sphere | `impl/sac_model_sphere.hpp` | sphere shell select/count/getDistances | `countWithinDistance` 已有 RVV；`selectWithinDistance` `vcompress` production；`getDistancesToModel` 保持标量 | PointXYZ select median `2.0989x`、count `3.5154x`；PointXYZI/RGB/RGBA select median `1.5901x` / `1.6225x` / `1.5528x`，RGBA 有 1/5 退化 | topic correctness、代表点型 correctness、board smoke | `selectWithinDistanceRVV` 可见 `vcompress.vm` | production-adopted for select/count；commit `f8f4c34ac` | `getDistancesToModel` squared-distance + scalar sqrt/store median `0.7723x`，不采纳；normal-sphere 不能继承 sphere 结论。 |
| circle2d | `impl/sac_model_circle.hpp` | circle2d select/count/getDistances | select/count/getDistances production RVV；select full-RVV error tail；getDistances full-RVV dense store | select Phase 090 median `2.5700x`；count median `1.4012x`；getDistances median `1.4737x` | Std/RVV correctness、identity indices、空 indices 和 PointXYZI correctness | `vcompress.vm`、`vfsqrt.v`、`vfwcvt.f.f.v`、`vse64.v` 归属闭合 | production-adopted；commit `77e166be6` | old sqr + scalar sqrt getDistances 候选负向；identity strided load select `0.9914x`、count `0.9698x`，已拒绝。 |
| line | `impl/sac_model_line.hpp` | line cross3 距离三入口 | count/select/getDistances production RVV；getDistances 和 select error 写回均改为 `vfwcvt + vse64` direct double store | Phase 060 median：count `4.8068x`、select `3.2460x`、getDistances `4.0460x` | Std/RVV 9 tests、select 输出顺序、dense distance、bench-scale 输入 | 三入口 helper 均有 RVV 指令归属 | production-adopted；commit `5e2080450` | identity-index strided load strict A/B 已拒绝；更多点型和 `Scalar=double` 未覆盖。 |
| stick | `impl/sac_model_stick.hpp` | stick line-like 几何核 | count/select/getDistances production direct；getDistances vector writeback | Phase 080 count/select median `4.1729x` / `3.3023x`；Phase 100 getDistances median `3.6879x` | topic correctness、代表点型 correctness、board smoke | production asm、`vfwcvt + vse64` 写回归属 | production-adopted；commits `e03eb4279` / `4205955f3` | 双阈值语义和 penalty 已闭合当前范围；更多点型性能需新 scope。 |
| cylinder | `impl/sac_model_cylinder.hpp` | cylinder 法线距离三入口 | count/select/getDistances production RVV；normal angle helper、indexed gather、`vcompress` 和 dense double store；Phase 040 已扩展代表点型 | `PointXYZ + Normal` 5-run median：count `5.3124x`、select `4.6445x`、getDistances `6.9318x`；新增代表点型全部 positive | Std/RVV 11 tests、public-vs-Standard、dense distance、fallback、三组新增代表点型 correctness | 三入口 helper 均有 RVV 指令归属；count/select/getDistances 为 306 / 338 / 298 RVV lines | production-adopted | 覆盖 direct indexed 四组代表点型；自定义点型全集、`PointXYZRGBA`、`PointXYZINormal`、其它 layout 和真实 workload 需新触发证据。 |
| normal-sphere | `impl/sac_model_normal_sphere.hpp` | normal-sphere 法线距离三入口 | count/select/getDistances production RVV；球心方向 `n_dir`、normal angle helper、indexed gather、`vcompress` 和 dense double store | Phase060 production-public 5-run median：`PointXYZ + Normal` select/count/getDistances 为 `3.039x` / `3.332x` / `4.456x`；PointXYZI/RGB/RGBA 三入口也全部 positive | Std/RVV 8 tests、production detail helper、large input public dispatch、fallback 和四种 source 点型 correctness | production helper 可见 `computeNormalSphereDistanceRVV`、`vfsqrt.v`、`vcpop.m`、`vcompress.vm`、`vfwcvt.f.f.v` 和 `vse64.v` | production-adopted | 覆盖 direct indexed 四组 source 点型 + `pcl::Normal`；其它 normal 点型、自定义 source 点型、`Scalar=double`、真实 workload 和其它硬件需新触发证据。 |

### 3.2 可复用模式与失败边界

| 模式标签 | 来自哪些已完成主题 | 成立条件 | 失败 / 回退边界 | 对后续保留候选的影响 |
| --- | --- | --- | --- | --- |
| `indexed AoS gather + arithmetic-dense geometry` | plane、sphere、circle2d、line、stick、normal-plane | 公开入口主循环随 `indices_->size()` 增长，字段 offset 可 gate，后续几何公式足够摊薄 gather 成本 | 自定义 layout、非法 index、双侧 gather 或公式过薄未单独证明 | 支持 cylinder、normal-sphere、circle3d 进入函数级评估，但必须重新闭合模型公式和边界。 |
| `mask + vcompress ordered output` | normal-plane、sphere、circle2d、line、stick | select 输出可预分配，chunk 内顺序等价于标量 `indices_` 顺序，命中数量可由 `vcpop` 驱动 | 输出依赖 `push_back` 副作用、写回需保留逐点特殊处理、内点极稀疏导致峰值内存风险 | 支持 cylinder / normal-sphere select 作为 production-shaped diagnostic；circle3d select 可做 component ablation。 |
| `full-RVV sqrt + double store` | circle2d、line、stick | `vfsqrt` 留在 RVV chunk 内，结果用 `vfwcvt + vse64` 写回 dense 或 compressed double output | squared-distance RVV 后接 scalar sqrt/store 在 sphere、circle2d、line 旧形态中已经负向或被替代 | circle3d 的 getDistances 不应从旧 sqr+scalar 形态开始；应优先回答 count/select 投影核。 |
| `normal angle helper usable but non-transferable` | normal-plane、cylinder | source 和 normal cloud layout gate 清楚，angle helper 的误差预算、fallback 和 correctness 可闭合 | cone / normal-sphere 的目标法线方向每点变化，early continue 和退化点更多 | cylinder 已完成 production-adopted；normal-sphere 仍可按自身边界推进，cone 的 cylinder 依赖已解除但仍需独立评估。 |
| `identity strided load is not default upgrade` | plane、circle2d、line | 只有同边界 RVV-vs-RVV A/B 显示正向，且入口收益不伤害 select/getDistances | circle2d、line 中 identity family 已负向或不稳 | 保留候选首阶段不把 identity indices 作为主收益假设。 |
| `tail / method post-processing dilution` | sphere getDistances 负向、MSAC/RMSAC 二轮暂缓、registration 完成主题中的 search / solver 稀释经验 | 后处理片段需要证明接近入口主成本，或只是作为其它 topic 的 ablation | 模型距离、随机采样、map/SVD、`exp/log`、median/sort 或外部 solver 主导 | MLESAC 和 registration 不作为默认启动项；需要 profile 或上游使用场景触发。 |

## 4. 筛选口径修正 / 复筛变化理由

本轮对函数评估队列中的 6 个保留候选作如下修正：

1. `cylinder` 已从“建议启动”推进到 production-adopted：count/select/getDistances 三入口均完成 production direct board repeated、QEMU correctness、反汇编和 Evidence Doctor；该结论为 cone 的依赖条件提供正向输入，但不能自动外推到 cone。
2. `normal-sphere` 已从“建议启动”推进到 production-adopted：它没有直接继承 sphere 或 normal-plane，而是完成自身 production-shaped diagnostic、PI1 计划、PI2-PI5 生产接入、Phase060 production-public repeated board 和正式 `doc-rvv`。
3. `circle3d` 从“保留等待”升级为建议启动：circle2d、line、stick 已证明 full-RVV sqrt / double store 和 `vcompress` 可行，但 `circle3d` 的 double 投影、`N.dot(N)` 除法和 projected radius normalize 是新问题；应先做 count/select component ablation，不直接承诺 production 接入。
4. `cone` 的 cylinder 依赖已解除，但仍不能直接继承 cylinder 生产结论：它比 cylinder 多 `pointToAxisDistance` helper、height normalize、axis normalize、cone normal 合成和 opening angle 几何退化。若继续 sample_consensus 保留队列，cone 可以重新进入独立函数级评估或 component ablation（组件消融）。
5. `registration` 仍暂缓：距离核本身是直接循环，但双索引 gather、target cloud layout、`correspondences_` map、SVD staging 和模块归属问题仍未被 sample_consensus 已完成主题关闭；缺少真实 profile 时不排入默认启动队列。
6. `MLESAC` 仍暂缓：本地 EM / log-likelihood 循环在连续 `std::vector<double>` 上可诊断，但每次迭代先调用模型 `getDistancesToModel`，且 `exp/log` 近似、浮点规约顺序、final compress 和 median/sort 会稀释收益。模型距离核完成后仍显示后处理占比高时再启动。

## 5. 保留实施候选逐项复筛

本节按推荐动作拆分表格。`建议启动函数级评估` 表只表示可以建立单 topic S1-S2 入口评估和证据计划，不表示可以跳过 full diagnostic 或直接修改生产路径。

### 5.1 建议启动函数级评估

| 主题 | 关键入口 | 主成本覆盖类型 | 默认评估路径 / 首阶段证据问题 | 匹配的已验证模式 | 主要风险 | 推荐理由 | 证据来源 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| cylinder 法线距离诊断 | `impl/sac_model_cylinder.hpp` 三个距离入口 | `production-adopted` | 已完成 production-shaped diagnostic、PI2-PI5 production integration、Phase 030 getDistances dense output、Phase 040 代表点型扩展和正式 `doc-rvv`。 | indexed AoS gather、normal angle helper、mask + `vcompress`、full-RVV sqrt + double store | 当前批准 direct indexed 四组代表点型；自定义点型全集、`PointXYZRGBA`、`PointXYZINormal`、其它 layout 和真实 workload 需新触发证据。 | 三入口接入后 12 项板卡 comparison 均 positive，已从建议启动队列移入已完成生产主题。 | `test-rvv/sample_consensus/sac_model_cylinder/`；`doc-rvv/sample_consensus/sac_model_cylinder-RVV.zh.md` |
| normal-sphere 法线距离诊断 | `impl/sac_model_normal_sphere.hpp` 三个距离入口 | `production-adopted` | 已完成 production-shaped diagnostic、PI1 计划、PI2-PI5 production integration、Phase060 production-public board repeated 和正式 `doc-rvv`。 | sphere shell mask、normal-plane normal angle、mask count/compress、full-RVV dense store | 当前批准 direct indexed 四种 source 点型 + `pcl::Normal`；其它 normal 点型、自定义 source 点型、`Scalar=double`、真实 workload 和其它硬件需新触发证据。 | 三入口接入后 12 项板卡 comparison 均 positive，Evidence Doctor 0/0/0，已从建议启动队列移入已完成生产主题。 | `test-rvv/sample_consensus/sac_model_normal_sphere/`；`doc-rvv/sample_consensus/sac_model_normal_sphere-RVV.zh.md` |
| circle3d 投影核诊断 | `impl/sac_model_circle3d.hpp`：`getDistancesToModel` 138-181、`selectWithinDistance` 185-234、`countWithinDistance` 237-276 | `diagnostic`，首选 count/select 投影距离核 | `component ablation`：先隔离 x/y/z gather、点到圆平面投影、projected radial normalize、squared distance threshold 的成本与误差；getDistances 需等 count/select 明确后再考虑 full-RVV sqrt / double store | indexed AoS gather、mask + count/compress、full-RVV sqrt/double-store 作为后续候选 | `Eigen::Vector3d` double 中间量；`N.dot(N)` 除法；`helper_vectorP_projC.normalized()` 在投影点接近圆心时退化；`getDistances` 与 count/select 的 `lambda` 符号写法不同，需先对拍 | 没有法线输入，控制流比 cone/cylinder 简单；circle2d 和 line/stick 证明 select/count/output 模式可行，但 3D 投影公式必须单独消融 | 函数评估队列保留项；`sac_model_circle3d.hpp` 源码；circle2d / line / stick 完成主题文档；`quadric_models` Circle3D RANSAC |

### 5.2 暂缓 / 不单独实施

| 主题 | 主成本覆盖类型 | 暂缓原因 | 重新考虑条件 | 证据来源 |
| --- | --- | --- | --- | --- |
| cone 法线距离诊断 | `diagnostic` | `getDistancesToModel` 149-202、`selectWithinDistance` 205-270、`countWithinDistance` 273-328 的每点公式包含 axis projection、`pointToAxisDistance` 515-520 的 sqrt、height normalize、point direction normalize、cone normal 合成和 `getAngle3D`；比 cylinder 多一层几何退化和三角函数参数边界。 | cylinder 已证明 normal-angle + radial/axis distance 模式在自身边界正向；cone 仍需独立 count/select component ablation、退化点 correctness、asm、board 和 Doctor。 | 函数评估队列保留项；`sac_model_cone.hpp` 源码；`quadric_models` Cone RANSAC |
| registration 归属 / profile | `diagnostic` / `profile prerequisite` | `getDistancesToModel` 100-143、`selectWithinDistance` 146-200、`countWithinDistance` 203-249 是双索引 source/target gather + 4x4 transform + norm；但 `computeModelCoefficients` 和 `optimizeModelCoefficients` 依赖 `correspondences_` map、SVD staging 252-321 与 Eigen `umeyama`。当前缺少 sample_consensus 内真实热度和模块归属证据。 | 有明确 sample_consensus registration workload/profile，或 registration 模块已完成的 row-source / transform 模式能直接映射到本 public entry，且确认不会与 registration 模块 topic 重复时再启动。 | 函数评估队列保留项；`sac_model_registration.hpp` 源码；registration 已完成主题的 search / solver 稀释边界 |
| MLESAC 后处理诊断 | `tail-compress` / `diagnostic` | `computeModel` 每轮先调用 `sac_model_->getDistancesToModel` 生成 distances；本地 122-141 的 EM `exp` / gamma / `log` 循环、194-202 的 final inlier compress、211-280 的 MAD/minmax/median 都是后处理或初始化片段，且 median 依赖排序/选择算法。 | 已完成模型距离核后，profile 仍显示 MLESAC EM 或 final compress 接近主成本；或有明确需要评估 `exp/log` double 近似与规约顺序的 test-only diagnostic。 | 函数评估队列保留项；`mlesac.hpp` 源码；`plane_models` MLESAC 回归；MSAC/RMSAC 二轮暂缓边界 |

## 6. 新的执行清单 / 状态表

### 6.1 建议启动函数级评估

| 顺序 | 主题 | 主文件 | 推荐入口 / 第一 RVV 目标 | 依据模式 / 证据来源 | 状态 | 当前结论 / 下一步条件 |
| ---: | --- | --- | --- | --- | --- | --- |
| 1 | cylinder 法线距离诊断 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_cylinder.hpp` | `countWithinDistance` / `selectWithinDistance` / `getDistancesToModel` 的 weighted euclid + normal angle 距离核 | normal-plane normal angle；line/stick axis geometry；`quadric_models` cylinder tests | completed / production-adopted | `test-rvv/sample_consensus/sac_model_cylinder/` 已完成三入口 production direct。`PointXYZ + Normal` median 为 count `5.3124x`、select `4.6445x`、getDistances `6.9318x`；`PointXYZI + Normal`、`PointXYZRGB + Normal`、`PointXYZ + PointNormal` 三组新增代表点型全部 positive，Evidence Doctor 0/0/0；正式文档为 `doc-rvv/sample_consensus/sac_model_cylinder-RVV.zh.md`。 |
| 2 | normal-sphere 法线距离诊断 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_sphere.hpp` | `countWithinDistance` / `selectWithinDistance` / `getDistancesToModel` 的球心方向 + normal angle 核 | sphere shell；normal-plane normal angle；`quadric_models` NormalSphere RANSAC | completed / production-adopted | `test-rvv/sample_consensus/sac_model_normal_sphere/` 已完成三入口 production direct。Phase060 5-run median：`PointXYZ + Normal` select/count/getDistances 为 `3.039x` / `3.332x` / `4.456x`；`PointXYZI + Normal` 为 `2.900x` / `3.358x` / `4.157x`；`PointXYZRGB + Normal` 为 `2.973x` / `3.308x` / `4.290x`；`PointXYZRGBA + Normal` 为 `2.906x` / `3.411x` / `4.161x`；Evidence Doctor 0/0/0；正式文档为 `doc-rvv/sample_consensus/sac_model_normal_sphere-RVV.zh.md`。 |
| 3 | circle3d 投影核诊断 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle3d.hpp` | count/select 的 3D 投影距离核；getDistances 等 full-RVV sqrt/double-store 形态另行评估 | circle2d、line、stick 的 compress / sqrt / double-store 经验；`quadric_models` Circle3D RANSAC | completed / production-not-adopted | `test-rvv/sample_consensus/sac_model_circle3d/` 已完成 component ablation、select production probe 和点类型扩展：count candidate mean `0.5713x` 被拒绝；select production-public `PointXYZ` 10-run mean `0.9748x`、median `0.9991x`、5/10 退化，Evidence Doctor 有 Error；`PointXYZI/RGB/RGBA` 扩展均负向。当前 production patch 等待用户确认是否回滚，不创建正式 `doc-rvv`。 |

### 6.2 暂缓 / 不单独实施

| 顺序 | 主题 | 主文件 | 推荐入口 / 第一 RVV 目标 | 依据模式 / 证据来源 | 状态 | 当前结论 / 下一步条件 |
| ---: | --- | --- | --- | --- | --- | --- |
| 4 | cone 法线距离诊断 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_cone.hpp` | 可重新复筛；以 count/select component ablation 为首阶段，不直接承诺 production 接入 | cylinder 正向依赖已解除；cone 源码复杂度仍高 | dependency_unblocked / needs own topic | cylinder 正向只解除等待条件；cone 仍需独立函数级评估、退化点测试、反汇编和板卡证据。 |
| 5 | registration 归属 / profile | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_registration.hpp` | 暂不启动；先确认 workload/profile 和模块归属 | 双索引 gather；map/SVD/Eigen 稀释风险；registration 模块经验 | 暂缓 / 不单独实施 | 有 sample_consensus registration 热点证据或明确不与 registration 模块重复时再启动。 |
| 6 | MLESAC 后处理诊断 | `sample_consensus/include/pcl/sample_consensus/impl/mlesac.hpp` | 暂不启动；模型距离核完成后再看 EM / final compress 占比 | tail-compress 稀释；MSAC/RMSAC 边界；plane_models MLESAC 回归 | 暂缓 / 不单独实施 | 只有 profile 显示后处理仍是热点，或用户明确要关闭 `exp/log` diagnostic 风险时再启动。 |

本轮复筛没有发现需要补充到 workflow、rvv-test、implementation 或 documentation skill 的通用新规则。后续若按本表推进，第一条已完成主题是 `cylinder 法线距离诊断`，第二条已完成主题是 `normal-sphere 法线距离诊断`。`circle3d 投影核诊断` 已完成当前候选的生产探针，接入后证据不支持采纳。

## 7. Closeout

- 输出文档：`doc-rvv/library-screening/sample_consensus/sample_consensus-retained-candidate-rescreen.zh.md`
- 保留实施候选输入总数：6。
- 复筛后建议启动 / 已推进函数级评估：3。
- 复筛后暂缓 / 不单独实施：3。
- 已完成 production-adopted 主题：`cylinder 法线距离诊断`、`normal-sphere 法线距离诊断`。
- 当前第一条仍未 production-adopted 的保留候选：`circle3d 投影核诊断` 已完成 production probe 但不采纳；默认下一可重新评估项是 `cone 法线距离诊断`，需另开 topic。
- 当前新增正式主题文档：`doc-rvv/sample_consensus/sac_model_cylinder-RVV.zh.md`、`doc-rvv/sample_consensus/sac_model_normal_sphere-RVV.zh.md`。
