# Phase 050 Result: full-chain-timing-and-production-eligibility

## 执行范围

本阶段按 `plan.zh.md` 把 Phase 040 的 linearized map copy（线性化图拷贝）正向信号放回 production-shaped full-chain split（生产形态分阶段计时）中复核。Std/RVV 两侧都保留标量 energy map generation（能量图生成）、score accumulation（分数累加）和 score scan（分数扫描）；只有 `EnergyMaps -> LinearizedMaps` 的 8-bin copy 在 RVV build（RVV 构建）中替换为 stride-load copy（跨步加载拷贝）。

本阶段未修改 `recognition/src/linemod.cpp`，也不覆盖 `LINEMOD_USE_SEPARATE_ENERGY_MAPS`、NMS（非极大值抑制）、averaged detection（邻域加权检测）、semi-scale（半尺度不变）或真实 public dispatch（公开入口分流）。

## 动作回填

| action | status | evidence / command | conclusion |
| --- | --- | --- | --- |
| RED：新增 multi-bin linearized copy correctness | done | `make run_test_compare TEST_ARGS="--gtest_filter=LINEMODTemplateScoring.LinearizedMapsCopyMatchesAllBinsLayout"` 先因 `linearizeEnergyMapsStd/RVV` 缺失编译失败 | 测试先于 helper 实现，失败原因符合预期 |
| GREEN：实现 multi-bin helper | done | `include/impl/linemod_template_scoring_candidates.hpp` 中 `linearizeEnergyMapsStd/RVV` | 输出布局为 `bin -> map_row -> map_col -> row -> col`，与 Phase 050 bench 读取方式一致 |
| correctness 回归 | done | `make run_test_compare TEST_ARGS="--gtest_filter=LINEMODTemplateScoring.*"` | Std/RVV 各 6 个测试通过 |
| bench harness 扩展 | done | `src/bench_linemod_template_scoring.cpp` 新增 `LINEMOD_SCORE_BENCH_FULL_CHAIN_LINEARIZED_COPY_ONLY` | 输出 full-chain 四个阶段 label、total label、`energy_checksum` 和 `linearized_checksum` |
| Makefile / manifest 支撑 | done | `Makefile` 的 Phase 050 target；`script/generate_linemod_template_scoring_evidence_manifest.py` 的 Phase 050 case metadata | collect / manifest / doctor / registry / freshness target 均可恢复 |
| QEMU smoke | done | `BENCH_ARGS="257 17 1 0"` 的 Std/RVV 小规模运行 | 两侧 `checksum=8749505781958690145`、`energy_checksum=8699816572726103351`、`linearized_checksum=9173840362279509015` 一致；QEMU timing 不作为性能结论 |
| asm attribution（反汇编归属） | done | `make dump_full_chain_split_bench_rvv`；`build/asm/riscv/bench_linemod_template_scoring_full_chain_split_rvv.full.asm` | 专用二进制中出现 `vlse8.v`、`vse8.v` 和 `vsetvli` |
| board repeated | done | `make collect_full_chain_split_repeated_board BENCH_ARGS="4096 96 200 5"` | 5-run board correctness 通过；full-chain total 和 linearized copy 均为 positive |
| Evidence Doctor / registry | done | `make run_full_chain_split_evidence_doctor`、`make record_full_chain_split_evidence_state BENCH_ARGS="4096 96 200 5"`、`make check_full_chain_split_evidence_freshness` | Doctor `Errors=0, Warnings=0, Suggestions=0`；registry fresh |

## 计划偏差

原计划希望同时把 full-chain split 的 5 个 timing label 放入 summary。第一次 Evidence Doctor 运行后，energy map generation、score accumulation 和 score scan 这三个 label 因两侧都强制走标量 helper，被脚本按“候选对比项”解释并触发无关 Error。处理动作是把 manifest 和 doctor 输入收窄到真正参与 EvidenceDecision 的两个 label：

- `LINEMOD full-chain total`
- `LINEMOD full-chain linearized map copy`

三个标量解释阶段仍保留在 bench 输出中，作为成本拆分参考；它们不再作为 Phase 050 的 RVV candidate decision（候选决策）条目。

## 诊断证据链

| evidence | result | path |
| --- | --- | --- |
| correctness | Std/RVV `LINEMODTemplateScoring.*` 各 6/6 通过 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| QEMU smoke | full-chain split 小规模 Std/RVV checksum、energy_checksum、linearized_checksum 一致 | `log/qemu/run_bench_std.log`、`log/qemu/run_bench_rvv.log` |
| asm attribution | full-chain split RVV bench 二进制包含 linearized copy 需要的 `vlse8.v` / `vse8.v` / `vsetvli` | `build/asm/riscv/bench_linemod_template_scoring_full_chain_split_rvv.full.asm` |
| board repeated total | `LINEMOD full-chain total` median speedup `1.565x`，min `1.500x`，max `1.624x`，degrade frequency `0/5` | `doc/phases/050-full-chain-timing-and-production-eligibility/full-chain-split-repeated-summary.md` |
| board repeated copy | `LINEMOD full-chain linearized map copy` median speedup `1.790x`，min `1.769x`，max `1.840x`，degrade frequency `0/5` | `doc/phases/050-full-chain-timing-and-production-eligibility/full-chain-split-repeated-summary.md` |
| Evidence Doctor | `Errors=0, Warnings=0, Suggestions=0` | `doc/phases/050-full-chain-timing-and-production-eligibility/full-chain-split-repeated-evidence-doctor.md` |
| evidence registry | Phase 050 summary / manifest / doctor 已登记，freshness check 为 fresh | `log/evidence_registry.json` |

板卡运行期间出现远端 Makefile clock skew warning（时间戳偏差警告）。该 warning 没有导致编译、测试、bench、summary、manifest、doctor 或 registry target 失败；本阶段把它作为环境备注保留，不扩大性能结论。

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `production_shaped_diagnostic` |
| A/B boundary | `test_helper` |
| 当前决策问题 | `implementation-shape` and `RVV-vs-scalar` for linearized-copy-only full-chain split |
| diagnostic 是否可外推到 production | `unknown`；本阶段仍使用 synthetic quantized byte stream 和 test helper，不覆盖真实 modality 对象、`LinearizedMaps` aligned allocation、`getOffsetMap(feature.x, feature.y)` 后续读取、NMS、averaging、semi-scale 或 production dispatch |
| comparison-boundary / baseline mismatch 风险 | `yes`；A/B 只改变 copy helper，且完整链路仍不是 `LINEMOD::detectTemplates` 或 `matchTemplates` 的真实公开入口 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | `yes`；本阶段结果为 positive 且 doctor clean，因此允许进入 PI1 production integration plan，但不允许直接 clean-adopt |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前无已采用 RVV family；若 PI 阶段新增多个 copy family，需要同一 production boundary 内的 RVV-vs-RVV A/B |

## Evidence Doctor 处理

最终 Evidence Doctor 使用机器可读 manifest，比较项为两个真实决策 label。`Errors=0, Warnings=0, Suggestions=0` 说明脚本规则覆盖范围内没有 checksum、A/B boundary、degradation frequency、metadata 或 asm boundary 异常。该结果仍只证明 production-shaped diagnostic 边界干净，不证明 production direct 已闭合。

## Matrix / Roadmap 更新

Phase 050 decision 是 `attempted-positive / production-ready-for-PI1-plan`。full-chain total 在 5-run board repeated 中保持 `positive`，说明只接入 linearized copy 的方向值得进入 PI1 计划。Phase 010/020/030 的负向子核仍保持 rejected/attempted-negative，不随本阶段 positive 翻转。

本阶段新增的默认下一动作是 `060-pi1-production-integration-plan`：冻结真实 `recognition/src/linemod.cpp` 中可修改的入口、fallback、dispatch、宏边界和 production direct evidence（真实生产路径证据）计划。

## Continue / Stop Decision

`stop_condition_hit: production_patch_requires_explicit_authorization`。Phase 050 自身没有性能、正确性、Evidence Doctor、registry 或板卡 blocker；但是继续到 PI2 会修改 production 源码，而当前短 prompt 只默认授权 topic-local 测试资产和文档。默认下一阶段已创建为 `060-pi1-production-integration-plan`；PI2 production patch 需要用户明确授权进入 production integration loop（生产接入闭环）。
