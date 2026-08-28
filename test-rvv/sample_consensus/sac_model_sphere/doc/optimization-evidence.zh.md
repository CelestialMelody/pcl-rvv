# sac_model_sphere 优化证据

## 当前结论摘要

| 状态 | candidate family | 当前结论 |
| --- | --- | --- |
| adopted/current production behavior | existing count RVV | `countWithinDistance` 已有 RVV production path，Phase 000 复核为 positive-stable。 |
| adopted/current production behavior | production select `vcompress` RVV | Phase 045/046 已把 `vcompress` 接入并采纳为 production patch，`PointXYZ` public entry 5-run board median `2.0989x`。 |
| adopted for `PointXYZI` | point type expansion | Phase 050 `PointXYZI` public `selectWithinDistance` 5-run board median `1.5901x`，select row Doctor clean。 |
| adopted for RGB/RGBA tested point types | point type expansion | Phase 060 `PointXYZRGB` median `1.6225x` 且 5/5 run 正向；`PointXYZRGBA` median `1.5528x`，但 1/5 run 退化且有长尾 warning。 |
| adopted | test support include / impl layout | Phase 055 已把测试支撑迁移到 `include` / `include/impl`，保持测试和 bench 合同不变。 |
| rejected | getDistances RVV squared-distance + scalar sqrt/store | 当前 candidate 5/5 board run 退化，不进入 production。 |
| historical diagnostic | select test-only candidate | 诊断候选已被 production direct 证据取代，只保留为 Phase 000/020 接入前证据。 |
| turn_stop_deferred with stop_condition_hit | further custom point type expansion | 自定义 registered xyz 尚未验证；缺少代表类型和输入边界，不能凭空扩大。 |

## 优化方式总表

| candidate family | 代码路径 | 测试路径 | bench / board evidence | asm evidence | decision | 边界 |
| --- | --- | --- | --- | --- | --- | --- |
| existing count RVV | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_sphere.hpp` / `countWithinDistanceRVV` | `src/test_sac_model_sphere.cpp` | production repeated `PointXYZ` median `3.5154x` | 符号级 `rvv_instr_count=19` | adopted/current production behavior | 只代表 count 回归，不代表 getDistances。 |
| production select `vcompress` RVV | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_sphere.hpp` / `selectWithinDistanceRVV` | `ProductionSelectWithinDistanceMatchesStandardHelper`、`PointXYZRGBAndRGBALayoutsMatchReference` | Phase 045 `PointXYZ` median `2.0989x`；Phase 050 `PointXYZI` median `1.5901x`；Phase 060 RGB `1.6225x` clean、RGBA `1.5528x` with warning | 符号级 `rvv_instr_count=27`，可见 `vcompress.vm` | adopted/current production behavior; RGBA with stability warning | 当前性能覆盖内建代表点型、direct indexed `indices_` 和 registered float xyz layout；不外推到自定义点型或其它规模。 |
| select test-only candidate | `include/impl/sac_model_sphere_access.hpp` | `DiagnosticCandidateMatchesPublicEntries` | Phase 020 manifest median `1.4161x` | inline partial | historical diagnostic | 已被 production direct select 证据取代，不再作为采纳主证据。 |
| getDistances test-only candidate | `include/impl/sac_model_sphere_access.hpp` | `DiagnosticCandidateMatchesPublicEntries` | production repeated median `0.7775x` | inline partial | rejected | 只拒绝当前 scratch + scalar sqrt 形态。 |
| Phase 020 production select baseline | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_sphere.hpp` / historical `selectWithinDistanceRVV` | `ProductionSelectWithinDistanceMatchesStandardHelper` | Phase 020 production repeated median `1.5020x` | historical `rvv_instr_count=17` | superseded baseline | 若用户要求回滚 Phase 045，可回到该实现族。 |
| RVV sqrt/helper getDistances | not_started | not_started | not_started | not_started | deferred | 需要先做数学 helper 语义审计。 |

## 标量路径与 RVV 路径差异

现有 count RVV 直接对 `indices_` 做 indexed gather（按索引离散加载），计算 squared distance（平方距离）并用 shell mask（球壳掩码）计数。Phase 020 采纳的 select RVV 复用同一平方距离计算，把每个 VL chunk（可变向量长度分块）的 squared distances 写入 scratch buffer，再用标量 `sqrt` 和顺序 `push_back` 保留 public entry 的输出顺序。getDistances candidate 也保留标量 `sqrt`，但 dense store 路径增加 scratch 内存流量，当前板卡证据显示这一路径不值得接入 production。

## 细粒度 target 字典

| target / script | 隔离对象 | 证据角色 |
| --- | --- | --- |
| `run_test_compare` | public entries 和 test-only candidates | correctness gate。 |
| `dump_bench_rvv` | RVV bench binary | asm attribution 输入。 |
| `board_smoke` | board test + 单次 bench | board 可运行和输出形状。 |
| `generate_board_evidence_manifest` | 5-run repeated raw logs | Evidence Doctor manifest。 |
| `test-rvv/script/evidence_doctor.py --manifest` | repeated manifest | Error / Warning / Suggestion 检查。 |
| `record_repeated_board_evidence_state` | repeated manifest / doctor | 记录 evidence registry。 |
| `repeated_evidence_status` | registry + doc refs | 检查 evidence freshness。 |
| `collect_production_repeated_board_evidence` | production public select/count/getDistances 和保留 candidate | 接入后 board repeated 输入。 |
| `record_production_board_evidence_state` | production manifest / doctor | 记录接入后 evidence registry。 |
| `production_evidence_status` | registry + doc refs | 检查接入后 evidence freshness。 |

## 结论边界

Phase 045 后 `selectWithinDistance` 的 `vcompress` production patch 已在当前 production scope 内取得正向证据，
Phase 046 已按用户确认升级为 adopted。Phase 050 证明 `PointXYZI` 点型在同一 production public
entry 下仍为 positive。Phase 060 继续证明 RGB/RGBA 内建代表点型 median 正向；本轮重跑中 RGB
5/5 run 正向，RGBA 有 1/5 run 退化和长尾 warning，不能写成 clean all-run stable。`getDistancesToModel`
需要新的 RVV sqrt/helper 或消融证据后才可恢复。
