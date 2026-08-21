# OpenNI2 Grabber RVV Topic

本目录是 `io/src/openni2_grabber.cpp` 的 RVV topic-local（主题本地）测试、bench（性能测试）和阶段文档入口。当前状态是 `adopted production behavior / production-detail positive`：Phase 020 已把 `convertToXYZPointCloud` / `PointXYZ` / 连续 depth path 接入生产内部 helper，接入后板卡重测 median 1.19x；用户已确认有收益即可采纳。

## 先读路径

| 目的 | 路径 |
| --- | --- |
| 当前函数级结论 | `doc/openni2_grabber-evaluation.zh.md` |
| 阶段恢复入口 | `doc/phases/README.zh.md` |
| Phase 000 结果 | `doc/phases/000-current-state-and-diagnostic-scaffold/result.zh.md` |
| Phase 020 PI5 结果 | `doc/phases/020-production-depth-connection/result.zh.md` |
| Phase 030 泛型诊断结果 | `doc/phases/030-rgb-point-type-diagnostic/result.zh.md` |
| 长期生产文档 | `../../../doc-rvv/io/openni2_grabber-RVV.zh.md` |
| 测试和 bench 总览 | `doc/testing-overview.zh.md` |
| 证据摘要与日志边界 | `doc/benchmark-and-evidence.zh.md` |
| 代码地图 | `doc/test-support-code-map.zh.md` |

## 常用命令

| command | 作用 | 证据边界 |
| --- | --- | --- |
| `make run_test_compare` | Std/RVV correctness 汇总入口 | 正确性，不是性能 |
| `make run_bench_rvv BENCH_ARGS="--case-filter all --iterations 2 --warmup-iterations 1"` | QEMU RVV bench smoke | 日志形状，不是性能 |
| `make dump_bench_rvv` | 生成 RVV bench 反汇编 | asm attribution |
| `make check_openni2_grabber_rvv_asm` | 检查关键 RVV 指令是否存在 | asm attribution |
| `make check_openni2_grabber_production_rvv_asm` | 检查 production-detail bench 中关键 RVV 指令 | asm attribution |
| `make run_board_openni2_grabber_smoke` | 板卡单次 Std/RVV bench compare | smoke，不是 repeated 结论 |
| `make run_board_openni2_grabber_production_smoke` | 板卡单次 production-detail Std/RVV bench compare | smoke，不是 repeated 结论 |
| `make collect_board_openni2_grabber_repeated` | 5-run repeated board 采集 | 目标硬件性能 |
| `make collect_board_openni2_grabber_production_repeated` | 5-run production-detail repeated board 采集 | 接入后目标硬件性能 |
| `make generate_board_openni2_grabber_repeated_summary` | 生成 repeated summary | 输入 Evidence Doctor |
| `make generate_board_openni2_grabber_repeated_evidence_manifest` | 生成 repeated board JSON manifest | Evidence Doctor 输入 |
| `make run_board_openni2_grabber_evidence_doctor` | 生成 manifest-based Evidence Doctor | reviewer aid |
| `make run_board_openni2_grabber_production_evidence_doctor` | 生成 production-detail Evidence Doctor | reviewer aid |
| `make record_board_openni2_grabber_repeated_evidence_state` | 登记本地 evidence freshness | 恢复 / 提交前检查 |
| `make record_board_openni2_grabber_production_evidence_state` | 登记 production-detail evidence freshness | 恢复 / 提交前检查 |
| `make check_openni2_grabber_production_evidence_freshness` | 检查 production-detail summary / manifest / Doctor 文档引用 | freshness guard |
| `make evidence_status` | 检查本 topic evidence registry | freshness guard |

## 当前可提交 / 默认不提交

可提交候选是 topic-local 源码、Makefile、board 配置和文档。`build/`、raw board logs、QEMU logs、私有板卡配置和本机 `config.mk` 默认不提交；summary / Evidence Doctor 只有在证据提交策略明确时才作为单独 evidence commit 候选。

## Production 文档适用性

Phase 020 已进入 production integration loop（生产接入闭环），因此 `doc-rvv/io/openni2_grabber-RVV.zh.md` 已创建为 adopted production behavior（已采纳生产行为）文档。该文档使用接入后的板卡数据；当前已按用户确认写成 adopted。
