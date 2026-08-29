# Test Support Code Map

| 符号 / 文件 | 层级 | 作用 | 证据角色 |
| --- | --- | --- | --- |
| `include/gc.h` | aggregation header | 统一导入 topic-local helper | test entry |
| `include/impl/gc_candidates.hpp` | diagnostic helper | 标量参考和 RVV 候选 | correctness / diagnostic |
| `src/test_gc.cpp` | correctness target | gtest 对拍 | correctness gate |
| `src/bench_gc.cpp` | bench target | batch predicate benchmark | diagnostic bench |
| `script/generate_gc_evidence_manifest.py` | analysis script | summary / manifest / doctor | evidence metadata |

