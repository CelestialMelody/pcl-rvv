# 测试支撑代码地图

| 文件 | 职责 | 说明 |
| --- | --- | --- |
| `include/occlusion_reasoning.h` | aggregator header（聚合头） | test / bench 的稳定 include 入口，统一暴露 test helper |
| `include/impl/occlusion_reasoning_reference.hpp` | reference path（标量参考链路） | 复刻 `filter()` 的 indices 语义和 checksum helper；不碰 production dispatch |
| `include/impl/occlusion_reasoning_candidates.hpp` | candidate path（RVV 候选链路） | 生产形态诊断 helper，命中 RVV 时记录 path hook 和 diagnostics checksum |
| `recognition/include/pcl/recognition/hv/occlusion_reasoning.h` | production public inline implementation（生产公开内联实现） | 当前 adopted production path；public `filter()` / `getOccludedCloud()` 的真实 dispatch / fallback、test hook 和 RVV helper 都在这里 |
| `recognition/include/pcl/recognition/impl/hv/occlusion_reasoning.hpp` | production implementation（生产实现） | 当前 adopted production path；`filter(model, indices)` 的真实 dispatch / fallback、depth map 修复和 RVV helper 都在这里 |
| `src/test_occlusion_reasoning.cpp` | correctness tests | gtest 字典，覆盖 reference、path-hit、矩形 depth map 回归和 production direct |
| `src/bench_occlusion_reasoning.cpp` | bench harness | 生产直连 bench 入口，打印 Dataset / Iterations / checksum / vector_chunks |
| `src/bench_occlusion_reasoning_public_inline.cpp` | bench harness | public inline bench 入口，打印 Dataset / Iterations / checksum / vector_chunks |
| `script/generate_occlusion_reasoning_evidence_manifest.py` | topic-local analysis script | 把 repeated board logs 转成 summary、manifest 和 Evidence Doctor 输入 |
| `Makefile` / `board.mk` | run harness | 组织 `run_test_compare`、`check_inline_filter_rvv_asm`、`check_occlusion_filter_rvv_asm`、`inline_board_repeated`、`board_repeated` 和 registry 刷新 |

这些测试支撑文件不被 production 源码包含；它们证明的是 path-hit、正确性、bench 边界和
证据可追踪性。production dispatch 已接入与否，要看 `run_test_compare`、board repeated、
asm 和 Evidence Doctor 的组合证据，不靠 support map 自己单独下结论。
