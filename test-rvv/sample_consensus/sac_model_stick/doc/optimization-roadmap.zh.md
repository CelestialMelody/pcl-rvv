# sac_model_stick optimization roadmap

## 当前边界

当前 topic 来自 `doc-rvv/library-screening/sample_consensus/sample_consensus-function-evaluation-queue.zh.md` 的顺序 6。Phase 080 已把 `countWithinDistance`、`selectWithinDistance` 和 `getDistancesToModel` 三个公开入口接入 production direct（真实生产入口直连）RVV 路径。Phase 100 进一步把 `getDistancesToModelRVV` 的 staged scalar lane（暂存后逐向量通道标量写回）改成 RVV mask / merge（掩码 / 合并）、float-to-double widening（float 到 double 扩宽）和 `vse64.v` 向量写回。接入后的 Phase 100 5-run board repeated（板卡重复采集）显示 public getDistances speedup min / median / max 为 3.4122x / 3.6879x / 3.7150x，Evidence Doctor（证据体检）为 Errors=0、Warnings=0、Suggestions=0；当前状态为 `production-adopted`。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| count-indexed-gather-f32m2-dual-count | 当前 stick 源码 + line 结构参照 | direct indexed `countWithinDistance` | 减少每点 Eigen cross3 标量开销，并批量完成内外阈值计数 | stick 返回 `max(nr_i - nr_o, 0)`，不是普通 count；阈值边界易错 | Std/RVV correctness、asm、board repeated、Evidence Doctor | adopted via Phase 080 production path | Phase 000 diagnostic completed；Phase 080 production adopted |
| select-indexed-gather-f32m2-compress | line/sphere/circle 的 select 候选经验 + Phase 020 | `selectWithinDistance` | 复用距离 mask 并用 `vcompress` 保序写回 inliers 和平方距离 | 需要维护 `error_sqr_dists_` 顺序和容量语义 | select correctness、asm、board repeated、Evidence Doctor | adopted via Phase 080 production path | Phase 020 diagnostic completed；Phase 080 production adopted |
| getDistances-indexed-gather-f32m2-sqrt-store | 当前源码 + Phase 040 | `getDistancesToModel` | 批量距离计算减少标量 cross3 和 sqrt loop 开销 | 该入口的系数 3-5 当方向，和 count/select 的端点语义不一致 | coefficient audit、same-chain correctness、asm、board repeated、Evidence Doctor | adopted via Phase 080 production path | Phase 040 diagnostic completed；Phase 080 production adopted |
| production-stick-three-entry-rvv | Phase 000/020/040 positive-stable candidates + Phase 080 production patch | production `countWithinDistance`、`selectWithinDistance`、`getDistancesToModel` | 在真实公开入口保持诊断候选收益，并保留 Standard fallback | deprecated class 的维护收益有限；点型性能范围仍需独立扩展 | PI2 patch、production direct tests、fallback、asm、board repeated、Evidence Doctor、正式 `doc-rvv` | production-adopted | Phase 080 completed |
| doc-suite-structure | Phase loop maturity audit | topic-local docs | 让 reviewer 从稳定 role 文档恢复测试、bench、证据、代码地图和 production gate | 只改变文档结构，不改变证据结论；新增文档必须进入 artifact tracking | README、evaluation、phase index、matrix、registry freshness、diff check | completed | Phase 060 completed |
| getDistances-target-granularity | Phase 060 target 粒度审计 | topic-local Makefile alias | 让 reviewer 快速单跑 getDistances 两个 correctness case | 只改变测试入口粒度，不改变 correctness 覆盖范围或 evidence decision | RED/GREEN alias 验证、run_test_compare、registry freshness、diff check | completed | Phase 070 completed |
| point-type-expansion | PCL traits gate 策略 | common xyz AoS point types | 扩大当前 traits-gated production path 的 correctness 证据范围 | dedicated board performance 仍缺，不能外推 `PointXYZ` 性能 | `PointXYZI` / RGB / RGBA / RGBNormal correctness；若做性能再补 board / Evidence Doctor | adopted_for_representative_point_type_correctness | Phase 090 completed |
| getDistances-vector-penalty-writeback | 用户指出 getDistances RVV loop 内仍有 scalar lane 写回 + normal_plane 写回形态参照 | production `getDistancesToModelRVV` | 移除 `sqr/dist` 临时 staging 和逐 lane 标量写回，直接用 RVV mask / merge 和 `vse64.v` 写 dense distance | 旧/新 RVV family 严格 delta 需要同轮 A/B；历史 Phase 080 只能作 baseline 对照 | focused correctness、aggregate correctness、source/asm gate、5-run board repeated、Evidence Doctor、registry | production-adopted | Phase 100 completed |
| dedicated-point-type-performance | Phase 090 follow-up | common xyz AoS point types | 若维护者需要，把性能结论从 `PointXYZ` 扩大到常见点型集合 | 需要新 bench label、板卡预算和 Evidence Doctor；deprecated class 的收益边界需控制 | point-type-specific bench、board repeated、Evidence Doctor、registry | turn_stop_deferred with stop_condition_hit | new board-performance scope |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| `000-stick-count-diagnostic` | production-count-rvv-probe | candidate median 4.1767x 且 min 4.1429x，支持尝试有界生产探针 | 用户授权、production direct correctness、fallback、asm、board repeated、Evidence Doctor | high |
| `020-stick-select-diagnostic` | production-select-rvv-probe | candidate median 3.4432x 且 min 3.4208x，支持尝试有界生产探针 | 用户授权、production direct correctness、fallback、asm、board repeated、Evidence Doctor | high |
| `040-stick-getdistances-diagnostic` | production-getDistances-rvv-probe | candidate median 2.5553x 且 min 2.5416x，支持尝试有界生产探针；public 行的 0.9359x median 已降级为未接 RVV 交叉检查 | 用户授权、production direct correctness、fallback、asm、board repeated、Evidence Doctor | high |
| `060-stick-doc-suite-structure` | getDistances correctness alias | 当前 aggregate correctness 和 role docs 已足以恢复证据，但没有单独 `run_stick_getdistances_tests` 快捷 target | Phase 070 补 Makefile alias 和 README 命令 | low |
| `070-stick-getdistances-target-granularity` | no new RVV candidate | target 粒度缺口已关闭；没有暴露新的测试资产结构动作 | 若进入 PI2，再按对应 PI1 plan 增加 production direct / fallback target | none |
| `080-stick-production-integration` | `090-stick-point-type-expansion` | 当前生产 gate 已允许 traits-gated xyz AoS `PointT` 命中 RVV，但 board 性能证据只覆盖 `PointXYZ` | 为 `PointXYZI`、RGB/RGBA 和 RGBNormal 补 public correctness；性能扩展另建 scope | medium |
| `090-stick-point-type-expansion` | dedicated point-type board performance | 代表点型 correctness 已闭合，但 dedicated performance 未覆盖 | 新 bench label、板卡复跑预算、Evidence Doctor 和 registry | low |
| `100-stick-getdistances-vector-writeback` | no same-scope implementation-shape candidate | 用户指出的 scalar lane 写回已改为 RVV mask / merge + `vse64.v`，并通过接入后 board repeated；同边界 getDistances 写回形态暂未暴露新的高价值动作 | 若 reviewer 要求严格 family delta，再做旧/新 RVV A/B；否则无需作为默认下一 phase | none |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| identity-index strided load | 当前 stick 公开入口固定扫描 `indices_`，没有独立 identity row source；相邻 circle Phase 040 的同边界 A/B 也显示该方向不占优。 | 只有源码出现真实 identity-index 热路径，或 profile 显示 gather 是主瓶颈时再复筛。 |
| upstream RANSAC full-flow bench | 会把采样、模型验证和上游控制流混入计时边界，不能直接解释三条 public helper 的收益。 | 若维护者要回答端到端用户场景，再另建上游 profile / E2E scope。 |
| all custom point types | 代表点型 correctness 已覆盖常见显式实例，但自定义点型全集无法在当前 topic 中穷尽。 | 具体用户点型出现编译或运行需求时，按 traits / layout / fallback 单独补 case。 |

## roadmap_default_recovery_queue

| phase | scope | status | blocker | next action |
| --- | --- | --- | --- | --- |
| `080-stick-production-integration` | production `countWithinDistance`、`selectWithinDistance`、`getDistancesToModel` | completed / production-adopted | none | 已闭合；当前生产行为保留 |
| `090-stick-point-type-expansion` | common xyz AoS point type correctness | completed / adopted_for_representative_point_type_correctness | none | 已闭合；不作为下一轮默认动作 |
| `100-stick-getdistances-vector-writeback` | production `getDistancesToModelRVV` implementation shape | completed / production-adopted | none | 已闭合；当前生产行为保留 |
| dedicated-point-type-performance | common xyz AoS point type performance | turn_stop_deferred with stop_condition_hit | 需要新 board-performance scope 和复跑预算 | 只有维护者需要这些点型的性能结论时才启动 |
| `060-stick-doc-suite-structure` | topic-local doc suite | completed | none | 已闭合；不作为下一轮默认动作 |
| `070-stick-getdistances-target-granularity` | topic-local correctness alias | completed | none | 已闭合；不作为下一轮默认动作 |
