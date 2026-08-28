# Phase 040 Result: line production integration

## 结果摘要

Phase 040 已完成 `SampleConsensusModelLine<PointT>` 三个公开入口的 production integration loop（生产接入闭环）PI2-PI5：

- `countWithinDistance`
- `selectWithinDistance`
- `getDistancesToModel`

当前 production patch（生产补丁）已经在工作区，覆盖范围限定为 RVV 构建、`RVVXYZAoSFloatLayout<PointT>`（xyz 为 float 的 AoS 点布局）、`pcl::index_t` 为 signed 32-bit、点云规模可用 32-bit byte offset（字节偏移）表达，以及 direct indexed `indices_` 行来源。非覆盖点型、非 float xyz AoS layout、非 RVV 构建或规模超过 offset gate 时回到 Standard helper（标量 helper）。

PI5 结论是 `adopted_production_behavior`：接入后的 production direct（真实生产入口直连）板卡数据为 positive-stable，且本轮 prompt override（提示词覆盖）明确要求“板卡上的测试结果如果显示有收益即可采纳”。正式 `doc-rvv/sample_consensus/sac_model_line-RVV.zh.md` 使用本阶段接入后的板卡证据，不使用 Phase 000/010/030 的诊断数据作为生产性能结论。

## 实际改动范围

| 类别 | 文件 / target | 实际状态 |
| --- | --- | --- |
| production header | `sample_consensus/include/pcl/sample_consensus/sac_model_line.h` | 新增 protected Standard/RVV helper 声明；不改变 public API（公开接口）。 |
| production implementation | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_line.hpp` | 公开入口先做 `isModelValid`，再在 RVV gate 命中时 short-circuit（提前返回）到 RVV helper，否则调用 Standard helper。 |
| asm gate | `script/check_line_production_asm.py` + `make check_production_asm` | 三个 production RVV helper 均有反汇编归属。 |
| production repeated evidence | `collect_repeated_board_production_evidence` | 5-run board repeated（板卡重复性能测试）完成。 |
| Evidence Doctor | `production-repeated-evidence-doctor.md/json` | Errors=0 / Warnings=0 / Suggestions=0。 |
| evidence registry | `log/evidence_registry.json` | Phase 040 manifest、doctor Markdown 和 doctor JSON 已登记。 |

## Production direct 证据

命令：

```bash
SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_line collect_repeated_board_production_evidence
make -C test-rvv/sample_consensus/sac_model_line record_repeated_board_production_evidence_state
```

证据路径：

- `test-rvv/sample_consensus/sac_model_line/doc/phases/040-line-production-integration/production-repeated-evidence-manifest.json`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/040-line-production-integration/production-repeated-evidence-doctor.md`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/040-line-production-integration/production-repeated-evidence-doctor.json`

| production public entry | Std avg ms | RVV avg ms | B/A values | median / min / max | decision bucket |
| --- | ---: | ---: | --- | --- | --- |
| `countWithinDistance` | 1.783107 | 0.370203 | `4.8157x, 4.8178x, 4.7916x, 4.8321x, 4.8258x` | `4.8178x / 4.7916x / 4.8321x` | positive-stable |
| `selectWithinDistance` | 2.449200 | 0.796956 | `3.0800x, 3.0420x, 3.0744x, 3.1055x, 3.0640x` | `3.0744x / 3.0420x / 3.1055x` | positive-stable |
| `getDistancesToModel` | 2.227100 | 0.651839 | `3.4036x, 3.3409x, 3.3977x, 3.5229x, 3.4197x` | `3.4036x / 3.3409x / 3.5229x` | positive-stable |

这些数据来自接入后的公开入口 Std/RVV 构建对比。QEMU（仿真器）不用于性能结论。

## Correctness、asm 和 Evidence Doctor

| 证据 | 命令 / 路径 | 结果 | 边界 |
| --- | --- | --- | --- |
| QEMU correctness（QEMU 正确性验证） | `make -C test-rvv/sample_consensus/sac_model_line run_test_compare` | Std/RVV 各 7 个 gtest 全部通过。 | 证明功能和路径可运行，不证明目标硬件性能。 |
| production asm attribution（生产反汇编归属） | `make -C test-rvv/sample_consensus/sac_model_line check_production_asm` | `countWithinDistanceRVV`、`selectWithinDistanceRVV`、`getDistancesToModelRVV` 均找到预期 RVV 指令。 | 证明接入后的 production helper 有 RVV 机器码归属。 |
| Evidence Doctor（证据体检） | `production-repeated-evidence-doctor.md` | Errors=0 / Warnings=0 / Suggestions=0。 | 脚本范围内未发现重复板卡数据异常。 |
| evidence registry（证据登记表） | `make -C test-rvv/sample_consensus/sac_model_line repeated_production_evidence_status` | 文档引用补齐后应为 fresh。 | `log/evidence_registry.json` 是本地 ignored metadata。 |

板卡日志里出现 `script/rvv-board-run.mk` clock skew warning（远端文件时间戳告警）。所有 gtest、bench 和 fetch 命令返回 0，checksum 与 manifest 中记录一致；该告警作为环境风险保留，不改变本阶段 decision bucket。

## Fallback 矩阵

| fallback gate | 当前 production 行为 | 证据 / 边界 |
| --- | --- | --- |
| 非 `__RVV10__` 构建 | 只编译并运行 Standard helper。 | Std side `run_test_compare` 7/7 通过。 |
| 点型不满足 `RVVXYZAoSFloatLayout<PointT>` | `if constexpr` 不实例化 RVV helper，公开入口回到 Standard helper。 | 源码 gate；非覆盖点型未做 dedicated production bench。 |
| `pcl::index_t` 不是 signed 32-bit | `if constexpr` 回到 Standard helper。 | 源码 gate；当前 PCL 构建为 signed 32-bit。 |
| 点云规模超过 `rvvMaxU32ByteOffsetElements<PointT>()` | RVV helper 内部调用 Standard helper。 | 源码 gate；未用超大点云实际触发。 |
| model coefficients 无效 | 保持原公开入口副作用：count 返回 0，select/getDistances 不写输出。 | 现有 gtest 覆盖 getDistances invalid model；count/select invalid 边界依赖 public entry gate。 |

## diagnostic-to-production mismatch audit 回填

| question | result |
| --- | --- |
| evidence role | Phase 000/010/030 是 production-shaped diagnostic；本阶段新增 production direct 证据。 |
| A/B boundary | 当前 A/B 是接入后的 public overload Std/RVV 构建对比，不再是 test helper。 |
| 当前决策问题 | 当前 public RVV path 是否快于当前 public scalar path。 |
| diagnostic 是否外推到 production | 不再外推；本阶段用 production direct 重新测量，结果确认三入口仍 positive-stable。 |
| comparison-boundary / baseline mismatch 风险 | 诊断阶段的 helper / public 边界风险已由 Phase 040 production direct bench 覆盖；未覆盖范围仍包括其它点型、其它 layout 和更宽 row source。 |
| 弱 / 负 / 中性 / 不稳定时 bounded production probe | 未触发；三入口 5-run bucket 均为 positive-stable。 |
| clean adoption 是否需要 RVV-vs-RVV detail A/B | 当前决策是 RVV-vs-scalar production adoption，没有已采用的 line RVV family 需要互比；不需要 RVV-vs-RVV detail A/B。 |

## Optimization matrix 更新

| candidate family | production direct board | asm | doctor | decision |
| --- | --- | --- | --- | --- |
| `count-indexed-gather-f32m2` | public count median/min/max `4.8178x / 4.7916x / 4.8321x` | `countWithinDistanceRVV` 有 RVV 指令归属 | 0/0/0 | `adopted_production_behavior` |
| `select-vcompress-f32m2` | public select median/min/max `3.0744x / 3.0420x / 3.1055x` | `selectWithinDistanceRVV` 有 RVV 指令归属 | 0/0/0 | `adopted_production_behavior` |
| `getDistances-vfsqrt-store` | public getDistances median/min/max `3.4036x / 3.3409x / 3.5229x` | `getDistancesToModelRVV` 有 RVV 指令归属 | 0/0/0 | `adopted_production_behavior` |
| `getDistances-sqr-rvv-scalar-sqrt-store` | 未接 production | Phase 020 diagnostic asm | Phase 020 Errors=1 | `rejected for current diagnostic boundary` |

## 继续 / 停止决策

`continue_stop_decision`: `continue_to_S11_production_doc_rvv_closeout`

`stop_condition_hit`: not_hit。当前 production patch 已保留，接入后 board repeated 证据显示三入口 positive-stable，并满足本轮采纳条件。

`next_phase_default`: `S11-production-doc-rvv-closeout`。用本阶段 post-integration board 数据创建 `doc-rvv/sample_consensus/sac_model_line-RVV.zh.md`，并同步 evaluation、queue doc、Handoff 和 freshness check。

当前仍可进一步尝试的优化方向是 `identity-index-strided-load`（恒等索引跨步加载）和 `point-type-expansion`（点型扩展），但它们都不属于本次 adopted production boundary：前者会改变 production family，后者会扩大点型/layout 证据边界。建议先完成当前三入口 doc-rvv closeout，再单独开下一 phase。
