# sac_model_circle 测试入口总览

## 本文职责

本文只说明 test（测试）、bench（性能测试）、board（板卡）和 evidence target（证据目标）的分类与边界。每个 gtest 的输入和断言见 `doc/correctness-tests.zh.md`，候选取舍见 `doc/optimization-evidence.zh.md`。

## 运行入口分类

| 类别 | 入口 | 证明范围 |
| --- | --- | --- |
| correctness aggregate（正确性汇总入口） | `make -C test-rvv/sample_consensus/sac_model_circle run_test_compare` | Std / RVV 两个 binary 的 gtest 全量通过；QEMU 不证明性能。 |
| correctness alias（正确性别名） | `run_circle_public_tests` | RVV binary 下 select/count 的 public / Standard / RVV 对拍与基础 fallback 语义。 |
| select error-tail candidate correctness | `run_circle_select_error_tail_candidate_test` | Phase 080 test-only full-RVV error tail 候选与 public/direct RVV 输出一致。 |
| candidate correctness（候选正确性） | `run_circle_getdistances_candidate_test` | `getDistancesToModelCandidate` 与 public `getDistancesToModel` 数值一致。 |
| full-RVV candidate correctness | `run_circle_getdistances_full_rvv_candidate_test` | Phase 050 `vfsqrt + vfwcvt + vse64` 测试候选与 public companion 数值一致。 |
| production getDistances correctness | `run_circle_getdistances_production_test` | Phase 060 接入后 public、Standard helper 和 direct RVV helper 在 `1e-6` 误差预算内一致，Phase 070 已采纳。 |
| board smoke（板卡小型验证） | `run_board_circle_public_tests`、`run_board_circle_select_error_tail_candidate_test`、`run_board_circle_getdistances_candidate_test`、`run_board_circle_getdistances_full_rvv_candidate_test`、`run_board_circle_getdistances_production_test` | 板卡上对应 gtest 可运行并通过；单次 smoke 不等于性能结论。 |
| board repeated（重复板卡证据） | `collect_production_repeated_board_evidence`、`collect_getdistances_repeated_board_evidence`、`collect_getdistances_full_rvv_repeated_board_evidence`、`collect_getdistances_production_repeated_board_evidence`、`collect_select_error_tail_repeated_board_evidence`、`collect_select_error_tail_production_repeated_board_evidence` | 生成 5-run board input，供 manifest 和 Evidence Doctor 使用。 |
| doctor / registry（证据体检 / 登记） | `production_evidence_status`、`getdistances_evidence_status`、`getdistances_full_rvv_evidence_status`、`getdistances_production_evidence_status`、`select_error_tail_evidence_status`、`select_error_tail_production_evidence_status` | 检查 summary evidence 的异常信号和登记新鲜度。 |
| asm attribution（反汇编归属） | `dump_bench_rvv`、`check_getdistances_full_rvv_asm`、`check_getdistances_production_asm`、`check_select_error_tail_asm`、`check_select_production_error_tail_asm` | 生成反汇编并确认 full-RVV / production helper 符号中存在目标 RVV 指令。 |

## Target 粒度审计

| target 类别 | 当前状态 | 证据 / 缺口 |
| --- | --- | --- |
| correctness aggregate | adopted | `run_test_compare` 是当前完整 correctness 入口。 |
| correctness aliases | adopted | public select/count 和 getDistances candidate 均有细分 alias。 |
| bench diagnostic aliases | adopted | bench 同时打印 public select/count/getDistances、旧 diagnostic candidate 和 full-RVV candidate；case-filter 由 manifest mode 区分 production / getDistances / full-RVV / getDistances production。 |
| QEMU smoke aliases | adopted | QEMU 只用于 gtest correctness 和日志形状；不运行完整 bench compare 作为性能证据。 |
| board smoke aliases | adopted | 两个 board gtest alias 分别覆盖 production public tests 和 getDistances candidate。 |
| board repeated aliases | adopted | Phase 000 production direct、Phase 020 production-shaped diagnostic、Phase 050 full-RVV diagnostic、Phase 060 getDistances production direct、Phase 080 select RVV-vs-RVV detail A/B、Phase 090 select production direct 和 Phase 040 historical identity RVV-vs-RVV A/B 均有 collect / summary target；identity replay 受显式历史探针开关保护。 |
| doctor / registry aliases | adopted | production、getDistances 旧候选、full-RVV 候选、getDistances production direct、select error-tail candidate 和 select error-tail production direct 各有 generate / doctor / record / status target。 |
| historical probe guarded aliases | adopted | `check_identity_strided_asm`、identity manifest regeneration（重新生成）和 gather baseline replay 需要 `ALLOW_HISTORICAL_IDENTITY_PROBE=1`，避免常规验证误跑已拒绝候选。 |

## 输入数据总览

gtest 使用 10 点合成圆 shell 数据，覆盖圆上点、threshold shell 内外点、乱序 `indices_`、identity indices（顺序索引）和显式空 indices。bench 使用 65536 点 synthetic direct indexed shell cloud，并对相邻 index 做局部乱序，避免把结果误写成纯连续 stride load（跨步加载）收益。

当前已验证点型为 `PointXYZ` 和 `PointXYZI` correctness；板卡性能只覆盖 `PointXYZ` / float x-y AoS（结构数组）布局。

## 覆盖矩阵

| 路径 | correctness | QEMU | asm | board repeated | Evidence Doctor | production 结论 |
| --- | --- | --- | --- | --- | --- | --- |
| `selectWithinDistance` public RVV | yes | yes | yes | yes | yes | adopted production behavior；当前数据来自 Phase 090 full-RVV error tail 接入后证据 |
| `countWithinDistance` public RVV | yes | yes | yes | yes | yes | adopted production behavior |
| `getDistancesToModel` public RVV | yes | yes | yes | yes | yes | adopted production behavior |
| `getDistancesToModelCandidate` test-only | yes | yes | yes | yes | yes | rejected with diagnostic evidence |
| `getDistancesToModelFullRVV` test-only | yes | yes | yes | yes | yes | production probe entered |
| identity-index strided-load historical candidate | historical replay only | historical replay only | historical replay only | yes | yes | rejected with strict RVV-vs-RVV A/B evidence |

## 当前可提交证据和默认排除项

可提交候选限于 phase result、manifest、Evidence Doctor 摘要、optimization matrix、roadmap、evaluation、README 和 `log/evidence_registry.json`。`build/`、`log/qemu/`、`log/board/` raw output、`script/__pycache__/` 和本机远端路径默认排除。
