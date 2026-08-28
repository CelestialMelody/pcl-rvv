# Phase 030: line getDistancesToModel vfsqrt diagnostic result

## 执行范围

本阶段按 `plan.zh.md` 验证 `getDistances-vfsqrt-store` 候选。范围保持在 test-rvv（测试资产）和 topic-local docs 内，没有修改 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_line.hpp`。已验证边界是 `PointXYZ + direct indexed indices_ + float xyz AoS + dense std::vector<double>` 输出。

## 计划动作回填

| action | status | command / evidence | result |
| --- | --- | --- | --- |
| RED test | done | `make -C test-rvv/sample_consensus/sac_model_line run_line_get_distances_vfsqrt_tests` | 实现前因缺少 `getDistancesToModelVFSqrtCandidate` 失败。 |
| GREEN helper | done | `make -C test-rvv/sample_consensus/sac_model_line run_line_get_distances_vfsqrt_tests` | QEMU RVV 侧 1 个 vfsqrt gtest 通过。 |
| full correctness | done | `make -C test-rvv/sample_consensus/sac_model_line run_test_compare` | Std/RVV 两个 QEMU 构建各 7 个 gtest 通过。 |
| manifest script | done | `python3 -m py_compile test-rvv/sample_consensus/sac_model_line/script/generate_line_board_evidence_manifest.py` | 脚本语法检查通过。 |
| asm | done | `make -C test-rvv/sample_consensus/sac_model_line dump_bench_rvv` | `getDistancesToModelVFSqrtCandidateRVV` 内出现 `vluxseg3ei32.v`、`vfmacc.vv`、`vfsqrt.v` 和 `vse32.v`。 |
| board repeated | done | `SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_line collect_repeated_board_get_distances_vfsqrt_evidence` | 5-run board gtest 每轮 7/7 通过；candidate checksum 每轮为 `65740`。 |
| doctor / registry | done | `make -C test-rvv/sample_consensus/sac_model_line record_repeated_board_get_distances_vfsqrt_evidence_state`、`make -C test-rvv/sample_consensus/sac_model_line repeated_get_distances_vfsqrt_evidence_status` | Evidence Doctor 为 Errors=0 / Warnings=0 / Suggestions=0；registry check 为 `fresh`。 |

## Board Summary

| case | role | Std avg ms | RVV avg ms | B/A values | median / min / max | decision bucket |
| --- | --- | ---: | ---: | --- | --- | --- |
| public getDistancesToModel | summary_only_unknown | 2.272596 | 2.291348 | not_applicable | about `1.00x` companion | production unchanged |
| diagnostic candidate getDistancesToModel vfsqrt | production_shaped_diagnostic | 2.266758 | 0.653100 | `3.3746x, 3.3924x, 3.5378x, 3.5283x, 3.5208x` | `3.5208x / 3.3746x / 3.5378x` | positive-stable |

Evidence paths:

- `test-rvv/sample_consensus/sac_model_line/doc/phases/030-line-get-distances-vfsqrt-diagnostic/repeated-evidence-manifest.json`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/030-line-get-distances-vfsqrt-diagnostic/repeated-evidence-doctor.md`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/030-line-get-distances-vfsqrt-diagnostic/repeated-evidence-doctor.json`

## Evidence Doctor

`repeated-evidence-doctor.md` 报告 Errors=0 / Warnings=0 / Suggestions=0。Manifest（证据清单）记录 `production_shaped_diagnostic` 证据角色、`test_helper` A/B boundary（对照边界）、`direct_indexed_indices` row source（行来源）、200 次 iteration（迭代）和 5 次 warmup（预热）。脚本仍没有记录 taskset、governor、freq、temperature 和 binary hash；这些 metadata（元数据）缺口不改变本阶段 diagnostic bucket，但 production direct（真实生产路径证据）前应补齐。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production_shaped_diagnostic`。 |
| A/B boundary | `test_helper`；baseline 是 Std build 下的 `getDistancesToModelVFSqrtCandidate` fallback，candidate 是 RVV build 下的 vfsqrt helper。 |
| 当前决策问题 | RVV-vs-scalar 和 implementation-shape。 |
| diagnostic 是否可外推到 production | 只能作为 PI1 输入。production dispatch、fallback、真实公开入口 direct test、production asm 和 board production bench 仍未证明。 |
| comparison-boundary / baseline mismatch 风险 | 有。当前 helper 在测试派生类内，production 入口尚未接入。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段不是弱 / 负 / 中性 / 不稳定；若后续 production direct 证据退化，应停在 PI5 用户检查点。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 需要。当前结果不能写成 adopted production behavior。 |

## 结论和下一步

EvidenceDecision：`partial-production-candidate for getDistances-vfsqrt-store`。Phase 020 的 scalar-sqrt 形状保持 rejected；Phase 030 证明把 sqrt 移入 RVV chunk 后，在同一 diagnostic boundary 下恢复正向收益。

`continue_stop_decision`: `turn_stop_deferred with stop_condition_hit`。继续推进 count、select 和 getDistances 到 production integration loop 需要用户明确授权，因为下一步会冻结生产接入范围并可能修改 production 源码。当前没有创建 `doc-rvv/sample_consensus/sac_model_line-RVV.zh.md`。
