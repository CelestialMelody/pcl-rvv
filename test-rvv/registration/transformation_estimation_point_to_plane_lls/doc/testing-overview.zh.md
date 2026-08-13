# transformation_estimation_point_to_plane_lls 测试体系总览

## 本文职责

本文说明 `transformation_estimation_point_to_plane_lls` 专项测试工程的测试类型、运行入口、
覆盖矩阵和证据边界。它不重复 production 实现说明；生产行为长期归属仍是
`doc-rvv/registration/transformation_estimation_point_to_plane_lls-RVV.zh.md`。

专项目录：

```text
test-rvv/registration/transformation_estimation_point_to_plane_lls/
```

当前 production candidate 只覆盖 full-cloud public overload、`Scalar=float`、source xyz f32 AoS layout gate
和 target xyz+normal f32 AoS layout gate。其它公开入口和 `Scalar=double` 保持标量。test-rvv 中的
source-indexed、dual-indices 和 correspondences rows 是 historical diagnostic（历史诊断），不是 production direct
（真实生产路径证据）。

## 文档阅读路径

| 读者问题 | 先读文档 | 作用 |
| --- | --- | --- |
| 每个 gtest 名称是什么意思 | `doc/correctness-tests.zh.md` | 解释 TEST 名称、输入、断言和证明边界。 |
| bench label 和 case-filter 怎么解释 | `doc/benchmark-and-evidence.zh.md` | 解释 bench 入口、checksum、QEMU/board/asm/registry 边界。 |
| 每种优化方式有哪些证据 | `doc/optimization-evidence.zh.md` | 按候选族索引代码、target、board summary 和结论。 |
| 测试支撑代码怎么定位 | `doc/test-support-code-map.zh.md` | 解释聚合入口、内部头、production helper 和测试/bench 调用关系。 |
| EvidenceDecision 和风险是什么 | `doc/transformation_estimation_point_to_plane_lls-evaluation.zh.md` | 记录候选取舍、Traceability Map 和 remaining risk。 |

## 测试类型定义

| 测试类型 | 中文含义 | 本 topic 中的例子 | 证明范围 | 不能证明 |
| --- | --- | --- | --- | --- |
| 公开入口语义测试（public semantics） | 用 public estimator 对照 test-only reference。 | `StdDiagnosticMatchesPublicEstimator`、indices/correspondences std diagnostic tests。 | test-only 标量 reference 没偏离公开入口有效输入语义。 | RVV production dispatch 是否命中。 |
| 局部正确性测试（unit/correctness） | helper 或 candidate 与标量 reference 对拍。 | `FullCloudCandidateMatchesStd`、row source candidate tests。 | test-rvv helper 的 `accepted_points`、`ATA/ATb` 或 matrix 在预算内。 | 目标硬件性能。 |
| 数值一致性测试（numerical consistency） | 误差预算、invalid lane 和高动态范围压力。 | scale-stress、near-cancellation、invalid-lane tests。 | finite mask、reduction tree 和 fused formula 在预算内。 | bitwise 完全一致。 |
| 回退路径测试（fallback tests） | gate miss 后仍走标量。 | small input、layout gate failure、`Scalar=double` fallback。 | 非覆盖范围不误命中 RVV。 | 非覆盖范围有 RVV 性能。 |
| 真实生产路径测试（production direct） | 真实 public overload 命中 production dispatch 或 fallback。 | `ProductionFullCloud*`、`ProductionFusedFullCloud*`。 | full-cloud public overload 的 correctness / fallback。 | indexed、dual-indices、correspondences 或 weighted RVV。 |
| 生产形态诊断（production-shaped diagnostic） | 接近公开入口形态但不是真实 dispatch 的测试或 bench。 | `FullCloudBlockReductionPublicEntryShapeMatchesPublicWithinBudget`、public-entry-shaped bench row。 | public input/output shape 与 helper 兼容。 | production dispatch。 |
| 组件消融（component ablation） | 拆分 load、formula、mask、tail 或 no-solve 成本。 | `--component-only` bench rows。 | 成本归因线索。 | 端到端 production performance。 |
| 性能测试（bench/performance） | board 或目标硬件耗时证据。 | production-dispatch repeated board summary。 | 目标硬件上 std/RVV 或 RVV-vs-RVV 的性能信号。 | QEMU timing 不能作为性能结论。 |
| 反汇编归因（asm attribution） | 确认可见 RVV 指令归属。 | `dump_bench_rvv` 输出和 evaluation 中的 production-symbol asm 说明。 | RVV helper / public overload call site 出现预期指令。 | 不单独证明收益。 |
| 证据登记（evidence registry） | 检查 evidence 文件是否被覆盖或未登记。 | `log/evidence_registry.json`。 | freshness（新鲜度）和 doc refs。 | 不替代 correctness、Evidence Doctor 或 board performance。 |

## 运行入口分类

`run_test` 和 `run_bench` 是 Make target（构建/运行入口），不是单一证据类型。具体证明范围由源码 case 和参数决定。

| 入口 | 实际动作 | 默认输出 | 证据类型 | 性能结论 |
| --- | --- | --- | --- | --- |
| `make ... run_test` | 编译并运行当前 `TARGET_TEST`。 | `output/qemu/run_test.log` 或 compare 子目标指定文件。 | gtest correctness。 | 无。 |
| `make ... run_test_std` | 关闭 `__RVV10__` 编译 gtest。 | `output/qemu/run_test_std.log`；Phase 030 还登记了 `log/qemu/run_test_std.log`。 | std correctness log。 | 无。 |
| `make ... run_test_rvv` | 开启 `__RVV10__` 编译 gtest。 | `output/qemu/run_test_rvv.log`；Phase 030 还登记了 `log/qemu/run_test_rvv.log`。 | RVV correctness log。 | 无。 |
| `make ... run_test_compare` | 顺序运行 std 和 RVV gtest。 | std/RVV gtest logs。 | std/RVV correctness 对拍。 | 无。 |
| `make ... run_bench_compare` | 运行 std/RVV bench 并解析。 | `output/qemu/run_bench_std.log`、`run_bench_rvv.log`、`analyze_bench_compare.log`。 | QEMU 日志形状和 checksum 检查。 | QEMU timing 无性能结论。 |
| `make ... dump_bench_rvv` | 编译 RVV bench 并 objdump。 | `build/asm/riscv/bench_transformation_estimation_point_to_plane_lls_rvv*.asm`。 | asm path evidence。 | 无。 |
| `make ... run_board_test` | 部署并在板卡运行 RVV gtest binary。 | `output/board/run_test.log`。 | 单次板卡 correctness smoke。 | 不作为 repeated performance。 |
| `make ... run_board_bench_compare` | 部署并在板卡运行 bench compare。 | `output/board/run_bench_std.log`、`run_bench_rvv.log`、`analyze_bench_compare.log`。 | 单次板卡 bench smoke。 | 只作诊断，最终性能看 repeated summary。 |
| repeated board analyze | 对多轮 fetched analyze log 做 summary。 | `output/board/*_5run_summary.md`。 | 当前可提交 board evidence。 | 可支撑当前边界内性能结论。 |
| registry check | 检查登记文件 digest 和文档引用。 | stdout；登记表为 `log/evidence_registry.json`。 | freshness guard。 | 无。 |

## 覆盖矩阵

| 范围 | correctness | fallback | bench / board | 当前状态 |
| --- | --- | --- | --- | --- |
| full-cloud `PointNormal -> PointNormal`, `Scalar=float` | production direct、normal-equation、matrix、invalid lane、near-cancellation。 | small input fallback。 | production-dispatch 5-run board summary。 | production candidate adopted。 |
| full-cloud `PointXYZ -> PointNormal`, `Scalar=float` | generic source invalid lane / scale stress tests。 | layout / size gates。 | production-dispatch 5-run board summary。 | representative point type adopted。 |
| full-cloud `PointXYZ -> PointXYZINormal`, `Scalar=float` | generic target tests。 | layout / size gates。 | production-dispatch 5-run board summary。 | representative point type adopted。 |
| other gate-allowed f32 AoS point types | traits/layout gate and shared production direct logic。 | fallback if gate miss。 | no per-type repeated board。 | correctness covered by representative/gate tests; per-type performance not claimed。 |
| `Scalar=double` | fallback smoke。 | `ProductionFullCloudScalarDoubleFallbackSmoke`。 | no RVV bench conclusion。 | scalar fallback。 |
| source-indexed | diagnostic candidate tests。 | production remains scalar。 | historical diagnostic only。 | no-production / separate follow-up。 |
| dual-indices | diagnostic candidate tests。 | production remains scalar。 | historical diagnostic only。 | no-production / separate follow-up。 |
| correspondences | diagnostic candidate tests。 | production remains scalar。 | historical diagnostic only。 | no-production / separate follow-up。 |
| weighted LLS | not this topic。 | not this topic。 | not this topic。 | handled by weighted sibling topic。 |

## Evidence Policy

- QEMU correctness 证明构建、路径和功能，不证明真实性能。
- QEMU bench 只用于日志形状、case-filter 和 checksum。
- 性能结论只引用 board / target hardware repeated summary。
- summary-only board artifacts 可以提交；raw run 目录默认不提交。
- 新增或覆盖 summary / QEMU correctness log 后，先更新 `log/evidence_registry.json` 或把文档标成 stale。
