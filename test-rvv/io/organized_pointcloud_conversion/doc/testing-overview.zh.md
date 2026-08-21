# organized_pointcloud_conversion 测试总览

## 证据层级

| 层级 | 入口 | 证明内容 | 不证明内容 |
| --- | --- | --- | --- |
| correctness aggregate | `make run_test_compare` | Std / RVV gtest 全部通过，覆盖 diagnostic、decode v0 对拍和 production direct 代表点型 | 不证明性能 |
| production asm | `make check_production_rvv_asm` | production conversion probe 和 analyze detail probe 中存在 RVV 指令 | 不证明速度或完整压缩链路占比 |
| board smoke / repeated | `make run_board_bench_compare` 和 production repeated run 目录 | 板卡上 Std/RVV 日志、checksum、timing 可比较 | 单次 smoke 不作为最终性能结论 |
| Evidence Doctor | `make run_production_repeated_evidence_doctor` | manifest 契约、checksum、异常模式和 evidence role 复核 | Warning 需要解释，不自动表示 bug |

## Target 粒度审计

| target 类别 | 当前入口 | 状态 | 说明 |
| --- | --- | --- | --- |
| correctness aggregate | `run_test_compare` | adopted | 汇总 Std/RVV gtest。 |
| correctness aliases | gtest filter 可按 TEST 名过滤 | adopted | 当前 Makefile 未拆细 alias；TEST 名和本 doc 已提供字典。 |
| bench diagnostic aliases | `BENCH_ARGS=--case-filter <label>` | adopted | case label 可隔离 diagnostic、decode 和 production direct。 |
| QEMU smoke aliases | shared `rvv-topic.mk` build / run test target | adopted | 只作 correctness / log shape。 |
| board smoke aliases | shared board target，`REMOTE_BENCH_ARGS` 可设 case-filter | adopted | 用于单次可运行性，不单独写 production 结论。 |
| board repeated aliases | `PRODUCTION_REPEATED_DIR=log/board/production_direct_repeated` + `run_production_repeated_evidence_doctor` | adopted | 当前 production adoption 引用 5-run repeated summary。 |
| doctor / registry aliases | `run_board_evidence_doctor`、`run_production_repeated_evidence_doctor` | adopted | topic-local manifest wrapper 生成 JSON 后调用全局 doctor。 |
| historical probe guarded aliases | decode diagnostic case 仍在 bench binary 中，文档标为 rejected | adopted | decode v0 不进入 production adoption。 |

## 覆盖矩阵

| 路径 | 点类型 / 输入 | correctness | asm | board | doctor | production |
| --- | --- | --- | --- | --- | --- | --- |
| uncolored encode | `PointXYZ` | pass | pass | positive diagnostic + production direct | production Errors=0 | adopted |
| uncolored encode | `PointXYZI` | production direct helper 对拍 | pass via production probe | positive production direct | production Errors=0 | adopted representative |
| colored encode | `PointXYZRGB` RGB / mono | pass | pass | positive production direct | production one mono warning | adopted |
| colored encode | `PointXYZRGBA` RGB | production direct helper 对拍 | pass via production probe | positive production direct | production Errors=0 | adopted representative |
| decode disparity -> cloud | `PointXYZ` | pass | pass | negative diagnostic | shared manifest Errors=3 | rejected |
| full encode-shaped compression | organized cloud + PNG stream | smoke pass | not production-public | positive production-shaped | Doctor Errors=0 | attempted positive shaped diagnostic |
| `analyzeOrganizedCloud` production detail | organized `PointXYZ` / `PointXYZI` | pass | production detail helper | positive production-detail | detail Doctor Errors=0, Warnings=1 | adopted |

## 证据白名单

当前文档引用的可提交 summary 入口是：

- `log/board/production_direct_repeated/summary.md`
- `log/board/production_direct_repeated/evidence_doctor.md`
- `log/board/full_encode_repeated/summary.md`
- `log/board/full_encode_repeated/evidence_doctor.md`
- `log/board/analyze_component_repeated/summary.md`
- `log/board/analyze_component_repeated/evidence_doctor.md`
- `log/board/analyze_production_detail_repeated/summary.md`
- `log/board/analyze_production_detail_repeated/evidence_doctor.md`
- `log/board/full_encode_after_analyze_detail_repeated/summary.md`
- `log/board/full_encode_after_analyze_detail_repeated/evidence_doctor.md`
- `log/board/evidence_doctor.md`，仅用于解释 decode diagnostic rejected。

Raw run logs、build binaries 和本机 board 部署日志默认不提交。
