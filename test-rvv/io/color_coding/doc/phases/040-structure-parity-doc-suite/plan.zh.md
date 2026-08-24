# Phase 040 Plan: structure parity doc suite

## 阶段意图和边界

本阶段补齐 `test-rvv/io/color_coding` 的 topic-local doc suite（主题本地文档套件），让下一轮 worker / reviewer 不依赖聊天上下文就能恢复测试入口、bench 字典、证据边界、代码地图和当前 `partial-production-candidate`（局部生产候选）结论。

本阶段只允许修改：

- `test-rvv/io/color_coding/README.zh.md`
- `test-rvv/io/color_coding/doc/*.zh.md`
- `test-rvv/io/color_coding/doc/phases/README.zh.md`
- `test-rvv/io/color_coding/doc/phases/optimization-matrix.zh.md`
- `test-rvv/io/color_coding/doc/phases/040-structure-parity-doc-suite/*`
- `tmp/rvv-work-logs/io/color_coding/current-handoff/*`

本阶段不修改 `io/include/pcl/compression/color_coding.h`，不进入 PI2 production patch（生产补丁），不创建 `doc-rvv/io/color_coding-RVV.zh.md`，不刷新 raw board logs（原始板卡日志）。已有 phase 020 board summary / manifest / Evidence Doctor / registry 是当前 evidence truth（证据事实）。

## 当前状态清单

| item | current state |
| --- | --- |
| production source | 未修改；`color_coding.h` 只作为只读边界。 |
| current decision | `partial-production-candidate`：encode average / encode points average pass / default 可进入 PI1；decode 排除。 |
| latest phase evidence | `doc/phases/020-production-shaped-color-coder-precheck/result.zh.md`。 |
| PI1 plan | `doc/phases/030-pi1-encode-default-production-integration-plan/plan.zh.md`，等待生产补丁授权。 |
| board summary | `log/board/component_repeat_5/summary.md`，run label `board-color-coding-component-repeat-phase020`。 |
| Evidence Doctor | `log/board/component_repeat_5/evidence_doctor.md`：Errors=1, Warnings=13, Suggestions=2。 |
| registry | `log/evidence_registry.json` 记录当前 summary / manifest / doctor fresh。 |
| existing docs | README、evaluation、roadmap、phase index、optimization matrix 已存在。 |
| missing standalone roles | testing overview、correctness tests、benchmark/evidence、optimization evidence、test support code map。 |

## doc_suite_role_inventory 计划

| role | planned status | path / section | 本阶段动作 |
| --- | --- | --- | --- |
| topic_navigation | standalone | `README.zh.md` | 更新阅读顺序、role 分工、提交边界和当前停止条件。 |
| testing_overview | standalone | `doc/testing-overview.zh.md` | 新建，覆盖 target 粒度审计、运行流程和覆盖矩阵。 |
| correctness_tests | standalone | `doc/correctness-tests.zh.md` | 新建，逐个 gtest 说明输入、断言和证明边界。 |
| benchmark_and_evidence | standalone | `doc/benchmark-and-evidence.zh.md` | 新建，列 bench labels、case-filter、board repeated、Doctor / registry 和提交边界。 |
| optimization_evidence | standalone | `doc/optimization-evidence.zh.md` | 新建，把 candidate family 映射到源码、target、board、asm、Doctor 和 decision。 |
| optimization_roadmap | standalone | `doc/optimization-roadmap.zh.md` | 更新默认恢复队列，把 doc-suite parity 置为本阶段 adopted。 |
| test_support_code_map | standalone | `doc/test-support-code-map.zh.md` | 新建，定位聚合头、internal helper、test / bench / script / output。 |
| phase_index | standalone | `doc/phases/README.zh.md` | 增加 phase 040，更新默认恢复入口。 |
| evaluation_diagnostic | standalone | `doc/color_coding-evaluation.zh.md` | 补文档归属和 Traceability Map 的 doc role 行。 |
| evaluation_production | not_applicable with evidence | `doc/color_coding-evaluation.zh.md#文档归属` | 无 production patch / PI5 证据，本阶段不适用。 |
| production_topic_doc | not_applicable with evidence | `doc-rvv/io/color_coding-RVV.zh.md` | 没有 adopted production behavior，本阶段不创建。 |

## target granularity audit 计划

| target 类别 | 当前入口 | planned decision |
| --- | --- | --- |
| correctness aggregate | `make -C test-rvv/io/color_coding run_test_compare` | adopted；Std/RVV 各 6 个 gtest。 |
| correctness aliases | 无 topic-local 细分 alias；可用 gtest filter 手动筛选 | not_applicable with evidence for current phase；production direct / fallback 需 PI2 后新增。 |
| diagnostic bench aliases | `run_bench_rvv` / `run_bench_std` + `BENCH_ARGS="--case-filter <label>"` | adopted；label 字典写入 benchmark/evidence。 |
| QEMU smoke aliases | `run_qemu_smoke`、窄 `run_bench_rvv BENCH_ARGS=...` | adopted；只证明构建 / correctness / 日志形状。 |
| board smoke aliases | `run_board_color_coding_smoke` | adopted；只证明可运行和单次输出，不作为 repeated performance。 |
| board repeated aliases | `run_board_color_coding_repeated` | adopted；5-run summary / manifest / Doctor / registry。 |
| doctor / registry aliases | repeated target 内生成；`check_evidence_freshness` 检查 | adopted。 |
| historical probe guarded aliases | 无历史 production probe target | not_applicable with evidence；尚未有 production patch 或回滚探针。 |

## diagnostic-to-production mismatch audit

本阶段不新增性能证据，但会整理 phase 020 诊断证据如何参与 production 取舍。

| question | answer |
| --- | --- |
| evidence role | mixed `component_ablation` + `production_shaped_diagnostic`。 |
| A/B boundary | test helper / production-shaped helper；不是 public overload（公开重载）或 production detail helper（生产细节 helper）。 |
| 当前决策问题 | RVV-vs-scalar diagnostic 支撑 PI1 scope selection（范围选择），不是 clean adoption（干净采纳）。 |
| diagnostic 是否可外推到 production | unknown；可作为 bounded production probe（有界生产探针）的前置筛选，但不能替代 production direct evidence（真实生产入口证据）。 |
| comparison-boundary / baseline mismatch 风险 | yes；component timing 不含 octree traversal、entropy coder、真实 leaf distribution 和 public dispatch。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes for encode/default if PI2 获授权且 scope / fallback / size gate 保守；decode no，除非另做 implementation-shape audit 并消除 Doctor Error。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes；PI5 通过且用户确认采纳前不能写 adopted production behavior。 |

## 执行动作和完成判据

| action | artifact | completion |
| --- | --- | --- |
| 新建 testing overview | `doc/testing-overview.zh.md` | target 粒度审计覆盖 Makefile、board.mk、test / bench、script、registry。 |
| 新建 correctness 字典 | `doc/correctness-tests.zh.md` | 6 个 gtest 都列出输入、断言、证明范围和不覆盖范围。 |
| 新建 benchmark/evidence 字典 | `doc/benchmark-and-evidence.zh.md` | 21 个 bench label 分类，写清 QEMU / board / Doctor / registry / asm 边界。 |
| 新建 optimization evidence | `doc/optimization-evidence.zh.md` | adopted / partial / rejected / deferred candidate 都有证据路径和下一步。 |
| 新建 code map | `doc/test-support-code-map.zh.md` | 聚合头、internal helper、test / bench / script / output 和 production 对照可定位。 |
| 同步入口文档 | README、evaluation、roadmap、phase index、matrix | 默认恢复入口从 phase 030 checkpoint 改为 phase 040 result 后的授权边界。 |
| 写 phase result | `040-.../result.zh.md` | 回填审计表、artifact tracking、验证命令和继续 / 停止决定。 |
| 更新 Handoff | `tmp/.../current-handoff.*` | 显式记录 doc suite adopted，production patch 仍需用户授权。 |

## Evidence Doctor 和 registry 规则

本阶段不生成新的 board evidence，不覆盖 `component_repeat_5` 证据。文档引用的 current truth 仍是：

- `log/board/component_repeat_5/summary.md`
- `log/board/component_repeat_5/evidence_manifest.json`
- `log/board/component_repeat_5/evidence_doctor.md`
- `log/evidence_registry.json`

阶段结束前运行 `make -C test-rvv/io/color_coding check_evidence_freshness`，确认这些引用仍 fresh。

## 验证命令

阶段完成前运行：

```bash
make -C test-rvv/io/color_coding run_test_compare
make -C test-rvv/io/color_coding check_evidence_freshness
git diff --check -- test-rvv/io/color_coding tmp/rvv-work-logs/io/color_coding
git status --short --untracked-files=all -- test-rvv/io/color_coding tmp/rvv-work-logs/io/color_coding io/include/pcl/compression/color_coding.h
```

本阶段只改文档，因此不重跑板卡 repeated，也不重新生成 asm；沿用 phase 020 已登记证据。

## 继续 / 停止条件

若文档套件、phase result、roadmap、matrix 和 Handoff 均同步，且 production patch 仍未获明确授权，则合法停止在 `turn_stop_deferred with stop_condition_hit=production_patch_authorization_required`。下一步默认是等待用户确认 PI2 production patch；若用户不授权生产补丁，可另开 decode implementation-shape audit 或保留为 partial-production-candidate 诊断资产。
