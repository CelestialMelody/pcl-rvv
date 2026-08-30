# keypoints 模块 RVV 保留候选复筛

本文档对 `doc-rvv/library-screening/keypoints/keypoints-function-evaluation-queue.zh.md` 第 5 节的 6 个“保留实施的候选文件”执行保留候选复筛。复筛目的不是重跑第二轮 high/mid，也不是补入 low 文件，而是结合已完成的 keypoints 主题与当前源码，再判断这些保留候选里哪些值得进入下一轮函数级评估，哪些应继续暂缓。

本轮不修改 production 源码，不建立 `test-rvv` topic，不运行板卡 bench，也不提交。

## 1. 输入依据与复筛原因

- 阶段规则：`.agents/skills/rvv-screening/SKILL.md`
- 候选标准：`.agents/skills/rvv-screening/references/screening-criteria.md`
- 队列规则：`.agents/skills/rvv-screening/references/stage-and-queue-policy.md`
- 证据边界：`.agents/skills/rvv-screening/references/evidence-boundaries.md`
- 保留候选复筛模板：`.agents/skills/rvv-screening/references/templates/retained-candidate-rescreen-template.md`
- 第二轮函数评估队列：`doc-rvv/library-screening/keypoints/keypoints-function-evaluation-queue.zh.md`
- 已完成主题文档：`doc-rvv/keypoints/harris_2d-RVV.zh.md`
- 已完成主题文档：`doc-rvv/keypoints/trajkovic_2d_response_grid-RVV.zh.md`
- 已完成主题文档：`doc-rvv/keypoints/trajkovic_3d-RVV.zh.md`
- 已完成主题文档：`doc-rvv/keypoints/brisk_2d-RVV.zh.md`
- 已完成主题 topic-local 文档：`test-rvv/keypoints/iss_3d/doc/iss_3d-evaluation.zh.md`

复筛原因是：keypoints 目录里已经出现了 4 条真实完成的主题，且它们给出了一组清晰的边界条件。`harris_2d`、`trajkovic_2d_response_grid`、`trajkovic_3d` 证明 organized response grid / fixed-neighbor stencil 这类路径可以在窄范围内采纳；`brisk_2d` 则说明 helper chain 可以局部正向，但公开入口未必一起正向。这个结果足以重新排序保留候选，而不需要重跑第二轮。

## 2. 筛选统计

| 项目 | 数量 | 说明 |
| ---- | ---: | ---- |
| 保留实施候选输入总数 | 6 | 仅来自第二轮第 5 节 |
| 已完成执行清单 | 2 | `ISS 3D` 已完成 no-production closeout；`SIFT` 已完成窄范围 production adoption |
| 当前仍建议启动函数级评估 | 0 | 本复筛文档中的建议项均已执行或进入完成状态 |
| 暂缓 / 不单独实施 | 4 | 当前更像 search / Eigen / 状态机 / 动态集合主导 |
| 重新纳入 | 0 | 未从队列外补入候选 |
| 合并候选 | 0 | 无需合并主题 |
| 删除或源码冲突 | 0 | 未发现候选路径失效 |

`建议启动函数级评估` 的默认评估路径分布：

| 默认评估路径 / 当前状态 | 数量 | 候选 |
| ----------------------------- | ---: | ---- |
| `component ablation -> no-production closeout` | 1 | `impl/iss_3d.hpp` |
| `production adopted narrow scope` | 1 | `impl/sift_keypoint.hpp` |

## 3. 已完成主题经验总结

### 3.1 已完成主题证据包

| 主题 | 主文件 | 函数评估队列原始定位 | 实际覆盖范围 | 目标硬件结论 | 正确性证据 | 反汇编证据 | 生产接入状态 | 回退 / 暂缓原因 |
| ---- | ------ | -------------------- | ------------ | ------------ | ---------- | ---------- | ------------ | ---------------- |
| Harris 2D organized response | `keypoints/include/pcl/keypoints/impl/harris_2d.hpp` | 建议启动主题 1 | `Scalar=float`、organized dense input、public `compute()`、`nonmax=false` 的 response map | production-public 5-run 三组 case median `1.070x / 1.090x / 1.180x` | Std/RVV 各 7 tests pass | `responseRVV()` 归属到 public compute 内联范围 | adopted narrow-scope production RVV | NMS、泛型 `IntensityT` 和更广点型仍需另开 phase |
| Trajkovic 2D response grid | `keypoints/include/pcl/keypoints/impl/trajkovic_2d.hpp` | 建议启动主题 2 | `PointXYZI`、默认 accessor、organized full-cloud、`window_size == 3`、EIGHT_CORNERS | production-public 5-run median `1.725x` 和 `1.442x` | Std/RVV 各 4 tests pass | 命中 `vlse32.v` / `vfmul.vv` / `vfmin.vf` / `vfdiv.vf` / `vse32.v` | adopted narrow-scope production RVV | FOUR_CORNERS、RGB accessor、非 3x3 和 NMS 保持标量 |
| Trajkovic 3D normal response grid | `keypoints/include/pcl/keypoints/impl/trajkovic_3d.hpp` | 建议启动主题 3 | `FOUR_CORNERS` / `EIGHT_CORNERS`、`window_size=3`、precomputed normals、organized dense full cloud | production-public 5-run median `1.790x` / `1.677x` | Std/RVV 各 10 tests pass | 命中 response helper 内联 RVV 指令 | adopted narrow-scope production RVV / topic closed | normal estimation、NMS 和更宽点型不在当前采纳范围 |
| BRISK downsample helper chain | `keypoints/src/brisk_2d.cpp` | 建议启动主题 4 | `Layer::halfsample()`、`Layer::twothirdsample()`、`ScaleSpace::constructPyramid()` | helper 链 weak-positive；public `compute()` 近中性 `1.017x` | Std/RVV 对拍通过 | downsample helper 命中 `vlse8` / `vsse8` 等指令 | adopted / topic closeout | AGAST/OAST detector 和 descriptor 仍是标量，public 入口没有同等幅度收益 |
| ISS 3D scatter matrix | `keypoints/include/pcl/keypoints/impl/iss_3d.hpp` | 保留候选复筛建议项 1 | `getScatterMatrix` 的 f64 scatter component ablation；production probe 只覆盖 public `compute()` 中的 scatter 累加 | diagnostic 5-run median `1.662x / 1.859x / 1.213x`；production public median 仅 `1.014x`，bucket 为 `neutral` | Std/RVV 各 4 tests pass | diagnostic helper 命中 indexed xyz load 和 vector reduction；production asm 只作为历史 probe 证据 | rollback/no-production；production patch 已移除，当前生产源码保持标量 | search、Eigen EVD、NMS 和输出构造稀释局部收益；checksum 为 0 的 synthetic public case 不支撑长期采纳 |

### 3.2 可复用模式与失败边界

| 模式标签 | 来自哪些已完成主题 | 成立条件 | 失败 / 回退边界 | 对后续候选的影响 |
| -------- | ------------------ | -------- | ---------------- | ---------------- |
| organized response grid can be adopted | Harris 2D；Trajkovic 2D；Trajkovic 3D | organized 输入、固定 stencil、窄范围 gate、标量 NMS | 只凭“大循环”不能自动升优先级；NMS / 输出顺序仍要单独审计 | `ISS` 的 scatter matrix / response pass 可继续考虑；`harris_3d` 只能从局部 kernel 角度再评估 |
| exact gate and fallback matter | Harris 2D；Trajkovic 2D；Trajkovic 3D | 点型、accessor、window size 和 dense gate 可精确固定 | 泛型布局或更宽 window 没证据时不能外推 | `ISS` / `SIFT` 若要启动，必须先把入口和正确性边界钉住 |
| helper chain or component positive does not imply public positive | BRISK；ISS 3D | helper 链或局部 component 正向，但 public `compute()` 接近中性 | AGAST/OAST detector、descriptor、search、EVD、NMS 和完整 public pipeline 未证实收益 | `src/agast_2d.cpp` 不能因为 BRISK downsample 成功而自动升入建议队列；ISS 类候选必须先证明局部收益能穿透公开入口 |
| search / Eigen / dynamic-state still need split | BRISK 反例 + ISS 3D 反例 + 2D/3D organized 成功边界 | 先找出可复核的局部 kernel，再看 public dilution | 不能把 radiusSearch、Eigen solver、动态集合、排序或状态机当成隐含收益 | `harris_3d`、`harris_6d`、`susan` 先压回暂缓 |

## 4. 筛选口径修正 / 复筛变化理由

1. 这轮不再用“是否有大循环”做主要分界，而是看能否像已完成的 2D organized response / BRISK downsample 那样，形成窄范围、可复核、可回退的入口边界。
2. `BRISK` 结果提醒我们，helper chain 的正向不能直接外推到更大的 detector / descriptor 路径；因此 `src/agast_2d.cpp` 即使靠近 BRISK，也不能仅凭关联关系进入建议队列。
3. `harris_2d` / `trajkovic_2d` / `trajkovic_3d` 证明 organized image / stencil 适合 RVV；相对地，`harris_3d` / `harris_6d` / `susan` 的 search、Eigen 和动态状态没有这类直接边界，必须先压到 component ablation 或 profile prerequisite。
4. `ISS` 和 `SIFT` 是本复筛文档里两类具备明确 test 入口和可分离局部 kernel 的文件；二者都已执行。`SIFT` 的 public board 证据足够窄范围采纳，`ISS` 则证明局部 scatter 收益不能自动穿透 public `compute()` 边界。

## 5. 保留实施候选逐项复筛

### 5.1 已完成 / 建议启动函数级评估

| 主题 / 文件 | 关键入口 | 主成本覆盖类型 | 默认评估路径 / 首阶段证据问题 | 匹配的已验证模式 | 主要风险 | 推荐理由 | 证据来源 |
| ----------- | -------- | -------------- | ----------------------------- | ---------------- | -------- | -------- | -------- |
| ISS 3D salient points / `keypoints/include/pcl/keypoints/impl/iss_3d.hpp` | `getScatterMatrix`、`detectKeypoints` 中的 scatter / non-max / output pass | `rollback/no-production` | 已完成 `component ablation` 和 bounded production probe。局部 scatter diagnostic 为 positive，但 public `compute()` 只有 `1.014x` median、decision bucket 为 `neutral`。 | organized response grid 证明 keypoints 里有窄范围可采纳路径；ISS 3D 反例说明 search / Eigen / NMS 可稀释局部收益。 | `searchForNeighbors`、BoundaryEstimation、Eigen 3x3 solver、NMS 和 checksum 为 0 的 synthetic public case 使长期生产收益不足。 | 当前 topic 已关闭，不建议继续自动推进；只有新增 profile 或非零输出 public case 时再开新 phase。 | `test-rvv/keypoints/iss_3d/doc/iss_3d-evaluation.zh.md`；`test-rvv/keypoints/iss_3d/doc/phases/010-production-integration/result.zh.md`；`tmp/rvv-topic-stats/3/keypoints-iss_3d-stats.zh.md`；commit `9b502c061` |
| SIFT scale-space / `keypoints/include/pcl/keypoints/impl/sift_keypoint.hpp` | `computeScaleSpace`、`findScaleSpaceExtrema`、`detectKeypointsForOctave` | `production adopted narrow scope` | 已完成 `computeScaleSpace()` Gaussian weight loop（高斯权重循环）窄范围 production-public（真实公开入口）接入；`findScaleSpaceExtrema()` 仍保持标量 | Phase 000 诊断显示局部 Gaussian 权重循环值得 probe；Phase 010 public board 显示 5-run median speedup 1.309x，public trace 0 diff | 当前只覆盖 `PointXYZI -> PointWithScale` organized dense synthetic 320x240；真实 workload、其它点型和 extrema scan 仍需新 phase | 当前 retained candidate 已完成窄范围生产采纳；后续只有在 post-adoption profile 或点型扩展需求明确时继续 | `test-rvv/keypoints/sift_keypoint/doc/phases/010-production-full-gaussian-rvv/result.zh.md`；`doc-rvv/keypoints/sift_keypoint-RVV.zh.md`；`keypoints/include/pcl/keypoints/impl/sift_keypoint.hpp` |

### 5.2 暂缓 / 不单独实施

| 主题 / 文件 | 主成本覆盖类型 | 暂缓原因 | 重新考虑条件 | 证据来源 |
| ----------- | -------------- | -------- | ------------ | -------- |
| AGAST/OAST detector / `keypoints/src/agast_2d.cpp` | `diagnostic` | 主 detector 是生成式深分支 / `goto` 决策树；`computeCornerScores` 只是 post-pass，和 BRISK downsample 的正向不能直接互推 | 如果 profile 明确显示 `computeCornerScores` 或 detector row scan 是热点，再做 component ablation | `keypoints/src/agast_2d.cpp`；`doc-rvv/keypoints/brisk_2d-RVV.zh.md` |
| 3D Harris covariance / `keypoints/include/pcl/keypoints/impl/harris_3d.hpp` | `diagnostic` | 每点 `radiusSearch`、Tomasi `eigen33`、refine `LDLT` 和 NMS 叠加，公开入口主成本过重 | 如果能固定 precomputed normals 并把 `calculateNormalCovar` 单独证实为热点，再重开 | `keypoints/include/pcl/keypoints/impl/harris_3d.hpp`；`test/keypoints/test_iss_3d.cpp` |
| 6D Harris covariance / `keypoints/include/pcl/keypoints/impl/harris_6d.hpp` | `diagnostic` | 6x6 `SelfAdjointEigenSolver`、gradient staging 和 21 项 covariance 过重，且没有 keypoints 专项 test | 若先有 precomputed normals / gradients 的稳定 oracle，并证明 21 项 covariance 是主成本，再考虑 | `keypoints/include/pcl/keypoints/impl/harris_6d.hpp` |
| SUSAN keypoint / `keypoints/include/pcl/keypoints/impl/susan.hpp` | `diagnostic` | 每点 `radiusSearch`、动态 `usan` vector 和几何验证主导；当前没有专用 keypoints test | 如果后续有稳定输入和 oracle，且能把 USAN construction 独立成 component，再重开 | `keypoints/include/pcl/keypoints/impl/susan.hpp` |

## 6. 新的执行清单 / 状态表

| 顺序 | 主题 | 主文件 | 推荐入口 / 第一 RVV 目标 | 依据模式 / 证据来源 | 状态 | 当前结论 / 下一步条件 |
| ----: | ---- | ------ | ------------------------ | -------------------- | ---- | -------------------- |
| 1 | ISS 3D salient points | `keypoints/include/pcl/keypoints/impl/iss_3d.hpp` | `getScatterMatrix` scatter component ablation 已完成；production probe 已撤回 | diagnostic 正向但 public `compute()` neutral；commit `9b502c061` | 已完成 | `rollback/no-production`。当前 production 源码保持标量；如要重启，需要新的 profile、非零输出 public case 或用户明确接受 near-threshold production patch 风险。 |
| 2 | SIFT scale-space | `keypoints/include/pcl/keypoints/impl/sift_keypoint.hpp` | `computeScaleSpace` Gaussian weight loop 已接入；`findScaleSpaceExtrema` 保持标量 | Phase 010 production-public board：median speedup 1.309x，Evidence Doctor Errors=0 / Warnings=0；public trace 0 diff | 已完成 | `production-adopted-narrow-scope`。若继续，先做 post-adoption profile 或点型 / workload 扩展 phase，不能把当前 320x240 `PointXYZI` 证据外推。 |

## 7. Closeout

- 输出文档位置：`doc-rvv/library-screening/keypoints/keypoints-retained-candidate-rescreen.zh.md`
- 复筛输入：第二轮第 5 节的 6 个保留实施候选文件
- 结果：本复筛执行清单中的 2 项已完成；`ISS 3D` 为 `rollback/no-production`，`SIFT` 为 `production-adopted-narrow-scope`；`暂缓 / 不单独实施` 仍为 4 项。
- 第一条建议主题 `ISS 3D salient points` 已完成，默认路径 `component ablation` 的局部收益没有穿透 production public 边界。
- 复筛结论：keypoints 模块里真正可继续推进的，仍然是有 test、能拆出独立 kernel、且不会被 search / Eigen / 动态状态一口吞掉的候选；`BRISK` 和 `ISS 3D` 都说明局部 helper / component 收益不能直接外推到完整 public pipeline。
- 本轮没有发现需要补充到 `rvv-screening`、`rvv-test`、`rvv-workflow`、implementation 或 documentation skill 的新通用规则
