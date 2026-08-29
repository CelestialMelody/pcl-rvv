# color_modality Test Support Code Map

| 符号 / 文件 | 层级 | 作用 | 上游入口 | 下游消费者 | 证据角色 |
| --- | --- | --- | --- | --- | --- |
| `recognition/include/pcl/recognition/color_modality.h` | production | `processInputData()`、Std/RVV helper 和 fallback | PCL recognition caller | QuantizedMap / feature extraction | production boundary |
| `test-rvv/recognition/color_modality/include/cm.h` | test aggregator | 聚合测试支撑头 | test / bench src | internal helper | include map |
| `include/impl/cm_color_quantize.hpp` | diagnostic reference / candidate | scalar reference 和 RVV candidate helper | `test_cm.cpp`、`bench_cm.cpp` | assertions / checksum | diagnostic and production-shaped evidence |
| `src/test_cm.cpp` | correctness target | gtest、forced scalar/RVV hook、public entry 对拍 | `run_test_compare` | QEMU test logs | correctness gate |
| `src/bench_cm.cpp` | bench wrapper | production direct 和 diagnostic bench case | board / QEMU bench targets | summary / manifest | performance input |
| `script/generate_cm_evidence_manifest.py` | analysis script | 解析 repeated board run，生成 summary 和 manifest | `record_evidence_state_repeated` | Evidence Doctor / registry | evidence metadata |
| `Makefile` | target registry | 构建、QEMU、asm、board、doctor 和 registry target | worker commands | output artifacts | recovery entry |
| `board.mk` | board config | 远端 bench/test 名称和共享 board fragment | board targets | remote execution | board harness |

## 布局审计

当前 topic 已使用 `src/`、`include/`、`include/impl/` 和 `script/`。没有旧 `test_support/` 目录需要迁移。`cm` 是长 topic 名 `color_modality` 的缩写 token，已在 README 和文件名中保持一致。当前 helper 未超过硬拆分阈值；职责虽包含 reference 和 candidate，但仍在一个 253 行内部头中，可审查且由本文档列出边界。
