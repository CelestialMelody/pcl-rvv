# ONI Grabber RVV Topic

本目录是 `io/src/oni_grabber.cpp` 的 RVV topic-local（主题本地）测试、bench（性能测试）和阶段文档入口。当前状态是 `adopted production behavior / production-detail positive`：`ONIGrabber::convertToXYZPointCloud` 的 depth-only `PointXYZ` production-detail（生产内部边界）补丁已在板卡 repeated summary（重复板卡摘要）中得到 median `1.18x`、min `1.16x`、max `1.22x`，并已按用户“接入后板卡测试有收益即可采纳”的确认进入 production closeout（生产收尾）。该结果不覆盖 RGB/RGBA、IR、真实 ONI 文件读取、replay reader 调度或完整 public-entry throughput（公开入口吞吐）。

## 先读路径

| 目的 | 路径 |
| --- | --- |
| 当前函数级评估 | `doc/oni_grabber-evaluation.zh.md` |
| 阶段恢复入口 | `doc/phases/README.zh.md` |
| Phase 000 计划 | `doc/phases/000-current-state-and-diagnostic-scaffold/plan.zh.md` |
| Phase 000 结果 | `doc/phases/000-current-state-and-diagnostic-scaffold/result.zh.md` |
| Phase 010 生产接入计划 | `doc/phases/010-production-depth-probe/plan.zh.md` |
| Phase 010 生产接入结果 | `doc/phases/010-production-depth-probe/result.zh.md` |
| 优化路线图 | `doc/optimization-roadmap.zh.md` |
| 优化矩阵 | `doc/phases/optimization-matrix.zh.md` |
| 长期 production 主题文档 | `../../../doc-rvv/io/oni_grabber-RVV.zh.md` |
| 测试专用 helper | `include/oni_grabber.h` |

## 常用命令

| command | 作用 | 证据边界 |
| --- | --- | --- |
| `make run_test_compare` | Std/RVV correctness 汇总入口 | 正确性，不是性能 |
| `make run_bench_rvv BENCH_ARGS="--case-filter xyz_depth_full_640x480 --iterations 2 --warmup-iterations 1"` | QEMU RVV bench smoke | 日志形状，不是性能 |
| `make run_bench_rvv BENCH_ARGS="--case-filter prod_xyz_depth_full_640x480 --iterations 2 --warmup-iterations 1"` | QEMU production-detail smoke | 日志形状，不是性能 |
| `make check_oni_grabber_rvv_asm` | 检查关键 RVV 指令是否存在 | asm attribution（反汇编归属） |
| `make check_oni_grabber_production_rvv_asm` | 检查 production-detail hook 和 RVV 指令 | asm attribution |
| `make run_board_oni_grabber_smoke` | 板卡单次 Std/RVV bench compare | smoke，不是 repeated 结论 |
| `make run_board_oni_grabber_production_smoke` | 板卡 production-detail 单次 compare | smoke，不是 repeated 结论 |
| `make collect_board_oni_grabber_repeated` | 5-run repeated board 采集 | 目标硬件性能 |
| `make generate_board_oni_grabber_repeated_summary` | 生成 repeated summary | 后续 Evidence Doctor 输入 |
| `make collect_board_oni_grabber_production_repeated` | 5-run production-detail repeated board 采集 | 目标硬件性能 |
| `make run_board_oni_grabber_production_evidence_doctor` | production-detail repeated summary 的 Evidence Doctor | summary-only reviewer aid |

## 当前可提交 / 默认不提交

可提交候选是 topic-local 源码、Makefile、board 配置和文档。`build/`、raw board logs、QEMU logs、私有板卡配置和本机 `config.mk` 默认不提交；summary / Evidence Doctor 只有在证据提交策略明确时才作为单独 evidence commit 候选。

## Production 文档适用性

当前 production patch（生产补丁）已按接入后板卡收益采纳；正式长期说明见 `doc-rvv/io/oni_grabber-RVV.zh.md`。当前没有继续同轮推进的未阻塞优化方向：RGB/RGBA/IR 需要新 profile 或新 candidate，完整 ONI replay public-entry 需要外部 ONI 场景输入。
