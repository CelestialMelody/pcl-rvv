# sac_model_sphere 优化路线图

## 当前边界

本 topic 覆盖 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_sphere.hpp` 中基础球模型的三个距离入口：`countWithinDistance`、`selectWithinDistance` 和 `getDistancesToModel`。当前源码已有 `countWithinDistanceRVV`，Phase 020 又为 `selectWithinDistance` 增加 production RVV dispatch（生产 RVV 分流）；`getDistancesToModel` 保持标量。

Phase 045/046 后的当前事实是：`countWithinDistance` 的现有 production RVV 路径稳定正向；`selectWithinDistance` 当前采用 `vcompress` production patch，`PointXYZ` public entry 板卡 repeated median 为 `2.0989x`；Phase 050 证明 `PointXYZI` public entry median 为 `1.5901x`；Phase 060 证明 `PointXYZRGB` median 为 `1.6225x` 且 5/5 run 正向，`PointXYZRGBA` median 为 `1.5528x` 但有 1/5 退化和长尾 warning。`getDistancesToModel` 的当前 scratch + scalar sqrt 候选为 negative，不进入 production。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| existing shell mask count | 当前 sphere `countWithinDistanceRVV` | production count entry | 已接入 count 的球壳双边界 mask（掩码）计数 | 后续生产改动可能影响 count，需要回归守住 | QEMU correctness、asm、board bench、Evidence Doctor | adopted/current production behavior | 已由 Phase 000 复核；后续 production phase 继续作为 regression（回归测试）保留 |
| RVV squared-distance + scalar output for select | 当前 select 标量循环 | production `selectWithinDistance` public entry | RVV 承担 xyz gather 和平方距离，标量处理 sqrt 与有序输出 | 已被 Phase 045 `vcompress` patch supersede；仍是可回滚 baseline | production direct correctness、board repeated、asm attribution、Evidence Doctor | adopted historical baseline | Phase 020 complete |
| RVV squared-distance + scalar sqrt/store candidate for getDistances | 当前 getDistances 标量循环 | production-shaped diagnostic for `getDistancesToModel` | 理论上 RVV 承担平方距离，标量 sqrt/store 输出 double | 5-run repeated board 全部退化，scratch 存储和标量 sqrt 可能主导成本 | 若恢复需先有 sqrt helper audit 或 component ablation | rejected for current family | 030-sqrt-helper-audit（仅在出现新 helper 或 profile 证据时恢复） |
| `vcompress` select writeback | 当前 select 输出 push_back 和 error distance 写回 | production `selectWithinDistance` public entry | RVV 压缩 inliers，减少标量输出扫描 | 自定义 xyz 点型和其它规模仍未覆盖；`getDistancesToModel` 不接入 | production direct correctness、board repeated、asm attribution、Evidence Doctor、用户确认 | adopted/current production behavior | Phase 045/046 complete；Phase 050/060 已覆盖 `PointXYZI` / RGB / RGBA |
| topic-local doc suite structure | `rvv-test` / `rvv-documentation` 结构门禁 | README、testing overview、correctness、bench/evidence、optimization evidence、code map | 提高短 prompt 恢复和 reviewer 定位能力 | 若后续新增 production direct 或 `vcompress`，可能需要继续拆 internal helper | artifact tracking、doc role inventory、`git status --untracked-files=all` | adopted | Phase 010 complete |
| evidence registry target alias | Phase 010 target 粒度审计 | repeated manifest / doctor / registry freshness | 让 Phase 000 summary evidence 可通过正式 target 恢复 | 当前仍不提交 raw logs；production direct 需新 registry entry | `record_repeated_board_evidence_state`、`repeated_evidence_status` | adopted | Phase 015 complete |
| production integration loop for select | Phase 000 select 正向后 | production `selectWithinDistance` public entry | 补齐 count 之外的 select 公开入口 | 不包含 getDistances；点型性能扩展和 `vcompress` 仍需独立 phase | PI1-PI5 | adopted | Phase 020 complete |
| point type expansion | 泛型点类型 gate 策略 | `PointXYZI` / `PointXYZRGB` / `PointXYZRGBA` / 自定义 xyz 点型 | 避免把 `PointXYZ` board 结果外推成泛型 production performance | RGBA 有 1/5 public select 退化 warning；自定义点型没有代表类型 | correctness、fallback、asm、board、Evidence Doctor | adopted for built-in tested point types; RGBA with stability warning | 自定义点型 only after representative type is defined |
| test support include / impl layout | 用户反馈 + `.agents/config/defaults.yaml` | topic test / bench support sources | 让 fixture、candidate、bench harness 和 assertions 有稳定聚合入口，降低后续点型扩展的 src 大文件负担 | 后续若 helper 继续膨胀，可再按 fixtures/assertions/bench_cases 细拆 | structure phase plan、QEMU correctness、bench log shape、git diff check | adopted | Phase 055 complete |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 000 | 独立 sphere topic scaffold | 历史 `quadric_models` 混合多个模型，不适合作为 select/getDistances 的恢复入口。 | README、evaluation、phase result、matrix、QEMU/board 输出。 | high |
| 000 | select 与 getDistances 分拆决策 | repeated board 显示 select 候选 positive-stable，而 getDistances 当前候选 negative。 | select PI1 计划；getDistances sqrt/helper audit 另行恢复。 | high |
| 000 | doc-suite structure parity | 本 topic 已产生 board repeated manifest 和 Evidence Doctor，evaluation 单文件不足以承担所有恢复职责。 | README、testing overview、correctness tests、benchmark/evidence、optimization evidence、test-support code map。 | high |
| 010 | PI1 生产边界冻结 | doc suite 已让 Phase 000 证据可恢复，可以把 select 候选推进到生产接入计划。 | Phase 020 已按该计划进入 PI2-PI5。 | high |
| 015 | registry freshness target | Phase 010 target 粒度审计发现 repeated doctor / registry alias 未闭合。 | 已补 Make target 并验证 `repeated_evidence_status` fresh；production direct 需新 entry。 | high |
| 020 | production direct select adoption | public `selectWithinDistance` 接入后 5-run board repeated median `1.5020x`，符号级 RVV 指令归属闭合。 | Phase 040/045 已继续验证 `vcompress` 实现族。 | historical |
| 040 | `vcompress` RVV-vs-RVV detail A/B | test-only helper 相对 Phase 020 production select median `1.3020x`，但存在 mixed-boundary Warning。 | Phase 045 已接入 production direct 重跑。 | historical |
| 045 | production `vcompress` user checkpoint | public `selectWithinDistance` 5-run board repeated median `2.0989x`，select row Doctor clean，生产符号内可见 `vcompress.vm`。 | 用户已确认收益即可采纳，Phase 046 已升级为 adopted。 | historical |
| 046 | `vcompress` production closeout | 用户确认 Phase 045 positive-stable 结果可采纳。 | 长期 `doc-rvv` 和 topic-local docs 已刷新为 adopted truth。 | historical |
| 050 | `PointXYZI` point-type expansion | public `selectWithinDistance<PointXYZI>` 5-run board median `1.5901x`，select row Doctor clean，点型实例 asm attribution 闭合。 | 继续扩 RGB/RGBA 前先处理 test support `include/impl` 布局。 | high |
| 055 | test support `include/impl` layout | 用户指出结构差异；配置和 phase-loop 规则也记录聚合头 / internal helper 质量门槛。 | 已迁移并通过 correctness、PointXYZI bench smoke 和 asm dump。 | high |
| 060 | RGB/RGBA point-type expansion | `PointXYZRGB` median `1.6225x` 且 5/5 正向；`PointXYZRGBA` median `1.5528x` 但有 1/5 退化和长尾 warning。 | 自定义点型需要先定义代表类型；`getDistancesToModel` 仍需新实现族证据。 | review-ready |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| circle2d 合并实现 | circle 只需要 x/y 字段，虽然 shell 公式相似，但点型布局和输出边界不同。 | sphere Phase 000 结束后回到队列表顺序 4。 |
| normal-sphere 扩展 | normal-sphere 有法线方向、angle 和权重边界，不能从基础 sphere 外推。 | normal-plane / normal-sphere 策略单独复筛。 |
| 当前 getDistances scratch + scalar sqrt 候选 | Phase 000 5-run repeated board median `0.7781x`，5/5 run 退化；当前形态额外 scratch store 后仍要逐点标量 sqrt。 | 只有出现 RVV sqrt/helper 语义审计、profile 或新的 dense-store 消融计划时恢复。 |

## 默认恢复动作

1. 整理 / review-ready 检查：复核 docs、registry freshness、stale text、diff check 和 handoff；本轮收尾后可进入提交选择。
2. 自定义 registered xyz 点型扩展：只有先定义代表类型、字段 offset、规模和 expected gate 后启动；当前不作为未阻塞优化方向继续推进。
3. `030-sqrt-helper-audit`：仅当需要恢复 `getDistancesToModel` 时启动，先审计 RVV sqrt/helper 语义和成本；不得沿用 Phase 000 的 negative helper 直接接 production。
