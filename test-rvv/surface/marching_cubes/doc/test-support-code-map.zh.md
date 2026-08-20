# 测试支撑代码地图

| 文件 | 角色 |
| --- | --- |
| `include/marching_cubes.h` | 聚合入口 |
| `include/impl/marching_cubes_core.hpp` | 测试 helper、candidate 和 RVV 扫描实现 |
| `src/test_marching_cubes.cpp` | Std/RVV correctness 对拍 |
| `src/bench_marching_cubes.cpp` | bench 入口与 case-filter |
| `script/generate_marching_cubes_board_summary.py` | board summary / manifest 生成 |
| `Makefile` | QEMU、bench、board summary / Evidence Doctor 目标 |
| `board.mk` | 板卡远端路径和 binary 名称 |

## 读法

- 先看 `include/impl/marching_cubes_core.hpp`，再看 `src/test_marching_cubes.cpp` 和 `src/bench_marching_cubes.cpp`。
- 证据产物看 `log/board/*`；当前 generic repeated 入口是 `generic_xyz_repeated`、`generic_xyzi_repeated`、`generic_xyzrgb_repeated`、`generic_xyzrgba_repeated`。
- 阶段顺序看 `doc/phases/README.zh.md`。
