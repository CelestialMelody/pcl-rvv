# color_gradient_dot_modality 测试总览

本文件做什么：
这里只回答“有哪些测试入口、每个入口证明什么、不能证明什么”，不承担逐条 correctness 细节，也不写
bench 统计。阅读顺序是 `README.zh.md` -> 本文件 -> `correctness-tests.zh.md` ->
`benchmark-and-evidence.zh.md` -> `color_gradient_dot_modality-evaluation.zh.md`。

## 入口分类

| 类别 | 当前入口 | 证明什么 | 不能证明什么 |
| --- | --- | --- | --- |
| correctness aggregate | `make -C test-rvv/recognition/color_gradient_dot_modality run_test_compare` | Std/RVV 两侧 gtest 通过，生产入口与标量参考链路对拍。 | 性能、板卡收益、长期 production 形态。 |
| correctness aliases | 当前 topic 没有额外拆分的 topic-local correctness alias。 | 共享测试目标仍由 `run_test_compare` 汇总。 | 不能把 aggregate 当成多个独立公共 correctness 入口。 |
| QEMU smoke | `run_qemu_smoke`（当前别名仍指向 `run_test_compare`） | 构建、路径和日志形状正确。 | 不能证明板卡性能。 |
| bench diagnostic | `run_bench_rvv` + `CGDM_REPEATED_BENCH_ARGS` | 运行 `bench_cgdm` 的 case-filter，产出 helper / production shape 的 timing 与 checksum。 | 不能单独代表 board repeated 结论。 |
| board smoke | 当前 topic 依赖共享 board 运行框架；topic-local 只保留 repeated 入口。 | 单次板卡运行可用于可运行性和日志收集。 | 不能替代 repeated evidence。 |
| board repeated | `board_repeated` | 5-run repeated board summary、manifest 和 Evidence Doctor。 | 不能退化成单次 smoke。 |
| doctor / registry | `evidence_manifest_repeated`、`evidence_doctor_repeated`、`record_evidence_state_repeated`、`check_evidence_freshness` | 将 summary / manifest / doctor / registry 绑定到当前文档。 | 不能跳过文档引用或 freshness 检查。 |
| historical probe | 不适用。 | 当前 topic 已进入 adopted production closeout。 | 不能继续把旧 diagnostic 当新 probe。 |

## Target 粒度审计

| target 类别 | 当前 shape scan | 决策 | 说明 |
| --- | --- | --- | --- |
| correctness aggregate | `run_test_compare` | adopted | 这是当前最主要的 correctness 入口。 |
| correctness aliases | 只有汇总入口，没有额外 topic-local alias | not_applicable with evidence | 测试族已经足够小，不必强行拆成更多公共入口。 |
| bench diagnostic aliases | `run_bench_rvv` + case-filter | adopted | case 名能区分 dominant-map 与 production entry。 |
| QEMU smoke aliases | `run_qemu_smoke` | adopted | 只证明 correctness / log-shape。 |
| board smoke aliases | 共享 board smoke 框架 + topic-local repeated 入口 | adopted | 单次板卡入口用于收集 raw log，性能结论仍以 repeated summary 为准。 |
| board repeated aliases | `board_repeated` | adopted | 当前 production direct 的主要板卡入口。 |
| doctor / registry aliases | `evidence_manifest_repeated`、`evidence_doctor_repeated`、`record_evidence_state_repeated`、`check_evidence_freshness` | adopted | 这组入口支撑 phase 010 证据归档。 |
| historical probe guarded aliases | 无 | not_applicable with evidence | 当前 topic 已收口，不保留旧探针入口。 |

## 当前可引用证据与提交边界

- 正确性摘要：`test-rvv/recognition/color_gradient_dot_modality/log/`
- 生产 direct 摘要：`test-rvv/recognition/color_gradient_dot_modality/log/board/repeated_phase010_production_direct/summary.md`
- Evidence Doctor：同目录下 `evidence_doctor.md` / `evidence_doctor.json`
- registry：`test-rvv/recognition/color_gradient_dot_modality/log/evidence_registry.json`

默认 `topic-only` 提交不强行加入 `log/` 下的 evidence files（证据文件）。如果用户要求单独提交 evidence
summary（证据摘要），只应 `git add -f` 精确加入 summary / manifest / doctor / registry，不加入 raw board
logs、编译中间产物或本机临时调试输出。
