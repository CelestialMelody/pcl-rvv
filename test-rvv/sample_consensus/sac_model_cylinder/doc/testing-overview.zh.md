# sac_model_cylinder 测试总览

## 本文职责

本文说明 `sac_model_cylinder` RVV topic（主题）的测试入口、target 粒度、QEMU / board（仿真器 / 板卡）边界和证据白名单。
每个 gtest 的具体语义见 `doc/correctness-tests.zh.md`；bench（性能测试）标签、manifest（证据清单）和
Evidence Doctor（证据体检）见 `doc/benchmark-and-evidence.zh.md`。

## 运行入口分类

| 类别 | 当前入口 | 证明什么 | 不能证明什么 |
| --- | --- | --- | --- |
| correctness aggregate（正确性汇总入口） | `make -C test-rvv/sample_consensus/sac_model_cylinder run_test_compare` | Std/RVV 两个构建各跑 11 个 gtest，覆盖 diagnostic candidate、production public-vs-Standard、dense distance、fallback 和三组代表点型。 | 不证明板卡性能，也不覆盖所有模板点型。 |
| correctness alias（正确性别名） | `make -C test-rvv/sample_consensus/sac_model_cylinder run_cylinder_count_select_tests` | Phase 000 count/select diagnostic 局部回归。 | 不覆盖 production getDistances 或全部 fallback。 |
| production correctness alias | `make -C test-rvv/sample_consensus/sac_model_cylinder run_board_cylinder_production_tests` | 板卡侧 production direct 子集 gtest。 | 不替代 host/QEMU aggregate，也不输出性能。 |
| diagnostic bench | `make -C test-rvv/sample_consensus/sac_model_cylinder dump_bench_rvv` 与 bench binary | 构建 bench、生成 RVV 反汇编和日志形状。 | QEMU timing 不作为性能结论。 |
| QEMU smoke（小型验证） | 窄参数 `BENCH_ARGS='128 2 shuffled'` 可运行性检查 | 输出格式、checksum 行和 wrapper 可运行。 | 不进入 EvidenceDecision 的性能排序。 |
| board smoke | `make -C test-rvv/sample_consensus/sac_model_cylinder board_smoke` | 板卡能运行 correctness / bench 输出。 | 单次 smoke 不等于 repeated performance。 |
| board repeated | `make -C test-rvv/sample_consensus/sac_model_cylinder collect_production_repeated_board_evidence` | 5-run production repeated board 原始输出。 | raw log 默认不提交，需 manifest / doctor 摘要。 |
| doctor / registry | `make record_production_board_evidence_state`、`make production_evidence_status` | 生成 manifest、Evidence Doctor、registry，并检查 evidence freshness（证据新鲜度）。 | Doctor 只检查规则覆盖的数据模式，仍需 reviewer 读源码和文档边界。 |
| historical probe | `collect_repeated_board_evidence`、`record_repeated_board_evidence_state` | Phase 000 production-shaped diagnostic 历史证据。 | 不作为当前 production 性能结论。 |

## 输入和覆盖矩阵

| 维度 | 当前覆盖 | 未覆盖范围 |
| --- | --- | --- |
| public entry（公开入口） | `countWithinDistance`、`selectWithinDistance`、`getDistancesToModel` 真实 production RVV dispatch。 | `optimizeModelCoefficients`、`projectPoints`、`doSamplesVerifyModel` 保持标量。 |
| row source（行来源） | direct indexed `indices_`，bench 默认 shuffled adjacent pairs。 | 其它上游分布、特殊 identity A/B、correspondence 或双索引 row source。 |
| point type / Scalar / layout（点型 / 标量 / 布局） | `PointXYZ + Normal`、`PointXYZI + Normal`、`PointXYZRGB + Normal`、`PointXYZ + PointNormal` 四组代表点型，float xyz/normal AoS，`Eigen::VectorXf` coefficients。 | custom layout、未测复合点型、`Scalar=double`。 |
| correctness | 11 个 gtest 覆盖 count/select/getDistances 对 Standard helper、diagnostic 语义、typed representative correctness、select stale state 和 normal 不覆盖 fallback。 | NaN/Inf、真实超大内存 offset、全部模板实例性能。 |
| performance | 5-run board repeated production direct，四组代表点型的三入口均 positive。 | 更多规模、custom point types、真实 workload profile。 |

## 推荐测试流程

```bash
make -C test-rvv/sample_consensus/sac_model_cylinder run_test_compare
make -C test-rvv/sample_consensus/sac_model_cylinder clean_bench_rvv check_production_asm
make -C test-rvv/sample_consensus/sac_model_cylinder record_production_board_evidence_state
make -C test-rvv/sample_consensus/sac_model_cylinder production_evidence_status
```

QEMU 只用于 correctness 和日志形状。板卡性能使用 `collect_production_repeated_board_evidence` 采集，再用
`record_production_board_evidence_state` 刷新 manifest、Doctor 和 registry。

## 当前可提交证据和默认排除项

可提交候选是 topic-local 源码、文档、manifest、Evidence Doctor 摘要和 `log/evidence_registry.json`。
raw board logs、QEMU logs、`build/` 二进制、`script/__pycache__/` 和本机 `config.mk` 默认排除。

## 当前结论边界

当前生产采纳覆盖 direct indexed `indices_`、四组代表点型、float xyz/normal AoS 布局和 65536 点
shuffled adjacent pairs 板卡证据。自定义点型全集、`Scalar=double`、identity-index 专门路径和上游真实分布
需要新 phase 或 profile 触发后独立批准。
