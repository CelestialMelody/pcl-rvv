# SHOT benchmark and evidence

## 本文职责

本文记录 bench（性能测试）case-filter、计时边界、checksum、board（板卡）证据、asm attribution（反汇编归因）、Evidence Doctor（证据体检）和提交边界。性能结论只来自 board / target hardware（目标硬件），不使用 QEMU timing（仿真器计时）。

## Bench 输出格式

`src/bench_shot.cpp` 输出每个 case 的 label、平均耗时和 checksum。Std / RVV compare summary 由共享 Makefile / analysis 脚本解析，当前板卡 summary 位于 `log/board/analyze_bench_compare.log`。

## CLI 和 case-filter 字典

| case-filter | 计时边界 | 当前证据角色 |
| --- | --- | --- |
| `public_shot352_fixed_lrf` | 固定 LRF 下 SHOT352 public compute | public-shaped smoke；约 1x，不是 production speedup。 |
| `public_shot1344_fixed_lrf` | 固定 LRF 下 SHOT1344 public compute | public-shaped smoke；约 1x。 |
| `normalize_352_component` | 352 维 descriptor normalization helper | component positive，约 1.58x-1.63x。 |
| `normalize_1344_component` | 1344 维 descriptor normalization helper | component positive，约 1.49x-1.52x。 |
| `shape_bin_component` | SoA normal dot + bin distance | component positive，2.38x-2.47x。 |
| `shape_bin_aos_component` | contiguous `pcl::Normal` AoS | component positive，1.76x-1.90x。 |
| `shape_bin_indexed_component` | `pcl::Normal` + `pcl::Indices` gather | partial-production-candidate，1.65x-1.86x。 |
| `production_shape_bin_direct` | production `createBinDistanceShape` detail helper direct bench | production-detail weak-positive，PI2 单次 board 为 1.07x。 |
| `interpolation_geometry_component` | indexed xyz projection / distance staging | attempted，修正后 0.97x。 |
| `interpolation_bin_selection_component` | bin-selection scalar-tail staging | attempted / unstable，0.84x、1.12x、1.17x。 |
| `color_lab_distance_component` | normalized LAB arithmetic | arithmetic-only positive，1.10x-1.23x。 |
| `color_rgb_lut_indexed_component` | indexed RGB/LUT scalar staging + RVV LAB distance | attempted / neutral-weak，1.02x。 |

## 推荐命令

```bash
make -C test-rvv/features/shot run_test_compare
make -C test-rvv/features/shot dump_bench_rvv
make -C test-rvv/features/shot run_board_production_shape_bin_direct_evidence
make -C test-rvv/features/shot run_board_shot_case_evidence SHOT_BOARD_EVIDENCE_CASE=public_shot352_fixed_lrf SHOT_BOARD_EVIDENCE_ROLE=production-public SHOT_BOARD_EVIDENCE_RUN_LABEL=board-shot-public-shot352-production-pi2
make -C test-rvv/features/shot run_board_shot_case_evidence SHOT_BOARD_EVIDENCE_CASE=public_shot1344_fixed_lrf SHOT_BOARD_EVIDENCE_ROLE=production-public SHOT_BOARD_EVIDENCE_RUN_LABEL=board-shot-public-shot1344-production-pi2
make -C test-rvv/features/shot fetch_board_logs
make -C test-rvv/features/shot run_evidence_doctor
make -C test-rvv/features/shot evidence_status
```

定向 board run 使用 `BENCH_ARGS`，不是 `CASE_FILTER`。优先使用 `run_board_*_evidence` alias，因为它们会写入 `log/board/board-shot-*` 独立目录并登记 `log/evidence_registry.json`；根 `log/board/analyze_bench_compare.log` 属于覆盖式历史路径，不作为 freshness target。

## 计时边界和 checksum

Public cases 包含合成输入已构造后的 public compute；component cases 只计对应 helper 批处理和 checksum 消费。Checksum 来自 bench wrapper 对输出数组或 descriptor 的汇总，用于确认 Std / RVV 输出同边界一致；checksum match 不是性能结论，也不替代 gtest。

## 当前 board 证据

| phase | case | board result | decision |
| --- | --- | --- | --- |
| 010 | normalization 352 / 1344 | 1.58x-1.63x / 1.49x-1.52x | partial-production-candidate component |
| 020 | shape-bin SoA | 2.38x-2.47x | partial-production-candidate component |
| 030 | shape-bin AoS | 1.76x-1.90x | partial-production-candidate component |
| 040 | shape-bin indexed | 1.65x-1.86x | PI1 production probe plan exists; PI2 requires authorization |
| 050 | interpolation geometry | 0.97x after fix | attempted, not recommended |
| 060 | color LAB distance | 1.10x-1.23x | arithmetic-only partial candidate |
| 070 | color RGB/LUT indexed | 1.02x | attempted / neutral-weak |
| 080 | interpolation bin-selection | 0.84x、1.12x、1.17x | attempted / unstable |
| 100 | interpolation bin-selection alias verification | 0.83x | alias / registry 可运行；candidate 仍 not recommended |
| PI2 | production shape-bin direct | 1.07x | production-detail weak-positive；不足以单独采纳 |
| PI2 | public SHOT352 / SHOT1344 after production patch | 0.98x / 0.99x | production-public negative / neutral-negative；PI3 已按用户确认回滚 |

## Evidence Doctor / manifest

Topic-local wrapper 是 `script/generate_shot_evidence_manifest.py`。默认 doctor target 输出根 `log/board/evidence_doctor.md`；Phase 100 和 PI2 的 board alias 会输出到 `log/board/board-shot-*/evidence_doctor.md` 并登记 `log/evidence_registry.json`。PI2 中 `production_shape_bin_direct` Doctor 为 Errors=0、Warnings=1，两个 production-public case 均为 Errors=1、Warnings=1，Error 是 `ba_degradation_frequency`。处理结果是阻断采纳；PI3 已按用户确认回滚 production patch。

PI2 当前登记证据路径：

| run label | summary | manifest | doctor |
| --- | --- | --- | --- |
| `board-shot-production-shape-bin-direct-pi2` | `test-rvv/features/shot/log/board/board-shot-production-shape-bin-direct-pi2/analyze_bench_compare.log` | `test-rvv/features/shot/log/board/board-shot-production-shape-bin-direct-pi2/evidence_manifest.json` | `test-rvv/features/shot/log/board/board-shot-production-shape-bin-direct-pi2/evidence_doctor.md` |
| `board-shot-public-shot352-production-pi2` | `test-rvv/features/shot/log/board/board-shot-public-shot352-production-pi2/analyze_bench_compare.log` | `test-rvv/features/shot/log/board/board-shot-public-shot352-production-pi2/evidence_manifest.json` | `test-rvv/features/shot/log/board/board-shot-public-shot352-production-pi2/evidence_doctor.md` |
| `board-shot-public-shot1344-production-pi2` | `test-rvv/features/shot/log/board/board-shot-public-shot1344-production-pi2/analyze_bench_compare.log` | `test-rvv/features/shot/log/board/board-shot-public-shot1344-production-pi2/evidence_manifest.json` | `test-rvv/features/shot/log/board/board-shot-public-shot1344-production-pi2/evidence_doctor.md` |

PI3 前的补充 side-run（旁路复核 run）同样登记在 registry 中，只作为 historical production probe（历史生产探针）辅助证据，不改变 `rollback/no-production` 结论：

| run label | summary | manifest | doctor | result |
| --- | --- | --- | --- | --- |
| `board-shot-production-shape-bin-direct-side-20260825-once` | `test-rvv/features/shot/log/board/board-shot-production-shape-bin-direct-side-20260825-once/analyze_bench_compare.log` | `test-rvv/features/shot/log/board/board-shot-production-shape-bin-direct-side-20260825-once/evidence_manifest.json` | `test-rvv/features/shot/log/board/board-shot-production-shape-bin-direct-side-20260825-once/evidence_doctor.md` | `production_shape_bin_direct` 0.94x，Doctor Errors=1、Warnings=1。 |
| `board-shot-public-shot352-side-20260825-once` | `test-rvv/features/shot/log/board/board-shot-public-shot352-side-20260825-once/analyze_bench_compare.log` | `test-rvv/features/shot/log/board/board-shot-public-shot352-side-20260825-once/evidence_manifest.json` | `test-rvv/features/shot/log/board/board-shot-public-shot352-side-20260825-once/evidence_doctor.md` | `public_shot352_fixed_lrf` 0.98x，Doctor Errors=1、Warnings=1。 |
| `board-shot-public-shot1344-side-20260825-once` | `test-rvv/features/shot/log/board/board-shot-public-shot1344-side-20260825-once/analyze_bench_compare.log` | `test-rvv/features/shot/log/board/board-shot-public-shot1344-side-20260825-once/evidence_manifest.json` | `test-rvv/features/shot/log/board/board-shot-public-shot1344-side-20260825-once/evidence_doctor.md` | `public_shot1344_fixed_lrf` 0.99x，Doctor Errors=1、Warnings=1。 |

## ASM attribution

`dump_bench_rvv` 输出 `build/asm/riscv/bench_shot_rvv.asm`。PI2 历史 probe 中 production `createBinDistanceShape` 符号附近可见 `vluxei32.v`、`vfwcvt.f.f.v` 和 `vcpop.m`，说明当时 production helper 命中过 indexed gather、f32-to-f64 widen 和 NaN count RVV 路径。PI3 回滚后，当前 `shot.hpp` 不再包含 production RVV helper；后续 asm attribution（反汇编归因）只应写 test-only helper / bench callsite，除非重新授权新的 production probe。

## 提交边界

默认不提交 `build/`、`log/qemu/`、`log/board/`、完整 asm 和 raw board logs。当前可提交候选是 topic-local 文档、测试支撑源码、Makefile、manifest wrapper、phase 文档和队列表状态。若用户要求提交证据日志，必须另行脱敏并拆分 evidence commit。
