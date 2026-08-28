# sac_model_plane 优化路线图

## 当前边界

本 topic 覆盖 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_plane.hpp` 中基础平面模型的
三个距离入口：`countWithinDistance`、`selectWithinDistance` 和 `getDistancesToModel`。不扩大到
normal-plane、sphere、circle、line/stick 或 SAC 方法后处理。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| indexed xyz gather + shared distance kernel | 既有 `countWithinDistanceRVV` 和 common RVV load helper | PointXYZ-like、registered single-float x/y/z、direct indexed `indices_` | 复用 `distRVV_f32m2`，让 select/getDistances 避免逐点 Eigen 临时对象 | 32-bit byte offset gate、float 计算与 double 输出边界、环境 metadata 待增强 | production direct correctness、fallback、QEMU、asm、board bench、Evidence Doctor | adopted | 000-base-plane-select-distance-production |
| select `vcompress` writeback | 当前 base-plane 输出顺序需求 | `selectWithinDistance` | 连续写回 inliers 和 distance buffer | 输出容器 resize/shrink 合同、阈值附近差异、环境 metadata 待增强 | direct helper 对拍、public entry 对拍、board bench | adopted | 000-base-plane-select-distance-production |
| identity-index strided load for select/count | 默认 cloud-only 构造会生成 `indices[i]=i` | `selectWithinDistance`、`countWithinDistance` | 用 `vlsseg3e32` 替代 `vluxei32` gather，降低默认整云路径访存成本 | identity check 成本、shuffled fallback 是否退化 | identity/shuffled bench、QEMU correctness、asm、board repeated、Evidence Doctor | adopted_narrow | 010-identity-index-strided-load |
| identity-index strided load for getDistances | Phase 010 mixed branch | `getDistancesToModel` | 理论上减少 gather 成本 | 实测收益不稳定，且早期 mixed gate 让 shuffled 路径退化 | 同一 production boundary RVV-vs-RVV detail A/B | rejected with evidence | reopen only with new design |
| point type expansion | generic point type strategy | 非 `PointXYZ` 但满足 registered float xyz 的点型 | 验证现有 traits gate 是否能安全覆盖更宽模板实例 | dedicated board performance 仍缺，不能外推 `PointXYZ` 性能 | representative correctness、board RVV gtest smoke | adopted for correctness | 020-point-type-expansion |
| explicit empty `indices_` correctness | reviewer gap review | `PointXYZ` 显式空子集 | 防止把默认整云 identity 误当成空 `indices_` 覆盖 | correctness-only，不产生性能收益 | Std/RVV public entry 对拍、board RVV smoke | adopted for correctness | 025-empty-indices-correctness-gap |
| topic closeout / submit readiness | doc and implementation closeout gate | 当前 `sac_model_plane` topic | 结束 topic 前确认文档结构、分发规范和提交边界 | 不产生性能收益；只做 closeout | doc-suite audit、production dispatch audit、验证命令、私有信息扫描 | ready_for_topic_commit | 030-topic-closeout-submit-readiness |
| evidence registry / metadata hardening | workflow quality gate | 当前 topic 证据登记 | 让复跑和文档 freshness 可自动审计 | topic-local manifest wrapper 已有；完整 registry、环境字段和 binary hash 未接入 | board repeated summary、Evidence Doctor JSON、registry status | turn_stop_deferred: not a performance candidate | 030-evidence-registry-hardening |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 000 | board evidence recovered through SSH agent | production patch、QEMU correctness 和 asm 已闭合；通过 `SSH_AUTH_SOCK=<agent-socket>` 跑通 5-run repeated board，三个入口均为 positive。 | PI5 用户确认后采纳；若需要更强环境证据，补 taskset、governor、freq、temperature 和 binary hash。 | done |
| 000 | `PointXYZI` correctness added | traits-based gate 覆盖面大于 exact `PointXYZ`，需要至少一个非 exact 点型回归。 | 后续补更多 PointXYZ-like 点型和 board evidence。 | medium |
| 010 | identity fast path should be per-entry | select/count 有收益，getDistances 的 identity branch 不稳定。 | 若重开 getDistances，必须做 RVV-vs-RVV detail A/B，而不是只看 Std/RVV public speedup。 | low |
| 020 | representative point types compile and pass | base-plane 只读 `x/y/z`，`PointXYZRGB/RGBA` 和 `PointXYZINormal` 的额外字段不参与输出语义。 | 若要写这些点型的性能结论，新增 dedicated board repeated bench。 | closed for correctness |
| 025 | explicit empty `indices_` is distinct from cloud-only identity | 原有 `{}` helper 表示不调用 `setIndices`，不能证明显式空子集。 | 已补 public entry 与 Standard 对拍；不需要性能证据。 | closed for correctness |
| 030 | closeout confirms no further performance candidate | 当前剩余事项是证据归档硬化，不是 RVV 性能路线。 | 已补 doc suite、production dispatch 和提交边界审计。 | ready_for_topic_commit |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| `getDistancesToModel` identity strided load | Phase 010 mixed gate 不稳定，当前 gather-only 已有 Phase 000 positive。 | 出现低开销 identity 检测、不同 VL/LMUL 方案或 profile 显示 gather 是主瓶颈。 |
| sphere/circle 复用 | 不属于本 topic，且距离公式与输出入口不同。 | base-plane closeout 后回到 sample_consensus 队列。 |
| normal-plane 角度近似 | normal-plane 已有独立 topic，acos 近似风险不适用于 base-plane。 | normal-plane topic 恢复时处理。 |

## 默认恢复动作

1. 当前 RVV performance candidate 已无未阻塞默认项：Phase 000/010 已采纳，getDistances identity 已拒绝。
   Phase 025 关闭的是 correctness 覆盖缺口，不是新性能候选。
2. `030-evidence-registry-hardening` 可在准备提交或归档证据时执行，目标是补完整 registry、
   环境 metadata 和 binary hash，不改变性能结论。
3. `getDistancesToModel` identity branch 暂不继续；只有新设计能降低 identity 检测成本时才重开。
