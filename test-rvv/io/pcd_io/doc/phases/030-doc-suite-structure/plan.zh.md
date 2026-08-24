# Phase 030 计划：topic-local doc suite 结构补齐

本阶段补齐 topic-local doc suite（主题本地文档套件）。它不修改 production（生产源码），不修改
test / bench 源码，也不重跑 board（板卡）证据。阶段目标是让 reviewer（审查者）能从稳定文档入口定位
测试、bench、脚本、证据和下一阶段恢复动作。

## 阶段意图和边界

validated_scope：

- `test-rvv/io/pcd_io` 目录内的文档结构。
- `testing_overview`、`correctness_tests`、`benchmark_and_evidence`、`optimization_evidence`、
  `test_support_code_map` 五个 role 文档。
- README、evaluation、roadmap、phase index 和 optimization matrix 的文档导航同步。

unvalidated_scope：

- `io/src/pcd_io.cpp` production patch。
- PI3 production direct tests、PI4 production board repeated evidence。
- test / bench 源码重构。当前 `include/impl`、`src/`、`script/` 结构已可审查，本阶段只记录结构。

phase_closeout_boundary：

- 本阶段只能声明 doc-suite role inventory（文档职责清单）和 target granularity audit（测试 target 粒度审计）已补齐。
- 本阶段不改变 EvidenceDecision，不把 diagnostic evidence（诊断证据）升级为 production evidence（生产证据）。

## 当前状态清单

| area | current shape scan | decision before phase |
| --- | --- | --- |
| topic_navigation | `README.zh.md` 已有入口、命令和 production doc 适用性 | keep / update |
| evaluation | `doc/pcd_io-evaluation.zh.md` 承担函数级评估、Traceability Map 和诊断证据链 | keep / link new roles |
| testing_overview | 缺独立 role 文档；命令散在 README 和 phase result | adopt |
| correctness_tests | 缺独立 role 文档；TEST 语义只在源码注释和 phase result 中出现 | adopt |
| benchmark_and_evidence | 缺独立 role 文档；case-filter、summary、manifest、registry 分散 | adopt |
| optimization_evidence | 缺独立 role 文档；候选状态在 roadmap / matrix 中分散 | adopt |
| test_support_code_map | 缺独立 role 文档；helper、bench wrapper、script 关系只在 evaluation Traceability Map 中概括 | adopt |
| production_topic_doc | `doc-rvv/io/pcd_io-RVV.zh.md` 不适用 | not_applicable with evidence |

## 实现动作

| action | artifact | completion criterion |
| --- | --- | --- |
| D1 新增 testing overview | `doc/testing-overview.zh.md` | target 分类、运行顺序、覆盖矩阵和证据边界可审查。 |
| D2 新增 correctness role | `doc/correctness-tests.zh.md` | 每个 gtest 的输入、断言、证明范围和缺口可定位。 |
| D3 新增 benchmark/evidence role | `doc/benchmark-and-evidence.zh.md` | bench case、case-filter、board target、doctor、registry 和提交边界可定位。 |
| D4 新增 optimization evidence role | `doc/optimization-evidence.zh.md` | candidate 状态、证据路径和 production 接入前置条件可定位。 |
| D5 新增 test support code map | `doc/test-support-code-map.zh.md` | helper、src、script、output 和 production 对照边界可定位。 |
| D6 同步导航和 phase 状态 | README、evaluation、roadmap、phase index、matrix | 默认恢复入口保持 PI2 授权检查点，同时记录 doc-suite phase complete。 |
| D7 验证 | `git diff --check`、artifact tracking、registry check、writing-style scan | 文档无 whitespace error；既有证据 freshness 不变。 |

## Target 粒度审计计划

| target 类别 | 当前入口 | 本阶段处理 |
| --- | --- | --- |
| correctness aggregate | `run_test_compare` | 写入 overview 和 correctness role。 |
| correctness aliases | 无独立 gtest-filter target | 记录为 phase_deferred；PI3 production direct 后再补。 |
| bench diagnostic aliases | `--case-filter all`、`writer_payload`；单 case label 也可过滤 | 写入 benchmark/evidence。 |
| QEMU smoke aliases | `run_qemu_smoke` | 写明 QEMU timing 不作性能证据。 |
| board smoke aliases | shared `run_board_bench_compare` | 写明单次 target 是 smoke，不替代 repeated。 |
| board repeated aliases | `run_board_pcd_io_repeated`、`run_board_pcd_io_writer_payload_repeated` | 写入 benchmark/evidence 和 optimization evidence。 |
| doctor / registry aliases | `run_board_pcd_io_*_evidence_doctor`、`record_board_pcd_io_*_evidence_state` | 写入 benchmark/evidence。 |
| historical probe guarded aliases | 无 | `not_applicable with evidence`。 |

## Evidence Doctor 和 registry 规则

本阶段不新增性能数据。Phase 000 和 Phase 010 的 summary / manifest / doctor 仍是当前性能证据。阶段结束前只做 freshness check：

- `component_ablation_repeat_5/{summary.md,evidence_manifest.json,evidence_doctor.md}`
- `writer_payload_repeat_5/{summary.md,evidence_manifest.json,evidence_doctor.md}`

## 继续 / 停止条件

- 若文档套件补齐成功，默认恢复入口仍是 `PI2 production_patch for writer std::ostream 4-byte field payload`，等待用户确认。
- 若发现 doc-suite 文件引用不存在或 artifact tracking 不清，先修文档，不进入 PI2。
- 若继续需要修改 production，停在 `production_patch_requires_user_confirmation`。

next_phase_default：`PI2 production_patch for writer std::ostream 4-byte field payload`，等待用户确认。
