# test support code map

| 文件 | 作用 | 备注 |
| --- | --- | --- |
| `include/triangulation.h` | 测试支撑聚合头；包含 scalar reference、RVV candidate、mesh checksum helper。 | 不属于 PCL public API。 |
| `src/test_triangulation.cpp` | correctness tests。 | 只验证测试支撑 helper 的等价性。 |
| `src/bench_triangulation.cpp` | benchmark 入口。 | 支持 `--case-filter`、`--iterations`、`--warmup-iterations`。 |
| `script/generate_triangulation_board_manifest.py` | 从 archived board smoke logs 生成 Evidence Doctor manifest。 | 当前只解析本 topic 的 `tri_param_grid_512` 和 `tri_surface_eval_256`。 |
| `Makefile` | QEMU、asm、board smoke 和 manifest/doctor target。 | 包含公共 `../../mk/rvv-topic.mk`。 |
| `board.mk` | 板卡端最小运行规则。 | 由 `deploy_files` 同步为远端 Makefile。 |
