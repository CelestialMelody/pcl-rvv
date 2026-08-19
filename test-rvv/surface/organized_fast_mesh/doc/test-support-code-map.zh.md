# Test Support Code Map

| 文件 | 作用 | 证据角色 |
| --- | --- | --- |
| `include/organized_fast_mesh.h` | 稳定聚合入口 | 测试入口 |
| `include/impl/ofm_types.hpp` | 小类型、checksum、比较 helper | 证据支撑 |
| `include/impl/ofm_fixtures.hpp` | 合成 organized cloud | 输入构造 |
| `include/impl/ofm_reference.hpp` | 标量参考 | 正确性对拍 |
| `include/impl/ofm_candidates.hpp` | test-only RVV candidate | 诊断候选 |
| `src/test_organized_fast_mesh.cpp` | correctness test | correctness |
| `src/bench_organized_fast_mesh.cpp` | benchmark / diagnostic entry | bench |
