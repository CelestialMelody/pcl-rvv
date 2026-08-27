# Testing Overview

## Target 粒度

| target 类别 | 当前入口 | 覆盖范围 | 状态 |
| --- | --- | --- | --- |
| correctness aggregate | `make run_test_compare` | QEMU Std/RVV gtest 对拍 | adopted |
| correctness alias | `make run_test_std`、`make run_test_rvv` | 单侧构建和运行 | adopted |
| QEMU smoke | `make run_qemu_smoke`、受保护的 `make run_bench_compare ALLOW_QEMU_BENCH_COMPARE=1 ...` | correctness、bench 输出格式、checksum | adopted；不作性能结论 |
| asm attribution | `make dump_test_rvv`、`make dump_bench_rvv` | RVV 指令存在性和 helper hot loop 归属 | adopted |
| board smoke | `make run_board_test fetch_board_logs` | 板卡 gtest 可运行性和 correctness | adopted |
| board repeated | `make board_repeated` | 默认 `score_pipeline_641x481_four_images`；Phase 000/020/030 通过变量覆盖复现 | adopted |
| doctor / registry | `make evidence_doctor_repeated`、`make refresh_evidence_registry`、`make evidence_status`、`make check_evidence_doc_refs` | manifest、Doctor、local-only freshness | adopted |
| production direct | none | 未修改 production，未进入 PI1-PI5 | not_applicable with evidence |

## 覆盖矩阵

| 入口 | 输入形态 | 点类型 / Scalar | layout | 能证明 | 不能证明 |
| --- | --- | --- | --- | --- | --- |
| `updateScoresStd` / `updateScoresRVV` | synthetic row-major score image | `float` score buffer | contiguous | 3x3 score-update 公式、阈值 gate、符号 gate、边界 fallback | `RangeImage` 点查询、`LocalSurface`、shadow / veil、`computeFeature()` |
| `extractProductionScoreImages` fixture path | synthetic full-finite `RangeImage` | `PointWithRange` / `LocalSurface` | row-major organized image | `extractBorderScoreImages()` score generation、真实 production 实现链接和 checksum 稳定性 | inf / max range / unobserved 分支、shadow / veil、完整 public output |
| `bench_range_image_border_extractor.cpp` | synthetic score images and RangeImage fixture | `float` score buffer / `PointWithRange` | contiguous, including tail width | 同边界 Std/RVV helper timing、production-shaped component ablation | 完整 public `computeFeature()` production dispatch |

QEMU 只用于 correctness 和 log-shape。板卡 repeated 才能作为本阶段性能证据。
