# Normal-plane Optimization Roadmap

## 当前边界

当前 production 已在 `SampleConsensusModelNormalPlane<PointT, PointNT>` 的受控布局上提供三条 RVV helper。当前已证明范围是 `PointXYZ + Normal` 的 repeated board helper performance，`PointXYZ`、`PointXYZI`、`PointXYZINormal` source + `Normal` 的 public dispatch correctness，`PointXYZI` / `PointXYZINormal` 代表性 source protected helper performance，`PointXYZ + PointNormal`、`PointXYZ + PointXYZINormal` normal cloud 的 public dispatch correctness，以及 `PointXYZI` / `PointXYZINormal` source 与 `PointNormal` / `PointXYZINormal` normal cloud 的 4 个代表性交叉组合 public correctness。dispatch gate 已收紧到 source AoS byte-offset layout 和 normal/curvature AoS-compatible layout；本 roadmap 把更多 normal-like 点型、完整泛型点型、公开入口性能和 `Scalar=double` 继续保留为独立扩展队列。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| existing `f32m2` public-entry closeout | 当前源码与 sample_consensus 队列 | `PointXYZ + Normal` public `select/count/getDistances` | 证明已有 RVV patch 真实服务公开入口 | helper bench 不能证明 fallback 和 dispatch | public-entry correctness、QEMU、asm、board summary、Evidence Doctor | closed positive | 000 |
| stronger AoS layout gate | 当前 `RVVXYZAoSFloatLayout` / helper static_assert 审计 | representative AoS source + `Normal` public dispatch | 降低弱 field gate 外推风险 | 新增点型 correctness 不能自动变成性能结论 | compile/fallback test、traits 审计、public direct test、board public alias | closed representative correctness | 040 |
| representative AoS source performance | Phase 040 扩展后新增的未验证性能范围 | `PointXYZI + Normal`、`PointXYZINormal + Normal` | 判断 source stride / extra fields 是否改变 helper hot-path收益 | 仍不是完整 public overload 计时，也不覆盖其它 source 点型 | dedicated board summary、Evidence Doctor、registry refresh | closed positive-stable | 050 |
| normal layout expansion | Phase 040 normal gate 仍只验证 `Normal` 和 double-curvature fallback | representative registered normal-like `PointNT` | 判断 normal/curvature AoS-compatible gate 是否可扩大 | 不覆盖所有 normal 点型或 source × normal 交叉组合 | compile test、public fallback/direct tests、board public alias | closed representative correctness | 060 |
| source × normal cross-product | Phase 070 已补 Phase 060 后留下的交叉组合缺口 | representative source × representative normal | 判断两侧模板参数同时变化时 byte-offset helper 是否仍按各自 layout 工作 | 不覆盖更多 normal-like 点型或公开入口性能 | public direct tests、board public alias、freshness check | closed representative correctness | 070 |
| test support structure split | worker-quality-gates 结构门槛 | `plane_models` 测试资产 | 提高 reviewer 可读性，隔离 plane 与 normal-plane topic | 后续 point-type expansion 可能需要再拆 helper header | `src/`、README/doc suite、artifact tracking | closed positive | 010 |
| Evidence Doctor manifest / registry | rvv-test evidence policy | board compare / asm / checksum summary | 让 board 结果可复核并发现 stale evidence | 后续仍需在复跑后继续 record/check | manifest、doctor report、registry 或人工 freshness check | closed positive | 020 |
| repeated board summary | rvv-test board evidence policy | protected helper board compare | 把当前 run_count=1 positive 提升为 repeated summary，降低偶然波动风险 | 不覆盖泛型点类型或新的 RVV 实现族选择 | 5-run summary、Evidence Doctor、registry refresh | closed positive-stable | 030 |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 000 | helper 直接调用也需要自保 resize | RED 测试证明 protected helper wrapper 可能绕过公开入口预分配 | 已补 production resize，并由 QEMU / board unit test 验证 | high |
| 000 | board fixture 参数需要区分 host path 和 remote path | 默认 board compare 把 host 绝对 PCD 路径传到板卡导致失败 | 后续结构 phase 修正 Makefile 边界，避免每次手工 override | high |
| 010 | topic-local doc suite 需要和 phase/evidence 分工 | source layout 与 board fixture 修正后，README、testing、correctness、benchmark/evidence、optimization evidence 和 code map 可以稳定引用当前路径 | role docs 已创建；后续只需补 registry / manifest alias | high |
| 010 | evidence registry 是下一项默认结构动作 | 当前 manifest / doctor 是 phase-local 手工维护，尚不能发现后续 Make target 覆盖日志后的未登记变化 | topic-local wrapper、registry target、doctor alias、sanitized summary check | high |
| 020 | repeated board summary 成为下一项证据增强动作 | manifest wrapper 和 registry 已关闭，剩余证据缺口集中在 board compare 仍是 run_count=1 | 5-run board compare summary、doctor / registry refresh、文档 freshness sync | high |
| 030 | repeated board summary 已关闭 | 5-run board compare 完成，三条 helper 的 median/min 均超过 positive-stable 阈值 | 后续只需在提交或复跑前检查 `repeated_evidence_status` | closed |
| 040 | source AoS gate 已关闭代表点型 correctness | 弱 `RVVXYZFloatLayout` gate 会让注册 xyz 但非 standard-layout 的 source 点型误实例化 byte-offset helper；收紧后 public dispatch 与 helper 前提一致 | 后续若要写新增点型性能，需 dedicated board performance phase；若要扩大 normal layout，需新增 normal 点型 gate phase | closed for current scope |
| 050 | representative AoS source performance 已关闭 | `PointXYZI` 和 `PointXYZINormal` 的 extra fields 改变 source stride，但 5-run board summary 仍全部强正向；Evidence Doctor Warning 说明不能按组均值外推 | 后续若继续，优先审计其它 normal-like layout；更多 source 点型扩展降为低优先级 | closed positive-stable |
| 060 | representative normal layout correctness 已关闭 | `PointNormal` 和 `PointXYZINormal` 作为 normal cloud 时，public entry 与 direct RVV helper 一致；registered float normal/curvature 但非 standard-layout 的 normal 点型会回退 Standard helper | 后续若继续，优先审计 source × normal 点型交叉组合；如果关注收益，另建 public-overload performance probe | closed representative correctness |
| 070 | source × normal 交叉 correctness 已关闭 | `PointXYZI` / `PointXYZINormal` source 与 `PointNormal` / `PointXYZINormal` normal cloud 的 4 个组合均通过 public-vs-direct RVV correctness；板卡 public alias 13/13 通过 | 后续若继续，需要明确是否扩大到更多 normal-like 点型、公开入口 repeated performance 或 `Scalar=double` 新 helper family | closed representative correctness |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| full generic point-type adoption | Phase 040/060/070 只关闭代表性 source、representative normal layout 与两者交叉 correctness；完整泛型点类型仍需要更宽点型采样、fallback、bench 和 board。 | 用户确认继续扩大到完整点型集合后，另建 point-type expansion phase。 |
| wider source point-type performance | Phase 050 已覆盖 `PointXYZI` / `PointXYZINormal` 两个代表点型，更多 source 点型预计只改变 source stride 或额外字段布局，不再是当前最高优先级。 | 用户要求扩大到 `PointXYZRGB`、`PointXYZRGBA` 或自定义 registered AoS xyz 点型，或现有代表点型 evidence 出现方向反转。 |
| more normal-like layouts | Phase 060 已覆盖 `PointNormal` 和 `PointXYZINormal` representative normal correctness，但未覆盖 `PointXYZRGBNormal`、`PointXYZLNormal` 或用户自定义 normal 类型全集。 | 生产调用点或用户要求指向更多 normal 点型时，补 traits audit、public direct/fallback correctness 和必要 board alias。 |
| source × normal cross-product | Phase 070 已关闭 4 个代表性交叉 correctness，不再是当前未闭合缺口。 | 若新增更多 source 或 normal 点型，再随对应 point-type expansion phase 组合覆盖。 |
| new RVV math approximation family | 本阶段不是实现族选择；已有 `getAcuteAngle3DRVV_f32m2` 已被 production 使用。 | Evidence Doctor 或 numerical test 显示当前近似无法支撑生产边界。 |
| internal helper header split | 当前没有共享 test helper header 或旧 `test_support/` 目录；强行创建 `include/impl` 会制造空结构。 | point-type expansion 或 RVV-vs-RVV A/B 新增共享 fixture / assertion / bench harness 时重新审计。 |

## roadmap_default_recovery_queue

| order | phase | scope | state | blocker | next action |
| ---: | --- | --- | --- | --- | --- |
| 1 | 000-normal-plane-current-state-and-public-entry-boundary | public-entry correctness / fallback / asm / board evidence | closed positive | none | 已完成，见 phase result |
| 2 | 010-normal-plane-test-support-structure | topic-local test support layout、doc suite、board fixture 参数边界 | closed positive | none | 已完成，见 phase result |
| 3 | 020-normal-plane-evidence-registry-target-alias | topic-local manifest wrapper、Evidence Doctor / registry alias、summary artifact freshness check | closed positive | none | 已完成，见 phase result |
| 4 | 030-normal-plane-repeated-board-summary | protected helper board compare repeated summary | closed positive-stable | none | 已完成，见 phase result |
| 5 | 040-normal-plane-aospoint-gate-expansion | source AoS gate、representative source point-type correctness、non-AoS fallback | closed representative correctness | none | 已完成，见 phase result |
| 6 | 050-normal-plane-representative-aos-source-performance | `PointXYZI` / `PointXYZINormal` representative source helper performance | closed positive-stable | none | 已完成，见 phase result |
| 7 | 060-normal-plane-normal-layout-expansion | representative registered normal-like `PointNT` layout correctness / fallback | closed representative correctness | none | 已完成，见 phase result |
| 8 | 070-normal-plane-cross-point-type-layout | source × normal representative cross-product correctness | closed representative correctness | none | 已完成，见 phase result |
| 9 | 080-normal-plane-public-overload-performance-probe | public overload repeated performance probe | turn_stop_deferred with stop_condition_hit | needs explicit performance scope | 只有需要回答公开入口计时是否仍强正向时启动。 |
| 10 | 080-normal-plane-more-normal-like-pcl-types | 更多 normal-like PCL 点型 correctness | turn_stop_deferred with stop_condition_hit | needs explicit point-type scope | 只有生产调用点或用户要求指向更多 normal-like 点型时启动。 |
| 11 | 080-normal-plane-scalar-double-helper-family | `Scalar=double` helper family | turn_stop_deferred with stop_condition_hit | expands implementation family | 需要新 RVV helper 设计和用户授权后启动。 |
