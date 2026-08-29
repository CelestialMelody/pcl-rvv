# Phase 030 Result: response-buffer-reuse

## 阶段结论

Phase 030 已完成。`response-buffer-reuse` 与 Phase 020 的 production direct-window RVV path 一起采纳。
当前 production-public（真实公开入口）板卡 5-run 结果为 positive：
`dotmod_production_detecttemplates_total` median speedup `3.298x`，range
`3.276x - 3.304x`，`B/A < 1 = 0/5`，checksum 一致。

本阶段证据支持把 `recognition/src/dotmod.cpp` 的 production patch 视为 adopted production behavior
（已采纳生产行为）。用户本轮 prompt 已覆盖默认 PI5 人工暂停规则：接入后板卡显示收益即可采纳。

## 计划动作回填

| action | status | evidence |
| --- | --- | --- |
| production patch | done | `recognition/src/dotmod.cpp` 将 `responses` vector 移到 row/col 窗口循环外，窗口内用 `std::fill` 清零 |
| correctness | done | `make -C test-rvv/recognition/dotmod_template_matching run_production_direct_test_compare` 通过 Std/RVV 两侧各 2 个 gtest |
| asm | done | `make -C test-rvv/recognition/dotmod_template_matching dump_production_direct_bench_rvv` 和 `record_production_direct_evidence_state` 均确认 `dotmodScoreWindowDirectRVV`、`vle8`、`vand`、`vmsne`、`vcpop` |
| board repeated | done | `SSH_AUTH_SOCK=/run/user/1001/keyring/ssh make -C test-rvv/recognition/dotmod_template_matching collect_production_direct_repeated_board BENCH_ARGS='256 192 24 16 100 5 8 2 0.9 1'` |
| Evidence Doctor / registry | done | `make -C test-rvv/recognition/dotmod_template_matching record_production_direct_evidence_state BENCH_ARGS='256 192 24 16 100 5 8 2 0.9 1'` |

## 证据摘要

| case | role | median | min | max | B/A < 1 | decision |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| `dotmod_production_detecttemplates_total` | production-public | `3.298x` | `3.276x` | `3.304x` | `0/5` | positive |

证据路径：

- Summary：`test-rvv/recognition/dotmod_template_matching/log/board/repeated_phase020_production_direct/summary.md`
- Manifest：`test-rvv/recognition/dotmod_template_matching/log/board/repeated_phase020_production_direct/evidence_manifest.json`
- Evidence Doctor：`test-rvv/recognition/dotmod_template_matching/log/board/repeated_phase020_production_direct/evidence_doctor.md`
- Registry：`test-rvv/recognition/dotmod_template_matching/log/evidence_registry.json`

## Evidence Doctor

`Errors=0`，`Warnings=0`，`Suggestions=2`。

- `environment_metadata_missing`：summary 缺少 taskset、governor、freq、temperature。当前 5-run 全部正向且没有长尾反转，该 suggestion 不阻塞采纳；后续若要做跨设备或异常复核，应补环境字段。
- `binary_identity_missing`：summary 缺少 binary hash。当前 registry 记录了 summary、manifest 和 doctor 的 sha256；若后续出现方向反转，应补二进制身份后重跑。

## Diagnostic To Production Mismatch Audit

| question | result |
| --- | --- |
| evidence role | `production-public` |
| A/B boundary | real public `DOTMOD::detectTemplates()` compiled from current production source |
| 当前决策问题 | `RVV-vs-scalar` and `implementation-shape` |
| diagnostic 是否可外推到 production | 本阶段不依赖诊断外推；结论来自 production direct board evidence |
| comparison-boundary / baseline mismatch 风险 | Std/RVV 两侧使用同一 test helper、同一输入构造和同一 production source，只改变 RVV 编译宏；Phase 020 pre-Phase030 数字只作历史对照 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | not applicable |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前没有既有 DOTMOD RVV family；`response-buffer-reuse` 是同一 production patch 的低风险实现形态小改，最终采纳以 post-patch production-public positive 为准 |

## Doc Suite Parity Audit

| area | current shape scan | quality bar / supplemental calibration | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| README navigation | `README.zh.md` 提供阅读顺序、命令、当前证据和提交边界 | `doc-suite-quality-bar.zh.md` 要求入口导航和证据白名单 | adopted | 本阶段已刷新 README | none |
| testing overview | 测试入口较少，已在 README、evaluation 和 Makefile 注释中说明 | 复杂 topic 可拆独立 role；当前 2 个 diagnostic test、1 个 production direct test 和对应 bench 可合并说明 | adopted as merged | `doc/dotmod_template_matching-evaluation.zh.md#测试与证据计划` | none |
| correctness tests | diagnostic 4 个 gtest、production direct 2 个 gtest | 需要说明输入、路径和断言 | adopted as merged | evaluation 和测试源码注释覆盖 | none |
| benchmark and evidence | Makefile 提供 repeated board、doctor、registry target；summary/manifest/doctor 已生成 | 需要 case-filter、计时边界、QEMU 边界和 registry | adopted as merged | evaluation、README、summary、registry | none |
| optimization evidence | phase result 和 matrix 记录 adopted/deferred candidate | 需要候选到证据的映射 | adopted | `doc/phases/optimization-matrix.zh.md` | none |
| test support code map | `include/`、`include/impl/`、`src/`、`script/` 已按配置布局 | 需要定位 helper、bench、script 和 production 入口 | adopted as merged | evaluation Traceability Map | none |
| production topic doc | `doc-rvv/recognition/dotmod_template_matching-RVV.zh.md` 适用 | 生产已采纳时必须写长期 production 行为 | adopted | 长期文档已创建 | none |
| phase suite | Phase 000/010/020/030 均有 plan/result，matrix 已刷新 | phase loop 要可恢复 | adopted | `doc/phases/README.zh.md` | none |
| artifact tracking | topic 目录和长期文档为 untracked/to-be-staged 候选；raw logs 默认 excluded | 需要路径限定扫描和提交边界说明 | adopted | Handoff 和最终 verification 记录扫描 | none |

## Continue / Stop Decision

`continue_stop_decision`: stop_for_review / ready_for_review。

`stop_condition_hit`: current matrix and roadmap have no high-priority unblocked action inside current topic boundary。

继续尝试 `threshold-output-rvv`、真实 workload profile、standalone `QuantizedMap::getSubMap()` 或 DOTMOD
modality preprocessing 都会扩大到新输入边界或其它 topic。当前不建议在本轮继续增加微优化。
