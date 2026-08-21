# Phase 040 结果：doc-suite parity

## 执行范围

本阶段只新增和同步 topic-local 文档，没有修改 production 源码、test-rvv C++ 源码或板卡脚本行为。

## Doc-suite 审计表

| area | current shape scan | quality bar | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| README navigation | 缺少 topic 根 README | 需要入口导航、命令、证据白名单和 production doc 适用性 | adopted | 新增 `README.zh.md` | 无 |
| testing overview | evaluation 只列摘要 | 需要 target 粒度审计和覆盖矩阵 | adopted | 新增 `doc/testing-overview.zh.md` | 无 |
| correctness tests | TEST 细节只在源码中 | 需要 TEST 字典和证明边界 | adopted | 新增 `doc/correctness-tests.zh.md` | 无 |
| benchmark and evidence | summary / doctor 存在但缺少 case 字典 | 需要 bench label、Evidence Doctor、asm 和提交边界 | adopted | 新增 `doc/benchmark-and-evidence.zh.md` | 无 |
| optimization evidence | matrix 有状态，缺少候选证据索引 | 需要 candidate -> code/test/bench/doctor 映射 | adopted | 新增 `doc/optimization-evidence.zh.md` | 无 |
| test-support code map | evaluation Traceability Map 不够细 | 需要 test-only helper、bench、script、output 代码地图 | adopted | 新增 `doc/test-support-code-map.zh.md` | 若 PI2 后 helper 增长，再拆 `include/impl/` |
| production topic doc | 无 `doc-rvv/io/image_depth-RVV.zh.md` | 只有 adopted production behavior 或 PI5 用户确认后适用 | not_applicable with evidence | 当前未改 production，只有 diagnostic evidence | PI5 用户确认采纳后创建 |
| artifact tracking | 新增文档均在 topic 路径下 | README / evaluation / phase result 引用的文件必须可由路径限定 status 发现 | adopted | final verification 使用 `git status --short --untracked-files=all -- test-rvv/io/image_depth` | 无 |

## 文档同步

新增文档：

- `README.zh.md`
- `doc/testing-overview.zh.md`
- `doc/correctness-tests.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc/test-support-code-map.zh.md`

更新文档：

- `doc/phases/README.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/image_depth-evaluation.zh.md`

## Continue / Stop Decision

`continue_stop_decision = turn_stop_deferred with stop_condition_hit`。

doc-suite parity 已闭合当前授权范围内的结构文档缺口。剩余默认下一动作仍是 PI2 production patch，会修改 `io/src/image_depth.cpp`，需要用户明确授权 production integration loop。当前不把 topic 标成 complete，因为没有 production direct、PI5 用户确认或 adopted production behavior。
