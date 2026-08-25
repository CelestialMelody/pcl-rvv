# SHOT 测试入口总览

## 本文职责

本文说明 `test-rvv/features/shot` 的测试入口、target 粒度和证据边界。具体 gtest（GoogleTest 单元测试）断言见 `correctness-tests.zh.md`；bench（性能测试）case、board（板卡）和 Evidence Doctor（证据体检）见 `benchmark-and-evidence.zh.md`。

## 阅读路径

| 文档 | 职责 |
| --- | --- |
| `README.zh.md` | 当前结论、常用命令和阅读入口。 |
| `correctness-tests.zh.md` | 逐个 gtest 的输入、被测路径和证明边界。 |
| `benchmark-and-evidence.zh.md` | bench case-filter、board run、manifest、doctor 和 asm 证据。 |
| `optimization-evidence.zh.md` | 各 candidate family（候选族）的状态和取舍。 |
| `test-support-code-map.zh.md` | test-only helper、bench wrapper 和 script 的代码定位。 |
| `shot-evaluation.zh.md` | S2 函数级评估、Traceability Map（可追踪性地图）和 production 决策。 |

## 测试类型定义

| 类型 | 当前入口 | 能证明什么 | 不能证明什么 |
| --- | --- | --- | --- |
| correctness aggregate（正确性汇总入口） | `make -C test-rvv/features/shot run_test_compare` | Std / RVV 两侧 gtest 是否通过。 | 目标硬件性能、production dispatch（生产分流）。 |
| correctness alias（正确性细分入口） | `run_test_public`、`run_test_normalize`、`run_test_shape_bin`、`run_test_interpolation`、`run_test_color`。 | 单个测试族的局部 correctness。 | 不等于 production direct。 |
| diagnostic bench（诊断性能测试） | `bench_shot_* --case-filter <case>` | 单个 public smoke 或 component case 的计时和 checksum。 | QEMU timing 不代表真实性能；component 不代表 production。 |
| QEMU smoke（仿真器小型验证） | `run_test_compare`、`dump_bench_rvv` | 构建、correctness、asm dump 和日志形状。 | 性能结论。 |
| board smoke（板卡冒烟） | `board_smoke` | 板卡可运行、一次 correctness / bench smoke。 | repeated performance（重复性能证据）。 |
| board targeted run（板卡定向 run） | `run_board_*_evidence` alias 或 `run_board_bench_compare BENCH_ARGS="--case-filter <case>"` | 单 case 目标硬件 A/B；PI2 使用 production-detail 和 production-public roles。 | 没有 run budget 时不能写稳定结论；detail 正向不能覆盖 public 负向。 |
| doctor / manifest / registry | `run_evidence_doctor`、`record_board_evidence_state`、`evidence_status`、`check_evidence_doc_refs` | 检查 summary / manifest 的 Errors、Warnings、Suggestions，并记录 per-run 文件状态。 | 不自动替代人工 production 判断。 |
| historical guarded probe（历史保护探针） | not_applicable | 当前没有历史 production probe target。 | 不适用。 |

## 运行入口分类

| 入口 | 层级 | 用法 |
| --- | --- | --- |
| `run_test_compare` | correctness aggregate | 构建并运行 `test_shot_std` 与 `test_shot_rvv`。 |
| `run_test_public` / `run_test_normalize` / `run_test_shape_bin` / `run_test_interpolation` / `run_test_color` | correctness aliases | 用 gtest filter 拆分常用测试族。 |
| `dump_bench_rvv` | asm attribution（反汇编归因） | 构建 RVV bench 并导出 `build/asm/riscv/bench_shot_rvv.asm`。 |
| `board_smoke` | board smoke | 部署并运行默认测试 / bench；用于快速确认板卡路径。 |
| `run_board_shape_bin_indexed_evidence` / `run_board_production_shape_bin_direct_evidence` / `run_board_interpolation_bin_selection_evidence` / `run_board_color_rgb_lut_evidence` | board case aliases | 通过 `BENCH_ARGS` 传入 `--case-filter`，写入 `log/board/board-shot-*` 独立目录并登记 registry。Production-public case 可用 `run_board_shot_case_evidence` 传入 `SHOT_BOARD_EVIDENCE_ROLE=production-public`。 |
| `fetch_board_logs` | evidence refresh | 从板卡取回 `log/board`。 |
| `run_evidence_doctor` | evidence validation | 生成 `log/board/evidence_manifest.json` 和 `log/board/evidence_doctor.md`。 |
| `evidence_status` | evidence freshness | 检查 `log/board/board-shot-*` 下已登记证据是否 stale。 |

## Target 粒度审计

| target 类别 | decision | evidence | next action |
| --- | --- | --- | --- |
| correctness aggregate | adopted | `Makefile` 的 `run_test_compare`。 | none。 |
| correctness aliases | adopted | Makefile 已提供五个细分 alias。 | none。 |
| bench diagnostic aliases | adopted | `src/bench_shot.cpp` 的 `--case-filter`。 | none。 |
| QEMU smoke aliases | adopted | `dump_bench_rvv` 用于构建 / asm，不把 QEMU bench timing 写成结论。 | none。 |
| board smoke aliases | adopted | `board_smoke`。 | none。 |
| board case aliases | adopted | 常用代表 case 已有 `run_board_*_evidence` alias 和 per-run label 目录。 | repeated summary 仍需新问题时单独设计。 |
| doctor / registry aliases | adopted | `record_board_evidence_state`、`evidence_status`、`check_evidence_doc_refs` 已有。 | none。 |
| historical guarded aliases | not_applicable with evidence | PI3 已按用户确认回滚；当前不新增 rollback probe target。 | none。 |

## 输入数据总览

测试覆盖 `PointXYZ`、`PointXYZRGBA`、`PointNormal`、`Normal`、`SHOT352` 和 `SHOT1344`；`Scalar` 当前是 `float` 输入和 double 中间距离 / 权重。row source（行来源）主要是 fixed-LRF public entry（固定局部参考系公开入口）、连续数组 component、indexed gather component（按索引离散加载组件）、staging arrays（暂存数组）component 和 PI2 historical production direct helper（历史生产直连 helper）。当前不覆盖 correspondences（对应关系索引）、完整 generic point type（泛型点类型）或新的 production RVV path。

## 覆盖矩阵

| 路径 | correctness | bench | asm | board | production direct |
| --- | --- | --- | --- | --- | --- |
| public SHOT352 fixed-LRF | yes | yes | binary-level only | smoke / historical | no |
| public SHOT1344 fixed-LRF | yes | yes | binary-level only | smoke / historical | no |
| normalization component | yes | yes | helper-level | repeated positive | no |
| shape-bin SoA / AoS / indexed | yes | yes | helper-level | repeated positive | no |
| PI2 production shape-bin helper | yes | yes | historical production helper | detail 1.07x, public 0.98x / 0.99x | rollback/no-production |
| interpolation geometry / bin-selection | yes | yes | helper-level | neutral / unstable | no |
| color LAB / RGB-LUT staging | yes | yes | helper-level | positive arithmetic / neutral staging | no |

## 提交边界

默认 evidence policy（证据策略）是 `summary-only`。`build/`、`log/qemu/`、`log/board/` 和完整 asm 属于生成产物，不默认提交；被 phase result 或 evaluation 引用的 summary path 可作为本机复核线索，但不是默认 commit candidate。
