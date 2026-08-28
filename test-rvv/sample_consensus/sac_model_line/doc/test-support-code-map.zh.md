# sac_model_line 测试支撑代码地图

## 当前文件形态

| 文件 | 职责 | 证据角色 | 备注 |
| --- | --- | --- | --- |
| `Makefile` | 交叉编译、QEMU、board、manifest、doctor 和 registry target。 | test harness（测试运行框架）。 | 使用共享 `../../mk/rvv-topic.mk`。 |
| `board.mk` | 板卡侧二进制和输出路径配置。 | board harness。 | 不保存私有地址，实际 SSH 配置来自环境 / `config.mk`。 |
| `src/test_sac_model_line.cpp` | correctness fixture 和 count/select/getDistances gtest。 | QEMU correctness。 | 当前覆盖 production public entry 和 diagnostic candidate 对拍，仍可保持单文件。 |
| `src/bench_sac_model_line.cpp` | bench 输入构造、计时和 count/select/getDistances 输出 label。 | board performance diagnostic / production direct。 | Phase 060 后 public rows 是当前接入后 production direct 数据来源，diagnostic rows 是历史候选对照。 |
| `include/impl/sac_model_line_diagnostic.hpp` | 测试专用 count/select/getDistances candidate、scalar fallback 和 RVV helper。 | production-shaped diagnostic。 | 不进入 production include；production 已有对应 RVV helper。本文件保留为候选对照和后续消融入口。 |
| `script/generate_line_board_evidence_manifest.py` | 解析 repeated board logs，生成 count/select/getDistances manifest。 | summary evidence。 | topic-bound parser，只理解本 topic label。 |

## Layout Audit

| area | current shape scan | decision | evidence |
| --- | --- | --- | --- |
| test/bench source layout | 已使用 `src/`。 | adopted | 符合 `artifact_layout.source_subdir`。 |
| aggregator and internal helpers | 当前只有 `include/impl/sac_model_line_diagnostic.hpp`，没有聚合头。 | rejected with evidence | helper 已增长到 554 行并包含 count/select/getDistances 三个候选族，但它只作为历史 diagnostic / 消融入口保留，不承担 production dispatch。当前 production closeout 已有独立 `doc-rvv`、evaluation 和 role docs，拆分不会改变采纳决策；若进入新的 identity-index 或点型扩展 scope，再按新增候选职责重审是否拆分。 |
| old `test_support/` | not_present。 | not_applicable with evidence | 当前没有旧目录。 |
| script and bench registry | topic-local script 已存在；registry target 已接入。 | adopted | `record_repeated_board_evidence_state`。 |
| target granularity | 已有 correctness、board smoke、repeated、doctor、registry target。 | adopted | `doc/testing-overview.zh.md`。 |
| legacy compatibility | 无旧路径、无 compatibility alias（兼容别名）。 | not_applicable with evidence | 新 topic。 |

## 维护边界

当前 helper 同时包含 scalar fallback 和 count/select/getDistances RVV candidates。production include 已有三条对应 RVV helper；测试 helper 继续作为 line distance diagnostic（直线距离诊断）共享公式和后续消融入口保留。若后续进入 identity-index 或点型扩展 phase，应在新 phase plan 里重新审计是否拆出聚合入口，并把 candidates、assertions 和 bench-facing helper 的职责分清。
