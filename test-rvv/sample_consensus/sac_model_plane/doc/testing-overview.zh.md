# sac_model_plane 测试总览

## 本文职责

本文说明当前 topic 的测试入口、bench target、QEMU / board 证据边界和 target 粒度。每个
gtest 的详细断言见 `correctness-tests.zh.md`；bench 输出和 Evidence Doctor（证据体检）见
`benchmark-and-evidence.zh.md`。

## 运行入口分类

| 类别 | 入口 | 证明范围 | 不证明范围 |
| --- | --- | --- | --- |
| correctness aggregate（正确性汇总入口） | `make -C test-rvv/sample_consensus/sac_model_plane run_test_compare` | Std/RVV 两个构建下 7 个 gtest 都通过。 | 不给性能结论。 |
| correctness alias（正确性别名） | `run_base_plane_public_tests` | RVV 构建下只跑基础平面公开入口相关 gtest。 | 不覆盖其它模型或未写入的点型扩展。 |
| bench build（性能测试构建） | `TARGET_BENCH=bench_sac_model_plane_*` | 证明 bench 二进制可生成。 | QEMU bench timing（QEMU 计时）不作为性能证据。 |
| bench mode（性能测试模式） | `BENCH_ARGS='65536 200 identity'` 或 `BENCH_ARGS='65536 200 shuffled'` | 区分恒等索引和乱序索引，验证 identity fast path 与 gather fallback。 | 不覆盖其它点型、其它规模和其它 row source。 |
| asm attribution（反汇编归属） | `dump_bench_rvv` | 在 RVV bench 二进制中定位 `vluxei32`、`vlsseg3e32`、`vcompress` 等关键指令。 | 不证明速度。 |
| board smoke（板卡小型验证） | `board_smoke` | 部署、板卡 gtest、单次 Std/RVV bench 和日志抓回。 | 单次结果不替代 repeated board。 |
| board repeated（重复板卡测试） | `log/board/repeated/run-01..run-05/` 和 `log/board/phase-010/*/repeated/run-01..run-05/` | 5-run 结果支撑 production performance（生产性能）结论。 | 不覆盖环境 metadata 缺失导致的长尾解释。 |
| doctor / manifest（证据体检 / 清单） | `generate_board_repeated_evidence_manifest`、`generate_phase_010_identity_evidence_manifest`、`generate_phase_010_shuffled_evidence_manifest` | 生成 Evidence Doctor 可读 JSON manifest。 | 不自动提交 raw logs。 |

## 覆盖矩阵

| 范围 | 当前状态 | 证据 |
| --- | --- | --- |
| `PointXYZ` shuffled direct indexed | adopted | Phase 000 board repeated + Phase 010 shuffled repeated。 |
| `PointXYZ` identity direct indexed | adopted for select/count | `CloudOnlyIdentityIndicesMatchStandardPath` + Phase 010 identity repeated。 |
| `PointXYZ` explicit empty `indices_` | correctness covered | `ExplicitEmptyIndicesMatchStandardPath`；不产生性能结论。 |
| `PointXYZI` direct indexed | correctness covered | `PointXYZILayoutMatchesStandardPath` 和 `AdditionalAoSPointTypesIdentityMatchStandardPath`；仍缺 dedicated board performance。 |
| `PointXYZRGB/RGBA` direct indexed | correctness covered | `AdditionalAoSPointTypesMatchStandardPath` 和 `AdditionalAoSPointTypesIdentityMatchStandardPath`；不写性能结论。 |
| `PointXYZINormal` direct indexed | correctness covered | 只读 source `x/y/z`，normal 字段不参与 base-plane 距离输出；不写性能结论。 |
| `getDistancesToModel` identity strided load | rejected | mixed gate 试验后撤回；当前反汇编仅保留 gather。 |
| unsupported layout fallback | deferred | compile-time gate 已存在，缺 runtime fixture。 |

## Target 粒度审计

当前 target 能支持 Phase 010/020 closeout。后续可继续增强的粒度是点型专用 bench alias、
环境 metadata 采集，以及 registry freshness check（证据登记新鲜度检查）。这些增强属于提交或
归档硬化，不改变当前性能采纳结论。
