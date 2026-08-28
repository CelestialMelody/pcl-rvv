# Normal-plane 代表性 AoS Source 性能扩展 Phase Plan

## 阶段意图和边界

本阶段只回答一个问题：Phase 040 已通过 correctness（正确性）证明的
`PointXYZI + Normal` 和 `PointXYZINormal + Normal` 代表性 AoS source（结构数组 source
点类型）在板卡 benchmark（性能测试）中是否仍保持 RVV 相对标量的正向收益。

本阶段不改变 production（生产源码）实现族，不改变 public dispatch（公开入口分流逻辑），
不把代表点型结果外推成完整泛型点类型全集，也不新增 normal layout（法线点类型布局）
扩展结论。

| 维度 | validated_scope |
| --- | --- |
| public / helper entry | `SampleConsensusModelNormalPlaneBench` 暴露的 protected helper hot path（受保护 helper 热点路径） |
| row source | ordered `indices_`，source 点和 normal 点按相同 index 读取 |
| source 点类型 | `PointXYZ`、`PointXYZI`、`PointXYZINormal` |
| normal 点类型 | `Normal` |
| Scalar | threshold / weight 仍按当前 `float f32m2` RVV helper 计算；公开 API 的 double 入参只做既有 cast |
| 规模 | topic PCD fixture 的 3283 点默认规模 |
| evidence role | production-shaped diagnostic（生产形态诊断）性能证据；不替代 public-entry correctness |

| 维度 | unvalidated_scope |
| --- | --- |
| 更多 source 点类型 | `PointXYZRGB`、`PointXYZRGBA`、自定义 registered AoS xyz 点型 |
| normal 点类型 | 除 `Normal` 外的其它 registered normal / curvature 单 float layout |
| row source | 非 ordered `indices_` 的其它索引策略 |
| Scalar / math family | `Scalar=double` 或新的 RVV math approximation family（数学近似实现族） |
| production decision | 本阶段不做 PI5 采纳 / 回滚判断，只补代表点型性能证据 |

## 当前状态清单

| area | 当前状态 | 路径 |
| --- | --- | --- |
| Phase 040 | 已关闭代表性 AoS source public correctness / fallback。 | `040-normal-plane-aospoint-gate-expansion/result.zh.md` |
| Phase 030 | 已有 `PointXYZ + Normal` 5-run 板卡 repeated summary，median speedup 分别为 9.64x、12.72x、12.02x。 | `030-normal-plane-repeated-board-summary/result.zh.md` |
| bench source | 当前 `bench_sac_normal_plane.cpp` 默认只实例化 `PointXYZ + Normal`。 | `test-rvv/sample_consensus/plane_models/src/bench_sac_normal_plane.cpp` |
| manifest wrapper | 当前 case 字典只理解旧三项 `PointXYZ + Normal` case。 | `test-rvv/sample_consensus/plane_models/script/generate_normal_plane_board_evidence_manifest.py` |
| matrix / roadmap | `representative AoS source performance` 仍为 deferred。 | `optimization-matrix.zh.md`、`doc/optimization-roadmap.zh.md` |

## 假设与候选族

候选族是 `representative AoS source performance`。source 只读 `x/y/z`，因此 `PointXYZI`
和 `PointXYZINormal` 的额外字段不会参与输出语义；但它们会改变 `sizeof(PointT)` 和 gather
stride（按结构体大小离散加载的步幅），可能改变 cache locality（缓存局部性）和 RVV gather 成本。
本阶段用同一 bench wrapper、同一 PCD fixture、同一 normal cloud、同一 5-run repeated board
流程分别测三种 source 点型。

## 优化矩阵

| candidate family | row source | point type / layout | correctness / fallback target | bench / board | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| representative AoS source performance | ordered `indices_` | `PointXYZ + Normal` | 已由 Phase 040 public tests 覆盖 | 复核旧默认 bench case；保留 Phase 030 边界 | `dump_bench_rvv` | repeated doctor | expected positive-stable |
| representative AoS source performance | ordered `indices_` | `PointXYZI + Normal` | Phase 040 public-vs-direct RVV correctness | 新增显式 all-source bench case + 5-run board summary | `dump_bench_rvv` | repeated doctor | planned |
| representative AoS source performance | ordered `indices_` | `PointXYZINormal + Normal` | Phase 040 public-vs-direct RVV correctness | 新增显式 all-source bench case + 5-run board summary | `dump_bench_rvv` | repeated doctor | planned |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| A1 bench 参数化 | 修改 `src/bench_sac_normal_plane.cpp`，默认保持旧三项输出；传入 `--representative-aos-sources` 时输出三种 source 点型的独立 case label。 | 旧 Phase 030 bench 默认语义不变；新模式能被 compare 脚本解析。 |
| A2 manifest 扩展 | 修改 topic-local manifest wrapper 和 Makefile，新增 Phase 050 repeated board 目录、target、case metadata 和 registry 记录。 | Evidence Doctor 能读取新 case，不要求旧 Phase 030 日志包含新 case。 |
| A3 correctness smoke | 运行 `run_normal_plane_public_tests` 与 `run_test_compare`。 | QEMU correctness 仍通过；QEMU timing 不进入性能结论。 |
| A4 asm attribution | 运行 `dump_bench_rvv`。 | RVV bench 二进制中三条 normal-plane RVV helper 符号仍有 RVV 指令归属。 |
| A5 board repeated performance | 运行 Phase 050 专用 5-run board compare、analyze、Evidence Doctor 和 registry target。 | 每个新点型 case 形成 median/min/max/p10/p90；doctor 无未处理 Error。 |
| A6 文档同步 | 更新 result、phase index、matrix、roadmap、evaluation、benchmark/evidence、optimization evidence、长期 `doc-rvv` 和 queue。 | 文档写清新性能范围和不能外推的范围。 |

## Evidence Doctor 与 Registry

本阶段使用 topic-local wrapper 生成 manifest，再调用全局 Evidence Doctor（证据体检）。
输入和输出计划如下：

| artifact | path |
| --- | --- |
| repeated board summary | `log/board/normal-plane-phase050-representative-aos-source-performance/summary.md` |
| manifest | `log/board/normal-plane-phase050-representative-aos-source-performance/evidence-manifest.json` |
| doctor markdown | `log/board/normal-plane-phase050-representative-aos-source-performance/evidence-doctor.md` |
| doctor json | `log/board/normal-plane-phase050-representative-aos-source-performance/evidence-doctor.json` |
| registry | `log/evidence_registry.json` |

Errors 必须修复或降级后才能关闭性能结论。Warnings 必须在 result 中解释；Suggestions
不阻塞当前阶段，但必须写入 matrix 或 roadmap。

## 板卡复跑预算和决策桶

| 项 | 值 |
| --- | --- |
| run count | 5 |
| warm-up | bench 程序内部 3 次 warm-up |
| 最大自动复跑 | 本阶段默认不做第二批 5-run；若 doctor Error 或远端运行失败，可修复后重跑同一批 |
| positive-stable | 每个 case median > 1.20x，min > 1.00x，doctor Error=0 |
| weak-positive | median 在 1.05x 到 1.20x，min 不稳定或存在需解释 Warning |
| neutral / negative | median <= 1.05x 或 min 长期低于 1.00x |
| unstable | 5-run 内方向摇摆，且无法用日志污染或环境字段解释 |

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic performance。 |
| A/B boundary | `SampleConsensusModelNormalPlaneBench` protected helper hot path；Std build vs RVV build。 |
| 当前决策问题 | RVV-vs-scalar for representative source point type performance。 |
| diagnostic 是否可外推到 production | 只能外推到相同 dispatch gate 下的 helper hot path 性能倾向；不能替代 public-entry correctness，也不能证明其它 source / normal 点型。 |
| comparison-boundary / baseline mismatch 风险 | 风险较低：同一 bench wrapper、同一输入、同一 normal cloud 和同一 case label；仍不是 production public overload 计时。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes；若新增点型性能弱或负，只能降级该点型性能边界，不回滚已通过 correctness 的 dispatch gate。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要；本阶段没有选择新 RVV family。 |

## 完成条件和停止条件

本阶段完成需要同时满足：

- 新 bench 默认模式不改变 Phase 030 旧三项输出。
- `--representative-aos-sources` 模式输出三种 source 点型的独立 Std/RVV compare 表。
- QEMU correctness、asm dump、5-run board summary、Evidence Doctor 和 registry freshness 都有结果。
- result、matrix、roadmap 和 topic docs 同步写清性能范围。

真实停止条件：

- 板卡不可达或 deploy / fetch 无法完成；
- 新 bench 在 QEMU 或板卡上无法可靠解析 source 点型 case；
- Evidence Doctor Error 无法修复；
- 继续需要扩大到其它 normal layout、`Scalar=double`、新 production API 或其它 topic。

## roadmap 同步动作

阶段结束后：

- `representative AoS source performance` 按证据改为 adopted / weak / negative / unstable。
- 若新增点型仍 positive-stable，roadmap 保留更宽 `PointXYZRGB` / `PointXYZRGBA` 性能扩展为低优先级候选。
- 若新增点型出现弱或负向，roadmap 新增 source stride / cache locality 消融候选，但不自动修改 production。
