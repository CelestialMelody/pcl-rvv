# Test Support Code Map

| 路径 | 职责 | 说明 |
| --- | --- | --- |
| `include/range_image_border_extractor.h` | aggregator | topic-local 稳定聚合入口 |
| `include/impl/range_image_border_extractor_score_update.hpp` | reference + candidate | `updateScoresStd`、`updateScoresRVV` 和单点 scalar formula；只服务 test-rvv |
| `include/impl/range_image_border_extractor_range_fixture.hpp` | fixture + production-shaped helper | 构造 synthetic full-finite `RangeImage`，配置 extractor，抽取 production score images 和 checksum |
| `src/test_range_image_border_extractor.cpp` | correctness test | gtest 对拍 score-update 输出 |
| `src/bench_range_image_border_extractor.cpp` | bench harness + bench cases | CLI、synthetic score image、RangeImage component timing、checksum 和 comma-separated case-filter |
| `script/generate_range_image_border_extractor_evidence_manifest.py` | evidence wrapper | repeated board summary、manifest、binary hash、Doctor 输入 |
| `Makefile` | local targets | QEMU test、QEMU smoke、asm、board repeated、Doctor |
| `board.mk` | board-side targets | 远端 test / bench binary 名称和共享板卡运行规则 |
| `doc/` | topic-local docs | evaluation、test overview、bench/evidence、candidate evidence 和 code map |
| `doc/phases/` | phase loop | plan、result、matrix 和默认恢复入口 |

生产源码 `features/include/pcl/features/impl/range_image_border_extractor.hpp` 与 `features/src/range_image_border_extractor.cpp` 当前未修改，只作为标量语义和后续 production-shaped 诊断的 source of truth（事实来源）。
