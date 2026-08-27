# GrabCut RVV 函数级评估

## 范围和目标源码

本评估覆盖 `segmentation/include/pcl/segmentation/impl/grabcut_segmentation.hpp` 的 GrabCut 模板入口，伴随记录 `segmentation/src/grabcut_segmentation.cpp` 的 GMM（高斯混合模型）与 max-flow（最大流）后端边界。首阶段来自保留候选复筛清单中的 “GrabCut staging / n-link / GMM”。当前已完成并采纳两个窄范围 production patch（生产补丁）：`initGraph()` 中 unknown trimap terminal weight（未知 trimap 端点权重）批量计算由 RVV helper 接管，`learnGMMs()` 中 component assignment（分量归属选择）由 RVV helper 接管；固定标签、n-link、GaussianFitter relearn 和 solver 仍保持标量。

## 函数 / 函数族作用速览

| 函数 / 函数族 | 作用 | 输入 / 输出状态 | 与主流程关系 | RVV 判断 |
| --- | --- | --- | --- | --- |
| `initCompute` | 检查 RGB/RGBA 字段、构造 `Image<Color>`、初始化 trimap / GMM / n-links。 | 读 `input_` / `indices_`，写 `image_`、`trimap_`、`n_links_`。 | `setBackgroundPointsIndices` 和 `extract` 前置。 | color staging 可诊断；生产 gate 暂缓。 |
| `computeBetaOrganized` | 扫描 organized cloud 的右、下、右下、左下邻接边，累计 color distance 并写入 `NLinks`。 | 读 `image_`，写 `n_links_`、`beta_`。 | organized preprocessing hot loop。 | Phase 000 首要 diagnostic。 |
| `computeNLinksOrganized` | 把保存的 color distance 转为 graph n-link weight。 | 读写 `n_links_.weights`。 | `initGraph` 前置。 | Phase 000/010 诊断；`exp` helper 需额外审计。 |
| `computeBetaNonOrganized` | 对 indices 中点执行 KNN，再保存 neighbor color distance。 | 读 search tree，写 `n_links_`、`beta_`。 | non-organized preprocessing。 | KNN search 稀释，首阶段暂缓。 |
| `GMM::probabilityDensity(i, c)` | 计算单个 Gaussian（高斯分量）对某个颜色的概率密度。 | 读 `mu`、inverse covariance（协方差逆矩阵）和 determinant（行列式），输出 float 概率。 | `GMM::probabilityDensity(c)` 和 `learnGMMs` 会反复调用。 | Phase 010 已有 positive diagnostic。 |
| `GMM::probabilityDensity(c)` | 对 K=5 个 Gaussian 做 `pi` 加权求和。 | 读整个 GMM 和 `Color`，输出混合概率。 | `initGraph` terminal weights 和 `learnGMMs` component assignment 的前置。 | Phase 020 已有 positive diagnostic。 |
| `initGraph` | 写 terminal weights 和 n-link edges。 | 读 GMM / n-links，写 graph。 | 每轮 refine 都重建 graph。 | 当前 production patch 只接管 unknown trimap terminal weight batch；fixed-label trimap、n-link 和 solver 保持标量。 |
| `refineOnce` / `refine` | 学习 GMM、重建 graph、求解 max-flow、更新 hard segmentation。 | 改写 GMM / graph / segmentation。 | public segmentation 主循环。 | 状态机和 solver 不作为首轮 RVV 目标。 |

## 函数级结论

当前结论是 `adopted_production_behavior_and_topic_stop`：GrabCut organized n-link 已有测试资产和 bench case，但历史 all-case board 观察不支持优先接 production。`GMM::probabilityDensity(i, c)` 的 Phase 010 component diagnostic 给出正向信号；Phase 020 进一步证明 K=5 mixture（混合概率）和 terminal `-log` 形态在 Milkv-Jupiter 5-run repeated 中仍为正向；Phase 030 在 `initGraph` no-solve diagnostic（不含求解诊断）里追加 lightweight terminal write sink（轻量端点写入消耗点）后，5-run B/A 为 `1.5486, 1.5416, 1.5418, 1.5396, 1.5389`，repeated Evidence Doctor 为 `Errors=0, Warnings=0, Suggestions=0`。Phase 060 已把该路线接入真实 `GrabCut<PointXYZRGB>::initGraph()` production-detail helper 边界，Milkv-Jupiter 5-run B/A 为 `3.1292, 3.1280, 3.0998, 3.0944, 3.1982`，production repeated Evidence Doctor 为 `Errors=0, Warnings=0, Suggestions=0`。用户确认保留 / 采纳后，Phase 070 补跑真实公开入口 `setBackgroundPointsIndices()` + `extract()`：clean 96x72 5-run B/A 为 `1.124687, 1.110452, 1.112645, 1.113482, 1.112866`，median `1.112866x`，Evidence Doctor 为 `Errors=0, Warnings=0, Suggestions=0`。证据支持保留当前窄范围 patch，并触发正式 `doc-rvv/segmentation/grabcut_segmentation-RVV.zh.md`。

Phase 080/090/100 是采纳后的后续诊断探索。Phase 080 `public_extract_profile` 在 Milkv-Jupiter clean
96x72 5-run 中得到 median B/A `1.167619x`，Evidence Doctor 为 `Errors=0, Warnings=0, Suggestions=0`，
并显示 `learn_gmms` 在 Std profile 中约占 `11.46%`。Phase 090 针对 `learnGMMs()` 第一段 component
assignment（分量归属选择）新增测试专用 RVV candidate，Milkv-Jupiter clean 96x72 5-run median B/A
`3.528303x`，checksum 一致，Evidence Doctor 为 `Errors=0, Warnings=0, Suggestions=0`。Phase 100 把该
assignment candidate 放回完整 `learnGMMs()` 局部形态，继续执行 scalar GaussianFitter relearn（标量高斯拟合器
重新训练），Milkv-Jupiter clean 96x72 5-run median B/A `3.048585x`，Std median `4.078463 ms`，RVV median
`1.337755 ms`，checksum 一致，Evidence Doctor 为 `Errors=0, Warnings=0, Suggestions=0`。

Phase 110 已将该路线接入真实 `pcl::segmentation::grabcut::learnGMMs()` production path（生产路径）。
接入后 production-detail（生产细节）`production_learn_gmms` 5-run median B/A 为 `2.755010x`，Std median
`4.887261 ms`，RVV median `1.774198 ms`；production-public（公开生产入口）`public_extract` 5-run
median B/A 为 `1.207907x`，Std median `1532.368265 ms`，RVV median `1265.073598 ms`。两层 checksum
一致，Evidence Doctor 均为 `Errors=0, Warnings=0, Suggestions=0`。用户确认按接入后板卡 positive
采纳 `learnGMMs()` production patch，因此当前结论升级为 adopted production behavior（已采纳生产行为）。

Phase 120 在两个 production patch 均采纳后重跑 `public_extract_profile`。Milkv-Jupiter clean 96x72
5-run median B/A 为 `1.290653x`，checksum 一致，Evidence Doctor 为 `Errors=0, Warnings=0, Suggestions=0`。
组件剖析显示 RVV profile 中 `learn_gmms` 中位耗时 `57.534405 ms`，占 `4.592618%`，低于计划中的
`profile_actionable >= 5%` 阈值；剩余主成本为 `initgraph_refine` 和 `graph_solve`。因此当前结论从
`active_diagnostic_followup` 降级为 `adopted_production_behavior_and_topic_stop`：已采纳实现保留，但
当前 topic 内没有值得自动继续推进的新 RVV helper。

## 标量流程与 RVV 诊断流程对照

| 标量流程 | Phase 000 诊断对应 | 不能证明的范围 |
| --- | --- | --- |
| `initCompute` 将点云 RGB/RGBA 转为 `Color` image。 | synthetic organized `PointXYZRGB` 直接构造 color image 或 cloud fixture。 | RGB/RGBA traits gate、production dispatch。 |
| `computeBetaOrganized` 写 4-neighbor color distance、distance 和 indices。 | test-only reference 复刻同样的边顺序、边界条件和 beta 公式。 | 浮点 reduction 顺序改变后的 RVV 误差预算。 |
| `computeNLinksOrganized` 用 beta 和 lambda 生成权重。 | correctness test 对固定小图手算 expected；bench 统计大图 component。 | strict libm-compatible RVV `exp` 是否存在。 |
| `GMM::probabilityDensity(i, c)` 计算三维二次型并执行 `std::exp(-0.5*d)`。 | Phase 010 test-only batch wrapper 使用 same-chain 标量 reference 和 RVV candidate 对拍。 | K=5 `pi` 加权求和、terminal `-log`、真实 graph mutation 和 production dispatch。 |
| `GMM::probabilityDensity(c)` 对 K=5 分量做 `pi` 加权求和。 | Phase 020 test-only terminal weight wrapper 同时计算 background/foreground mixture，再执行 `-log`。 | `setTerminalWeights`、n-link graph edge mutation、trimap 固定标签分支和 max-flow。 |
| `initGraph` 逐点加 terminal 和 n-link graph edge。 | Phase 060 production patch 在 graph node allocation 后批量计算 unknown trimap terminal weights，并真实调用 `setTerminalWeights` 写 graph terminal capacities。 | n-link `addEdge`、max-flow、完整 public `extract/refineOnce` wall time、`Scalar=double` 和自定义点型性能仍未覆盖。 |
| `learnGMMs` 第一段按 hard segmentation 选择 foreground/background GMM，再为每个像素选择概率最大的 K=5 component；随后按 component vector 重新训练 foreground/background GMM。 | Phase 090 test-only helper 对 assignment 子阶段做 RVV component A/B；Phase 100 full helper 复用 RVV assignment 后继续执行 scalar GaussianFitter relearn；Phase 110 已接入真实 production `learnGMMs()` 并补 production-detail / production-public 两层板卡证据。 | 当前只证明 `Image<Color>`、ordered `Indices`、float `Color`、K=5 GMM 和 96x72 public bench；GaussianFitter accumulation、LMUL/ILP 新 family、其它 layout 和 solver 不在本阶段内。 |

## 测试计划和 bench 计划

| 测试 / bench | 层级 | 作用 |
| --- | --- | --- |
| `GrabCutDiagnosticReference.ComputesOrganizedNLinksForSmallImage` | unit correctness（单元正确性） | 用 3×2 RGB 样本锁定 organized beta、边界邻接和 n-link weight 公式。 |
| `GrabCutDiagnosticReference.GMMProbabilityBatchMatchesScalarReference` | numerical consistency（数值一致性） | 用固定 Gaussian 和 BGR 样本验证 `GMM::probabilityDensity(i, c)` 的 same-chain RVV candidate。 |
| `GrabCutDiagnosticReference.TerminalWeightsMatchScalarReference` | public-shaped diagnostic correctness（公开入口形态诊断正确性） | 用两个 K=5 GMM 和 BGR 样本验证 terminal weight 的 mixture + `-log` 口径。 |
| `GrabCutDiagnosticReference.InitGraphNoSolveMatchesScalarReference` | production-shaped diagnostic correctness（生产形态诊断正确性） | 在 terminal weight 公式后追加轻量写入 sink，验证 RVV candidate 穿过 no-solve 外壳后仍保持数值语义。 |
| `GrabCutProductionDirect.RvvTerminalWeightsMatchScalarAndFixedLabelsFallback` | production direct correctness（真实生产路径正确性） | 通过测试专用子类调用真实 `GrabCut<PointXYZRGB>::initGraph()`，验证 RVV 写入的 graph terminal capacities 与标量一致，并证明 fixed-label trimap 仍走标量 fallback。 |
| `GrabCutDiagnosticReference.LearnGMMComponentAssignmentMatchesScalarReference` | component diagnostic correctness（组件诊断正确性） | 对 `learnGMMs()` 第一段 assignment 子阶段对拍标量 reference；证明 RVV candidate 输出同一 component vector。 |
| `GrabCutDiagnosticReference.LearnGMMsFullMatchesScalarReference` | production-shaped diagnostic correctness（生产形态诊断正确性） | 对完整 `learnGMMs()` 局部形态做标量 / RVV 对拍；证明 RVV assignment 后的 components 和重新训练后的 GMM 参数一致。 |
| `GrabCutProductionDirect.LearnGMMsMatchesScalarReference` | production direct correctness（真实生产路径正确性） | 直接调用真实 `pcl::segmentation::grabcut::learnGMMs()`，验证 production RVV dispatch 后 components 和 GMM 参数与参考链路一致。 |
| `GrabCutProductionDirect.LearnGMMsSmallInputFallbackMatchesReference` | fallback correctness（回退路径正确性） | 用小输入触发 RVV helper 返回 false，验证 public free function 自然回退到 `learnGMMsStd()`。 |
| `bench_grabcut --case organized_nlinks` | component ablation（组件消融） | 只测 organized beta + n-link component，不包含 graph solve。 |
| `bench_grabcut --case gmm_probability` | component ablation | 只测单 Gaussian probability density batch，不包含 K=5 加权、terminal `log`、graph mutation 或 max-flow。 |
| `bench_grabcut --case terminal_weights` | production-shaped diagnostic（生产形态诊断） | 测 K=5 mixture + terminal `-log`，不包含 graph mutation 或 max-flow。 |
| `bench_grabcut --case initgraph_no_solve` | production-shaped diagnostic | 测 terminal weight loop + lightweight terminal write sink，不包含真实 `graph_.addSourceEdge` / `addTargetEdge`、n-link edge 或 max-flow。 |
| `bench_grabcut --case production_initgraph_terminal` | production-detail（生产细节性能测试） | 测真实 `initGraph()` terminal weight helper 和 graph terminal capacity 写入；`n_links_` 为空，因此不包含 n-link edge 或 max-flow。 |
| `bench_grabcut --case public_extract` | production-public（真实公开入口性能测试） | 构造 organized `PointXYZRGB`，调用真实 `setBackgroundPointsIndices()` 和 `extract()`；计时包含 `fitGMMs`、`learnGMMs`、多轮 `initGraph()`、n-link edge、max-flow 和输出聚类。 |
| `bench_grabcut --case public_extract_profile` | diagnostic-profile（诊断剖析） | 用测试专用 wrapper 拆分 public-shaped components；只能定位下一候选，不能替代 production-public 证据。 |
| `bench_grabcut --case learn_gmm_assignment` | component diagnostic（组件诊断） | 只测 `learnGMMs()` assignment 子阶段，不包含 GaussianFitter relearn 或 GMM 参数更新。 |
| `bench_grabcut --case learn_gmms_full` | production-shaped diagnostic | 测 assignment + GaussianFitter relearn；不包含 `initGraph()`、max-flow、public `extract()` wall time 或 production dispatch。 |
| `bench_grabcut --case production_learn_gmms` | production-detail（生产细节性能测试） | 直接调用真实 `pcl::segmentation::grabcut::learnGMMs()`；计时包含 assignment 和 GaussianFitter relearn，不包含 `initGraph()`、max-flow 或 public `extract()` wall time。 |
| `bench_grabcut --case public_extract_profile` after Phase 110 | diagnostic-profile（诊断剖析） | Phase 120 在两个已采纳 helper 后重新拆分 public-shaped components；用于决定是否继续当前 topic，不替代 production-public 采纳证据。 |

QEMU 只跑 correctness 和 log-shape smoke（日志形状小型验证）；性能结论必须来自板卡或目标硬件。

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `GrabCut<PointT>::computeBetaOrganized` | production preprocessing helper | 构造 organized n-link color distance 和 beta。 | `initCompute` | `computeNLinksOrganized`、`initGraph` | production boundary（生产边界） | `segmentation/include/pcl/segmentation/impl/grabcut_segmentation.hpp` |
| `GrabCut<PointT>::computeNLinksOrganized` | production preprocessing helper | 生成 n-link graph weight。 | `initCompute` | `initGraph` | production boundary | 同上 |
| `grabcut_diag::computeOrganizedNLinksReference` | diagnostic reference（诊断参考链路） | 复刻 organized n-link 标量语义。 | `src/test_grabcut.cpp`、`src/bench_grabcut.cpp` | assertions / bench checksum | correctness gate（正确性验收） | `test-rvv/segmentation/grabcut_segmentation/include/` |
| `grabcut_diag::computeGMMProbabilityCandidate` | candidate formula / RVV math | RVV 构造三维二次型并调用 `pcl::expf_RVV_f32m2`。 | `src/test_grabcut.cpp`、`src/bench_grabcut.cpp` | assertions / bench checksum | diagnostic correctness、asm attribution、board component bench | `test-rvv/segmentation/grabcut_segmentation/include/impl/grabcut_diagnostic_reference.hpp` |
| `grabcut_diag::computeTerminalWeightsCandidate` | production-shaped diagnostic helper | 调用 RVV per-Gaussian helper，完成 K=5 mixture 和 terminal `-log`。 | `src/test_grabcut.cpp`、`src/bench_grabcut.cpp` | assertions / bench checksum | public-shaped diagnostic correctness、asm、board repeated | `test-rvv/segmentation/grabcut_segmentation/include/impl/grabcut_diagnostic_reference.hpp` |
| `grabcut_diag::computeInitGraphNoSolveCandidate` | production-shaped diagnostic helper | 在 terminal weight candidate 后追加轻量写入 sink。 | `src/test_grabcut.cpp`、`src/bench_grabcut.cpp` | assertions / bench checksum | partial-production-candidate diagnostic、board repeated、PI1 输入 | `test-rvv/segmentation/grabcut_segmentation/include/impl/grabcut_diagnostic_reference.hpp` |
| `GrabCut<PointT>::initGraphTerminalWeightsRVV` | production detail helper（生产细节 helper） | 收集 unknown trimap 点，批量计算 background / foreground GMM mixture 后写 graph terminal capacities。 | `GrabCut<PointT>::initGraph()` | `setTerminalWeights` | production direct correctness、asm attribution、board production-detail bench | `segmentation/include/pcl/segmentation/impl/grabcut_segmentation.hpp` |
| `grabcut_diag::assignGMMComponentsCandidate` | diagnostic candidate（诊断候选） | 批量计算 `learnGMMs()` assignment 子阶段中每个像素的最佳 Gaussian component。 | `src/test_grabcut.cpp`、`src/bench_grabcut.cpp` | component vector checksum | Phase 090 correctness、QEMU smoke、board component bench | `test-rvv/segmentation/grabcut_segmentation/include/impl/grabcut_diagnostic_reference.hpp` |
| `grabcut_diag::learnGMMsReference` / `grabcut_diag::learnGMMsCandidate` | production-shaped diagnostic helper | 对齐完整 `learnGMMs()` 局部函数形态；candidate 只替换 assignment 子阶段，随后按相同 component vector 重新训练 GMM。 | `src/test_grabcut.cpp`、`src/bench_grabcut.cpp` | components、GMM 参数 checksum | Phase 100 correctness、QEMU smoke、board full learnGMMs bench | `test-rvv/segmentation/grabcut_segmentation/include/impl/grabcut_diagnostic_reference.hpp` |
| `pcl::segmentation::grabcut::learnGMMs` / `learnGMMsRVV` | production free function | 真实生产 `learnGMMs()` 入口；RVV 只接管 assignment 子阶段，GaussianFitter relearn 保持标量。 | `GrabCut<PointT>::refineOnce()`、`src/test_grabcut.cpp`、`src/bench_grabcut.cpp` | components、background/foreground GMM | Phase 110 production direct correctness、fallback、asm、production-detail board、production-public board | `segmentation/src/grabcut_segmentation.cpp` |
| `src/test_grabcut.cpp` | correctness test | 固定小图样本和 expected。 | `make run_test_compare` | gtest assertions | QEMU correctness | `test-rvv/segmentation/grabcut_segmentation/src/test_grabcut.cpp` |
| `src/bench_grabcut.cpp` | bench wrapper（性能测试包装层） | 构造大 organized image 并计时 component。 | `make run_bench_rvv` / board target | output log | component ablation | `test-rvv/segmentation/grabcut_segmentation/src/bench_grabcut.cpp` |
| `script/generate_grabcut_board_evidence_manifest.py` | analysis script（分析脚本） | 把 GrabCut board `BENCH` 行转成 Evidence Doctor manifest。 | `make run_gmm_evidence_doctor` | `evidence_doctor.py` | evidence boundary check（证据边界检查） | `test-rvv/segmentation/grabcut_segmentation/script/generate_grabcut_board_evidence_manifest.py` |
| `doc/phases/010-gmm-probability-density-diagnostic/evidence-doctor.md` | evidence output summary（证据摘要输出） | 记录 GMM 专用 Evidence Doctor 结果。 | Phase 010 result / Handoff | reviewer | `Errors=0, Warnings=1`，warning 为低 run count | `test-rvv/segmentation/grabcut_segmentation/doc/phases/010-gmm-probability-density-diagnostic/evidence-doctor.md` |
| `doc/phases/020-terminal-weight-public-shaped-diagnostic/repeated-evidence-doctor.md` | evidence output summary | 记录 terminal-weight 5-run repeated Evidence Doctor 结果。 | Phase 020 result / Handoff | reviewer | `Errors=0, Warnings=0` | `test-rvv/segmentation/grabcut_segmentation/doc/phases/020-terminal-weight-public-shaped-diagnostic/repeated-evidence-doctor.md` |
| `doc/phases/030-initgraph-no-solve-diagnostic/repeated-evidence-doctor.md` | evidence output summary | 记录 `initgraph_no_solve` 5-run repeated Evidence Doctor 结果。 | Phase 030 result / PI1 plan / Handoff | reviewer | `Errors=0, Warnings=0, Suggestions=0` | `test-rvv/segmentation/grabcut_segmentation/doc/phases/030-initgraph-no-solve-diagnostic/repeated-evidence-doctor.md` |
| `doc/phases/040-production-integration-plan/plan.zh.md` | production integration plan | 冻结 PI2 候选范围、fallback、production direct tests、asm 和 board evidence plan。 | PI1 result / Handoff | 下一轮 worker | production authorization boundary（生产授权边界） | `test-rvv/segmentation/grabcut_segmentation/doc/phases/040-production-integration-plan/plan.zh.md` |
| `doc/phases/060-production-initgraph-terminal-evidence/repeated-evidence-doctor.md` | production evidence summary（生产证据摘要） | 记录 `production_initgraph_terminal` 5-run repeated Evidence Doctor 结果。 | Phase 060 result / Handoff | reviewer | `Errors=0, Warnings=0, Suggestions=0` | `test-rvv/segmentation/grabcut_segmentation/doc/phases/060-production-initgraph-terminal-evidence/repeated-evidence-doctor.md` |
| `doc/phases/060-production-initgraph-terminal-evidence/result.zh.md` | phase result | 记录 production patch、测试、QEMU smoke、asm、board repeated、registry 和 PI5 EvidenceDecision。 | Phase 060 plan / current worker | reviewer / next worker | adopted after user confirmation | `test-rvv/segmentation/grabcut_segmentation/doc/phases/060-production-initgraph-terminal-evidence/result.zh.md` |
| `doc/phases/070-public-extract-wall-time-adoption-check/repeated-evidence-doctor.md` | production-public evidence summary | 记录 `public_extract` 5-run repeated Evidence Doctor 结果。 | Phase 070 result / `doc-rvv` | reviewer | `Errors=0, Warnings=0, Suggestions=0` | `test-rvv/segmentation/grabcut_segmentation/doc/phases/070-public-extract-wall-time-adoption-check/repeated-evidence-doctor.md` |
| `doc/phases/070-public-extract-wall-time-adoption-check/result.zh.md` | phase result | 记录接入后公开入口 wall-time、320x240 too-heavy partial run 和继续 / 停止判断。 | Phase 070 plan / current worker | reviewer / next worker | production-public positive；no high-value unblocked next candidate | `test-rvv/segmentation/grabcut_segmentation/doc/phases/070-public-extract-wall-time-adoption-check/result.zh.md` |
| `doc/phases/080-public-extract-component-profile/result.zh.md` | phase result | 记录接入后 public-shaped profile、组件占比、无效历史 run 和下一候选排序。 | Phase 080 plan / current worker | reviewer / next worker | diagnostic-profile；profile_actionable | `test-rvv/segmentation/grabcut_segmentation/doc/phases/080-public-extract-component-profile/result.zh.md` |
| `doc/phases/090-learn-gmm-component-assignment-diagnostic/result.zh.md` | phase result | 记录 `learnGMMs()` assignment-only helper、correctness、QEMU smoke、板卡 repeated、Doctor 和继续判断。 | Phase 090 plan / current worker | reviewer / next worker | positive diagnostic；requires full `learnGMMs` diagnostic before production | `test-rvv/segmentation/grabcut_segmentation/doc/phases/090-learn-gmm-component-assignment-diagnostic/result.zh.md` |
| `doc/phases/100-learn-gmms-full-production-shaped-diagnostic/result.zh.md` | phase result | 记录 full `learnGMMs()` helper、correctness、QEMU smoke、板卡 repeated、Doctor 和 production integration 授权边界。 | Phase 100 plan / current worker | reviewer / next worker | positive production-shaped diagnostic；requires explicit authorization before production | `test-rvv/segmentation/grabcut_segmentation/doc/phases/100-learn-gmms-full-production-shaped-diagnostic/result.zh.md` |
| `doc/phases/110-learn-gmms-production-integration-plan/result.zh.md` | phase result | 记录真实 `learnGMMs()` production patch、fallback、QEMU smoke、asm、production-detail board、production-public board、Doctor、用户确认和 S11 closeout。 | Phase 110 plan / current worker | reviewer / next worker | adopted production behavior | `test-rvv/segmentation/grabcut_segmentation/doc/phases/110-learn-gmms-production-integration-plan/result.zh.md` |
| `doc/phases/120-post-learn-gmms-adoption-profile/result.zh.md` | phase result | 记录两个 helper 均采纳后的 public component profile、剩余热点、Doctor、registry 和停止判断。 | Phase 120 plan / current worker | reviewer / next worker | profile_non_actionable / topic stop | `test-rvv/segmentation/grabcut_segmentation/doc/phases/120-post-learn-gmms-adoption-profile/result.zh.md` |
| `doc-rvv/segmentation/grabcut_segmentation-RVV.zh.md` | production long-term doc（长期生产文档） | 记录已采纳生产行为、fallback、证据链和后续恢复条件。 | 用户确认采纳 / Phase 070 result | maintainer | adopted production behavior | `doc-rvv/segmentation/grabcut_segmentation-RVV.zh.md` |

## 生产接入判断

`initGraph()` unknown terminal helper 的 production integration loop（生产接入闭环）已完成并由用户确认保留 /
采纳。Phase 060 已完成 production patch、production direct tests、QEMU smoke、反汇编归属、Milkv-Jupiter
5-run repeated board 和 Evidence Doctor；Phase 070 已补充真实公开入口 production-public wall-time，证明
当前 patch 在完整 `extract()` 流程中仍有 `1.112866x` median 收益。正式长期文档是
`doc-rvv/segmentation/grabcut_segmentation-RVV.zh.md`。

`learnGMMs()` 的 production integration loop 已完成 PI5 和用户确认。`production_learn_gmms` 真实生产细节边界为
`2.755010x`，完整 `public_extract` 公开入口为 `1.207907x`，两层 Evidence Doctor 均 `0/0/0`。当前证据支持
采纳 `segmentation/src/grabcut_segmentation.cpp` 中的 `learnGMMs()` RVV assignment patch，用户已确认按接入后
板卡 positive 采纳。长期 `doc-rvv/segmentation/grabcut_segmentation-RVV.zh.md` 使用本阶段接入后的板卡数据。

Phase 120 的接入后 profile 不改变上述采纳结论。它只回答“是否继续写第三个 RVV helper”：当前答案是否定的。
`learn_gmms` 剩余占比低于阈值，GaussianFitter accumulation 和 LMUL / ILP variants 暂不进入新的
production integration loop。

## 当前状态

当前状态为 adopted `initGraph()` production behavior plus adopted `learnGMMs()` production behavior（已采纳
`initGraph()` 和 `learnGMMs()` 两个生产行为），并且 Phase 120 已完成接入后 profile。工作区保留
`learnGMMs()` production patch。默认下一动作是暂停当前 topic：organized n-link 历史信号弱负向，
max-flow 是状态机，non-organized KNN 和 color staging 当前没有 profile 支持；GaussianFitter accumulation
和 LMUL/ILP variants 在 Phase 120 后没有足够剩余热点，不值得自动另开实现 phase。
