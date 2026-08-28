# Phase 010: structure parity doc suite 计划

## 阶段目标和授权边界

本阶段只整理 `test-rvv/sample_consensus/sac_model_sphere/` 下的 topic-local doc suite（主题本地文档套件），让下一轮 worker / reviewer 不依赖聊天上下文即可恢复 Phase 000 的证据、命令、候选取舍和下一阶段入口。本阶段不修改 production（生产源码）、不新增 RVV candidate（候选实现）、不重跑板卡、不创建 `doc-rvv/sample_consensus/sac_model_sphere-RVV.zh.md`。

## 当前 evidence baseline

| 输入 | 路径 / 命令 | 当前状态 |
| --- | --- | --- |
| Phase 000 result | `doc/phases/000-sphere-select-distance-diagnostic/result.zh.md` | `selectWithinDistance` 测试专用候选为 `partial-production-candidate`；当前 `getDistancesToModel` 候选为 negative。 |
| optimization matrix | `doc/phases/optimization-matrix.zh.md` | 已区分 count adopted、select partial、getDistances rejected、doc suite phase deferred。 |
| roadmap | `doc/optimization-roadmap.zh.md` | 默认恢复动作为本阶段，之后进入 select PI1 计划。 |
| evaluation | `doc/sac_model_sphere-evaluation.zh.md` | 已有 Traceability Map 和诊断证据链；仍缺独立 role 文档。 |
| Evidence Doctor | `doc/phases/000-sphere-select-distance-diagnostic/repeated-evidence-doctor.md` | 当前 truth；早期 `evidence-doctor.md` 已降级。 |

## 执行动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| 补 topic navigation | `README.zh.md` | 说明当前结论、先读路径、常用命令、可提交证据和默认排除项。 |
| 补 testing overview | `doc/testing-overview.zh.md` | 覆盖 Makefile / board.mk / test / bench / script / evidence 的 target 粒度审计。 |
| 补 correctness tests | `doc/correctness-tests.zh.md` | 列出 3 个 gtest 的输入、断言、证明范围和不能证明范围。 |
| 补 benchmark/evidence | `doc/benchmark-and-evidence.zh.md` | 说明 bench label、计时边界、manifest、Evidence Doctor、asm 和日志提交边界。 |
| 补 optimization evidence | `doc/optimization-evidence.zh.md` | 把 adopted / partial / rejected / deferred candidate 映射到代码、证据和下一步。 |
| 补 test support code map | `doc/test-support-code-map.zh.md` | 给出 production、test helper、bench wrapper、script 和 output 的代码定位。 |
| 回填 phase result / matrix / evaluation | `result.zh.md`、`optimization-matrix.zh.md`、`sac_model_sphere-evaluation.zh.md` | doc suite role inventory 全部 adopted 或 not_applicable。 |

## Target 粒度审计输入

审计只以当前 topic 的真实文件为准：`Makefile`、`board.mk`、`src/test_sac_model_sphere.cpp`、`src/bench_sac_model_sphere.cpp`、`script/generate_sphere_board_evidence_manifest.py`、Phase 000 manifest / doctor 和 board repeated raw log。不得虚构不存在的 target；缺少 target 时写 `phase_deferred + unblocked` 或 `not_applicable with evidence`。

## 暂停条件

若发现需要修改 production、公共 Make 规则、其它 topic、无关 dirty 文件或无法确认当前 topic artifact tracking，本阶段停止并输出 Handoff。否则本阶段应闭合 doc suite，再进入 `020-select-production-integration-plan`。

## 文档发布边界

新增 role 文档属于 topic test asset，review 后可进入 topic 产物边界。raw board logs、`build/`、私有 board 路径和本地配置默认不提交；Phase 000 manifest 和 doctor 是 summary evidence（摘要证据），可作为 review 候选。
