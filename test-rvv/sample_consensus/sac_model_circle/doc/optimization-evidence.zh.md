# sac_model_circle optimization evidence

## 本文职责

本文记录已经尝试、保留、拒绝或暂缓的 RVV candidate family（候选实现族）及其证据入口。跨阶段搜索空间和恢复条件仍以 `doc/optimization-roadmap.zh.md` 为主归属。

## 当前结论摘要

| 状态 | candidate |
| --- | --- |
| superseded | `selectWithinDistance` RVV gather + `vcompress` + scalar exact error writeback；Phase 000 结果只作为 historical baseline（历史基线）。 |
| adopted | `selectWithinDistance` RVV gather + `vcompress` + full-RVV error tail（完整 RVV 误差尾段）。 |
| adopted | 既有 `countWithinDistanceRVV`，补齐 index / byte offset gate。 |
| rejected with diagnostic evidence | `getDistancesToModel` 的旧 test-only `RVV sqr + scalar sqrt/store` helper。 |
| adopted | `getDistancesToModel` 的 full-RVV production path。 |
| rejected with strict A/B evidence | identity-index strided load。 |
| deferred by scope | 更多 PointXYZ-like 点型 board、`Scalar=double` 和其它 SAC 模型。 |

## 优化方式总表

| candidate family | 代码路径 | 测试路径 | bench / board evidence | asm evidence | decision | 边界 |
| --- | --- | --- | --- | --- | --- | --- |
| select RVV gather + `vcompress` + scalar error tail | historical `impl/sac_model_circle.hpp` 的 `selectWithinDistanceRVV` | `PublicSelectWithinDistanceMatchesDirectRVVForSupportedLayout` | Phase 000 manifest：median 1.6702x，doctor 0/0/0 | helper 内 23 条 RVV 指令 | superseded by Phase 090 | 保留为 historical baseline，不代表当前 production truth。 |
| select RVV gather + `vcompress` + full-RVV error tail | `impl/sac_model_circle.hpp` 的 `selectWithinDistanceRVV` | `PublicSelectWithinDistanceMatchesDirectRVVForSupportedLayout`、`SelectFullRVVErrorTailCandidateMatchesDirectRVV` | Phase 080 strict A/B median 1.6078x；Phase 090 接入后 public median 2.5700x，doctor 0/0/0 | production helper 确认 `vcompress.vm`、`vfsqrt.v`、`vfwcvt.f.f.v`、`vse64.v`，24 条 RVV 指令 | adopted | `PointXYZ` board；`PointXYZI` correctness only；正式数据使用 Phase 090。 |
| existing count RVV | `impl/sac_model_circle.hpp` 的 `countWithinDistanceRVV` | public tests 和 count 对拍 | Phase 000 manifest：median 1.4012x，doctor 0/0/0 | helper 内 15 条 RVV 指令 | adopted | 同 select 的 layout / index / offset gate。 |
| getDistances current test-only helper | `src/test_sac_model_circle.cpp`、`src/bench_sac_model_circle.cpp` | `GetDistancesCandidateMatchesPublicPath` | Phase 020 manifest：B/A median 0.6590x，doctor Error=1 / Warning=1 | helper 内 13 条 RVV 指令 | rejected with diagnostic evidence | 只拒绝当前 helper 形态；不是泛化的 no-production。 |
| getDistances full-RVV test-only helper | `include/impl/sac_model_circle_candidates.hpp`、`src/bench_sac_model_circle.cpp` | `GetDistancesFullRVVCandidateMatchesPublicPath` | Phase 050 manifest：B/A median 1.4745x，doctor 0/0/0 | `getDistancesToModelFullRVV` 内确认 `vfsqrt.v`、`vfwcvt.f.f.v`、`vse64.v` | production probe entered | 只支持进入 production integration loop，不直接采纳。 |
| getDistances full-RVV production | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle.hpp` | `PublicGetDistancesMatchesStandardAndDirectRVV` | Phase 060 manifest：B/A median 1.4737x，doctor 0/0/0 | `getDistancesToModelRVV` 内确认 `vfsqrt.v`、`vfwcvt.f.f.v`、`vse64.v` | adopted | 使用接入后的板卡数据作为正式采用依据。 |
| identity-index strided load | historical candidate only；当前 production 已撤回该分支 | `identity_evidence_status` 只检查已有 summary evidence；`check_identity_strided_asm` 需显式历史探针开关 | Phase 040 manifest：select median 0.9914x，count median 0.9698x，doctor Errors=2 | 候选二进制曾出现 `vlse32.v`；当前 production 不要求出现 | rejected with strict A/B evidence | 恒等索引场景本身负向，当前不再跑 shuffled non-regression。 |

## 标量路径与 RVV 路径差异

select/count 的 RVV 路径把离散 x/y load、平方距离和 shell 判断放到 RVV lane（向量通道）中执行。Phase 000 的 select 旧实现命中后仍用标量 `sqrt` 生成 `error_sqr_dists_`；Phase 080/090 已把这段压缩后的误差写回改成 RVV `vfsqrt`、abs、`vfwcvt` 和 `vse64`，因此当前 select production 不再有逐 active lane 标量 `sqrt` tail。

getDistances 旧候选只向量化平方距离前半段，但每个点都必须输出 `double` 距离，且候选需要临时 float buffer 和逐 lane `sqrt`。Phase 020 说明这一路组织方式的端到端成本超过 public row。Phase 050 / 060 的 full-RVV 路径把 `sqrt` 和 double 写回移入 RVV chunk，当前接入后板卡证据为正向。

## 代码级证据索引

| 对象 | 位置 | 证据角色 |
| --- | --- | --- |
| `selectWithinDistanceRVV` | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle.hpp` | Phase 090 接入、当前采纳的 production RVV helper。 |
| `countWithinDistanceRVV` | 同上 | production direct retained path。 |
| `getDistancesToModelCandidateRVV` | `src/bench_sac_model_circle.cpp` | production-shaped diagnostic，反汇编归属通过 `noinline` 稳定。 |
| `getDistancesToModelFullRVV` | `include/impl/sac_model_circle_candidates.hpp` | Phase 050 production-shaped diagnostic。 |
| `getDistancesToModelRVV` | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle.hpp` | Phase 060 接入、Phase 070 采纳的 production RVV helper。 |
| manifest generator | `script/generate_circle_board_evidence_manifest.py` | evidence manifest 生成和 case label 字典。 |
| evidence registry | `log/evidence_registry.json` | summary evidence freshness（证据新鲜度）登记。 |

## 结论边界

Phase 000 的 production-public Std/RVV positive（公开入口标量 / RVV 正向）证明当时 public RVV path 快于当时 public scalar path。Phase 080 又把问题转为 RVV-family-selection（RVV 实现族选择），用同一 RVV binary 内的 public adopted baseline 和 test-only full-RVV error tail candidate 做 strict A/B；Phase 090 接入后再用 public Std/RVV production direct 证据确认当前生产路径仍正向。因此正式采用结论以 Phase 090 接入后数据为准。

Phase 040 的 identity 候选已经变成 RVV-family-selection（RVV 实现族选择）问题，因此必须看同一 production boundary 内的 RVV-vs-RVV A/B。该 A/B 为负，所以不能 clean-adopt identity fast path；当前源码保持 gather-only RVV。

Phase 020 的 diagnostic negative（诊断负向）不能直接拒绝未来不同实现族。Phase 050 已证明 full-RVV 新实现族在 diagnostic boundary 下正向，Phase 060 已证明接入 production 后仍正向，Phase 070 已采纳该生产路径。Phase 090 又采纳了 select full-RVV error tail。当前 topic 内没有新的同边界高优先级性能候选。
