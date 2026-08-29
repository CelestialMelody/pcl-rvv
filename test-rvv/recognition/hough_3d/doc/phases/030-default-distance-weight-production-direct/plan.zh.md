# Phase 030 Plan: default-distance-weight-production-direct

## 阶段意图和边界

本阶段验证 `Hough3DGrouping::houghVoting()` 的默认 distance weight（距离权重）
配置：`use_interpolation=true`、`use_distance_weight=false`。phase 010 使用
`use_distance_weight=true`，而生产类默认值是 `false`，因此需要单独确认默认配置下
当前 vote generation RVV patch 是否能转成可采纳的 production direct（真实生产路径直连）证据。

本阶段不修改 production 源码，不扩大到 `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA`，
也不实现 `voteInt()` / voter tracking RVV。若默认配置的 production direct 仍为
`neutral`、`negative` 或 Evidence Doctor（证据体检）提示不可支撑采纳，则当前 patch
继续保持 attempted，不创建 `doc-rvv` 长期生产文档。

## 当前状态清单

| 项目 | 当前事实 | 路径 |
| --- | --- | --- |
| production patch | 已接入 vote generation RVV helper，但尚未 adopted | `recognition/include/pcl/recognition/impl/cg/hough_3d.hpp` |
| 默认配置 | `use_interpolation_{true}`、`use_distance_weight_{false}` | `recognition/include/pcl/recognition/cg/hough_3d.h` |
| phase 010 | `use_distance_weight=true` 的 production direct 为 `neutral`，median `0.995x` | `test-rvv/recognition/hough_3d/log/board/repeated_phase010_production_direct/summary.md` |
| phase 020 | no-interpolation 消融为 `neutral`，median `1.002x` | `test-rvv/recognition/hough_3d/log/board/repeated_phase020_no_interpolation/summary.md` |

## Phase Scope 与扩展队列

- `validated_scope`：`PointXYZ` / `float` / AoS + `ReferenceFrame`，correspondence-pair，
  `houghVoting()` public production boundary，`use_interpolation=true`、
  `use_distance_weight=false`。
- `unvalidated_scope`：其它点型、其它 source/target 组合、`Scalar`、layout、
  `use_interpolation=false` adopted path、`voteInt()` RVV 和 `findMaxima()`。
- `point_type_expansion_queue`：只有默认配置 production direct 出现可接受正收益后才恢复。
- `phase_closeout_boundary`：只关闭默认 distance weight 配置下的当前 production patch 是否值得采纳。

## 假设与候选族

| candidate family | 假设 | 风险 | 本阶段状态 |
| --- | --- | --- | --- |
| `production-direct-default-distance-weight-rvv` | 默认配置不使用 distance weight，RVV vote generation 可能少受后续权重计算影响 | `voteInt()` scatter 仍可能吞掉收益；旧 phase 010 的负向可能仍成立 | planned |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `production-direct-default-distance-weight-rvv` | correspondence-pair | `PointXYZ` / `float` / AoS + RF axes | `houghVoting()` 默认 distance weight 配置 | `run_test_compare` + `PCL.Hough3DProductionVotingPathRuns` | `board_phase030_default_distance_weight_repeated` | 待补 | `pcl::Hough3DGrouping<pcl::PointXYZ, pcl::PointXYZ, pcl::ReferenceFrame, pcl::ReferenceFrame>::houghVoting` | 待补 | planned | 跑板卡 repeated 后决定是否采纳 |

## 实现和测试动作

| action | 产物 / 命令 | 预期证据 | 完成判据 |
| --- | --- | --- | --- |
| 补 target | `Makefile` 新增 phase 030 board / manifest / doctor / registry target | 能复用现有 bench CLI 的第 8 个布尔参数关闭 distance weight | target 可解析且进入 freshness 检查 |
| 本地正确性 | `make -C test-rvv/recognition/hough_3d run_test_compare` | Std / RVV 对拍和 production path-hit 不退化 | exit 0 |
| 板卡 repeated | `SSH_AUTH_SOCK=/tmp/ssh-iEjvVUej0dT1/agent.101441 make -C test-rvv/recognition/hough_3d board_phase030_default_distance_weight_repeated` | 默认配置 production direct 的 repeated board summary | 有界 5-run |
| Evidence Doctor | `make -C test-rvv/recognition/hough_3d evidence_doctor_phase030_default_distance_weight` | 暴露 Errors / Warnings / Suggestions | 结果写入 result |
| registry | `make -C test-rvv/recognition/hough_3d record_evidence_state_phase030_default_distance_weight && make -C test-rvv/recognition/hough_3d check_evidence_freshness` | 证据登记 fresh | exit 0 |

## Evidence Doctor 和 Registry 规则

- summary：`test-rvv/recognition/hough_3d/log/board/repeated_phase030_default_distance_weight/summary.md`
- manifest：`test-rvv/recognition/hough_3d/log/board/repeated_phase030_default_distance_weight/evidence_manifest.json`
- doctor：`test-rvv/recognition/hough_3d/log/board/repeated_phase030_default_distance_weight/evidence_doctor.md`
- registry：`test-rvv/recognition/hough_3d/log/evidence_registry.json`

如果 Evidence Doctor 出现 Error，不能采纳 production patch；如果只有 Warning / Suggestion，
必须结合 speedup、退化频率和实现复杂度决定是采纳、继续下一 phase 还是暂停。

## 板卡复跑预算和决策桶

- 本阶段预算：5-run repeated board。
- `positive`：median >= `1.20x` 且 `B/A < 1` 为 `0/5`。
- `weak_positive`：`1.05x <= median < 1.20x` 且 `B/A < 1` 为 `0/5`；只有实现简单、fallback 清楚、Evidence Doctor 无 Error 时才可考虑采纳。
- `neutral`：`0.95x <= median < 1.05x`，默认不采纳。
- `negative`：median < `0.95x`，不采纳。
- `unstable`：方向摇摆或 Evidence Doctor 指出无法解释的异常，降级为需要人工判断。

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | production_direct |
| A/B boundary | public `houghVoting()` production boundary |
| 当前决策问题 | 默认配置下是否采纳当前 production patch |
| diagnostic 是否可外推到 production | 本阶段直接测 production，不依赖 diagnostic 外推 |
| comparison-boundary / baseline mismatch 风险 | 低；Std/RVV 都用同一 bench wrapper 和默认 distance weight 配置 |
| weak / neutral 时是否允许继续 probe | weak positive 可进入采纳审计；neutral / negative 默认停止 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前没有已 adopted RVV family；Std/RVV production direct 足以回答是否保留当前唯一 RVV patch |

## 继续 / 停止条件

若 phase 030 production direct 为 positive 或满足弱收益接入条件，则进入 production adoption
closeout，创建 `doc-rvv/recognition/hough_3d-RVV.zh.md` 并使用 phase 030 板卡数据。
若结果仍为 `neutral` / `negative` / `unstable`，或 Evidence Doctor Error 未能解释，
则不采纳当前 production patch，当前 topic 暂停。

## 文档更新清单

- 更新本 phase result、phase index、optimization matrix、optimization roadmap。
- 更新 README、benchmark/evidence、optimization evidence 和 evaluation。
- 更新 current Handoff。
- 只有采纳时才创建 production 长期 `doc-rvv`。
