# bilateral_upsampling 测试支撑源码地图

本文承担 `test_support_code_map` role：定位 test support、bench harness、production 对照和 evidence output 之间的调用关系。它不承担性能结论；性能数字见 `doc/benchmark-and-evidence.zh.md`，候选取舍见 `doc/optimization-evidence.zh.md`。

## 总调用图

```text
PCL production public entry
  -> BilateralUpsampling::process
  -> performProcessing
  -> color-gather RVV helper / compatibility shim / Std fallback

test-rvv aggregate header
  -> fixtures + scalar reference + diagnostic candidates
  -> gtest correctness
  -> bench harness
  -> QEMU logs / board summary / phase manifest / Evidence Doctor
```

## 稳定聚合入口

| 路径 | 职责 | 证据边界 |
| --- | --- | --- |
| `include/bilateral_upsampling.h` | topic-local stable aggregate header（稳定聚合头） | 只聚合 test support helper，不是 PCL public API。 |
| `include/impl/bilateral_upsampling_core.hpp` | fixtures、标量 reference、diagnostic candidate、误差统计、checksum | test support；用于对拍和 bench，不修改 production 头。 |
| `src/test_bilateral_upsampling.cpp` | correctness、NaN fallback、窗口边界、production public tests | QEMU / board correctness。 |
| `src/bench_bilateral_upsampling.cpp` | bench harness、production public / steady public、helper-only、nan-mask、color-gather precursor | 板卡性能输入；QEMU timing 不采信。 |
| `Makefile` | 构建、QEMU correctness、asm dump、board deploy 入口 | 复用 `test-rvv/mk/rvv-topic.mk`。 |
| `board.mk` | 板卡远端运行参数 | 复用 `test-rvv/mk/rvv-board-run.mk`。 |

## Fixtures 与 reference

| 符号 | 位置 | 职责 | 不能证明 |
| --- | --- | --- | --- |
| `RgbPoint` | `include/impl/bilateral_upsampling_core.hpp` | test-local RGBD 点结构，方便诊断 helper 固定布局 | 不代表所有 PCL 点型 |
| `makeCloud` | 同上 | 构造 dense / holes organized grid | 不代表真实 sensor 分布 |
| `computeTables` / `depthWeight` | 同上 | 复现 production depth / RGB 查表语义 | 不证明 `computeDistances` 是热点 |
| `processScalar` | 同上 | test scalar reference（标量参考实现） | 不替代 PCL production Std helper 的全部模板覆盖 |
| `compareClouds` / `errorWithinTolerance` | 同上 | 计算 xyz 误差和 NaN 统计 | 不证明性能 |
| `checksumCloud` | 同上 | bench 输出 checksum | 不替代 gtest correctness |

## Candidate / Diagnostic helper

| helper | 位置 | 当前状态 | 证据角色 |
| --- | --- | --- | --- |
| `processCandidate` | `include/impl/bilateral_upsampling_core.hpp` | rejected historical staged-window | phase 000 diagnostic correctness / bench 对照 |
| `processDirectDepthCandidate` | 同上 | historical precursor | phase 010 证明 direct depth load 有希望 |
| `performProcessingNanMaskK64RVV` | `src/bench_bilateral_upsampling.cpp` | attempted / negative | phase 050 component ablation，不覆盖 infinity |
| `performProcessingColorGatherRVV` | `src/bench_bilateral_upsampling.cpp` | accepted precursor | phase 060 bench-local color-gather evidence |
| `bilateralUpsamplingPerformProcessingColorGatherRVV` | `surface/include/pcl/surface/impl/bilateral_upsampling.hpp` | adopted production helper | phase 070 production direct evidence |
| `bilateralUpsamplingPerformProcessingRVV` | 同上 | old RVV fallback / historical | 旧 family 对照，不是当前主线收益来源 |

## Bench Harness 与 case registry

| 函数 / case 族 | 位置 | 计时边界 | 证据角色 |
| --- | --- | --- | --- |
| `runCase` | `src/bench_bilateral_upsampling.cpp` | table staged-window diagnostic | phase 000 historical rejected |
| `runDirectDepthCase` | 同上 | direct-depth diagnostic helper | phase 010 precursor |
| `runProductionCase` | 同上 | 真实 `BilateralUpsampling::process` public path | production direct public evidence |
| `runProductionSteadyStateCase` | 同上 | setters 和对象构造在 timed window 外，stdout 临时静默 | public shell overhead ablation / current steady public evidence |
| `runProductionDetailHelperCase` | 同上 | legacy compatibility label，当前实际走 color-gather helper | old helper-only ablation / current compatibility smoke |
| `runProductionDetailNanMaskK64Case` | 同上 | local nan-mask helper direct call | mask/chunk ablation |
| `runProductionDetailColorGatherCase` | 同上 | bench-local color-gather helper direct call | phase 060 precursor |

当前 bench 没有 CLI case-filter；`main` 固定运行全部 case，并通过 label 区分候选族、点型、规模和 holes / dense 输入。

## Scripts 与 evidence output

| 输出 | 生成 / 来源 | 角色 |
| --- | --- | --- |
| `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` | `run_test_compare` | correctness output |
| `build/asm/riscv/bench_bilateral_upsampling_rvv.asm` | `dump_bench_rvv` | asm attribution input |
| `log/board/analyze_bench_compare.log` | `board_smoke` | board Std/RVV summary |
| `doc/phases/060-production-detail-color-gather-ablation/evidence_manifest.json` | phase 060 closeout | precursor manifest |
| `doc/phases/060-production-detail-color-gather-ablation/evidence-doctor.md` | phase 060 Evidence Doctor | `Errors=0` |
| `doc/phases/070-production-detail-color-gather-production-probe/evidence_manifest.json` | phase 070 closeout | production direct manifest |
| `doc/phases/070-production-detail-color-gather-production-probe/evidence-doctor.md` | phase 070 Evidence Doctor | `Errors=0`、`Warnings=0`、`Suggestions=1` |
| `doc/phases/072-production-same-type-gate-alignment/evidence_manifest.json` | phase 072 closeout | historical same-type production direct manifest |
| `doc/phases/072-production-same-type-gate-alignment/evidence-doctor.md` | phase 072 Evidence Doctor | historical same-type `Errors=0`、`Warnings=0`、`Suggestions=0` |
| `doc/phases/073-cross-rgb-rgba-production-probe/evidence_manifest.json` | phase 073 closeout | current production direct manifest |
| `doc/phases/073-cross-rgb-rgba-production-probe/evidence-doctor.md` | phase 073 Evidence Doctor | current `Errors=0`、`Warnings=0`、`Suggestions=0` |

## Production 与 test support 边界

真实生产行为只在 `surface/include/pcl/surface/impl/bilateral_upsampling.hpp` 中。`test-rvv` 下的 helper 用于 reference、diagnostic、bench-local precursor 或消融；它们不能被写成 public API，也不能替代 production direct tests。当前 adopted 结论只来自 RGB/RGBA exact-family color-gather production helper 经 `BilateralUpsampling::process` 跑通后的 correctness、asm、board 和 Evidence Doctor。

## 拆分审计

当前 test support 代码规模仍可由一个 aggregate header、一个 internal header、一个 gtest source 和一个 bench source 承载。若后续扩点型、layout 或添加 case-filter / summary script，应考虑把 production-public wrapper、candidate helper 和 evidence script 拆成更细文件，并同步更新本文地图。
