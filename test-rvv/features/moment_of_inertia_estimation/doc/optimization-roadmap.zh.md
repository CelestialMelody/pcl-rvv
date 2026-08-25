# Optimization Roadmap

## 当前边界

当前 topic 针对 `MomentOfInertiaEstimation<PointT>` 的逐点规约热点。phase 000 和 phase 010 是 diagnostic（诊断）层 test/bench helper；phase 030 做过 mean/AABB-only production probe（只接入质心和轴对齐包围盒的生产探针），但 public compute（公开 `compute()`）板卡证据为 `neutral` 且 Evidence Doctor 有 Error，已按用户确认回滚。phase 040 接入 projected covariance fusion（投影协方差融合）production path，public compute 板卡证据为 `positive`，当前 production patch 已采纳。phase 050 补齐常见 PointXYZ-like typed scope（点型范围）证据，四组 typed public compute 都为 `positive`。

本轮 topic token（主题短标识）使用 `moi`，对应目录 `test-rvv/features/moment_of_inertia_estimation` 和源码文件 `src/test_moi.cpp`、`src/bench_moi.cpp`。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| fused xyz reductions | 当前源码 shape；features 队列表建议 | mean/AABB、covariance、single-axis inertia、OBB extrema | 减少逐点循环标量开销，复用 xyz load（坐标加载）和向量规约 | 浮点规约顺序变化；indexed gather 成本；小输入收益不稳定 | Std/RVV correctness、asm、board repeated、Evidence Doctor | attempted_positive_diagnostic：board median 2.082x，Errors=0 / Warnings=0 | production plan input |
| projected cloud covariance fusion | 当前源码每个角度先 materialize projected cloud 再协方差 | `getProjectedCloud` + projected `computeCovarianceMatrix` | 去掉临时点云分配和写回，减少内存流量 | 公式推导和 eccentricity 边界复杂；每个角度仍要 Eigen 求解 | 公式对拍、component bench、board A/B | attempted_positive_diagnostic：board median 1.247x，Errors=0 / Warnings=0 | production plan input |
| PI1 production integration plan | phase 000/010 positive diagnostic | public `compute()` 的窄范围生产接入设计 | 冻结真实 production helper、fallback、traits gate 和 direct evidence plan | 需要 traits / fallback / public direct test；生产源码修改未授权 | PI1 plan、fallback matrix、production direct commands、pause conditions | completed_plan_only | PI2 after explicit authorization |
| production mean/AABB RVV helper | phase 030 bounded production probe | public `compute()` 的 `computeMeanValue()` | 真实生产入口前置 mean/AABB 加速 | public compute 中占比过小；完整输出 checksum 仍只是诊断信号；Evidence Doctor Error | production direct tests、asm、5-run board、Evidence Doctor | historical rejected probe：public median 1.018x，bucket `neutral`，2/5 退化 | 已回滚，不再恢复 |
| projected covariance production probe | phase010 diagnostic 正向 + phase030 反思 | angle scan 内 projected covariance | 更接近 public compute 的重复热点，抵消 allocation + covariance 成本 | 已采纳的生产行为；后续仅剩点类型扩展、layout 扩展或新 workload 重新验证 | test-first production plan、public/detail board evidence、Evidence Doctor | adopted_production_behavior：public median 1.984x，bucket `positive`，0/5 退化 | 下一步进入 point type expansion 或保持当前 patch |
| PointXYZ-like production evidence expansion | phase040 adopted + phase050 typed board | common PointXYZ-like typed public compute | 验证同一 helper 在 PointXYZI / RGB / RGBA / RGBNormal 上仍保持收益 | 更宽 stride / helper 复用的边界是否依旧正向；但已没有新算法点可挖 | typed public compute board、Evidence Doctor、registry freshness | completed_common_scope：4 组 typed board 全部 positive，Doctor 0 Error / 0 Warning | 默认停止同类继续；custom point type / exotic layout 另开 phase |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 000 | reduction summary helper-only diagnostic 正向 | board repeated bucket 为 `positive`，且 Evidence Doctor 无 Error / Warning | PI1 需要把 reduction summary 拆成真实 production helper 或生产形态子 helper，并补 production direct 证据 | high |
| 010 | projected covariance fusion helper-only diagnostic 正向 | `getProjectedCloud` + projected covariance 公式可同构对拍，board median 1.247x | PI1 需要决定是否在 angle scan 内直接计算 projected covariance，还是先只接入 phase000 规约片段 | high |
| 010 | binary identity metadata 补强 | phase000/010 doctor 都提示缺二进制身份字段 | 生产证据重跑前在 manifest 中补 binary hash、build label 或等价身份字段 | medium |
| 020 | PI1 计划已冻结生产接入边界 | 继续需要修改 production 源码，命中授权边界 | 用户确认后执行 PI2，并按 plan 补 public direct correctness、fallback、asm、board 和 Evidence Doctor | high |
| 030 | mean/AABB-only production probe 不值得采纳 | public compute repeated board 为 `neutral`，Evidence Doctor Error=`ba_degradation_frequency` | 已按用户确认回滚，保留为 historical rejected probe | high |
| 040 | projected covariance production probe 已采纳 | public compute repeated board 为 `positive`，Evidence Doctor 0 Error / 0 Warning | 若继续推进，先做 point-type expansion 或 layout 扩展的新 phase | high |
| 050 | common PointXYZ-like typed scope positive | 4 组 typed public board 都是 `positive`，Doctor 无 Error / Warning | 当前 helper 的 common typed scope 已闭环；若还要继续，只能开 custom point type / layout 新 phase | high |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| full compute replacement | 直接替换 `compute()` 会把 Eigen 求解、角度扫描和 production fallback 全部绑在一起，无法 test-first 证明单一收益来源 | phase 000/010 形成 positive diagnostic evidence（诊断证据）并通过 PI1 production scope gate |
| mean/AABB-only clean adoption | phase030 production-public median 1.018x，2/5 退化，Evidence Doctor 有 Error；helper-only 2.082x 不能外推到 public compute | 需要新的 production-public 证据证明稳定 positive；当前不建议继续在该 patch 上加跑 |

## 默认恢复动作

| order | phase | scope | status | blocker / authorization | action |
| ---: | --- | --- | --- | --- | --- |
| 1 | `020-pi1-production-integration-plan` | topic-local plan 和 evaluation 更新，不改 production | completed | none | 已冻结 production 接入范围、traits gate、fallback matrix、direct test、asm 和 board 计划 |
| 2 | `030-production-mean-aabb-pi2-pi5` | mean/AABB-only production patch、public direct tests、board evidence | historical_rejected | 已按用户确认回滚 | 保留为历史证据，不再作为当前恢复入口 |
| 3 | `040-projected-covariance-production-probe` | angle scan 内 projected covariance fusion | adopted | none | 当前生产实现与长期文档同步 |
| 4 | `050-point-type-expansion` | 常见 PointXYZ-like typed scope | completed | none | 已完成常见 typed scope 证据；默认暂停，custom point type / layout 需新 phase |
