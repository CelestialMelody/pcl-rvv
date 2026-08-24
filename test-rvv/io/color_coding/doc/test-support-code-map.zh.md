# color_coding Test Support Code Map

本文定位 color_coding topic 的测试支撑代码、bench harness、脚本和证据输出。它不承担性能结论；性能与 Evidence Doctor 见 `doc/benchmark-and-evidence.zh.md`。

## 总调用图

```text
io/include/pcl/compression/color_coding.h  (production scalar reference after phase 080 full rollback)
        |
        v
test-rvv/io/color_coding/include/color_coding.h
        |
        v
include/impl/color_coding_support.hpp
  |-- reference helpers
  |-- RVV candidate helpers under __RVV10__
  |-- point-vector production-shaped helpers
        |
        +--> src/test_color_coding.cpp  -> run_test_compare
        |
        +--> src/bench_color_coding.cpp -> run_bench_* / board targets
                                              |
                                              v
        script/generate_color_coding_repeat_summary.py
        script/generate_color_coding_evidence_manifest.py
        ../../script/evidence_doctor.py
        ../../script/evidence_registry.py
```

## 稳定聚合入口

| artifact | role | callers |
| --- | --- | --- |
| `include/color_coding.h` | topic-local aggregator（聚合头） | test and bench sources |
| `include/impl/color_coding_support.hpp` | internal helper header（内部支撑头） | aggregator only |
| `Makefile` | target registry（目标入口登记） | worker / reviewer commands |
| `board.mk` | board remote parameters（板卡远端参数） | shared board runner |

当前布局已经使用配置默认的 `include/` + `include/impl/` + `src/` 结构。没有旧 `test_support/` 目录，也没有 legacy compatibility alias（兼容旧入口）。

## Fixtures 与输入构造

| helper / source | 输入 | 用途 |
| --- | --- | --- |
| `makeCloud()` in test | 5 个 `ColorPoint`，手写 RGBA | gtest 手算期望值。 |
| `makeCloud(count)` in bench | synthetic `ColorPoint` vector | component benchmark。 |
| `makePclCloud(count)` in bench | synthetic `pcl::PointXYZRGBA` vector | production-shaped benchmark；phase 080 后没有 current production direct label。 |
| `makeLeafIndices(cloud_size, leaf_size)` | `(i * 37 + 11) % cloud_size` | indexed leaf gather。 |
| `EncodedColorData` | average / differential byte vectors | 模拟 color coder byte streams。 |

## Reference helpers

| helper | production relation | evidence role |
| --- | --- | --- |
| `encodeAverageOfPointsReference` | 复刻 `ColorCoding::encodeAverageOfPoints` 的 byte average 语义 | scalar baseline。 |
| `encodePointsReference` | 复刻 average + differential stream 语义 | scalar baseline。 |
| `decodePointsReference` | 复刻 decode stream 到 RGBA 输出 | scalar baseline。 |
| `setDefaultColorReference` | 复刻 default white RGB 写入 | scalar baseline。 |
| `*ReferencePointVector` | 同一语义作用于 `std::vector<PointT>` | production-shaped diagnostic baseline。 |

这些 helper 不是 production code；它们只为 test / bench 提供 same-chain reference。

## Candidate helpers

| helper | RVV shape | fallback / current state |
| --- | --- | --- |
| `sumIndexedColorsRVV` | `vluxei32` indexed gather + `vredsum` RGB reduction | only under `__RVV10__`; otherwise scalar candidate wrappers return scalar reference. |
| `sumIndexedPointColorsRVV` | 使用 `sizeof(PointT)` 和 RGBA offset 生成 byte offsets | production-shaped diagnostic only。 |
| `encodeAverageOfPointsCandidate*` | RVV sum + scalar average byte append | partial-production-candidate。 |
| `encodePointsCandidate*` | RVV average pass + scalar diff push | partial-production-candidate with leaf4096 warning。 |
| `decodePointsCandidate*` | direct AoS output candidate | production candidate rejected/deferred；phase 050 direct path 仍 weak / unstable。 |
| `decodePointsCandidatePointVectorStagedStore` | RVV 先连续写 scratch `uint32_t`，再标量写回 AoS | phase 050 rejected with evidence；staged labels 触发 Doctor Errors。 |
| `setDefaultColorCandidate*` | vector store default color | production-shaped diagnostic positive, but phase 070 production-public default-only evidence is not recommended for adoption。 |

## Bench Harness 与 Case Registry

`src/bench_color_coding.cpp` owns（拥有）CLI 参数、case-filter、checksum、timing boundary 和 label registry（标签登记）。phase 080 后 `--case-filter production` 不再选择任何 current production case；`prod_set_default_color_4096` 只作为 phase 070 historical evidence 出现在文档和已登记 summary 中。`runCase()` 负责 warmup、timed iterations、batch repeats 和输出格式。完整 label 字典见 `doc/benchmark-and-evidence.zh.md`。

## Scripts 与 Evidence Output

| script / output | role |
| --- | --- |
| `script/generate_color_coding_repeat_summary.py` | 从 `run*/run_bench_{std,rvv}.log` 生成 repeated summary。 |
| `script/generate_color_coding_evidence_manifest.py` | 为 Evidence Doctor 增加 evidence role、row source、wrapper、checksum 和 threshold metadata。 |
| `log/board/component_repeat_5/summary.md` | 当前 board summary。 |
| `log/board/component_repeat_5/evidence_manifest.json` | 当前 Doctor input。 |
| `log/board/component_repeat_5/evidence_doctor.md` | 当前 Doctor report。 |
| `log/board/production_repeat_5/summary.md` | 当前 phase 070 default-only production summary。 |
| `log/board/production_repeat_5/evidence_manifest.json` | 当前 phase 070 production Doctor input。 |
| `log/board/production_repeat_5/evidence_doctor.md` | 当前 phase 070 production Doctor report。 |
| `log/evidence_registry.json` | 当前 evidence freshness registry。 |

## Production 与 Test Support 边界

`io/include/pcl/compression/color_coding.h` 当前没有 production RVV dispatch（生产分流）：phase 080 已完整回滚 encode average、encode points 和 default color 的生产 RVV path，`decodePoints` 一直保持标量。test support 中的 component / production-shaped candidate helper 仍是 diagnostic 或 production-shaped diagnostic。`pcl::PointXYZRGBA` helper 只证明字段 offset 和 AoS stride 能被 candidate 处理，不证明 full `OctreePointCloudCompression` public entry、entropy coder、fallback 或 dispatch。

## 拆分审计

| area | current shape | decision | next action |
| --- | --- | --- | --- |
| source layout | `src/test_color_coding.cpp` 和 `src/bench_color_coding.cpp` 已拆分 | adopted | 保持。 |
| aggregator/internal | `include/color_coding.h` + `include/impl/color_coding_support.hpp` | adopted | 若生产 patch 后 helper 激增，再按职责拆 internal header。 |
| scripts | summary / manifest 两个 topic-local script | adopted | 保持 Evidence Doctor metadata 更新。 |
| legacy aliases | 无旧入口或兼容 wrapper | not_applicable with evidence | 无动作。 |
| production direct test support | 已有真实 `ColorCoding` public method tests；phase 080 已删除 default-only hook 断言。 | adopted | 保持纯标量语义测试；未来只有新 production patch 才重新添加 direct dispatch 证据。 |
