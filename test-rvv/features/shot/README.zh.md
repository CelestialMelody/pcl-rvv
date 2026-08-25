# SHOT RVV topic 导航

本目录承载 `features/include/pcl/features/impl/shot.hpp` 的 RVV 函数级评估与诊断资产。SHOT 已完成 shape-bin indexed gather 的 production integration loop（生产接入闭环），但接入后 public entry（公开入口）没有收益；用户已确认回滚，`features/include/pcl/features/impl/shot.hpp` 当前保持原标量 production（生产源码）路径。

## 当前结论

`shot.hpp` 的局部组件曾显示 RVV 潜力，但接入后的 production-public（生产公开入口）证据不支持保留 shape-bin production patch（生产补丁）。Phase 010 已证明 `normalizeHistogram` 对应的 test-only normalization component（测试专用归一化组件）在板卡上稳定正向，352 维约 1.58x-1.63x，1344 维约 1.49x-1.52x；Phase 020-040 已证明 shape-bin SoA / AoS / indexed gather component 稳定正向，其中 indexed gather 为 1.65x-1.86x。PI2 把 indexed gather 接入 `createBinDistanceShape` 后，production-detail（生产细节 helper）只有 1.07x weak-positive（弱正向），而 `public_shot352_fixed_lrf` / `public_shot1344_fixed_lrf` 为 0.98x / 0.99x，两个 public case 的 Evidence Doctor（证据体检）均有退化 Error。PI3 已按用户确认回滚 production patch；当前结论是 `rollback/no-production`，不创建正式 `doc-rvv/features/shot-RVV.zh.md`。

## 阅读路径

| 文档 | 作用 |
| --- | --- |
| `doc/shot-evaluation.zh.md` | S2 函数级评估、Traceability Map（可追踪性地图）和生产接入判断。 |
| `doc/testing-overview.zh.md` | 测试入口分类、target 粒度审计和覆盖矩阵。 |
| `doc/correctness-tests.zh.md` | 15 个 gtest 的输入、断言和证明边界。 |
| `doc/benchmark-and-evidence.zh.md` | bench case-filter、board、Evidence Doctor、asm 和提交边界。 |
| `doc/optimization-evidence.zh.md` | 各 candidate family 的证据索引和取舍。 |
| `doc/test-support-code-map.zh.md` | test-only helper、bench wrapper、script 和 production 对照关系。 |
| `doc/phases/000-current-state-and-diagnostic-plan/result.zh.md` | Phase 000 的 correctness、asm、board smoke 和 Evidence Doctor 结果。 |
| `doc/phases/010-normalize-component-diagnostic/result.zh.md` | Phase 010 的 normalization component correctness、asm、board 和 Evidence Doctor 结果。 |
| `doc/phases/020-shape-bin-component-diagnostic/result.zh.md` | Phase 020 的 shape-bin SoA component correctness、asm、board 和 Evidence Doctor 结果。 |
| `doc/phases/030-shape-bin-aos-layout-diagnostic/result.zh.md` | Phase 030 的 shape-bin AoS layout correctness、asm、board 和 Evidence Doctor 结果。 |
| `doc/phases/040-shape-bin-indexed-gather-diagnostic/result.zh.md` | Phase 040 的 indexed gather correctness、asm、board 和 Evidence Doctor 结果。 |
| `doc/phases/PI1-shape-bin-indexed-production-probe-plan/plan.zh.md` | 只写计划的 production probe 范围、fallback、production direct tests 和暂停条件。 |
| `doc/phases/050-interpolation-geometry-staging-diagnostic/result.zh.md` | Phase 050 的 interpolation geometry staging correctness、asm、board 和 Evidence Doctor 结果。 |
| `doc/phases/060-color-lab-distance-component-diagnostic/result.zh.md` | Phase 060 的 color LAB distance arithmetic correctness、asm、board 和 Evidence Doctor 结果。 |
| `doc/phases/070-color-rgb-lut-indexed-diagnostic/result.zh.md` | Phase 070 的 indexed RGB/LUT staging correctness、asm、board 和 Evidence Doctor 结果。 |
| `doc/phases/080-interpolation-bin-selection-scalar-tail-diagnostic/result.zh.md` | Phase 080 的 interpolation bin-selection scalar-tail correctness、asm、board 和 Evidence Doctor 结果。 |
| `doc/phases/090-structure-parity-doc-suite-diagnostic/result.zh.md` | Phase 090 的 doc suite role inventory、target 粒度审计和结构缺口结果。 |
| `doc/phases/100-evidence-registry-target-alias-diagnostic/result.zh.md` | Phase 100 的 registry / alias 接入、代表 board alias 和停止条件。 |
| `doc/phases/PI2-shape-bin-indexed-production-probe/result.zh.md` | PI2-PI5 的 production patch、production direct correctness、asm、board public evidence 和回滚确认点。 |
| `doc/phases/PI3-shape-bin-production-rollback-closeout/result.zh.md` | 用户确认回滚后的 no-production closeout、验证结果和停止原因。 |
| `doc/phases/optimization-matrix.zh.md` | 跨阶段优化矩阵。 |
| `doc/optimization-roadmap.zh.md` | 主题级优化路线图和默认恢复动作。 |

## 常用命令

```bash
make -C test-rvv/features/shot run_test_compare
make -C test-rvv/features/shot run_test_shape_bin
make -C test-rvv/features/shot dump_bench_rvv
make -C test-rvv/features/shot run_evidence_doctor
make -C test-rvv/features/shot evidence_status
```

QEMU（仿真器）只用于 correctness（正确性）、构建和日志形状；性能结论只能来自 board（板卡）或目标硬件。

## 证据提交边界

默认策略是 `summary-only`。`log/qemu`、`log/board`、`build` 和完整反汇编属于生成产物，不默认提交；PI2 中被 phase result / evaluation / Handoff 引用的 per-run summary 可作为历史复核线索，但 raw log（原始日志）仍不默认提交。
