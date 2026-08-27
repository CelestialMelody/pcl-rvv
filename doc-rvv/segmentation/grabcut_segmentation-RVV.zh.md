# GrabCut RVV 生产实现

## 当前状态

`pcl::GrabCut<PointT>` 当前已有 adopted production behavior（已采纳生产行为）：在 `__RVV10__`
构建中，`GrabCut<PointT>::initGraph()` 会先尝试私有 helper `initGraphTerminalWeightsRVV()`，
批量计算 `TrimapUnknown` 的 terminal weight（端点权重）并写入 Boykov-Kolmogorov graph terminal
capacities（图端点容量）。如果 RVV helper 因输入规模或 unknown 点不足返回 false，则继续执行原标量分支。
非 RVV 构建不包含该 helper，行为保持原标量实现。

`pcl::segmentation::grabcut::learnGMMs()` 的 Phase 110 production patch（生产补丁）也已由用户确认采纳。
该 patch 在 `__RVV10__` 构建中只接管 component assignment（分量归属选择）子阶段，GaussianFitter
relearn（高斯拟合器重新训练）保持标量。production-detail（生产细节）和 production-public
（公开生产入口）板卡证据均为 positive，Evidence Doctor（证据体检）均无 finding，因此当前写成
adopted production behavior（已采纳生产行为）。

当前已采纳范围很窄，但包含两个生产行为：`initGraph()` 中 unknown trimap 的 background / foreground
GMM（Gaussian Mixture Model，高斯混合模型）probability density（概率密度）和 `-log` terminal cost
计算，以及 `learnGMMs()` 的 component assignment（分量归属选择）子阶段。fixed-label trimap
（固定标签）、n-link graph edge mutation（邻接边图写入）、max-flow solver（最大流求解器）、
color staging（颜色暂存）、non-organized KNN、公共 API（公开接口）和 `Scalar=double` 都保持标量或未覆盖状态。
Phase 110 的 `learnGMMs()` patch 覆盖的是 `Image<Color>`、ordered `Indices`、float `Color`、K=5 GMM
和 hard segmentation mask；不扩大到上述未覆盖范围。

用户已确认按板卡 positive 采纳 `initGraph()` helper。接入后的 production-public（真实公开入口）补证据也为 positive：
Milkv-Jupiter clean 96x72 5-run `public_extract` median B/A 为 `1.112866x`，Evidence Doctor（证据体检）
为 `Errors=0, Warnings=0, Suggestions=0`。Phase 110 再次补测接入 `learnGMMs()` 后的公开入口，
Milkv-Jupiter clean 96x72 5-run `public_extract` median B/A 为 `1.207907x`，Evidence Doctor 仍为
`Errors=0, Warnings=0, Suggestions=0`。本文档记录当前已采用实现；详细阶段计划和失败 /
历史 run 归属见 topic-local phase 文档。

Phase 120 接入后 `public_extract_profile` diagnostic-profile（诊断剖析）显示完整 public-shaped profile
仍为正向，Milkv-Jupiter clean 96x72 5-run median B/A 为 `1.290653x`；但 `learn_gmms` 在 RVV profile
中的中位占比只有 `4.592618%`，低于继续优化阈值。该结果不改变已采纳 production behavior，只说明当前
topic 内没有值得自动继续推进的新 RVV helper。

## 函数语义和标量路径

GrabCut 用前景 / 背景种子训练 GMM，再反复构图、求解 min-cut（最小割）并更新 hard segmentation
（硬分割）。公开入口主要从 `setBackgroundPointsIndices()` 初始化背景索引，再通过 `extract()` 执行
`refine()` 直到收敛。

| 阶段 | 标量行为 | 输出 / 后续消费者 |
| --- | --- | --- |
| `initCompute()` | 检查输入点云是否含 RGB/RGBA 字段，把点云颜色转换为 `Image<Color>`，初始化 `trimap_`、`hard_segmentation_`、GMM 和 `n_links_`。 | `fitGMMs()`、`computeNLinks*()` 和 `initGraph()` 使用这些对象状态。 |
| `fitGMMs()` | 用当前 trimap / hard segmentation 建立 foreground 和 background GMM。 | `initGraph()` 读取两个 GMM 计算 terminal weights。 |
| `learnGMMs()` | 按 hard segmentation 为每个像素选择 foreground/background GMM 的最佳 component，再按 component vector 重新训练两个 GMM。 | `refineOnce()` 每轮更新 GMM 后再进入 `initGraph()`。 |
| `computeBetaOrganized()` / `computeNLinksOrganized()` | organized cloud 中按右、下、右下、左下邻接计算 color distance、beta 和 n-link 权重。 | `initGraph()` 写 graph n-link edges。 |
| `computeBetaNonOrganized()` / `computeNLinksNonOrganized()` | non-organized cloud 中先做 KNN，再计算邻接权重。 | `initGraph()` 写 graph n-link edges。 |
| `initGraph()` | 为每个 index 建 graph node，按 trimap 写 terminal weights，再写 n-link edges。 | `refineOnce()` 后续调用 graph solver。 |
| `refineOnce()` / `refine()` | 学习 GMM、重建 graph、求解 max-flow、更新 hard segmentation，直到 changed pixel 为 0。 | `extract()` 输出 foreground cluster。 |

标量 terminal unknown 分支对每个 `point_index` 执行：

```cpp
fore = static_cast<float> (-std::log (background_GMM_.probabilityDensity ((*image_)[point_index])));
back = static_cast<float> (-std::log (foreground_GMM_.probabilityDensity ((*image_)[point_index])));
setTerminalWeights (graph_nodes_[i_point], fore, back);
```

每次 `probabilityDensity(c)` 会遍历 K=5 个 Gaussian component（高斯分量），对 RGB 三维差值计算二次型、
执行 `exp(-0.5 * d)`，再乘以 `pi / sqrt(determinant)` 后累加。

标量 `learnGMMs()` 分两段执行。第一段遍历 `indices`，根据 `hard_segmentation[idx]` 选择 foreground 或
background GMM，然后调用 `whichComponent((*image)[point_index])` 选择概率最大的 K=5 component。第二段按
`components[idx]` 把颜色样本累加到 foreground / background GaussianFitter，最后调用 `fit()` 更新两个 GMM。
Phase 110 RVV patch 只替换第一段 component selection；第二段保留标量，避免改变 bucket accumulation（按桶累加）
顺序和空 component 语义。

## 当前采用的优化方式

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| production dispatch | adopted | `initGraph()` 在 graph node allocation 后尝试 RVV helper，成功后只跳过 unknown terminal 标量分支；public API 不变。 | `make run_test_compare`；Phase 060 production-detail board。 | fixed-label trimap 和 n-link 分支仍在原循环中处理。 |
| unknown terminal batch | adopted | unknown 点通常成批出现，K=5 GMM probability 的 per-pixel 公式适合 VL chunk（可变向量长度分块）。 | Phase 060 median `3.127968x`；Phase 070 public median `1.112866x`。 | 只证明当前 `PointXYZRGB` / `Image<Color>` / float GMM 边界。 |
| color gather / staging | adopted inside helper | helper 先收集 unknown point indices 和 graph nodes，再按 VL chunk 把 `Image<Color>` 中 RGB 值暂存到连续 buffer 后向量加载。 | production direct test 与 asm attribution。 | 额外 staging 成本已由 production-detail board 覆盖；不覆盖 `initCompute()` RGB/RGBA conversion。 |
| probability accumulation | adopted | 对每个 Gaussian component 使用 RVV FMA（融合乘加）计算三维二次型，并调用 `pcl::expf_RVV_f32m2`。 | `initGraphTerminalWeightsRVV` 反汇编含 `vle32.v`、`vfmacc.*`、`vfadd.vv`、`vse32.v`。 | terminal `std::log` 和 graph terminal write 仍为标量 tail。 |
| `learnGMMs()` component assignment | adopted | Phase 100 已证明 full local shape 正向；Phase 110 接入真实 production 后 production-detail 和 production-public 均正向，并已由用户确认采纳。 | production-detail median `2.755010x`；接入后 public `extract()` median `1.207907x`；两层 Doctor `0/0/0`。 | GaussianFitter accumulation 仍保持标量。 |
| fixed-label trimap | scalar fallback | fixed foreground/background 分支只是常量 terminal cost，不值得单独 RVV 化，且必须保持原标签语义。 | `GrabCutProductionDirect.RvvTerminalWeightsMatchScalarAndFixedLabelsFallback`。 | 保持标量。 |
| n-link graph edge mutation | not_now | 真实 `graph_.addEdge` 是图结构写入，历史 organized n-link component 约 `0.98x`，不支持直接接 production。 | optimization matrix 中 organized n-link weak-negative diagnostic。 | 只有 profile 指向 n-link 主导时再开 phase。 |
| max-flow solver | rejected | `BoykovKolmogorov::solve` 是 map/deque/parent/orphan 状态机，不适合当前 RVV 直接改写。 | evaluation 和 roadmap 的 rejected with evidence。 | 保持标量。 |

## RVV 路径

`initGraphTerminalWeightsRVV()` 先扫描 `indices_`，只收集 `trimap_[point_index] == TrimapUnknown` 的
`point_index` 和对应 `graph_nodes_[i_point]`。unknown 数量少于 2 时返回 false，让原标量循环处理。

收集完成后，helper 为 background 和 foreground 分别维护一个 probability buffer。对每个 GMM component：

1. 跳过 `pi <= 0` 或 `determinant <= 0` 的无效 component。
2. 按当前 VL 从 `Image<Color>` 取出 RGB，减去 `G.mu` 后写入连续 scratch buffer。
3. 用 `vle32` 加载 `r/g/b`，通过 FMA 计算 `d = [r,g,b] * inverse * [r,g,b]^T`。
4. 调用 `pcl::expf_RVV_f32m2(-0.5f * d)`，乘以 `G.pi / sqrt(G.determinant)`。
5. 把该 component 的 weighted probability 累加到当前 GMM 的 probability buffer。

两个 probability buffer 计算完成后，helper 回到标量 tail：逐 unknown 点执行 `-std::log(probability)`，
并调用原 `setTerminalWeights()` 写 graph terminal capacities。这样做保留了 graph 写入、符号约定和负 capacity
归一逻辑，也避免把当前阶段扩大到 graph edge mutation 或 solver。

Phase 110 的 `learnGMMsRVV()` 使用同一类 K=5 GMM probability 公式，但目标从 terminal cost 改为 component
selection。helper 先把 foreground 和 background 的样本位置分组；每个分组按 VL chunk 读取 `Image<Color>`，
为 K=5 component 分别计算 probability，并用向量比较保留当前最大 probability 和 component id。结果写回
`components[idx]` 后，后续 `relearnGMMsFromComponentsStd()` 继续用原标量 GaussianFitter 累加和 `fit()`。
小输入或非 RVV 构建会走 `learnGMMsStd()`，公开 free function 不改变 ABI（应用二进制接口）或调用者参数。

## 覆盖范围与 fallback

| 条件 | 当前行为 | 证据 |
| --- | --- | --- |
| 非 `__RVV10__` 构建 | 不编译 RVV helper，`initGraph()` 完整走标量 terminal 分支。 | Std 4/4 correctness passed。 |
| `indices_->size() < 2` 或 unknown 点少于 2 | RVV helper 返回 false，原标量循环处理 unknown terminal weights。 | 源码 gate；fallback 语义由 production direct test 覆盖。 |
| fixed foreground / fixed background trimap | RVV helper 不处理，原 `initGraph()` switch 分支写常量 terminal costs。 | fixed-label fallback test passed。 |
| background / foreground GMM component 无效 | RVV helper 跳过 `pi <= 0` 或 `determinant <= 0` component，与标量 probability density 的有效 component 语义一致。 | production direct 数值对拍。 |
| n-link edge、organized / non-organized neighbor graph | 保持标量 `addEdge` 路径。 | 未作为当前 adopted 范围。 |
| public `extract()` | 间接命中已采纳 `initGraph()` helper；其余 pipeline 保持原实现。 | Phase 070 production-public board positive。 |
| `learnGMMs()` 小输入 | RVV helper 返回 false，随后调用 `learnGMMsStd()`。 | `GrabCutProductionDirect.LearnGMMsSmallInputFallbackMatchesReference`。 |
| `learnGMMs()` normal input in RVV build | 真实 production free function 先尝试 `learnGMMsRVV()`，成功后不再执行 assignment 标量主体。 | Phase 110 production direct correctness、asm 和 board evidence。 |
| `Scalar=double`、自定义点型或其它 color layout | 当前没有独立性能证据，不把 `PointXYZRGB` 结果外推。 | roadmap / matrix 标为 deferred 或 not covered。 |

## 范围决策表

| 范围 | 状态 | 证据 | 下一步 |
| --- | --- | --- | --- |
| `PointXYZRGB` / organized `Image<Color>` / unknown terminal batch | adopted | Phase 060 production-detail 5-run median `3.127968x`；Doctor 0 / 0 / 0。 | 当前生产实现保留。 |
| `PointXYZRGB` / public `setBackgroundPointsIndices()` + `extract()` | adopted evidence | Phase 070 clean 96x72 5-run median `1.112866x`；checksum 一致；Doctor 0 / 0 / 0。 | 说明完整公开入口仍有可见收益。 |
| production `learnGMMs()` RVV assignment | adopted | Phase 110 production-detail 5-run median `2.755010x`；Doctor 0 / 0 / 0；用户确认按接入后板卡 positive 采纳。 | 当前生产实现保留；GaussianFitter accumulation 仍保持标量。 |
| public `extract()` after `learnGMMs()` integration | adopted evidence | Phase 110 clean 96x72 5-run median `1.207907x`；checksum 一致；Doctor 0 / 0 / 0。 | 证明接入后完整公开入口仍有正向收益，但不能单独归因到一个 helper。 |
| post-adoption `public_extract_profile` | profile_non_actionable | Phase 120 clean 96x72 5-run median `1.290653x`；RVV `learn_gmms` 中位占比 `4.592618%`；Doctor 0 / 0 / 0。 | 不触发 GaussianFitter accumulation、LMUL / ILP variants 或其它新 helper。 |
| fixed-label trimap | scalar fallback | production direct fallback test。 | 保持标量。 |
| organized n-link | attempted / not promoted | 历史 all-case board 观察约 `0.98x`。 | 只有 profile 指向 n-link 主导时重开。 |
| non-organized KNN n-link | deferred | KNN search 和不规则 neighbor list 会主导风险。 | 需要 profile 或 component A/B。 |
| color staging from RGB/RGBA point cloud | deferred / profile required | 当前没有证据证明 `initCompute()` color conversion 主导公开入口。 | 需要 traits / fallback plan 和板卡证据。 |
| max-flow solver | rejected with evidence | 状态机密集，当前不适合 RVV。 | 保持标量。 |

## VL chunk 算例

假设一个 VL chunk 中有 4 个 unknown 点，RGB 颜色分别为 `c0..c3`，某个 Gaussian 的均值为 `mu`，
逆协方差矩阵为 `M`，权重为 `pi / sqrt(det)`。RVV helper 对每个 lane 计算：

```text
delta = color - mu
d     = delta.r * (M00*r + M10*g + M20*b)
      + delta.g * (M01*r + M11*g + M21*b)
      + delta.b * (M02*r + M12*g + M22*b)
p     = exp(-0.5 * d) * pi / sqrt(det)
```

所有 K=5 component 的 `p` 累加到同一个 probability buffer。background 和 foreground buffer 完成后，
标量 tail 对同一 lane 的两个 probability 分别取 `-log`，再调用 `setTerminalWeights(graph_node, fore, back)`。
因此 RVV 接管的是 per-color GMM 数学公式；graph terminal capacity 的符号归一和写入仍复用原实现。

## Bench 与证据

| 证据 | 命令 / 路径 | 结果 | 说明 |
| --- | --- | --- | --- |
| correctness | `make -C test-rvv/segmentation/grabcut_segmentation run_test_compare` | Std 4/4、RVV 6/6 passed。 | 覆盖 diagnostic reference、production direct 和 fixed-label fallback。 |
| QEMU smoke | `make -C test-rvv/segmentation/grabcut_segmentation run_bench_std BENCH_ARGS='--width 32 --height 24 --iterations 1 --warmup 0 --case public_extract'`；RVV 同命令 | Std/RVV checksum 均为 `13677801866028019573`。 | QEMU 只证明可运行和日志形状，不作为性能结论。 |
| asm attribution | `make -C test-rvv/segmentation/grabcut_segmentation dump_bench_rvv` | `initGraph()` 调用 `initGraphTerminalWeightsRVV()`；helper 符号内有 RVV load/store/FMA。 | 证明生产二进制包含目标 helper。 |
| production-detail board | `doc/phases/060-production-initgraph-terminal-evidence/repeated-evidence-manifest.json` | Milkv-Jupiter 640x480 5-run B/A `3.1292, 3.1280, 3.0998, 3.0944, 3.1982`，median `3.127968x`。 | 真实 `initGraph()` terminal helper 边界，不含 n-link edge 或 solver。 |
| production-detail Doctor | `doc/phases/060-production-initgraph-terminal-evidence/repeated-evidence-doctor.md` | `Errors=0, Warnings=0, Suggestions=0`。 | 支撑 PI5 采纳。 |
| production-public board | `doc/phases/070-public-extract-wall-time-adoption-check/repeated-evidence-manifest.json` | Milkv-Jupiter clean 96x72 5-run B/A `1.124687, 1.110452, 1.112645, 1.113482, 1.112866`，median `1.112866x`。 | 真实 `setBackgroundPointsIndices()` + `extract()` wall time。 |
| production-public Doctor | `doc/phases/070-public-extract-wall-time-adoption-check/repeated-evidence-doctor.md` | `Errors=0, Warnings=0, Suggestions=0`。 | 证明接入后公开入口仍为 positive bucket。 |
| `learnGMMs()` correctness | `make -C test-rvv/segmentation/grabcut_segmentation run_test_compare` | Std 8/8、RVV 10/10 passed。 | 覆盖真实 production `learnGMMs()` 和小输入 fallback。 |
| `learnGMMs()` QEMU smoke | `make -C test-rvv/segmentation/grabcut_segmentation run_bench_std BENCH_ARGS='--width 32 --height 24 --iterations 1 --warmup 0 --case production_learn_gmms'`；RVV 同命令 | Std/RVV checksum 均为 `11269213739271905300`。 | QEMU 只证明可运行和日志形状。 |
| `learnGMMs()` asm attribution | `make -C test-rvv/segmentation/grabcut_segmentation dump_bench_rvv` | `learnGMMsRVV` / `assignGMMComponentsForGroupRVV` 符号区域含 `vle32.v`、`vse32.v`、`vfmacc.vf`、`vfmacc.vv`。 | 证明 Phase 110 生产 helper 有 RVV 指令归属。 |
| `learnGMMs()` production-detail board | `doc/phases/110-learn-gmms-production-integration-plan/repeated-evidence-manifest.json` | Milkv-Jupiter clean 96x72 5-run B/A `2.676933, 2.755010, 2.782530, 2.743741, 2.838160`，median `2.755010x`。 | 真实 `learnGMMs()` free function，不含 `initGraph()` 或 solver。 |
| `learnGMMs()` production-detail Doctor | `doc/phases/110-learn-gmms-production-integration-plan/repeated-evidence-doctor.md` | `Errors=0, Warnings=0, Suggestions=0`。 | 支持 PI5 positive。 |
| post-`learnGMMs()` production-public board | `doc/phases/110-learn-gmms-production-integration-plan/public-repeated-evidence-manifest.json` | Milkv-Jupiter clean 96x72 5-run B/A `1.197422, 1.218683, 1.191227, 1.215751, 1.207907`，median `1.207907x`。 | 真实 `setBackgroundPointsIndices()` + `extract()` wall time，包含已采纳 terminal helper 和已采纳 `learnGMMs()` helper。 |
| post-`learnGMMs()` production-public Doctor | `doc/phases/110-learn-gmms-production-integration-plan/public-repeated-evidence-doctor.md` | `Errors=0, Warnings=0, Suggestions=0`。 | 证明接入后公开入口仍为 positive bucket。 |
| post-adoption profile | `doc/phases/120-post-learn-gmms-adoption-profile/repeated-evidence-manifest.json` | Milkv-Jupiter clean 96x72 5-run B/A `1.232341, 1.290653, 1.321640, 1.293081, 1.284386`，median `1.290653x`；RVV `learn_gmms` 中位占比 `4.592618%`。 | 只用于判断是否继续当前 topic；不替代 production-public 采纳证据。 |
| post-adoption profile Doctor | `doc/phases/120-post-learn-gmms-adoption-profile/repeated-evidence-doctor.md` | `Errors=0, Warnings=0, Suggestions=0`。 | 支撑停止继续写新 helper 的判断。 |
| evidence registry | `make -C test-rvv/segmentation/grabcut_segmentation evidence_status` | final verification 时要求 fresh。 | registry 负责防止 summary artifact 被覆盖后文档未刷新。 |

raw board logs（原始板卡日志）默认 local-only。summary-only（只提交摘要）策略下，长期文档只引用 manifest、
Doctor、phase result 和 evaluation，不引用 raw repeated run 目录作为默认提交内容。

## 正确性与高效性证据链

Correctness（正确性）由 `run_test_compare` 支撑：Std 构建覆盖标量 reference；RVV 构建覆盖 diagnostic
candidate、真实 `GrabCut<PointXYZRGB>::initGraph()` production direct 对拍，以及 fixed-label fallback。
QEMU smoke 证明 `public_extract` 的 Std/RVV checksum 一致，但 QEMU timing 不参与性能判断。

Path / asm evidence（路径 / 反汇编证据）显示 `initGraph()` 命中 `initGraphTerminalWeightsRVV()`，helper
范围内有 RVV load/store/FMA 和 `expf_RVV_f32m2` 链路；Phase 110 也显示 `learnGMMsRVV` /
`assignGMMComponentsForGroupRVV` 符号区域含 RVV load/store/FMA。Performance（性能）结论只引用
Milkv-Jupiter repeated board summary：已采纳 terminal helper 的 production-detail median `3.127968x`，
Phase 070 production-public median `1.112866x`；已采纳 `learnGMMs()` patch 的 production-detail median
`2.755010x`，接入后 production-public median `1.207907x`。Boundary（证据边界）只覆盖当前 helper、
`PointXYZRGB` public bench、float `Color` / GMM 和 Milkv-Jupiter。Risk（风险）是完整 pipeline 中
n-link 和 solver 仍占明显成本，因此公开入口收益小于 production-detail；该稀释不反对保留当前 helper。
Phase 120 profile 进一步说明剩余优化空间已经转向 graph mutation / solver 主导边界：`learn_gmms`
剩余占比低于 `5%`，因此当前不继续扩展 GaussianFitter accumulation 或 `learnGMMsRVV()` 实现族。

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `GrabCut<PointT>::setBackgroundPointsIndices` | production public entry | 设置背景 trimap 并触发初始化。 | 用户代码 / `public_extract` bench | `initCompute()`、`fitGMMs()` | public-entry setup | `segmentation/include/pcl/segmentation/impl/grabcut_segmentation.hpp` |
| `GrabCut<PointT>::extract` | production public entry | 执行 `refine()` 并输出 foreground cluster。 | 用户代码 / `public_extract` bench | `refine()`、`initGraph()` | production-public performance | `segmentation/include/pcl/segmentation/impl/grabcut_segmentation.hpp` |
| `GrabCut<PointT>::initGraph` | production dispatch / fallback | 创建 graph node、写 terminal weights 和 n-link edges。 | `refineOnce()` | `initGraphTerminalWeightsRVV()`、`setTerminalWeights()`、`addEdge()` | production boundary | 同上 |
| `GrabCut<PointT>::initGraphTerminalWeightsRVV` | production RVV helper | 批量计算 unknown terminal weights 并写 graph terminal capacities。 | `initGraph()` | `pcl::expf_RVV_f32m2`、`setTerminalWeights()` | adopted production path / asm | 同上 |
| `segmentation/src/grabcut_segmentation.cpp` GMM backend | production scalar/RVV backend | 保存 `GMM::probabilityDensity` 的标量数学语义，并在 `__RVV10__` 下提供 `learnGMMsRVV()` assignment path。 | `initGraph()`、`learnGMMs()` | terminal weights、component assignment | scalar formula source / Phase 110 production helper | `segmentation/src/grabcut_segmentation.cpp` |
| `src/test_grabcut.cpp` | correctness gate | diagnostic、production direct 和 fallback tests。 | `make run_test_compare` | gtest assertions | correctness / fallback | `test-rvv/segmentation/grabcut_segmentation/src/test_grabcut.cpp` |
| `src/bench_grabcut.cpp` | bench wrapper | `production_initgraph_terminal` 与 `public_extract` case。 | QEMU / board bench targets | BENCH log、manifest script | production-detail / production-public performance | `test-rvv/segmentation/grabcut_segmentation/src/bench_grabcut.cpp` |
| `script/generate_grabcut_board_evidence_manifest.py` | analysis script | 把 `BENCH grabcut_component` 行转换为 Evidence Doctor manifest。 | Doctor targets | `test-rvv/script/evidence_doctor.py` | evidence boundary check | `test-rvv/segmentation/grabcut_segmentation/script/generate_grabcut_board_evidence_manifest.py` |
| Phase 060 summary | evidence output summary | production-detail board 和 Doctor。 | `run_production_repeated_evidence_doctor` | 本文档、evaluation、Handoff | PI5 adoption evidence | `test-rvv/segmentation/grabcut_segmentation/doc/phases/060-production-initgraph-terminal-evidence/` |
| Phase 070 summary | evidence output summary | production-public board 和 Doctor。 | `run_public_extract_repeated_evidence_doctor` | 本文档、evaluation、Handoff | post-adoption public evidence | `test-rvv/segmentation/grabcut_segmentation/doc/phases/070-public-extract-wall-time-adoption-check/` |
| Phase 110 summary | evidence output summary | `learnGMMs()` production-detail board、接入后 production-public board 和 Doctor。 | `run_production_learn_gmms_repeated_evidence_doctor`、`run_production_learn_gmms_public_repeated_evidence_doctor` | 本文档、evaluation、Handoff | adopted production behavior after user confirmation | `test-rvv/segmentation/grabcut_segmentation/doc/phases/110-learn-gmms-production-integration-plan/` |
| Phase 120 summary | evidence output summary | 两个 helper 均采纳后的 public component profile 和停止判断。 | `run_post_learn_gmms_profile_repeated_evidence_doctor` | evaluation、Handoff | profile_non_actionable | `test-rvv/segmentation/grabcut_segmentation/doc/phases/120-post-learn-gmms-adoption-profile/` |
| function evaluation | evaluation | 保存候选取舍、测试矩阵、Traceability Map 和停止理由。 | phase results | 本文档、筛选队列 | decision audit | `test-rvv/segmentation/grabcut_segmentation/doc/grabcut_segmentation-evaluation.zh.md` |
| optimization roadmap / matrix | phase docs | 保存候选搜索空间和 evidence matrix。 | phase loop | Handoff / reviewer | recovery pointer | `test-rvv/segmentation/grabcut_segmentation/doc/optimization-roadmap.zh.md`、`doc/phases/optimization-matrix.zh.md` |

## 生产接入后的 closeout

| 文件 / 入口 | 状态 | 说明 |
| --- | --- | --- |
| `segmentation/include/pcl/segmentation/grabcut_segmentation.h` | adopted | 在 `__RVV10__` 下新增私有 `initGraphTerminalWeightsRVV()` 声明；public API 不变。 |
| `segmentation/include/pcl/segmentation/impl/grabcut_segmentation.hpp` | adopted | 新增 RVV helper，并在 `initGraph()` 中以 helper 成功标志跳过 unknown terminal 标量分支。 |
| `segmentation/src/grabcut_segmentation.cpp` | adopted | 新增 `learnGMMsStd()` / `learnGMMsRVV()` 分层，公开 free function 在 RVV 构建中先尝试 RVV assignment 后回退 Std；public API 不变。 |
| `test-rvv/segmentation/grabcut_segmentation` | adopted evidence assets | 保存 correctness、bench、manifest、Evidence Doctor、roadmap、matrix 和 phase docs。 |
| `doc-rvv/library-screening/segmentation/segmentation-retained-candidate-rescreen.zh.md` | updated | GrabCut 队列状态更新为 adopted production behavior。 |

当前无需回滚已采纳的 `initGraph()` helper 或 `learnGMMs()` helper。用户已确认 Phase 110 的
`learnGMMs()` patch 可按接入后板卡 positive 采纳。若生产证据需要重跑，必须先刷新 Phase 060 /
Phase 070 / Phase 110 / Phase 120 summary artifact、Evidence Doctor、registry、evaluation 和本文档，
再更新筛选队列。

## 后续方向

Phase 120 已完成接入后的 public component profile（公开入口组件剖析）。当前不建议继续自动推进新的
RVV helper：GaussianFitter accumulation 和 `learnGMMsRVV()` LMUL / ILP variants 没有足够剩余热点；
organized n-link 早期诊断约 `0.98x`，不建议直接生产扩展；max-flow solver 是状态机，当前拒绝 RVV
重写；non-organized KNN 和 color staging 仍需要新的 profile 或 component A/B 证明。恢复条件是新的
公开入口 profile、输入形态或用户新 scope 证明某个剩余组件重新成为主成本，并能闭合 correctness、
asm、board repeated 和 Evidence Doctor。
