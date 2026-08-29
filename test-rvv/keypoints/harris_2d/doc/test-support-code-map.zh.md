# harris_2d Test Support Code Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `include/harris_2d.h` | test support aggregator | 稳定聚合入口 | test / bench | candidate header | recovery pointer | `test-rvv/keypoints/harris_2d/include/harris_2d.h` |
| `ResponseImage` | test support core type | synthetic organized image 数据容器 | tests / bench | scalar、candidate、public wrappers | input shape | `include/impl/harris_2d_candidates.hpp` |
| `computeResponsesScalar()` | diagnostic reference | 复刻 production response map 标量语义 | tests / bench | derivative and second-moment helpers | correctness reference | `include/impl/harris_2d_candidates.hpp` |
| `computeResponsesCandidate()` | candidate helper | RVV build 目标候选，Std build fallback | tests / bench RVV build | scalar or RVV helpers | diagnostic candidate | `include/impl/harris_2d_candidates.hpp` |
| `computeResponsesPublic()` | production direct wrapper | 调用真实 public `compute()` | gtest | public output compare | production direct correctness | `include/impl/harris_2d_candidates.hpp` |
| `responseRVV()` | production RVV helper | dense RVV response map 和 direct intensity stride store | `responseHarris/Noble/Lowe/Tomasi()` | public output / NMS branch | adopted production path | `keypoints/include/pcl/keypoints/impl/harris_2d.hpp` |
| `computeSecondMomentMatrix()` | production scalar helper | 标量边界像素二阶矩；已修复多尺寸状态泄漏 | response helpers | formulas | correctness source | `keypoints/include/pcl/keypoints/impl/harris_2d.hpp` |
| `src/test_harris_2d.cpp` | correctness tests | 对拍 scalar reference、candidate 和 public entry | `run_test_compare` | gtest | correctness gate | `test-rvv/keypoints/harris_2d/src/test_harris_2d.cpp` |
| `src/bench_harris_2d.cpp` | bench wrapper | 输出 case、timing、checksum、correctness 和 vector_chunks | `run_bench_*` / board targets | analyze scripts | board performance evidence | `test-rvv/keypoints/harris_2d/src/bench_harris_2d.cpp` |
| `generate_harris_2d_evidence_manifest.py` | analysis script | repeated board logs 到 manifest / summary | `record_evidence_state_repeated` | Evidence Doctor / registry | summary producer | `test-rvv/keypoints/harris_2d/script/generate_harris_2d_evidence_manifest.py` |
| phase020 output | evidence output | production direct board summary、manifest、doctor | `board_repeated` | evaluation / doc-rvv | current performance truth | `test-rvv/keypoints/harris_2d/log/board/repeated_phase020_direct_intensity_stride_store_public_entry/` |

## 测试支撑形态扫描

当前 topic 采用 `src/`、`include/`、`include/impl/` 和 topic-local `script/` 布局。没有旧 `test_support/` 目录，没有 legacy alias，也没有根目录超长 `test_*.cpp` / `bench_*.cpp`。正式 production 长期文档只保存 adopted 行为，测试资产细节主归属仍在本 code map 和相关 topic-local 文档。
