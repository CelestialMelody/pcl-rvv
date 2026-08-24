# Phase 030 结果：topic-local doc suite 结构补齐

本阶段补齐 `test-rvv/io/pcd_io` 的 topic-local doc suite（主题本地文档套件）。本阶段只修改文档，不修改
production（生产源码）、test 源码、bench 源码或证据日志。

## 阶段范围回填

validated_scope：

- 新增 `testing-overview.zh.md`、`correctness-tests.zh.md`、`benchmark-and-evidence.zh.md`、
  `optimization-evidence.zh.md` 和 `test-support-code-map.zh.md`。
- README、evaluation、roadmap、phase index 和 optimization matrix 已同步文档职责和恢复入口。
- target granularity audit（测试 target 粒度审计）已从当前 Makefile、board.mk、test source、bench source、
  script 和 registry 入口抽取事实。

unvalidated_scope：

- PI2 production patch、PI3 production direct tests、PI4 production board repeated evidence。
- production 长期主题文档 `doc-rvv/io/pcd_io-RVV.zh.md`，当前仍不适用。

## 计划动作结果

| action | status | artifact | 结论 |
| --- | --- | --- | --- |
| D1 新增 testing overview | done | `doc/testing-overview.zh.md` | target 分类、覆盖矩阵和 QEMU / board 边界已独立归属。 |
| D2 新增 correctness role | done | `doc/correctness-tests.zh.md` | 4 个 gtest 的输入、断言、证明范围和缺口已独立归属。 |
| D3 新增 benchmark/evidence role | done | `doc/benchmark-and-evidence.zh.md` | bench label、case-filter、summary、doctor、registry 和提交边界已独立归属。 |
| D4 新增 optimization evidence role | done | `doc/optimization-evidence.zh.md` | candidate family、证据路径和 production 前置条件已独立归属。 |
| D5 新增 test support code map | done | `doc/test-support-code-map.zh.md` | helper、src、script、output 和 production 对照边界已独立归属。 |
| D6 同步导航和 phase 状态 | done | README、evaluation、roadmap、phase index、matrix | 默认恢复入口仍是 PI2 授权检查点。 |
| D7 验证 | done | 见“验证” | whitespace、style scan、registry freshness 和 artifact tracking 已检查。 |

## Doc Suite Role Inventory

| role | status | path / section | evidence |
| --- | --- | --- | --- |
| topic_navigation | standalone | `README.zh.md` | 已链接 role docs、常用命令和 production doc 适用性。 |
| testing_overview | standalone | `doc/testing-overview.zh.md` | 新增。 |
| correctness_tests | standalone | `doc/correctness-tests.zh.md` | 新增。 |
| benchmark_and_evidence | standalone | `doc/benchmark-and-evidence.zh.md` | 新增。 |
| optimization_evidence | standalone | `doc/optimization-evidence.zh.md` | 新增。 |
| optimization_roadmap | standalone | `doc/optimization-roadmap.zh.md` | 已同步 Phase 030。 |
| test_support_code_map | standalone | `doc/test-support-code-map.zh.md` | 新增。 |
| phase_index | standalone | `doc/phases/README.zh.md` | 已新增 Phase 030。 |
| evaluation_diagnostic | standalone | `doc/pcd_io-evaluation.zh.md` | 已链接 role docs。 |
| evaluation_production | not_applicable with evidence | `doc/pcd_io-evaluation.zh.md` | production patch 尚未进入 PI2。 |
| production_topic_doc | not_applicable with evidence | `doc-rvv/io/pcd_io-RVV.zh.md` | 无 adopted production behavior。 |

## Structure parity 审计

| area | current shape scan | config / quality bar | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| test/bench source layout | `src/test_pcd_io.cpp`、`src/bench_pcd_io.cpp` | `src/` 结构已满足当前 topic | adopted | `test-support-code-map.zh.md` | PI3 后按 production direct 复杂度复审。 |
| aggregator and internal helpers | `include/pcd_io.h`、`include/impl/pcd_io_support.hpp` | `include/` + `include/impl` | adopted | helper 259 行，低于 soft line limit | PI3 后若 helper 职责增加再拆。 |
| script and bench registry | topic-local manifest script + shared doctor / registry | topic-local parser，shared checker | adopted | `benchmark-and-evidence.zh.md` | PI4 新增 production manifest role。 |
| target granularity | aggregate、writer_payload repeated、doctor / registry 已存在 | doc-suite target audit | adopted with deferred PI3 aliases | `testing-overview.zh.md` | PI3 后补 production direct correctness alias。 |
| topic-local docs | 五个 role 文档已新增 | doc-suite quality bar | adopted | 本文件 | 无。 |
| long-term docs | no `doc-rvv/io/pcd_io-RVV.zh.md` | only after adopted production behavior | not_applicable with evidence | PI2-PI5 未发生 | 用户确认采纳后创建。 |
| legacy compatibility | 无旧 pointer / alias | 默认删除旧入口 | not_applicable with evidence | `test-support-code-map.zh.md` | 无。 |
| evidence freshness | registry 已有 Phase 000 / 010 | freshness check required | adopted | 见“验证” | PI4 新增 production registry record。 |

## Target 粒度审计回填

| target 类别 | decision | evidence | next action |
| --- | --- | --- | --- |
| correctness aggregate | adopted | `run_test_compare` | PI3 后新增 production direct coverage。 |
| correctness aliases | phase_deferred + unblocked | 当前 4 个 TEST 都在 aggregate 中运行 | PI3 production tests 后拆 alias。 |
| bench diagnostic aliases | adopted with later extension | Phase 030 时为 `--case-filter all`、单 case label、`writer_payload`；Phase 040 后当前字典见 `doc/benchmark-and-evidence.zh.md`。 | PI4 新增 production case-filter。 |
| QEMU smoke aliases | adopted | `run_qemu_smoke` | 保持 QEMU timing 不参与性能结论。 |
| board smoke aliases | adopted | shared `run_board_bench_compare` | 只作为 primitive。 |
| board repeated aliases | adopted | `run_board_pcd_io_repeated`、`run_board_pcd_io_writer_payload_repeated` | PI4 新增 production repeated target。 |
| doctor / registry aliases | adopted | Makefile generate / doctor / record target | PI4 新增 production manifest role。 |
| historical probe guarded aliases | not_applicable with evidence | 当前无历史 production probe | 无。 |

## EvidenceDecision

EvidenceDecision 保持：`partial-production-candidate / PI1 complete`。

production_decision 保持：

- 本阶段没有修改 `io/src/pcd_io.cpp`。
- 下一主线动作仍是 PI2 production patch（生产补丁），需要用户明确确认。
- `doc-rvv/io/pcd_io-RVV.zh.md` 仍 not_applicable。

## 验证

| check | command | result |
| --- | --- | --- |
| whitespace | `git diff --check -- test-rvv/io/pcd_io` | pass |
| writing style | `rg` 禁用词扫描 topic Markdown | pass |
| artifact tracking | `git status --short --untracked-files=all -- test-rvv/io/pcd_io` | topic 产物仍位于当前 topic 边界；raw build / log 默认 local-only。 |
| Phase 000 registry | `evidence_registry.py check ... component_ablation_repeat_5 ... --fail-on any` | fresh |
| Phase 010 registry | `evidence_registry.py check ... writer_payload_repeat_5 ... --fail-on any` | fresh |

## Continue / Stop Decision

Phase 030 已关闭 doc-suite structure 缺口。当前仍有两个后续方向：

- mainline：`PI2 production_patch for writer std::ostream 4-byte field payload`。这会修改 production 源码，命中
  `production_patch_requires_user_confirmation`。
- secondary：reader unpack / finite scan 后续 diagnostic phase。该方向不阻塞 writer PI2，但若用户暂缓 production patch，可以继续排 phase。

next_phase_default：`PI2 production_patch for writer std::ostream 4-byte field payload`，等待用户确认。
