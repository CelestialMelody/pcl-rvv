# Test Support Code Map

| 文件 | 作用 |
| --- | --- |
| `include/hough_3d.h` | 聚合入口。 |
| `include/impl/hough_3d_candidates.hpp` | 标量参考和 RVV 候选 helper。 |
| `src/test_hough_3d.cpp` | correctness 对拍。 |
| `src/bench_hough_3d.cpp` | production direct bench 入口。 |
| `script/generate_hough_3d_evidence_manifest.py` | 把 board summary 转成 manifest / doctor 输入。 |
