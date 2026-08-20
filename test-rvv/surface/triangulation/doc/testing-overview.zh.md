# triangulation 测试总览

本主题的测试资产是接入前诊断，不覆盖 production dispatch（生产分发）。目标是判断规则参数网格写入是否值得进入后续生产接入，而不是证明 `surface/src/on_nurbs/triangulation.cpp` 已经被 RVV 优化。

| 层级 | 命令 / 文件 | 覆盖范围 | 证据角色 |
| --- | --- | --- | --- |
| QEMU correctness | `make run_test_compare` | `PointXYZ` 参数网格、polygon 顺序、测试专用 surface checksum。 | `qemu_correctness` |
| QEMU asm | `make dump_bench_rvv` | `createParamGridCandidate` 内联热点中的 RVV store / multiply-add 指令。 | `asm_attribution` |
| board smoke | `make run_board_bench_param_grid` | `tri_param_grid_512`，只计参数网格写入。 | `diagnostic` |
| board smoke | `make run_board_bench_surface_eval` | `tri_surface_eval_256`，参数网格加测试专用 Evaluate-like sink。 | `diagnostic` |
| Evidence Doctor | `make doctor_board_smoke_manifests` | smoke manifest 的边界、checksum、B/A 值和 run count。 | evidence validation |

当前 RISC-V 安装库没有导出 on_nurbs / OpenNURBS 相关符号，因此不能在本阶段构建真实 `ON_NurbsSurface::Evaluate` direct bench。这个限制必须随任何性能结论一起报告。
