# point_coding 测试支撑代码地图

## 文件职责

| 文件 | 职责 | 主要符号 / target | 证据角色 |
| --- | --- | --- | --- |
| `Makefile` | 构建 Std/RVV test 和 bench，定义 QEMU smoke、board repeated、manifest 和 Evidence Doctor target。 | `run_test_compare`、`run_qemu_bench_smoke`、`collect_board_repeated`、`run_board_repeated_evidence_doctor` | harness（测试运行框架）入口。 |
| `board.mk` | 板卡侧 binary 名、远端目录和 bench 参数。 | `REMOTE_BENCH_STD`、`REMOTE_BENCH_RVV`、`REMOTE_TEST` | board evidence 配置。 |
| `include/point_coding.h` | 稳定聚合头。 | 包含 `impl/point_coding_support.hpp` | test aggregator（测试聚合入口）。 |
| `include/impl/point_coding_support.hpp` | 输入构造、reference、candidate、checksum、production-shaped decode context helper 和 public roundtrip feasibility helper。 | `encodePointsScalar`、`encodePointsRVV`、`decodePointsRVV`、`decodePointsCandidateToCloud`、`decodePointsProductionObject`、`decodePointsCandidateMultiLeafToCloud`、`decodePointsProductionObjectMultiLeaf`、`runPublicOctreeRoundtrip` | correctness baseline、RVV candidate、production-shaped diagnostic 和 public API feasibility smoke。 |
| `src/test_point_coding.cpp` | gtest correctness。 | 3 个 `PointCodingComponentAblation` TEST，2 个 `PointCodingProductionShapedScout` TEST，5 个 `PointCodingProductionDirect` TEST，1 个 `PointCodingPublicRoundtripFeasibility` TEST | correctness evidence、traits preservation、fallback evidence 和 public API feasibility evidence。 |
| `src/bench_point_coding.cpp` | synthetic component bench、decode context scout、multi-leaf context scout 和 production-direct bench。 | `encode_indexed_*`、`decode_contiguous_*`、`decode_context_*`、`decode_multileaf_*`、`decode_production_direct_*`、`decode_production_direct_traits_*` | board diagnostic / production-direct performance。 |
| `script/generate_point_coding_evidence_manifest.py` | topic-local manifest wrapper。 | `CASE_METADATA`、`collect_log_pairs` | Evidence Doctor 输入。 |

## 结构审计

| area | current shape scan | config / quality bar | decision | evidence | next action |
| --- | --- | --- | --- | --- | --- |
| test / bench source layout | 使用 `src/test_point_coding.cpp` 和 `src/bench_point_coding.cpp`，已包含 Phase 080 public decode / roundtrip bench labels。 | `artifact_layout` 期望 source subdir。 | adopted | 当前已符合。 | 无同边界未阻塞动作。 |
| aggregator and internal helpers | 使用 `include/point_coding.h` 和 `include/impl/point_coding_support.hpp`。 | `test_support.aggregator_directory=include`，`internal_directory=include/impl`。 | adopted | 当前已符合。 | 若 helper 超过职责阈值，可拆 `fixtures` / `candidates`。 |
| script and bench registry | 有 topic-local manifest wrapper，bench case 字典在脚本中。 | topic-specific parser 放 `script/`。 | adopted | Evidence Doctor 已能读取 manifest。 | 后续可补 `evidence_registry.json`。 |
| target granularity | 有 aggregate correctness、bench case-filter、QEMU smoke、board repeated、doctor target。 | doc-suite quality bar 要求 target 粒度可审查。 | adopted | `doc/testing-overview.zh.md` 已列 target 审计；`decode_context_*`、`decode_multileaf_*`、`decode_production_direct_*`、`decode_production_direct_traits_*`、`octree_*` 可由 case-filter 隔离。 | 无同边界未阻塞动作。 |
| legacy compatibility | 无旧 `test_support/` 或 compatibility alias。 | 默认不保留 legacy pointer。 | not_applicable with evidence | 新 topic 无旧路径。 | 无。 |

## Helper 调用关系

```text
test / bench
  -> include/point_coding.h
    -> include/impl/point_coding_support.hpp
      -> makePointCloud / makeEncodedDiffs
      -> encodePointsScalar / decodePointsScalar
      -> decodePointsProductionObject / decodePointsCandidateToCloud
      -> runPublicOctreeRoundtrip
      -> encodePointsCandidate / decodePointsCandidate
        -> __RVV10__ ? encodePointsRVV / decodePointsRVV : scalar reference
```

当前 helper 同时承担 fixtures（测试输入）、references（标量参考）、candidates（候选实现）、assertions/checksum（校验辅助）和 bench-facing helper（性能入口辅助）职责，但每类职责仍有明确函数边界。Phase 080 已加入 public end-to-end bench 支撑；当前不再建议仅为同类 synthetic public runs 拆分更多内部头。若未来基于真实 workload / profile 重开，再按职责拆分 fixtures、public stream 构造和 bench harness。
