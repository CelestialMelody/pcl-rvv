# 测试体系总览

## 本文职责

本文说明 `transformation_estimation_2D` topic 当前 test-rvv 资产如何分层，以及每类证据能证明什么、不能证明什么。当前 production 源码保留 ordered-cloud-pair 的窄范围 RVV dispatch；Phase 050 的 production-public probe 已完成真实板卡闭环，Phase 030 的三类 row-source test-only 诊断也已完成板卡 repeated，但不进入 production。

## 文档阅读路径

| 问题 | 文档 |
| --- | --- |
| 函数级数据流和生产边界 | `doc/transformation_estimation_2D-evaluation.zh.md` |
| 每个 gtest 覆盖什么 | `doc/correctness-tests.zh.md` |
| bench label、QEMU 和 board 证据边界 | `doc/benchmark-and-evidence.zh.md` |
| 候选族到证据的映射 | `doc/optimization-evidence.zh.md` |
| 测试支撑代码怎么定位 | `doc/test-support-code-map.zh.md` |

## 测试类型定义

| 类型 | 当前状态 | 证明范围 | 不能证明 |
| --- | --- | --- | --- |
| public semantics（公开入口语义） | `run_test_compare` 已覆盖 16 个 gtest | ordered-cloud-pair、source-indexed-cloud-pair、dual-indexed-cloud-pair 和 correspondence-pair 的当前标量边界；size mismatch 和非有限 x/y/z 行为。 | 非法 index / correspondence 的安全行为；production RVV 收益。 |
| numerical consistency（数值一致性） | `run_test_compare` 已覆盖 16 个 gtest | 两遍中心化 fused 2D correlation candidate 与标量参考链路一致；三类 row-source candidate 与 public scalar overload 对拍通过。 | 目标硬件性能；materialize-to-ordered candidate 的生产适用性。 |
| QEMU path / log shape（QEMU 路径 / 日志形状） | diagnostic、production-public 和 row-source smoke 均已运行 | RVV bench binary 可运行，能输出 label、iterations、warmup 和 checksum；manifest / doctor 可解析。 | 真实性能。 |
| asm attribution（反汇编归因） | diagnostic、production-public 和 row-source summary 均已生成 | 能区分 candidate lambda、row-source wrapper、production scalar、Eigen / stdlib、public overload 或 `runPublicCase` 内联边界。 | 指令存在不证明 board 性能。 |
| board performance（板卡性能） | ordered / production-public / row-source 均有 repeated | production-public 为 `4.222x / 5.310x / 4.947x`，Doctor 0/0/0；row-source 为弱收益但 64K 有退化/长尾，Doctor 1/2/6。 | row-source 诊断结果不能替代其它 production dispatch 的证据；QEMU timing 不能替代板卡。 |
| production direct（真实生产路径证据） | narrow candidate retained, user review pending | Phase 050 public boundary、QEMU、asm、board repeated 和 fallback correctness 已闭合。 | 只覆盖 exact `PointXYZ` / `float` / ordered-cloud-pair；其它入口仍标量。 |

## 运行入口分类

| target | 后端 | 主证据角色 | 输出 / 说明 |
| --- | --- | --- | --- |
| `run_test_compare` | QEMU / configured runner | correctness | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log`；当前 Std/RVV 均 16/16。 |
| `run_test_public_semantics` | QEMU / configured runner | public semantics | gtest filter 为 `TransformationEstimation2D.Public*`。 |
| `run_test_candidates` | QEMU / configured runner | candidate correctness | gtest filter 为 `TransformationEstimation2D.Fused*`。 |
| `run_bench_ordered_cloud_pair_smoke` | QEMU smoke only | bench log shape | `log/qemu/run_bench_ordered_cloud_pair_smoke_rvv.log`；不比较 Std/RVV timing。 |
| `run_bench_ordered_cloud_pair_public_smoke` | QEMU smoke only | production-public probe log shape | `log/qemu/production_public/run_bench_ordered_cloud_pair_public_rvv.log`；不写性能结论。 |
| `run_bench_row_source_smoke` | QEMU smoke only | 三类 row-source candidate 的日志形状 | `log/qemu/row_source/run_bench_row_source_fused_rvv.log`；9 个 case；不写性能结论。 |
| `generate_asm_attribution_summary` | 编译 + objdump | diagnostic asm attribution | `log/qemu/asm_attribution.md`、`log/qemu/asm_attribution.json`。 |
| `run_qemu_production_public_evidence_doctor` | QEMU + host Python | production-public QEMU contract | `log/qemu/production_public/evidence_manifest.json`、`log/qemu/production_public/evidence_doctor.md`。 |
| `run_qemu_row_source_evidence_doctor` | QEMU + host Python | row-source QEMU contract | `log/qemu/row_source/evidence_manifest.json`、`log/qemu/row_source/evidence_doctor.md`；当前 0/0/0。 |
| `run_board_bench_ordered_cloud_pair_repeated` | board / target hardware | board repeated diagnostic | `log/board/ordered_cloud_pair_repeated/summary.md`、manifest、doctor。 |
| `run_board_bench_ordered_cloud_pair_public_repeated` | board / target hardware | production-public board repeated probe | `log/board/ordered_cloud_pair_public_repeated/summary.md`、manifest、doctor。 |
| `run_board_bench_row_source_repeated` | board / target hardware | row-source diagnostic repeated | `log/board/row_source_fused_repeated/summary.md`、manifest、doctor；已完成，Doctor 1/2/6。 |
| `record_qemu_row_source_state` | host Python | row-source evidence registry update | 登记 row-source QEMU log、asm、manifest 和 Doctor。 |
| `evidence_status` | host Python | registry freshness | 检查 `log/evidence_registry.json` 与文档引用。 |

## Target 粒度审计

| target 类别 | 当前状态 | 决策 |
| --- | --- | --- |
| correctness aggregate（正确性汇总入口） | `run_test_compare` 覆盖 Std/RVV 16/16。 | adopted |
| correctness aliases（正确性细分入口） | `run_test_public_semantics` 和 `run_test_candidates` 可分组运行。 | adopted |
| bench diagnostic aliases（bench 诊断入口） | `ordered-cloud-pair-fused`、`ordered-cloud-pair-public` 和 `row-source-fused` 可区分候选族、production-public probe 和三类 row source。 | adopted |
| QEMU smoke aliases（QEMU 小型验证入口） | diagnostic、production-public 和 row-source smoke 都有独立 target。 | adopted |
| board smoke aliases（板卡小型验证入口） | repeated target 生成主证据；单次 compare 只作为 smoke。 | adopted |
| board repeated aliases（板卡重复采集入口） | diagnostic repeated、production-public repeated 和 row-source repeated 均有 target，且本轮均已运行。 | adopted |
| doctor / registry aliases（证据体检和登记入口） | QEMU、board 和 row-source registry target 已接入。 | adopted |

## 输入数据总览

| corpus | helper | 作用 |
| --- | --- | --- |
| deterministic ordered pair | `makePointXYZCloud` + `transformCloud2D` | 稳定覆盖常规 2D 刚体变换。 |
| near-cancellation | `makeNearCancellationCloud` | 保护较大公共偏移下的中心化 correlation 数值预算。 |
| valid indexed / correspondence scalar boundary | `makePrefixIndices` / `makePrefixCorrespondences` | 证明非 ordered public overload 保持当前标量 row pairing。 |
| size mismatch | public tests 内构造 | 验证公开入口在数量不匹配时保持输出矩阵不变。 |
| non-finite x/y/z | public tests 内构造 | 刻画 centroid finite check 与 demean 全量写出的既有组合语义。 |

## 覆盖矩阵

| scope | point type / Scalar | row source policy | correctness | QEMU smoke | board | decision |
| --- | --- | --- | --- | --- | --- | --- |
| public scalar baseline | `PointXYZ` / `float` | ordered-cloud-pair | pass | not_applicable | not_applicable | current scalar truth |
| fused 2D correlation diagnostic | dense finite `PointXYZ` / `float` | ordered-cloud-pair | pass | pass / log shape only | 5-run `weak_positive` | retained diagnostic |
| production-public candidate | exact `PointXYZ` / `float` | ordered-cloud-pair | pass | pass / log shape only | 5-run `4.222x / 5.310x / 4.947x`, Doctor 0/0/0 | production-candidate-supported / review pending |
| source-indexed-cloud-pair | `PointXYZ` / `float` | source indexed | public + candidate pass | pass / 3 cases | 5-run 1.073x / 1.023x / 1.038x; Doctor 1/2/6 | attempted / diagnostic-only |
| dual-indexed-cloud-pair | `PointXYZ` / `float` | dual indexed | public + candidate pass | pass / 3 cases | 5-run 1.081x / 1.014x / 1.038x; Doctor 1/2/6 | attempted / diagnostic-only |
| correspondence-pair | `PointXYZ` / `float` | correspondences | public + candidate pass | pass / 3 cases | 5-run 1.067x / 1.013x / 1.035x; Doctor 1/2/6 | attempted / diagnostic-only |

## 当前可提交证据

当前可提交候选是源码 scaffold、Makefile、board.mk、topic-local script 和 topic-local 文档。`log/`、`build/` 与 `log/evidence_registry.json` 是本地生成的证据状态，默认不提交。

## 当前结论边界

Phase 050 当前形成的是窄范围 production-candidate-supported 结论。它说明 exact `PointXYZ -> PointXYZ`、`Scalar=float`、dense finite ordered-cloud-pair 的 production-public probe 在目标板卡上稳定超过标量路径；production patch 保留，但仍等待用户审阅。Phase 030 对 source-indexed、dual-indexed 和 correspondence row source 使用 materialize-to-ordered candidate，展开成本已纳入 bench 边界；板卡证据已完成，但 64K 退化/长尾使它们保持 diagnostic-only，不能写成 production 结论。
