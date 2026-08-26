# 测试支撑代码地图

本文让 reviewer 从 topic-local 测试资产定位到 production 入口、reference（参考链路）、candidate（候选实现）、bench harness（性能测试驱动）和 evidence script（证据脚本）。

| 文件 / 符号 | 职责 | 调用者 | 证据角色 |
| --- | --- | --- | --- |
| `include/eppd.h` | topic 聚合头，暴露测试专用 reference / candidate 入口 | `src/test_eppd.cpp`、`src/bench_eppd.cpp` | stable include entry |
| `include/impl/eppd_reference.hpp` | 标量 reference，复刻 projection 后 scan、full scan、indexed 和 polygon XOR 语义 | correctness tests、diagnostic bench | scalar baseline / correctness oracle |
| `include/impl/eppd_candidates.hpp` | 测试专用 RVV candidate，包括 early post-projection 和 full-scan diagnostic | diagnostic tests / bench | production-shaped diagnostic；不是 production helper |
| `src/test_eppd.cpp` | gtest 驱动，含测试派生类访问 protected `segmentStd` / `segmentRvv` | `make run_test_compare`、`make run_board_test` | correctness / fallback gate |
| `src/bench_eppd.cpp` | benchmark CLI 和 case 构造，支持 diagnostic / production、dense / indexed、single / nested 和 point types | QEMU smoke、board repeated targets | performance harness |
| `script/generate_eppd_board_evidence_manifest.py` | 解析 repeated run，生成 summary 和 Evidence Doctor manifest | `make run_board_evidence_doctor` | evidence summary producer |
| `Makefile` | 本地 build、test、bench、board repeated 和 evidence doctor target | worker / reviewer | reproducible command map |
| `board.mk` | 板卡端 binary 名、remote 目录和共享 board run fragment | `make deploy_board` / repeated targets | board execution contract |
| production `segmentStd` | 原标量主体，作为 public fallback 和对拍 baseline | public `segment`、test derived class | scalar production baseline |
| production `segmentRvv` | adopted RVV 扫描段实现 | public `segment` | production RVV path |

## 结构审计

当前 topic 已使用 `include/` 聚合头、`include/impl/` 内部头、`src/` 测试 / bench 源和 `script/` 证据脚本，符合 `.agents/config/defaults.yaml` 的测试支撑布局。没有旧 `test_support/` 目录、compatibility alias（兼容别名）或重复 wrapper 需要迁移。
