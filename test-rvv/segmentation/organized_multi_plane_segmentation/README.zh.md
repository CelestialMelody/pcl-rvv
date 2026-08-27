# organized_multi_plane_segmentation RVV 诊断入口

本目录承载 `organized_multi_plane_segmentation` 的 RVV diagnostic（诊断）资产。当前结论是 `bench-only/no-production`：Phase 000 的局部 viewpoint projection（视点投影）组件在板卡上 positive，但 Phase 010 的 production-shaped diagnostic（生产形态诊断）在 `region_projected` 和 `region_gather_only` 两个边界都稳定 negative，因此不建议修改 production 源码。

## 阅读路径

1. `doc/organized_multi_plane_segmentation-evaluation.zh.md`：函数级评估、Traceability Map（可追踪性地图）、诊断证据链和 production 接入判断。
2. `doc/phases/README.zh.md`：phase 索引、当前状态和默认恢复动作。
3. `doc/phases/010-production-shaped-boundary-projection/result.zh.md`：最终 EvidenceDecision（证据决策）和 no-production 依据。
4. `doc/benchmark-and-evidence.zh.md`：bench case 字典、summary / manifest / Evidence Doctor 路径和复现命令。
5. `doc/test-support-code-map.zh.md`：测试支撑代码、helper、bench、script 和 output 定位。

## 常用命令

| target | 作用 | 证据边界 |
| --- | --- | --- |
| `make run_test_compare` | 分别运行 Std/RVV correctness（正确性）测试 | QEMU / 本地 runner 只证明函数结果一致，不证明性能 |
| `make dump_bench_rvv` | 导出 RVV bench binary 的反汇编 | 需要归属到 candidate helper 或 callee helper |
| `make run_board_omps_repeated` | 在板卡做 repeated component bench 并生成 Evidence Doctor 输入 | 只有板卡结果可支撑性能结论 |
| `make run_board_omps_repeated OMPS_REPEATED_DIR=log/board/phase010-region_projected OMPS_BENCH_ARGS='--size 262144 --iterations 8 --warmup 2 --case-filter region_projected'` | 单独复跑 Phase 010 projected 边界 | production-shaped diagnostic，不是 production direct |

## 证据白名单

当前可审查摘要证据位于：

- `test-rvv/segmentation/organized_multi_plane_segmentation/log/board/repeated/summary.md`
- `test-rvv/segmentation/organized_multi_plane_segmentation/log/board/repeated/evidence_manifest.json`
- `test-rvv/segmentation/organized_multi_plane_segmentation/log/board/repeated/evidence_doctor.md`
- `test-rvv/segmentation/organized_multi_plane_segmentation/log/board/phase010-region_projected/summary.md`
- `test-rvv/segmentation/organized_multi_plane_segmentation/log/board/phase010-region_projected/evidence_manifest.json`
- `test-rvv/segmentation/organized_multi_plane_segmentation/log/board/phase010-region_projected/evidence_doctor.md`
- `test-rvv/segmentation/organized_multi_plane_segmentation/log/board/phase010-region_gather_only/summary.md`
- `test-rvv/segmentation/organized_multi_plane_segmentation/log/board/phase010-region_gather_only/evidence_manifest.json`
- `test-rvv/segmentation/organized_multi_plane_segmentation/log/board/phase010-region_gather_only/evidence_doctor.md`

生成的 `build/`、QEMU timing（QEMU 计时）和 raw board run logs 默认不提交。`log/evidence_registry.json` 记录上述摘要证据的新鲜度。
