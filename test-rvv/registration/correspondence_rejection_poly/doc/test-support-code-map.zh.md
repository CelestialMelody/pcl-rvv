# 测试支撑代码地图

## 本文职责

本文说明 test-rvv（RVV 测试资产）中的 source layout（源码布局）、aggregator（聚合入口）、internal helper（内部 helper）、script（脚本）、output summary（输出摘要）和 production（生产源码）对照。它让 reviewer 能从文档跳到 helper、test、bench、manifest 和 Evidence Doctor（证据体检）路径。

## 总调用图

```text
README.zh.md
  -> doc/correspondence_rejection_poly-evaluation.zh.md
       -> production getRemainingCorrespondences / thresholdPolygon / thresholdEdgeLength
       -> src/test_correspondence_rejection_poly.cpp
       -> src/bench_correspondence_rejection_poly.cpp
  -> doc/benchmark-and-evidence.zh.md
       -> script/generate_crpoly_evidence_manifest.py
       -> script/summarize_crpoly_board_repeated.py
       -> log/qemu/*/evidence_doctor.md
       -> log/board/*/summary.md
  -> doc/phases/README.zh.md
       -> doc/phases/*/{plan,result}.zh.md
       -> doc/phases/optimization-matrix.zh.md
```

测试源码只包含 topic-local include 路径。production 源码当前没有本 topic diff，保持仓库标量实现。Phase 050 回滚前曾临时保留公开入口先尝试 `getRemainingCorrespondencesRVV`、失败时回到 `getRemainingCorrespondencesStandard` 的生产探针；该 replay 的 board 结果为 negative，用户已确认回滚。

## 目录形态

| 路径 | 角色 | 状态 |
| --- | --- | --- |
| `include/correspondence_rejection_poly.h` | 聚合入口 | adopted |
| `include/impl/correspondence_rejection_poly_candidates.hpp` | reference、fixtures、candidate、production-shaped gather diagnostic、checksum 轻量 helper | adopted |
| `src/test_correspondence_rejection_poly.cpp` | gtest correctness | adopted |
| `src/bench_correspondence_rejection_poly.cpp` | bench wrapper 和 case registry | adopted |
| `script/generate_crpoly_evidence_manifest.py` | topic-local Evidence Doctor manifest wrapper | adopted |
| `script/summarize_crpoly_board_repeated.py` | topic-local repeated board summary / manifest generator | adopted；production-direct case 会按当前 production diff 写证据边界 |
| `doc/phases/` | phase plan/result 和 optimization matrix | adopted |
| `log/evidence_registry.json` | evidence freshness registry | adopted |
| `test_support/` | legacy directory | not_present |

## 稳定聚合入口

`include/correspondence_rejection_poly.h` 是当前 topic 的稳定测试入口。test 和 bench 只 include 这个聚合头，再由它包含 `include/impl/correspondence_rejection_poly_candidates.hpp`。这样 reviewer 可以先看聚合头确认边界，再进入内部 helper。

当前不存在 `test_support/` legacy directory（旧测试支撑目录），也没有 compatibility alias（兼容别名）。因此没有需要保留的旧路径指针。

## Helper 职责

| helper family | 职责 | 调用者 |
| --- | --- | --- |
| `make_source_cloud`、`make_target_cloud_from_source`、`make_identity_correspondences` | fixture（测试夹具）构造 | gtest、full-entry bench |
| `make_edge_pairs`、`EdgePair` | production-shaped gather diagnostic 的 edge pair 输入构造 | gtest、edge-gather bench |
| `threshold_*_reference`、`remaining_correspondences_reference` | 标量参考链路 | gtest public-entry smoke |
| `compute_acceptance_rates_candidate` | accept rate RVV candidate | gtest、acceptance bench |
| `filter_by_acceptance_rate_candidate` | final filter RVV mask + scalar append | gtest、acceptance bench |
| `edge_similarity_batch_candidate` | edge similarity RVV candidate | gtest、edge bench |
| `edge_similarity_gather_reference`、`edge_similarity_gather_candidate` | correspondence index 读点、squared distance staging 和 RVV edge formula 诊断 | gtest、edge-gather bench |
| `checksum_*` | bench checksum | bench wrapper |
| `run_production_direct` | production-direct bench 输入构造 | `production-direct` case；target 可直接运行，证据角色由当前 production diff 决定 |

## Scripts 与 Evidence Output

| script / output | 输入 | 输出 | 证据角色 |
| --- | --- | --- | --- |
| `script/generate_crpoly_evidence_manifest.py` | QEMU bench compare log、asm summary、case metadata | `log/qemu/*/evidence_manifest.json` | Evidence Doctor 输入 |
| `script/summarize_crpoly_board_repeated.py` | board repeated run 目录、case-filter、binary hash 和环境字段 | `summary.md`、`evidence_manifest.json` | board repeated summary 和 doctor 输入 |
| `log/qemu/*/evidence_doctor.md` | QEMU manifest | Errors / Warnings / Suggestions | log-shape 和 manifest 合同检查 |
| `log/board/*/summary.md` | repeated board raw logs | speedup、decision bucket、checksum | 目标硬件性能摘要 |
| `log/board/*/evidence_doctor.md` | board manifest / summary | Errors / Warnings / Suggestions | 生产采用或诊断结论的异常检查 |
| `log/evidence_registry.json` | summary / doctor / asm / correctness 文件状态 | hash、run label、doc refs | evidence freshness（证据新鲜度） |

## Production 与 Test Support 边界

| 层级 | 当前状态 | 边界 |
| --- | --- | --- |
| production public entry | 当前仓库标量 `getRemainingCorrespondences` | production patch 已按用户确认回滚；无本 topic production diff |
| Phase 050 temporary production probe | 回滚前 `getRemainingCorrespondences` -> RVV helper -> Standard fallback | 只作为 negative replay evidence 保留 |
| historical production probe | Phase 030 的 `Standard` / `RVV` helper 分层和负向 board 结果 | 作为历史 evidence 保留，不能替代当前 replay |
| test-only reference | `remaining_correspondences_reference` 等 helper | 用于 correctness 对拍，不能混入 production |
| test-only RVV candidate | edge / gather / acceptance helpers | 用于诊断和 bench，不代表 production dispatch |
| bench wrapper | case-filter 和 checksum 输出 | 计时边界由 `benchmark-and-evidence.zh.md` 解释 |

## 拆分审计

内部 helper 584 行，低于 800 行软阈值。它同时包含 fixtures、reference、candidate、production-shaped gather diagnostic 和 checksum，已接近多职责拆分阈值；当前仍可审查，因为职责集中在一个 topic-local helper 且有本 code map 定位。`src/test_*.cpp` 和 `src/bench_*.cpp` 已位于 `artifact_layout.source_subdir` 解析出的 `src/` 下；聚合入口和 `include/impl` 内部目录也已采用配置布局。

Phase 030 已完成历史 production-direct 探针；Phase 050 只恢复既有 production 分层并完成 replay，随后按用户确认回滚，没有扩大到新的 helper 或 row-source candidate。因此拆分不是当前验证阶段的未阻塞必要项。若后续另开 profile / ablation phase，或新增 production-direct helper，再把该 header 拆成 `fixtures`、`references`、`candidates` 和 `bench_harness` 职责头文件。

## 拆分触发条件

后续任一条件成立时，应创建新的 test-support split phase（测试支撑拆分阶段）：

- `include/impl/correspondence_rejection_poly_candidates.hpp` 超过 800 行软阈值，或接近 1000 行硬阈值。
- 新增 profile / component ablation helper 后，fixtures、reference、candidate、bench harness 和 script glue 混合超过三类职责。
- 新增 production-direct candidate，需要把 test-only reference 与 production-shaped helper 分开审查。
- reviewer 无法从本 code map 定位失败 case 对应的 helper、bench case 和 output summary。
