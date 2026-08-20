# benchmark and evidence

## 证据边界

当前 board evidence（板卡证据）以 2026-08-20 5-run repeated 为当前 truth；早期 smoke 只保留为历史入口。

| case | summary | manifest | doctor | Std avg | RVV avg | Std/RVV |
| --- | --- | --- | --- | ---: | ---: | ---: |
| `tri_param_grid_512` | `log/board/param_grid_512_smoke/analyze_bench_compare.log` | `log/board/param_grid_512_smoke/evidence_manifest.json` | `log/board/param_grid_512_smoke/evidence_doctor.md` | `7.9730 ms` | `7.9861 ms` | `0.998x` |
| `tri_surface_eval_256` | `log/board/surface_eval_256_smoke/analyze_bench_compare.log` | `log/board/surface_eval_256_smoke/evidence_manifest.json` | `log/board/surface_eval_256_smoke/evidence_doctor.md` | `58.5395 ms` | `58.2759 ms` | `1.005x` |

两条 smoke 的 checksum 均一致。`tri_param_grid_512` 的 Evidence Doctor 结果为 `Errors=1, Warnings=1, Suggestions=0`，Error 是 `ba_degradation_frequency`。`tri_surface_eval_256` 的 Evidence Doctor 结果为 `Errors=0, Warnings=1, Suggestions=1`，风险是 `low_run_count` 和 `near_threshold_ba`。

## Repeated Board Summary

| case | summary | manifest | doctor | run_count | values | decision bucket |
| --- | --- | --- | --- | ---: | --- | --- |
| `tri_param_grid_512` | `log/board/param_grid_512_repeated_20260820_141209/summary.md` | `log/board/param_grid_512_repeated_20260820_141209/evidence_manifest.json` | `log/board/param_grid_512_repeated_20260820_141209/evidence_doctor.md` | 5 | `1.028x, 0.975x, 0.952x, 1.020x, 0.978x` | negative / unstable diagnostic |
| `tri_surface_eval_256` | `log/board/surface_eval_256_repeated_20260820_141331/summary.md` | `log/board/surface_eval_256_repeated_20260820_141331/evidence_manifest.json` | `log/board/surface_eval_256_repeated_20260820_141331/evidence_doctor.md` | 5 | `0.986x, 0.999x, 1.036x, 1.000x, 0.985x` | neutral / unstable diagnostic |

两个 repeated 的 checksum 均一致，Evidence Doctor 都为 `Errors=1, Warnings=0, Suggestions=0`，Error 均为 `ba_degradation_frequency`，退化频率均为 3/5。

## 板卡状态

2026-08-20 用户确认板卡恢复后，`make check_board_ssh` 通过，`ping -c 2 -W 2 192.168.55.2` 为 0% packet loss，路由经 `enp6s0`。本轮已完成 `tri_param_grid_512` 和 `tri_surface_eval_256` 的 5-run repeated board。

真实 OpenNURBS / on_nurbs direct bench 也尚不能推进：`nm -D /home/zoomin/codes/riscv/pcl-rvv/lib/libpcl_surface.so 2>/dev/null | c++filt | rg 'ON_NurbsSurface::Evaluate|pcl::on_nurbs::Triangulation::convertSurface2PolygonMesh|ON_3dPoint'` 无输出，说明当前 RISC-V 安装库仍缺少必要符号。

## Evidence Registry Map

`log/evidence_registry.json` 登记以下仓库相对路径：

- `test-rvv/surface/triangulation/log/board/param_grid_512_smoke/analyze_bench_compare.log`
- `test-rvv/surface/triangulation/log/board/param_grid_512_smoke/run_bench_std.log`
- `test-rvv/surface/triangulation/log/board/param_grid_512_smoke/run_bench_rvv.log`
- `test-rvv/surface/triangulation/log/board/param_grid_512_smoke/evidence_manifest.json`
- `test-rvv/surface/triangulation/log/board/param_grid_512_smoke/evidence_doctor.md`
- `test-rvv/surface/triangulation/log/board/surface_eval_256_smoke/analyze_bench_compare.log`
- `test-rvv/surface/triangulation/log/board/surface_eval_256_smoke/run_bench_std.log`
- `test-rvv/surface/triangulation/log/board/surface_eval_256_smoke/run_bench_rvv.log`
- `test-rvv/surface/triangulation/log/board/surface_eval_256_smoke/evidence_manifest.json`
- `test-rvv/surface/triangulation/log/board/surface_eval_256_smoke/evidence_doctor.md`
- `test-rvv/surface/triangulation/log/board/param_grid_512_repeated_20260820_141209/summary.md`
- `test-rvv/surface/triangulation/log/board/param_grid_512_repeated_20260820_141209/evidence_manifest.json`
- `test-rvv/surface/triangulation/log/board/param_grid_512_repeated_20260820_141209/evidence_doctor.md`
- `param_grid_512_repeated_20260820_141209-raw`
- `test-rvv/surface/triangulation/log/board/surface_eval_256_repeated_20260820_141331/summary.md`
- `test-rvv/surface/triangulation/log/board/surface_eval_256_repeated_20260820_141331/evidence_manifest.json`
- `test-rvv/surface/triangulation/log/board/surface_eval_256_repeated_20260820_141331/evidence_doctor.md`
- `surface_eval_256_repeated_20260820_141331-raw`
- `test-rvv/surface/triangulation/log/board/run_test.log`
- `test-rvv/surface/triangulation/build/asm/riscv/bench_triangulation_rvv.asm`

`run_bench_*.log` 是本地支撑 raw log（原始日志），默认不作为提交对象；当前文档结论引用 summary、manifest 和 Evidence Doctor。
