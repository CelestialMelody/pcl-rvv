# triangulation RVV 诊断主题

本目录维护 `surface/src/on_nurbs/triangulation.cpp` 的接入前 RVV（RISC-V Vector，可变长度向量扩展）诊断资产。当前主题不修改 production（生产）源码，只在 `test-rvv/surface/triangulation` 中复刻未裁剪 NURBS surface 的规则参数网格生成和测试专用 Evaluate-like（曲面求值替身）路径。

## 当前结论

`param_grid_rvv_store` 已完成 QEMU correctness（正确性）、board correctness（板卡正确性）、反汇编归属和 5-run repeated board（重复板卡性能诊断）。当前没有可采纳收益：`tri_param_grid_512` median `0.978x`、`tri_surface_eval_256` median `0.999x`，两者均 3/5 退化，Evidence Doctor（证据体检）均为 `Errors=1`。该诊断证据不能外推成 production evidence（生产证据），当前不进入 production integration loop（生产接入闭环）。

## 常用命令

| 命令 | 作用 |
| --- | --- |
| `make run_test_compare` | QEMU 下运行 Std / RVV correctness 对比。 |
| `make dump_bench_rvv` | 生成 RVV bench 反汇编并抽取 RVV 指令。 |
| `make run_board_bench_param_grid && make fetch_board_logs` | 板卡运行 `tri_param_grid_512` smoke。 |
| `make run_board_bench_surface_eval && make fetch_board_logs` | 板卡运行 `tri_surface_eval_256` smoke。 |
| `make doctor_board_smoke_manifests` | 从已归档 smoke 日志生成 manifest 并运行 Evidence Doctor。 |

当前 repeated 证据入口：

- `log/board/param_grid_512_repeated_20260820_141209/summary.md`
- `log/board/surface_eval_256_repeated_20260820_141331/summary.md`

## 文档入口

- `doc/triangulation-evaluation.zh.md`：函数级评估和 Traceability Map（可追踪性地图）。
- `doc/phases/000-current-state-and-grid-sampling-diagnostic/plan.zh.md`：阶段计划。
- `doc/phases/000-current-state-and-grid-sampling-diagnostic/result.zh.md`：阶段结果和继续 / 停止判断。
- `doc/optimization-roadmap.zh.md`：后续候选路线。
- `doc/phases/optimization-matrix.zh.md`：候选矩阵。
