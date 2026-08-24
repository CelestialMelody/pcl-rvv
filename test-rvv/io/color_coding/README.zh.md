# color_coding RVV topic

本 topic 评估 `io/include/pcl/compression/color_coding.h` 的 color component（颜色组件）是否值得 RVV 化。

阅读顺序：

1. `doc/color_coding-evaluation.zh.md`
2. `doc/testing-overview.zh.md`
3. `doc/correctness-tests.zh.md`
4. `doc/benchmark-and-evidence.zh.md`
5. `doc/optimization-evidence.zh.md`
6. `doc/test-support-code-map.zh.md`
7. `doc/phases/README.zh.md`
8. `doc/optimization-roadmap.zh.md`

文档分工：

| role | path | 主要职责 |
| --- | --- | --- |
| topic navigation | `README.zh.md` | 当前结论、阅读顺序、常用命令和提交边界。 |
| evaluation diagnostic | `doc/color_coding-evaluation.zh.md` | 函数级评估、诊断证据链和 production 接入判断。 |
| testing overview | `doc/testing-overview.zh.md` | target 粒度审计、运行入口和覆盖矩阵。 |
| correctness tests | `doc/correctness-tests.zh.md` | gtest 字典和 helper-level 证明边界。 |
| benchmark and evidence | `doc/benchmark-and-evidence.zh.md` | bench labels、board repeated、Evidence Doctor 和 registry。 |
| optimization evidence | `doc/optimization-evidence.zh.md` | candidate family 到证据和 decision 的映射。 |
| test support code map | `doc/test-support-code-map.zh.md` | 聚合头、internal helper、test / bench / script / output 定位。 |
| phase suite | `doc/phases/README.zh.md` | phase plan / result / matrix 的恢复入口。 |
| roadmap | `doc/optimization-roadmap.zh.md` | 搜索空间和下一阶段恢复队列。 |

常用命令：

```bash
make -C test-rvv/io/color_coding run_test_compare
make -C test-rvv/io/color_coding run_bench_rvv
make -C test-rvv/io/color_coding run_board_color_coding_production_repeated
make -C test-rvv/io/color_coding dump_bench_rvv
make -C test-rvv/io/color_coding check_evidence_freshness
```

QEMU（仿真器）只用于 correctness（正确性）、构建和日志形状。性能结论必须来自板卡或目标硬件。

当前生产板卡证据是 `log/board/production_repeat_5/summary.md` 和 `log/board/production_repeat_5/evidence_doctor.md`。phase 050 的 diagnostic summary 仍保留在 `log/board/component_repeat_5/`，但不能替代 phase 070 production direct evidence。

当前可作为提交候选的摘要证据：

- `log/board/component_repeat_5/summary.md`
- `log/board/component_repeat_5/evidence_manifest.json`
- `log/board/component_repeat_5/evidence_doctor.md`
- `log/board/production_repeat_5/summary.md`
- `log/board/production_repeat_5/evidence_manifest.json`
- `log/board/production_repeat_5/evidence_doctor.md`
- `log/evidence_registry.json`

默认不提交 `build/`、`log/board/component_repeat_5/run*/` raw logs（原始日志）、本机 `config.mk`、私有板卡地址或临时编译输出。

production 长期主题文档当前不适用：phase 080 已完整回滚 production RVV 分流，当前没有 adopted production behavior（已采用生产行为）。

当前决策：`no production RVV adopted`。Phase 080 已完整回滚 `color_coding.h` 的 production RVV 分流；Phase 070 的 default-only evidence（median `1.0035x`、min `0.9945x`、Evidence Doctor `Errors=1`）作为不采纳依据。当前没有建议继续推进的 RVV candidate。
