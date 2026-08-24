# color_coding Optimization Evidence

本文把已尝试的 RVV candidate family（候选族）映射到代码路径、测试入口、bench label、板卡证据、反汇编和当前 decision。搜索空间和未来恢复队列仍归 `doc/optimization-roadmap.zh.md`。

## 当前结论摘要

| 状态 | candidate |
| --- | --- |
| adopted diagnostic support | scalar same-chain reference、RVV component helper、production-shaped helper tests。 |
| production patch tested / not adopted | `encodeAverageOfPoints` 和 `encodePoints` RVV 分流已在 phase 070 回滚；`setDefaultColor` RVV 已在 phase 080 回滚。 |
| rejected / rolled back | `encodeAverageOfPoints` RVV production 分流，phase 060 两个规模均负向并触发 Doctor Error。 |
| weak / rolled back | `encodePoints` average-pass RVV，phase 060 只有约 1.03x median 且 large case 有退化 warning。 |
| rejected / rolled back | `setDefaultColor` RVV，phase 070 default-only median 1.0035x、min 0.9945x，Doctor Error 2/5 below 1；phase 080 已完整回滚。 |
| rejected / deferred for production | `decodePoints` RVV production candidate：direct path 仍 weak / unstable，staged-store path 在 phase 050 触发 Evidence Doctor Error。 |
| not_applicable | production long-term `doc-rvv` adopted 文档，因为没有 adopted production behavior。 |

## 优化方式总表

| candidate family | 代码路径 | correctness | bench labels | board evidence | asm evidence | decision / boundary |
| --- | --- | --- | --- | --- | --- | --- |
| scalar same-chain reference | `include/impl/color_coding_support.hpp` reference helpers | `run_test_compare` | Std side of all labels | baseline | scalar | adopted as diagnostic baseline |
| RVV indexed encode average | `sumIndexedColorsRVV` / `sumIndexedPointColorsRVV` | component + production-shaped gtest | `encode_average_leaf*`, `ps_encode_average_leaf*` | median positive；Warnings require separate reporting | `vluxei32`, `vredsum` | partial-production-candidate for average helper |
| RVV indexed encode points average pass | same indexed reduction helpers + scalar diff push | component + production-shaped gtest | `encode_points_leaf*`, `ps_encode_points_leaf*` | positive median；`ps_encode_points_leaf4096` 1/5 degradation Warning | `vluxei32`, `vredsum` | partial-production-candidate with size / fallback caution |
| RVV contiguous decode | decode candidate store path | component + production-shaped gtest | `decode_points_leaf*`, `ps_decode_points_leaf*` | direct path weak / near-threshold；`ps_decode_points_leaf4096` min 0.9116x Warning | strided load/store class | rejected/deferred for production |
| RVV staged-store decode | `decodePointsCandidatePointVectorStagedStore` | production-shaped gtest | `ps_decode_points_staged_leaf*` | leaf257 median 0.9918x；leaf4096 mean 0.9758x；Doctor Errors=2 | staged scratch `vse32` + scalar AoS writeback | rejected with evidence |
| RVV default color fill | default-color candidate | component + production-shaped gtest | `set_default_color_*`, `ps_set_default_color_4096` | positive / weak-positive | `vsse32` | partial-production-candidate for default helper |
| production public encode average | `io/include/pcl/compression/color_coding.h` historical phase 060 patch | production direct + fallback tests | historical `prod_encode_average_leaf257/4096` | negative；Doctor Errors=2 | historical `vluxei32`, `vredsum` | rejected and rolled back from current patch |
| production public encode points average pass | `io/include/pcl/compression/color_coding.h` historical phase 060 patch | production direct + fallback tests | historical `prod_encode_points_leaf257/4096` | weak-positive；large case 1/5 below 1 | historical `vluxei32`, `vredsum` | weak / not recommended; rolled back from current patch |
| production public default color | `io/include/pcl/compression/color_coding.h` historical phase 070 patch | production direct + fallback tests | historical `prod_set_default_color_4096` | median 1.0035x，min 0.9945x；Doctor Error | historical `vsse32` | rejected and rolled back from current patch |
| doc-suite parity | topic-local docs | not_applicable | not_applicable | not_applicable | not_applicable | adopted in phase 040 as reviewability improvement |

## 标量路径与 RVV 路径差异

| production helper | scalar work | RVV candidate work | scalar tail / retained work |
| --- | --- | --- | --- |
| `encodeAverageOfPoints` | indexed gather RGBA、sum R/G/B、divide by leaf size、write average bytes | indexed byte-offset gather + vector reduction | average divide and byte append remain scalar |
| `encodePoints` | first pass average；second pass XOR diff and push bytes | first pass average only | diff `push_back` stays scalar |
| `decodePoints` | read average / diff streams and write output colors | direct AoS store candidate attempted；staged scratch candidate rejected | keep scalar for production |
| `setDefaultColor` | loop over output range and write white RGB | vector store default color tried in phase 070 | phase 080 full rollback；current production path is scalar |

## 代码级证据索引

| artifact | role | evidence use |
| --- | --- | --- |
| `include/impl/color_coding_support.hpp` | reference + RVV candidate helpers | correctness、bench、asm attribution。 |
| `src/test_color_coding.cpp` | gtest correctness gate | component and production-shaped semantic equivalence。 |
| `src/bench_color_coding.cpp` | bench harness and case registry | board repeated summary input。 |
| `script/generate_color_coding_repeat_summary.py` | summary generator | aggregates 5-run board logs。 |
| `script/generate_color_coding_evidence_manifest.py` | Evidence Doctor manifest generator | labels evidence role, wrapper, row source, checksum and B/A values。 |
| `../../script/evidence_doctor.py` | shared Doctor | emits Error / Warning / Suggestion。 |
| `../../script/evidence_registry.py` | shared registry | records current evidence freshness。 |

## 当前 evidence paths

- Correctness: `make -C test-rvv/io/color_coding run_test_compare`
- Board summary: `log/board/component_repeat_5/summary.md`
- Production board summary: `log/board/production_repeat_5/summary.md`
- Manifest: `log/board/component_repeat_5/evidence_manifest.json`
- Production manifest: `log/board/production_repeat_5/evidence_manifest.json`
- Evidence Doctor: `log/board/component_repeat_5/evidence_doctor.md`
- Production Evidence Doctor: `log/board/production_repeat_5/evidence_doctor.md`
- Registry: `log/evidence_registry.json`
- Phase decision: `doc/phases/050-decode-implementation-shape-audit/result.zh.md`
- Phase 070 result: `doc/phases/070-pi5-partial-rollback-default-only/result.zh.md`

## 结论边界和下一步

当前证据已经完成 PI5 部分回滚、default-only production public probe 和 phase 080 完整回滚。真实 production direct 结果不支持任何已实现 RVV 分流 clean adoption：encode average 已因负向回滚，encode points 已因弱正 / 不稳定回滚，default color 在 default-only 复跑中退化为 near-threshold 并触发 Doctor Error 后已回滚。当前不建议继续推进本 topic。
