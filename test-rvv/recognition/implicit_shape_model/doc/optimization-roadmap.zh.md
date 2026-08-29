# Optimization Roadmap

## 当前边界

ISM 当前已有一个窄范围 adopted production behavior：`findObjects()` 公开入口中的
descriptor-to-cluster nearest assignment。当前 production patch、公开入口 correctness、反汇编归属、
production direct repeated board 和 Evidence Doctor 已闭合；正式长期文档为
`doc-rvv/recognition/implicit_shape_model-RVV.zh.md`。

本 roadmap 仍保留未覆盖候选，但这些候选不再属于当前 Phase 020 同 scope 的自动继续项。继续它们需要
新 phase plan，重新冻结入口、点型 / `Scalar`、计时边界和证据要求。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| descriptor cluster distance | 筛选清单和源码 `findObjects()` | descriptor-to-cluster L2 distance | 减少每 keypoint × cluster 的 FeatureSize 规约成本 | 入口周边成本会稀释局部收益 | Phase 000 local、Phase 010 production-shaped、Phase 020 production direct | adopted narrow | closed by Phase 020 |
| public `findObjects()` descriptor assignment | Phase 010 positive 后的生产接入 | 真实 `findObjects()` public entry | 小幅降低识别入口中最近 cluster 分配成本 | feature estimator 和 vote 生成仍占成本 | upstream correctness、asm、5-run board、Doctor、registry | adopted | closed by Phase 020 |
| sigma pairwise max-dot | 筛选清单和源码 `calculateSigmas()` | training cloud pairwise point loop | 内层 `j` loop 可向量化 | 只在训练阶段运行；入口收益未知 | trainISM-shaped profile / bench、correctness、asm、board、Doctor | deferred | `030-sigma-production-shaped-profile` if requested |
| vote density Gaussian sum | 源码 `getDensityAtPoint()` / `shiftMean()` | radiusSearch 后邻域权重求和 | 可复用 float RVV exp helper 和 sum reduction | production 使用 double `std::exp`；tree search 可能主导 | math semantic audit、tree boundary、production-shaped diagnostic | deferred | `030-density-math-boundary-audit` if requested |
| point type / FeatureSize expansion | Phase 020 窄范围证据 | 其它 `FeatureSize`、点型、layout | 扩大 production confidence | 当前 board 只覆盖 `FeatureSize=153`、`PointXYZ` / `Normal` | targeted public-entry correctness、bench、asm、board、Doctor | deferred | `030-findobjects-scope-expansion` if requested |
| full `trainISM()` path | 用户要求持续优化时的后续方向 | training entry | 可能覆盖更大训练成本 | KMeans、feature estimator、随机状态、Eigen 动态分配 | profile first，之后另建 production-shaped diagnostic | not_now | separate topic / phase |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 000 | descriptor nearest-cluster production-shaped diagnostic | descriptor local formula positive 且处于 `findObjects()` 识别入口 | production-shaped correctness、asm、5-run board、Doctor | completed |
| 010 | descriptor batch assignment in `findObjects()` shape | phase010 board positive 且 Doctor 无 Warning | PI1-PI5 production direct tests、asm、board | completed |
| 020 | scope expansion queue | production direct 只覆盖一个 fixture 和代表性点型 | 新 phase 的 public-entry correctness、board 和 Doctor | deferred |
| 020 | environment / binary metadata enrichment | Doctor Suggestion 指向环境字段和 binary identity 缺口 | 记录 device、taskset、governor、freq、temperature、binary hash | low |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| direct production patch before diagnostic | ISM 筛选清单原本要求 profile prerequisite，不能跳过局部和 production-shaped 证据 | 已由 Phase 000/010/020 闭合 descriptor path |
| sigma production patch in same phase | Phase 020 只冻结 `findObjects()` descriptor assignment；训练 path 和输入分布未闭合 | 有 trainISM profile 或 production-shaped bench 指向 sigma |
| density production patch in same phase | double `std::exp` 语义和 radiusSearch/tree boundary 未闭合 | 完成数学语义审计和 production-shaped diagnostic |
| continue optimizing same scope | 当前已接 helper 是小型规约；同 scope 内没有未尝试且风险可控的下一 RVV family | 若 reviewer 要求比较不同 LMUL / unroll，再建 RVV-vs-RVV detail A/B phase |

## roadmap_default_recovery_queue

| priority | action | status | stop / resume condition |
| ---: | --- | --- | --- |
| 1 | Phase 020 production closeout | completed | 已创建 `doc-rvv` 并刷新 topic-local docs |
| 2 | `030-findobjects-scope-expansion` | turn_stop_deferred with stop_condition_hit | 会扩大到其它点型 / `FeatureSize`，需新 phase 边界 |
| 3 | `030-sigma-production-shaped-profile` | turn_stop_deferred with stop_condition_hit | 会从识别入口切到训练入口，属于新证据问题 |
| 4 | `030-density-math-boundary-audit` | turn_stop_deferred with stop_condition_hit | 涉及 double math 语义和 tree boundary，需先读 math vectorization 规则 |
