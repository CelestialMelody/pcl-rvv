# color_gradient_dot_modality 测试支撑代码地图

本文件做什么：
这里把测试支撑、bench wrapper、脚本和证据输出连起来，让 reviewer 不用翻代码就能找到入口。

## 总图

| 符号 / 文件 | 作用 | 调用者 | 下游 | 证据角色 |
| --- | --- | --- | --- | --- |
| `include/cgdm.h` | 测试支撑聚合入口 | `src/test_cgdm.cpp`、`src/bench_cgdm.cpp` | `include/impl/cgdm_color_gradient.hpp` | navigation / aggregation |
| `include/impl/cgdm_color_gradient.hpp` | scalar reference 与 RVV candidate helper | correctness、bench | `pcl::ColorGradientDOTModality<PointXYZRGB>` / map artifact | test-only reference |
| `src/test_cgdm.cpp` | gtest correctness | `run_test_compare` | scalar / candidate / production direct 对拍 | correctness gate |
| `src/bench_cgdm.cpp` | bench wrapper | `run_bench_rvv` / board target | summary / checksum / repeated evidence | bench / evidence input |
| `script/generate_cgdm_evidence_manifest.py` | summary -> manifest 生成 | `evidence_manifest_repeated` | `evidence_doctor.py`、`evidence_registry.py` | machine-readable manifest |
| `Makefile` | topic-local target 汇总 | 用户 / CI | board repeated、doctor、registry、freshness | entry point |
| `board.mk` | board 侧参数 | shared board runner | repeated board collect | board integration |

## 角色边界

- `include/impl/*` 只做测试支撑，不是 production helper。
- `src/bench_cgdm.cpp` 只负责计时和 checksum，不承担算法结论。
- `script/generate_cgdm_evidence_manifest.py` 只把 summary 整成 Evidence Doctor 可读的 manifest。
- `record_evidence_state_repeated` 之后，证据才进入 registry。

## 需要记住的路径

- correctness 主入口：`run_test_compare`
- bench 主入口：`run_bench_rvv`
- board repeated：`board_repeated`
- freshness：`check_evidence_freshness`
