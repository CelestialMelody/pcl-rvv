# Phase 080 public extract component profile 计划

## 阶段意图和边界

本阶段只做 profile（性能剖析，用计时拆分定位热点）和 component A/B（组件对照，用同一入口形态比较标量构建与 RVV 构建）诊断，不修改 production（生产源码）。目标是把真实 `GrabCut<PointXYZRGB>::setBackgroundPointsIndices()` + `extract()` 的 wall time（总耗时）拆成可解释组件，判断是否还有值得进入下一轮 production integration loop（生产接入闭环）的新 RVV family（实现族）。

本阶段证明：

- 当前已采纳 `initGraphTerminalWeightsRVV()` 之外，`initCompute`、organized n-link、GMM build / learn、`initGraph`、`graph_.solve()`、`updateHardSegmentation()` 或输出聚类中是否存在高占比热点。
- 标量构建与 RVV 构建在同一 public-shaped wrapper（公开入口形态包装）下的组件耗时差异。

本阶段不证明：

- 新 RVV family 可采纳到 production。
- `Scalar=double`、自定义点型、non-organized KNN、其它 row source policy（行来源策略）或其它 layout 已覆盖。
- component timing（组件计时）可以替代 Phase 070 的 production-public board evidence（真实公开入口板卡证据）。

## 当前状态清单

| item | current evidence |
| --- | --- |
| adopted production patch | `segmentation/include/pcl/segmentation/grabcut_segmentation.h` 和 `segmentation/include/pcl/segmentation/impl/grabcut_segmentation.hpp` 已接入 `initGraphTerminalWeightsRVV()`；public API 不变。 |
| Phase 060 production-detail | `doc/phases/060-production-initgraph-terminal-evidence/result.zh.md`：5-run B/A median `3.127968x`，Evidence Doctor `0/0/0`。 |
| Phase 070 production-public | `doc/phases/070-public-extract-wall-time-adoption-check/result.zh.md`：clean 96x72 public `extract()` 5-run B/A median `1.112866x`，checksum 一致，Evidence Doctor `0/0/0`。 |
| current roadmap | `doc/optimization-roadmap.zh.md` 将 “public extract profile before new production family” 标为 deferred / requires new evidence phase。 |
| current matrix | `doc/phases/optimization-matrix.zh.md` 没有新 production candidate；color staging、organized n-link production expansion 和 non-organized KNN 需要 profile 或 component A/B 后再判断。 |
| evidence registry | `make evidence_status` 在 Phase 070 final verification 中为 `fresh`。 |

## 假设与候选族

| candidate family | 假设 | 本阶段处理 |
| --- | --- | --- |
| color staging | `initCompute()` 中 RGB/RGBA 到 `Image<Color>` 的转换可能有连续加载 / 存储空间 | 只测占比；若占比低，不进入 production。 |
| organized beta / n-link | organized cloud 的 beta reduction（规约）和 n-link `exp` 可能成为剩余热点 | 计时拆分；若占比高且 RVV 构建无收益，再考虑新公式或内存布局候选。 |
| GMM build / learn | Orchard-Bouman build 和 per-pixel component assignment 可能支配 `setBackgroundPointsIndices()` 或 refine loop | 计时拆分；本阶段不改算法。 |
| initGraph terminal / n-link / graph mutation | 已采纳 terminal RVV helper，但 n-link graph edge mutation 仍是标量 | 计时拆出 `initGraph()`，结合 Phase 060 判断剩余空间。 |
| max-flow solver | `graph_.solve()` 是状态机，当前不直接 RVV 化 | 只确认是否支配 wall time；若支配，默认仍不作为本 topic 的 RVV production 目标。 |
| output clustering | `extract()` 后按 `hard_segmentation_` 构造 clusters | 只测占比；若很低则排除。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / profile target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| public extract component profile | organized image grid | `PointXYZRGB` / float color / organized cloud | test-only subclass wrapper shaped like `setBackgroundPointsIndices()` + `extract()` | QEMU/std/RVV smoke checksum and existing `run_test_compare` | new `--case public_extract_profile` | planned 5-run board repeated at 96x72 | inherits Phase 060 RVV helper attribution; no new production symbol | planned manifest/Doctor | planned |
| color staging candidate | organized image grid | `PointXYZRGB` RGB packed fields | `initCompute` subcomponent only | not implemented | profile share only | not_applicable until profile supports | not_applicable | not_applicable | deferred pending profile |
| n-link production expansion | organized image grid | `PointXYZRGB` / float color | `computeBetaOrganized` + `computeNLinksOrganized` / graph n-link edge mutation | existing diagnostic only | profile share only | not_applicable until new candidate | existing diagnostic asm insufficient | not_applicable | deferred pending profile |
| max-flow solver | graph state machine | solver internal graph | `graph_.solve()` | not_applicable with evidence | profile share only | not_applicable | not_applicable | not_applicable | likely rejected unless profile forces separate non-RVV analysis |

## 实现和测试动作

| action | artifact / command | completion criterion |
| --- | --- | --- |
| add profile bench case | `src/bench_grabcut.cpp` 增加 `public_extract_profile` | Std/RVV 都输出 parseable `BENCH grabcut_profile_component` 行，并输出整次 profile checksum。 |
| expose protected steps in test-only subclass | `GrabCutBenchAccess` 增加 profile wrapper | 不修改 production；wrapper 只调用 protected `initCompute()`、`fitGMMs()`、`refineOnce()` 内部等价步骤或可审查拆分。 |
| add manifest metadata | `script/generate_grabcut_board_evidence_manifest.py` 增加 case label | Evidence Doctor 能识别 profile 证据角色和 A/B boundary。 |
| add Make targets | `Makefile` 增加 Phase 080 repeated board / manifest / doctor / registry target | 可以一条 target 采集、生成 manifest、运行 doctor 并登记 registry。 |
| QEMU smoke | `make run_bench_std ... --case public_extract_profile` 和 `make run_bench_rvv ... --case public_extract_profile` | checksum 一致，日志形状可解析；QEMU timing 不进入性能结论。 |
| board repeated | `collect_public_extract_profile_repeated_board` | 5-run `96x72`、`iterations=3`、`warmup=1`，生成 summary manifest 和 Doctor。 |
| docs refresh | `result.zh.md`、roadmap、matrix、README、evaluation、Handoff | 写清 profile 结论、是否产生下一 production candidate。 |

## Evidence Doctor 和 registry 规则

本阶段新增 summary manifest 路径：

- `doc/phases/080-public-extract-component-profile/repeated-evidence-manifest.json`
- `doc/phases/080-public-extract-component-profile/repeated-evidence-doctor.md`
- `doc/phases/080-public-extract-component-profile/repeated-evidence-doctor.json`

异常处理：

- `Error`：checksum 不一致、Std/RVV profile component 集合不一致、metadata 缺失或 repeated run 数不足时，先修复并重跑；不能关闭阶段。
- `Warning`：长尾、B/A 跨方向、component sum 与 public total 明显不一致时，解释并按需要复跑一次。
- `Suggestion`：补温度 / governor / binary hash 时记录为后续可选项，不阻塞当前诊断结论。

registry target 使用 `evidence_role=diagnostic-profile`；raw repeated logs 默认 local-only（仅本地保留）。

## 板卡复跑预算和决策桶

- 初始预算：1 次 5-run repeated board，`--width 96 --height 72 --iterations 3 --warmup 1 --case public_extract_profile`。
- 追加预算：只有 Evidence Doctor 出现未解释 Warning、checksum / component 集合不一致、或 median B/A 落在 `0.98-1.02` 且方向影响下一步时，最多追加 1 次同边界 repeated。
- 决策桶：
  - `profile_actionable`：某个未优化组件占 public profile 总耗时 >= 20%，且该组件有清晰 RVV 候选或同构 component A/B 需求。
  - `profile_non_actionable`：热点主要在 max-flow / graph mutation 状态机，或未优化组件占比不足以支撑生产改动。
  - `unstable`：两次 repeated 的热点排序或 B/A 方向跨桶摇摆。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic-profile（诊断剖析，只提供瓶颈线索） |
| A/B boundary | test-only subclass wrapper，形态模拟 public `setBackgroundPointsIndices()` + `extract()` |
| 当前决策问题 | implementation-shape：是否存在值得规划的新 RVV family |
| diagnostic 是否可外推到 production | no。它复用 protected 方法和测试包装，只能决定是否进入后续 production direct 计划。 |
| comparison-boundary / baseline mismatch 风险 | yes。component profile 可能改变 protected 调用切分和计时边界，不能替代真实 public wall-time。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no，除非 profile 明确显示未优化组件占比高且可写出同边界 production direct 计划。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes。任何新 family 若要替换或补充当前 adopted helper，必须先做同一 production boundary 内的 RVV-vs-RVV detail A/B。 |

## 阶段完成条件

- `completed / profile_actionable`：QEMU smoke、board repeated、Evidence Doctor 和 registry 均闭合，并识别出一个高占比、低风险、当前 topic 授权内的新候选；下一 phase 是该候选的 production-shaped diagnostic 或 RVV-vs-RVV detail A/B。
- `completed / profile_non_actionable`：证据显示剩余热点不适合当前 topic RVV production 推进；更新 roadmap / matrix 并暂停。
- `blocked`：工具、板卡、checksum、Doctor Error 或 dirty isolation 使当前证据无法归属。

## 继续 / 停止条件

默认继续到 QEMU smoke、板卡 repeated、Evidence Doctor、registry 和文档刷新。只有以下情况停止：

- profile 结果指向 production 源码新改动，且需要用户单独授权 production integration loop。
- 板卡不可达或复跑预算后结果仍 unstable。
- Evidence Doctor Error 未能修复。
- dirty isolation 显示用户改动与本阶段文件冲突。
- matrix 和 roadmap 已无当前授权内的高价值未阻塞动作。

## 文档更新清单

- 新增本阶段 `result.zh.md`。
- 更新 `doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md`、`doc/optimization-roadmap.zh.md`。
- 若 profile 只改变候选排序，不改变 adopted production truth，长期 `doc-rvv/segmentation/grabcut_segmentation-RVV.zh.md` 只需审计 freshness，不把诊断剖析写成 production 证据。
- 更新 current Handoff Markdown / YAML。

## roadmap 同步动作

本阶段会把 `public extract profile before new production family` 从 `deferred` 推进到 `planned / attempted`。根据结果，color staging、organized n-link production expansion、GMM learn/build 或 max-flow solver 分别进入 `profile_actionable`、`rejected with evidence`、`deferred with blocker` 或 `not_applicable with evidence`。
