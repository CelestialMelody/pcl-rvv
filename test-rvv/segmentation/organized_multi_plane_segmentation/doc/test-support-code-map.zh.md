# Test Support Code Map

## 代码与职责

| 文件 / 符号 | 职责 | 证据边界 |
| --- | --- | --- |
| `include/omps.h` | 聚合入口 | 只给当前 topic test/bench 使用 |
| `include/impl/omps_components.hpp` | Std/RVV helper、summary、production-shaped helper | 测试专用，不是 production detail helper |
| `src/test_omps.cpp` | correctness tests | QEMU correctness，不是性能证据 |
| `src/bench_omps.cpp` | bench harness 和 synthetic input | component / production-shaped diagnostic，不是真实 public entry |
| `script/generate_omps_board_evidence_manifest.py` | 从 repeated board logs 生成 summary / manifest | topic-local wrapper，补 case label 和 evidence role metadata |
| `Makefile` | 构建、QEMU、asm、board evidence target | 复用 `test-rvv/mk/rvv-topic.mk` |
| `board.mk` | 板卡端运行片段 | 复用 `test-rvv/mk/rvv-board-run.mk` |

## Helper 关系

`assembleRegionBoundariesRVV` 是 Phase 010 的 production-shaped helper。它按 region 循环调用 `gatherBoundaryCloudRVV`，在 `project_points=true` 时再调用 `projectBoundaryFromViewpointRVV`。这种结构刻意保留 per-region 临时 cloud 和输出消费成本，用来验证 Phase 000 局部 projection positive 是否能穿透更接近 production 的边界。

## 拆分审计

当前 topic 已使用 `src/`、`include/`、`include/impl/` 和 topic-local `script/`。单个内部头仍承载 reference、candidate、summary 和 production-shaped helper，但总量可审查，且没有旧 `test_support/` legacy alias。若未来重开并加入 production direct 或泛型点型扩展，应把 fixtures、references、candidates 和 bench harness 继续拆分。
