# Phase 030 Result: default-distance-weight-production-direct

## 当前状态

本阶段补测了 `Hough3DGrouping::houghVoting()` 的类默认配置：
`use_interpolation=true`、`use_distance_weight=false`。这个阶段存在的原因是
phase 010 使用了 `use_distance_weight=true`，而源码默认值是 `false`；如果默认配置
下出现稳定正收益，当前 production patch 才可能重新进入采纳审计。

## 执行范围

| action | 状态 | 命令 / 证据 |
| --- | --- | --- |
| 本地正确性 | done | `make -C test-rvv/recognition/hough_3d run_test_compare`，Std / RVV 均为 2/2 pass |
| 板卡 SSH | done | `SSH_AUTH_SOCK=/tmp/ssh-iEjvVUej0dT1/agent.101441 ssh -F /home/zoomin/.ssh/config -o BatchMode=yes -o ConnectTimeout=5 root@192.168.55.2 true` |
| phase 030 repeated board | done | `SSH_AUTH_SOCK=/tmp/ssh-iEjvVUej0dT1/agent.101441 make -C test-rvv/recognition/hough_3d board_phase030_default_distance_weight_repeated` |
| Evidence Doctor | done | `make -C test-rvv/recognition/hough_3d evidence_doctor_phase030_default_distance_weight` |

板卡运行过程中远端 `make` 多次提示 `Clock skew detected`。该提示说明远端文件时间戳
存在偏差，不等同于测试失败；本阶段把它作为环境风险保留在 Handoff，性能结论仍以
5-run summary 和 Evidence Doctor 为准。

## 结论

默认 distance weight 配置下，production direct 5-run median 为 `1.005x`，
values 为 `1.002x, 1.008x, 1.005x, 0.997x, 1.006x`，decision bucket 为
`neutral`。Evidence Doctor 报 `0 Error / 1 Warning / 1 Suggestion`：
`1/5` 低于 `1.0`，且 median 距离阈值不足 `0.05`。

这说明 phase 010 的非默认 distance weight 配置没有误伤一个明显正向的默认配置。
当前 production patch 仍不采纳，也不创建 `doc-rvv` 长期生产文档。

## 已登记证据

- `test-rvv/recognition/hough_3d/log/board/repeated_phase030_default_distance_weight/summary.md`
- `test-rvv/recognition/hough_3d/log/board/repeated_phase030_default_distance_weight/evidence_manifest.json`
- `test-rvv/recognition/hough_3d/log/board/repeated_phase030_default_distance_weight/evidence_doctor.md`
- `test-rvv/recognition/hough_3d/log/board/repeated_phase030_default_distance_weight/evidence_doctor.json`

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | production_direct |
| A/B boundary | public `houghVoting()` production boundary |
| 当前决策问题 | 默认配置下是否采纳当前 production patch |
| diagnostic 是否可外推到 production | 本阶段直接测 production，不依赖 diagnostic 外推 |
| comparison-boundary / baseline mismatch 风险 | 低；Std/RVV 都用同一 bench wrapper、同一输入和 `use_distance_weight=false` |
| weak / neutral 时是否允许继续 probe | 当前为 `neutral`，不继续把该 patch 推进到采纳 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前没有已 adopted RVV family；本阶段仍未达到采纳门槛 |

## 阶段反思

phase 020 已经证明 no-interpolation 边界只有 `1.002x` median，phase 030 又证明
默认 distance weight 边界只有 `1.005x` median。把两个配置同时关闭形成的
`no-interpolation + no-distance-weight` 组合只会覆盖更窄的非默认入口，而且不能解决
当前核心问题：`voteInt()` scatter / voter tracking 仍吞掉 vote generation 的局部收益。
因此该组合不再作为新的 high-priority phase 继续。

## Continue / Stop Decision

`continue_stop_decision`：停止当前 topic 的现有 production patch 推进。

`stop_condition_hit`：roadmap 和 optimization matrix 中已无当前授权范围内、风险可控且高价值的
unblocked RVV 候选。继续做 `voteInt()` / voter tracking RVV 需要复杂状态写回，
而现有 full production direct 与两个消融边界均为 `neutral`。

`next_phase_default`：等待用户指定新的候选方向；默认不继续现有 `houghVoting()` production patch。
