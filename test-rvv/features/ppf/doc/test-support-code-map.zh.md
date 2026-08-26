# PPF Test Support Code Map

本文说明 `test-rvv/features/ppf` 的测试支撑代码职责。当前布局已经使用配置解析出的
`src/`、`include/` 和 `include/impl/`，没有旧 `test_support/` 目录需要迁移。

## 文件地图

| 文件 | 职责 | 调用者 | 证据角色 |
| --- | --- | --- | --- |
| `include/ppf.h` | 聚合头文件，统一引入 topic-local reference 和 candidate helper。 | `src/test_ppf.cpp`、`src/bench_ppf.cpp` | reviewer 入口；不证明 production dispatch。 |
| `include/impl/ppf_reference.hpp` | 构造 synthetic input、indices、reference output 和 assertion helpers。 | correctness tests、bench baseline | scalar oracle（标量判定参考）。 |
| `include/impl/ppf_pair_batch_candidate.hpp` | Phase 010 SoA-staged pair-feature RVV candidate。 | `PPFCandidate.*` tests、bench pair-feature case | rejected component candidate。 |
| `include/impl/ppf_alpha_candidate.hpp` | Phase 020 closed-form helper 和 Phase 030 alpha batch RVV candidate。 | alpha correctness test、bench alpha case | diagnostic positive candidate。 |
| `src/test_ppf.cpp` | gtest aggregate，覆盖 reference、candidate、production direct 和 fallback。 | `run_test_*` targets | correctness / fallback gate。 |
| `src/bench_ppf.cpp` | bench harness（性能测试驱动）和 case-filter registry。 | `run_bench_*`、board repeated targets | diagnostic + production-public timing source。 |
| `script/generate_ppf_evidence_manifest.py` | 从 repeated board summary 生成 topic-local Evidence Doctor manifest。 | `evidence_manifest_repeated` | summary-only evidence bridge（摘要证据桥接）。 |
| `Makefile` | topic build、QEMU、board repeated 和 Doctor target。 | worker / reviewer | executable target registry（可执行入口登记）。 |
| `board.mk` | board-side binary names 和远端运行片段。 | `deploy_files` / board targets | board runtime boundary（板卡运行边界）。 |

## 拆分审计

| area | 当前 shape | decision | 理由 |
| --- | --- | --- | --- |
| source layout | tests / bench 在 `src/` 下 | adopted | 符合 `artifact_layout.source_subdir`。 |
| aggregator header | `include/ppf.h` | adopted | 聚合入口短小稳定。 |
| internal helper layout | `include/impl/*.hpp` | adopted | reference、alpha candidate、pair-feature candidate 已按职责拆分。 |
| line count | 最大 helper `272` 行，`src/test_ppf.cpp` `317` 行 | adopted | 均低于 hard line limit；职责边界清楚。 |
| compatibility alias | 无旧 alias / pointer | adopted | 没有旧 `test_support/` 目录或兼容入口要保留。 |
| script locality | `script/generate_ppf_evidence_manifest.py` | adopted | 依赖 PPF case label，放 topic-local script 合适。 |

## 生产源码映射

生产入口是 `features/include/pcl/features/impl/ppf.hpp`：

- `computePPFFeatureStd` 保存标量 fallback（回退路径）。
- `computePPFFeatureAlphaMRVV` 是 traits-gated RVV helper。
- `PPFEstimation::computeFeature` 在 `__RVV10__` 下先尝试 RVV helper，失败后回到 Std helper。

测试代码通过 trace symbol `pcl_rvv_ppf_alpha_m_trace_hits` 证明 public `compute()` 是否真实命中 RVV
helper。该 trace 只在测试翻译单元启用，不进入普通 production build。
