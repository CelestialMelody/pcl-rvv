# sac_model_line 测试总览

## 运行入口分类

| target 类别 | 当前 target | 证明范围 | 不证明范围 |
| --- | --- | --- | --- |
| correctness aggregate（正确性汇总入口） | `run_test_compare` | Std/RVV 两个构建下 count/select/getDistances candidates 与接入后 public entry 对拍。 | 其它点型和更宽 row source。 |
| correctness aliases（正确性细分入口） | `run_line_count_tests`、`run_line_select_tests`、`run_line_get_distances_tests`、`run_line_get_distances_vfsqrt_tests`、对应 `run_board_line_*` target | 分别只跑 line count、select、getDistances scalar-sqrt 或 getDistances vfsqrt 诊断 gtest。 | 接入后 production public 三入口的完整 repeated performance；当前证据由 `collect_repeated_board_production_select_vse64_evidence` 覆盖。 |
| bench diagnostic aliases（bench 诊断入口） | `board_smoke` | 单次 board test + public/candidate bench。 | repeated 稳定性。 |
| QEMU smoke aliases（QEMU 小型验证入口） | `run_test_compare`、`dump_bench_rvv` | correctness、构建和 asm。 | QEMU timing 不写入性能结论。 |
| board smoke aliases（板卡小型验证入口） | `board_smoke` | 单次可运行性和日志形状。 | 不单独支撑 EvidenceDecision。 |
| board repeated aliases（板卡重复采集入口） | `collect_repeated_board_evidence`、`collect_repeated_board_select_evidence`、`collect_repeated_board_get_distances_evidence`、`collect_repeated_board_get_distances_vfsqrt_evidence`、`collect_repeated_board_production_evidence`、`collect_repeated_board_production_vse64_evidence`、`collect_repeated_board_production_select_vse64_evidence` | 5-run board repeated performance；production target 覆盖接入后公开入口，Phase 060 target 是当前 production truth。 | 非覆盖点型和其它 row source。 |
| doctor / registry aliases（证据体检和登记入口） | `generate_*_board_evidence_manifest`、`run_repeated_board_*_evidence_doctor`、`record_repeated_board_*_evidence_state`、`repeated_*_evidence_status` | manifest、doctor 和 freshness check。 | raw log 脱敏和提交授权。 |
| historical probe guarded aliases（历史探针保护入口） | `collect_identity_repeated_board_evidence`、`collect_identity_shuffled_repeated_board_evidence`、`generate_identity_*_board_evidence_manifest`、`record_identity*_board_evidence_state` | Phase 070 identity-index strided load 是已拒绝的 production-detail probe；manifest / doctor 重生成默认受 `ALLOW_PHASE070_IDENTITY_REFRESH=1` 保护，避免误把历史候选当作当前 production truth。 | 只用于复核 rejected family selection evidence（已拒绝的实现族选择证据），不作为当前性能表。 |

## 覆盖矩阵

| 入口 | row source policy（行来源策略） | point type / layout | correctness | bench | board | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `countWithinDistanceCandidate` | direct indexed `indices_` | `PointXYZ` / float xyz AoS | pass | pass | 5-run positive-stable | 0/0/0 | historical diagnostic, closed by Phase 040/050 production adoption |
| `countWithinDistance` public | direct indexed `indices_` | `PointXYZ` / float xyz AoS | pass as production public entry | production direct | 5-run positive-stable | 0/0/0 | adopted production behavior |
| `selectWithinDistanceCandidate` | direct indexed `indices_` | `PointXYZ` / float xyz AoS | pass | pass | 5-run positive-stable | 0/0/0 | historical diagnostic, closed by Phase 040/050 production adoption |
| `selectWithinDistance` public | direct indexed `indices_` | `PointXYZ` / float xyz AoS | pass as production public entry | production direct | 5-run positive-stable | 0/0/0 | adopted production behavior |
| `getDistancesToModelCandidate` | direct indexed `indices_` | `PointXYZ` / float xyz AoS / dense double output | pass | pass | 5-run negative | Errors=1 / Warnings=0 / Suggestions=0 | rejected for scalar-sqrt diagnostic boundary |
| `getDistancesToModelVFSqrtCandidate` | direct indexed `indices_` | `PointXYZ` / float xyz AoS / dense double output | pass | pass | 5-run positive-stable | 0/0/0 | historical diagnostic, closed by Phase 040/050 production adoption |
| `getDistancesToModel` public | direct indexed `indices_` | `PointXYZ` / float xyz AoS | pass as production public entry | production direct | 5-run positive-stable | 0/0/0 | adopted production behavior |

## Artifact Tracking

新增 topic-local docs 和 summary artifacts 应出现在 path-limited `git status --short --untracked-files=all -- test-rvv/sample_consensus/sac_model_line` 输出中。`log/board/**` 和 `log/qemu/**` 是 ignored raw logs；`log/evidence_registry.json` 也是 ignored metadata，若需提交必须 `git add -f` 精确选择。
