# SIFT Keypoint Test Support Code Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `include/sift_keypoint.h` | test support aggregator | 稳定聚合入口 | test / bench | topic-local helper | recovery pointer | `test-rvv/keypoints/sift_keypoint/include/sift_keypoint.h` |
| `include/impl/sift_keypoint_scale_space.hpp` | core helper | synthetic fixture、scalar reference、RVV candidate | tests / bench | gtest / bench | diagnostic boundary | `test-rvv/keypoints/sift_keypoint/include/impl/sift_keypoint_scale_space.hpp` |
| `src/test_sift_keypoint.cpp` | correctness tests | helper 对拍和 public smoke | `run_test_compare` | gtest | correctness gate | `test-rvv/keypoints/sift_keypoint/src/test_sift_keypoint.cpp` |
| `src/bench_sift_keypoint.cpp` | bench wrapper | helper compare / timing / checksum | `run_bench_*` | analysis script | performance evidence | `test-rvv/keypoints/sift_keypoint/src/bench_sift_keypoint.cpp` |
| `script/generate_sift_keypoint_evidence_manifest.py` | analysis script | raw board logs -> summary / manifest | `record_evidence_state_repeated` / `record_evidence_state_public` | Evidence Doctor / registry | summary producer | `test-rvv/keypoints/sift_keypoint/script/generate_sift_keypoint_evidence_manifest.py` |
| `script/compare_sift_public_outputs.py` | analysis script | Std/RVV public output trace compare | `compare_public_trace` | phase result / evaluation | public correctness gate | `test-rvv/keypoints/sift_keypoint/script/compare_sift_public_outputs.py` |
| `computeScaleSpace()` RVV branch | production RVV helper inline | 批量计算 Gaussian weight 后交给 scalar tail 累加 | public `SIFTKeypoint::compute()` | DoG matrix / extrema scan | production boundary | `keypoints/include/pcl/keypoints/impl/sift_keypoint.hpp` |
| `repeated_phase010_public_compute/summary.md` | evidence output summary | production-public repeated board 摘要 | `record_evidence_state_public` | evaluation / `doc-rvv` / Handoff | board performance evidence | `test-rvv/keypoints/sift_keypoint/log/board/repeated_phase010_public_compute/summary.md` |
