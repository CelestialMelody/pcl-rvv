# 测试总览

本文说明 `extract_polygonal_prism_data` RVV topic 的 test / bench / board（板卡）入口分类。正式 production 长期主题文档只记录当前生产行为；这里负责让 reviewer（审查者）定位测试入口和证据边界。

## 入口分类

| 类别 | target / 文件 | 作用 | 不能证明什么 |
| --- | --- | --- | --- |
| correctness aggregate（正确性汇总入口） | `make run_test_compare`，`src/test_eppd.cpp` | 在 Std build 和 RVV build 下运行完整 gtest，覆盖 diagnostic helper、真实 public entry（公开入口）、fallback（回退路径）、nested polygon、多点型和小规模 gate | 目标硬件性能 |
| board correctness | `make run_board_test` | 在板卡运行 RVV gtest，证明目标硬件上功能路径可执行 | repeated performance（重复性能统计） |
| QEMU smoke（小型验证） | `make run_bench_*` 的窄规模 smoke 或 `run_test_compare` 中的 RVV build | 验证构建、日志形状和 checksum（校验和）一致性 | QEMU timing 不进入性能结论 |
| asm attribution（反汇编归属） | `make dump_bench_rvv` | 生成 `build/asm/riscv/bench_eppd_rvv.full.asm`，确认 `segmentRvv` 符号内有预期 RVV 指令 | 单独证明运行时热度 |
| board repeated aliases | `make run_board_eppd_repeated*` | 生成 repeated board summary、manifest（证据清单）和 Evidence Doctor（证据体检）报告 | raw log 默认不进入提交边界 |
| doctor / registry aliases | `make run_board_evidence_doctor`、`python3 ../../script/evidence_registry.py check ...` | 检查 summary 的 A/B boundary（对比边界）、metadata（元数据）和异常信号，并检查文档引用的 evidence 是否 fresh | 不能替代源码和测试审查 |

## 覆盖矩阵

| 范围 | correctness | board correctness | board performance | 状态 |
| --- | --- | --- | --- | --- |
| `PointXYZ` / single polygon / dense ordered | `SegmentRvvMatchesSegmentStdForDenseSinglePolygon` | `make run_board_test` | `log/board/repeated-production/summary.md` median 1.75x | adopted |
| `PointXYZ` / single polygon / indexed gather | `SegmentRvvMatchesSegmentStdForIndexedSinglePolygon` | `make run_board_test` | `log/board/repeated-production-indexed/summary.md` median 1.75x | adopted |
| `PointXYZ` / nested polygons / dense ordered | `SegmentRvvMatchesSegmentStdForNestedPolygons` | `make run_board_test` | `log/board/repeated-production-nested/summary.md` median 2.18x | adopted |
| `PointXYZ` / nested polygons / indexed gather | `SegmentRvvMatchesSegmentStdForIndexedNestedPolygons` | `make run_board_test` | `log/board/repeated-production-nested-indexed/summary.md` median 2.13x | adopted |
| `<32` input size | `SegmentRvvDeclinesSmallInputs` | `make run_board_test` | not_applicable；用于 fallback | adopted fallback |
| degenerate polygon | `SegmentRvvDeclinesDegeneratePolygons` | `make run_board_test` | not_applicable；用于 fallback | adopted fallback |
| `PointXYZI` / `PointXYZRGB` / `PointXYZRGBA` | 对应 `SegmentRvvMatchesSegmentStdForPoint*` | `make run_board_test` | post-gate summaries median 1.85x / 1.83x / 1.84x | adopted |
| `PointXYZINormal` | `SegmentRvvDeclinesPointXYZINormal` | `make run_board_test` | post-gate fallback median 1.00x | scalar fallback |

## 提交边界

`build/`、`log/qemu/*.log`、`log/board/*/run*/` 和 `config.mk` 默认不提交。若需要随 topic 提交 evidence summary（摘要证据），只选择 `summary.md`、`evidence_manifest.json` 和 `evidence_doctor.md`，并用 `git add -f` 精确加入。
