# range_image_border_extractor RVV 函数级评估

## S2 函数级评估

`RangeImageBorderExtractor` 的 public entry（公开入口）是 `computeFeature(PointCloudOut&)` / `compute(PointCloudOut&)`。入口拒绝 indices（索引）输入，要求 `range_image_` 已设置，然后通过 `getBorderDescriptions()` 触发 `classifyBorders()`。

标量路径分为五段：`extractLocalSurfaceStructure()` 为每个有效像素建立 `LocalSurface`；`extractBorderScoreImages()` 计算 left/right/top/bottom 四张分数图；`updateScoresAccordingToNeighborValues()` 对四张连续 float 分数图执行 3x3 邻域传播；`findAndEvaluateShadowBorders()` 寻找对应 shadow border（阴影边界）并调整分数；`classifyBorders()` 写 `BorderDescription`、shadow、veil 和方向 traits。`calculateBorderDirections()` 与 `calculateSurfaceChanges()` 是下游方向和 surface-change（表面变化）路径。

第一 RVV 候选选择 score-update（分数传播）循环，因为它位于用户点名的 `.hpp`，输入输出是连续 row-major float 图像，内部像素邻域固定，边界可保留标量。Phase 000/010 证明该局部 helper 有明显收益；Phase 020/030 进一步证明该收益在真实 `RangeImage` score-generation 边界下被稀释到 neutral。它不能支撑 production dispatch，也不能覆盖 shadow/veil 状态机或 principal curvature（主曲率）求解。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic / production-shaped diagnostic（生产形态诊断，贴近 production 数据形态但仍是测试专用 helper） |
| A/B boundary | Phase 000/010 是 test helper；Phase 020/030 是 production-linked bench wrapper，但 RVV 仍只存在于 test-only score-update helper |
| 当前决策问题 | RVV-vs-scalar 和 implementation-shape；判断 score-update / neighbor-score 是否值得进入 production integration loop |
| diagnostic 是否可外推到 production | Phase 020/030 只能外推到全有限 synthetic `RangeImage` 的 score-generation 子边界；不能外推到完整 `computeFeature()` 的 shadow/veil 和输出构造 |
| comparison-boundary / baseline mismatch 风险 | medium；真实 PCD、inf / max range / unobserved 分支、OpenMP 配置和 public output 未覆盖 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前不允许；RangeImage generation+update、local surface 和 after-surface score generation 均为 neutral，没有收益信号 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要；当前未接 production，没有 clean adoption 条件 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `updatedScoreAccordingToNeighborValues` | production helper | 单像素 3x3 分数传播公式 | `updatedScoresAccordingToNeighborValues` | 四张 border score 图像 | 标量语义来源 | `features/include/pcl/features/impl/range_image_border_extractor.hpp` |
| `updateScoresStd` | diagnostic reference | 测试专用标量参考链路 | gtest / bench | output score image | correctness baseline | `test-rvv/features/range_image_border_extractor/include/impl/range_image_border_extractor_score_update.hpp` |
| `updateScoresRVV` | candidate helper | 测试专用 RVV candidate | gtest / bench | output score image | diagnostic candidate | `test-rvv/features/range_image_border_extractor/include/impl/range_image_border_extractor_score_update.hpp` |
| `makeRangeImageFixture` / `extractProductionScoreImages` | production-shaped test helper | 构造全有限 `RangeImage` 并调用 production score generation | gtest / bench | four score images | production-shaped diagnostic | `test-rvv/features/range_image_border_extractor/include/impl/range_image_border_extractor_range_fixture.hpp` |
| `run_test_compare` | test target | Std/RVV correctness 对拍 | developer / board worker | QEMU logs | correctness gate | `test-rvv/features/range_image_border_extractor/Makefile` |
| `bench_range_image_border_extractor.cpp` | diagnostic bench | 同名 case 在 Std/RVV build 中分别跑标量和 RVV helper | `run_bench_compare` / `board_repeated` | compare summary / manifest | board performance diagnostic | `test-rvv/features/range_image_border_extractor/src/bench_range_image_border_extractor.cpp` |
| `generate_range_image_border_extractor_evidence_manifest.py` | evidence wrapper | 把 repeated board compare log 转为 summary 和 Evidence Doctor manifest | `evidence_doctor_repeated` | `summary.md`、`evidence_manifest.json`、`evidence_doctor.md` | evidence registry input | `test-rvv/features/range_image_border_extractor/script/generate_range_image_border_extractor_evidence_manifest.py` |

## 文档归属

| 信息类型 | 主归属 | 当前状态 |
| --- | --- | --- |
| phase plan/result 和优化矩阵 | `doc/phases/` | Phase 000-030 均已记录，矩阵当前为 no-production |
| 候选搜索空间 | `doc/optimization-roadmap.zh.md` | 已记录已尝试、拒绝和不建议继续路线 |
| 函数级评估和诊断证据链 | 本文件 | 当前承载 S2、诊断边界和 no-production 判断 |
| production 长期主题文档 | `doc-rvv/features/range_image_border_extractor-RVV.zh.md` | not_applicable；未接入 production |

## 诊断证据链

Phase 000-030 已完成未接 production 的诊断证据链：

- correctness：`make run_test_compare` 在 QEMU 上完成 Std/RVV 两侧 gtest，当前 3/3 通过；RED 阶段曾用全零 RVV candidate 观察到期望 mismatch，证明测试能抓公式 / mask 错误。`make run_board_test fetch_board_logs` 在板卡上 3/3 通过。
- QEMU log-shape smoke：`make run_bench_compare ALLOW_QEMU_BENCH_COMPARE=1 BENCH_ARGS="--case-filter score_update_threshold_sign_mix --repeat 1 --iterations 1 --warmup 0"` 只用于验证 bench 构建、输出格式和 checksum；不作为性能结论。
- asm attribution：`make dump_test_rvv` 和 `make dump_bench_rvv` 生成 filtered asm；bench RVV asm 中有 `vle32.v`、`vse32.v`、`vfadd.vv`、`vfmul.*`、`vfsub.*`、`vfabs.v`、`vmflt.vf`、`vmerge.vvm`、`vsetvli`，manifest 记录 RVV 指令相关行数 79。
- board performance：Phase 000 `score_update_641x481_tail` median `2.560x`，Phase 010 `score_pipeline_641x481_four_images` median `2.210x`。Phase 020 `range_image_score_generation_160x120` 与 `range_image_generation_plus_update_160x120` median 均 `1.000x`。Phase 030 `range_image_local_surface_160x120` 与 `range_image_border_scores_after_surface_160x120` median 均 `1.000x`。
- evidence paths：Phase 000 `log/board/repeated_phase000_score_update/summary.md`；Phase 010 `log/board/repeated_phase010_four_score_pipeline_clean/summary.md`；Phase 020 `log/board/repeated_phase020_range_image_generation_score_update/summary.md`；Phase 030 `log/board/repeated_phase030_score_generation_component_split/summary.md`。每个目录都有对应 `evidence_manifest.json` 和 `evidence_doctor.md`。
- Evidence Doctor：Phase 000/010 均为 `Errors=0, Warnings=0, Suggestions=1`；Phase 020/030 均为 `Errors=0, Warnings=2, Suggestions=4`。Warnings 是 1/5 B/A 低于 1，Suggestions 是缺少 taskset / governor / freq / temperature 与 near-threshold B/A。处理方式是将 production-shaped diagnostic 降级为 neutral，不进入 production integration loop。

当前 EvidenceDecision 是 `attempted-neutral / no-production`。score-update 3x3 连续 float 图像循环和四方向 batch 曾是局部 positive，但真实 RangeImage score-generation 与 component split 均无收益信号；当前没有值得继续推进的 production RVV 方向。该结论不证明完整 `computeFeature()` 永远不可优化，只说明当前已授权、低风险、可审查的 score-update / neighbor-score 路线不建议接生产。

## Doc suite role inventory

| role | 状态 | 证据 / 说明 |
| --- | --- | --- |
| topic_navigation | `standalone:README.zh.md` | 指向当前结论、常用 target 和证据白名单 |
| testing_overview | `standalone:doc/testing-overview.zh.md` | 记录 correctness / bench / board / doctor target 粒度 |
| correctness_tests | `standalone:doc/correctness-tests.zh.md` | 记录 gtest 覆盖和不能证明的范围 |
| benchmark_and_evidence | `standalone:doc/benchmark-and-evidence.zh.md` | 记录 bench case、repeated summary、manifest、Doctor 和 local-only log 策略 |
| optimization_evidence | `standalone:doc/optimization-evidence.zh.md` | 把 candidate family 映射到证据和 decision |
| optimization_roadmap | `standalone:doc/optimization-roadmap.zh.md` | 保存后续 production-shaped / public pipeline 候选 |
| test_support_code_map | `standalone:doc/test-support-code-map.zh.md` | 记录 include / src / script / log 职责 |
| phase_index | `standalone:doc/phases/README.zh.md` | Phase 000-030 completed，默认状态为暂停当前 topic |
| evaluation_diagnostic | `standalone:doc/range_image_border_extractor-evaluation.zh.md` | 本文件 |
| production_topic_doc | `not_applicable with evidence` | 未修改 production，未完成 PI1-PI5，暂不创建 `doc-rvv/features/range_image_border_extractor-RVV.zh.md` |
