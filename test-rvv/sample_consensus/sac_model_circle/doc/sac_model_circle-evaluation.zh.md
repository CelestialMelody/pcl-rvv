# sac_model_circle 函数级评估

## 范围和目标源码

本 topic 覆盖 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle.hpp` 中 `SampleConsensusModelCircle2D<PointT>` 的 2D 圆距离函数族：

- `selectWithinDistance`：筛选满足圆半径 shell（半径带状区间）的 inliers，并写回 `error_sqr_dists_`；Phase 090 已把误差写回尾段接入 full-RVV production（完整 RVV 生产路径）。
- `countWithinDistance`：只统计满足同一 shell 条件的点数。
- `getDistancesToModel`：为 `indices_` 中每个点输出到圆的距离；Phase 060 已接入 full-RVV production（完整 RVV 生产路径），Phase 070 已按用户确认采纳。

当前 production patch 修改 `sac_model_circle.h` 和 `impl/sac_model_circle.hpp`。测试、bench、manifest 和文档只位于 `test-rvv/sample_consensus/sac_model_circle/`。

## 函数族作用速览

| 函数 | 标量职责 | 当前 RVV 状态 |
| --- | --- | --- |
| `selectWithinDistance` | 逐 `indices_` 读取 x/y，计算平方距离是否落在 `[max(r-threshold,0)^2, (r+threshold)^2]`，命中时按原顺序写 inlier 和精确距离误差。 | Phase 090 已采纳 RVV production path：gather（离散加载）、mask（掩码）、`vcompress` 写 inlier，并在压缩后的 active lanes（有效向量通道）上用 `vfsqrt + vfwcvt + vse64` 写误差。 |
| `countWithinDistance` | 同一 shell 条件，只做计数。 | 既有 RVV helper 已保留，并补齐 signed 32-bit `pcl::index_t` 和 u32 byte offset gate（字节偏移准入）。 |
| `getDistancesToModel` | 逐点计算 `abs(sqrt((x-a)^2+(y-b)^2)-r)` 并写 `std::vector<double>`。 | Phase 020 旧 candidate 被拒绝；Phase 050 full-RVV diagnostic 为正向；Phase 060 接入后 production public Std/RVV 为正向，Phase 070 已采纳。 |

## 函数级结论

| 问题 | 当前结论 |
| --- | --- |
| select/count production patch | adopted production behavior（已采用生产行为）；`selectWithinDistance` 当前采用 Phase 090 接入后板卡数据，`countWithinDistance` 保持 Phase 000/090 回归正向。 |
| getDistances 旧候选 | rejected with diagnostic evidence（有诊断证据的拒绝）。 |
| getDistances full-RVV production | adopted production behavior；使用 Phase 060 接入后板卡数据作为正式采用依据。 |
| identity-index strided load | rejected with strict A/B evidence（严格 A/B 证据拒绝）；当前 select/count production 保持 gather-only RVV。 |
| production 长期主题文档 | applicable：`doc-rvv/sample_consensus/sac_model_circle-RVV.zh.md` 已创建。 |
| topic 默认下一步 | 当前 topic 内无新的同边界高优先级性能候选；更多点型、`Scalar=double`、`circle3d` 或新的 identity/load 组织需要新 phase / 新 scope。 |

## 标量流程与 RVV 流程对照

标量 `selectWithinDistanceStandard` 先清空 `inliers` 和 `error_sqr_dists_`，按 `indices_` 顺序读取点，计算 x/y 到圆心的平方距离；若平方距离落在内外半径边界之间，就追加原始 index，并用标量 `sqrt` 写入精确误差。RVV `selectWithinDistanceRVV` 用 gather、平方距离和 shell mask 批量筛选；`vcompress` 保持向量通道（lane，RVV 向量寄存器中的元素位置）顺序，并把命中的 index 写入 `inliers`。Phase 090 后，压缩后的平方距离继续在 RVV 中执行 `vfsqrt`、abs、`vfwcvt` 和 `vse64`，直接写 `error_sqr_dists_`。

`countWithinDistanceRVV` 使用同一 x/y gather 和 shell mask，然后用 `vcpop` 统计命中位数。`getDistancesToModelCandidateRVV` 只存在于测试和 bench 派生类中，它把平方距离写入临时 float buffer，再逐 lane 做标量 `sqrt` 和 double store，因此它是 production-shaped diagnostic（生产形态诊断），不是 production direct。Phase 050 的 `getDistancesToModelFullRVV` 把 `sqrt` 和 double 写回移入 RVV chunk；Phase 060 的 production `getDistancesToModelRVV` 使用同一实现族，直接服务公开入口。

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `selectWithinDistance` | production public entry | RVV / Standard dispatch（分流逻辑） | PCL sample consensus caller | `selectWithinDistanceRVV` 或 `selectWithinDistanceStandard` | production boundary | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle.hpp` |
| `selectWithinDistanceStandard` | production Std helper | 旧标量语义和 correctness reference（正确性参考） | public select、fallback、tests | gtest 对拍 | fallback coverage | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle.hpp` |
| `selectWithinDistanceRVV` | production RVV helper | x/y indexed gather、shell mask、compress 写回 inlier，并用 full-RVV error tail 写误差 | public select RVV gate | board production bench | Phase 090 adopted production direct / asm attribution | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle.hpp` |
| `countWithinDistanceRVV` | production RVV helper | shell mask + popcount 计数 | public count RVV gate | board production bench | production direct / regression | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle.hpp` |
| `getDistancesToModelCandidate` | production-shaped diagnostic | 测试专用 getDistances 候选入口 | gtest、bench 派生类 | Phase 020 manifest | diagnostic evidence | `src/test_sac_model_circle.cpp`、`src/bench_sac_model_circle.cpp` |
| `getDistancesToModelFullRVV` | production-shaped diagnostic | 测试专用 full-RVV getDistances 候选入口 | gtest、bench 派生类 | Phase 050 manifest | diagnostic positive / production probe entry | `include/impl/sac_model_circle_candidates.hpp` |
| `getDistancesToModelRVV` | production RVV helper | public getDistances 的 full-RVV helper | public getDistances RVV gate | Phase 060 production bench | adopted production direct | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle.hpp` |
| `bench_sac_model_circle` | bench wrapper | 计时 public select/count/getDistances 和 diagnostic candidate | Makefile / board.mk | manifest script | board performance input | `src/bench_sac_model_circle.cpp` |
| `generate_circle_board_evidence_manifest.py` | analysis script | 生成 production / getDistances manifest | Makefile doctor targets | Evidence Doctor、registry | evidence summary | `script/generate_circle_board_evidence_manifest.py` |
| `doc/phases/000.../result.zh.md` | phase result | Phase 000 生产候选证据主归属 | worker / reviewer | README、evaluation、matrix | production candidate decision | `doc/phases/000-circle-select-distance-production/result.zh.md` |
| `doc/phases/020.../result.zh.md` | phase result | Phase 020 负向诊断主归属 | worker / reviewer | README、evaluation、roadmap | diagnostic rejection | `doc/phases/020-circle-getdistances-ablation/result.zh.md` |
| `doc/phases/040.../result.zh.md` | phase result | Phase 040 production closeout 和 identity 负向 A/B 主归属 | worker / reviewer | README、evaluation、roadmap、matrix、`doc-rvv` | adopted / rejected decision | `doc/phases/040-production-closeout-and-identity-frontier/result.zh.md` |
| `doc/phases/050.../result.zh.md` | phase result | full-RVV getDistances 诊断正向主归属 | worker / reviewer | roadmap、matrix、Phase 060 plan | production probe decision | `doc/phases/050-getdistances-vfsqrt-full-rvv/result.zh.md` |
| `doc/phases/060.../result.zh.md` | phase result | getDistances 接入后 production direct 证据和当时 PI5 状态主归属 | worker / reviewer | Phase 070 closeout、README、evaluation、roadmap、matrix、Handoff | production direct input | `doc/phases/060-getdistances-production-probe/result.zh.md` |
| `doc/phases/070.../result.zh.md` | phase result | 用户确认后的 getDistances production closeout 主归属 | worker / reviewer | README、evaluation、roadmap、matrix、Handoff、`doc-rvv` | adopted closeout | `doc/phases/070-getdistances-production-closeout/result.zh.md` |
| `doc/phases/080.../result.zh.md` | phase result | select full-RVV error tail 的 RVV-vs-RVV detail A/B 主归属 | worker / reviewer | Phase 090 plan、roadmap、matrix | production probe input | `doc/phases/080-select-compressed-error-tail/result.zh.md` |
| `doc/phases/090.../result.zh.md` | phase result | select error-tail 接入后 production direct 证据和 adopted closeout 主归属 | worker / reviewer | README、evaluation、roadmap、matrix、Handoff、`doc-rvv` | adopted production direct | `doc/phases/090-select-error-tail-production-probe/result.zh.md` |
| `doc-rvv/sample_consensus/sac_model_circle-RVV.zh.md` | production long-term doc | 当前已采纳的 select/count/getDistances 生产 RVV 行为 | maintainer / reviewer | production closeout | adopted production behavior | `doc-rvv/sample_consensus/sac_model_circle-RVV.zh.md` |

## 实现方式审计

| candidate | 状态 | 证据 | 边界 / 恢复条件 |
| --- | --- | --- | --- |
| select RVV gather + `vcompress` + scalar exact error writeback | superseded | Phase 000 QEMU Std/RVV、board 5-run median 1.6702x、Evidence Doctor 0/0/0。 | Phase 090 已以 full-RVV error tail 取代该实现族；Phase 000 保留为 historical baseline。 |
| select RVV gather + `vcompress` + full-RVV error tail | adopted production behavior | Phase 080 RVV-vs-RVV median 1.6078x；Phase 090 接入后 public select median 2.5700x，Evidence Doctor 0/0/0。 | 当前采纳只覆盖 `PointXYZ` board、direct indexed `indices_`、float x/y AoS 和 u32 byte offset gate。 |
| existing count RVV with offset gate | adopted | Phase 000 board median 1.4012x、Evidence Doctor 0/0/0。 | 只覆盖 x/y float field layout、signed 32-bit index 和 u32 offset 范围。 |
| getDistances RVV sqr + scalar sqrt/store | rejected with diagnostic evidence | Phase 020 B/A median 0.6590x，5/5 低于 1；Evidence Doctor Errors=1、Warnings=1。 | 只拒绝当前 helper 形态。新实现族需先减少 `sqrt`、double store 或临时 buffer 成本。 |
| getDistances vfsqrt full-RVV store | adopted production behavior | Phase 050 diagnostic median 1.4745x；Phase 060 production public median 1.4737x；Evidence Doctor 均为 0/0/0。 | 当前采纳只覆盖 `PointXYZ` board、direct indexed `indices_`、float x/y AoS 和 u32 byte offset gate。 |
| identity-index strided load | rejected with strict A/B evidence | Phase 040 identity A/B：select median 0.9914x，count median 0.9698x，5/5 均低于 1；Evidence Doctor Errors=2。 | 当前实现族已撤回；只有新的低成本 identity 检测或不同访存组织能解释退化时才恢复。 |

## 当前证据

| 证据层 | 路径 / 命令 | 结论 |
| --- | --- | --- |
| QEMU correctness（QEMU 正确性，不代表真实性能） | `make -C test-rvv/sample_consensus/sac_model_circle run_test_compare` | 当前为 Std 5/5、RVV 5/5。 |
| production board evidence historical baseline（生产板卡历史基线） | `doc/phases/000-circle-select-distance-production/production-repeated-evidence-manifest.json` | 第一版 select scalar error tail median 1.6702x，count median 1.4012x，doctor 0/0/0。 |
| diagnostic board evidence（诊断板卡证据） | `doc/phases/020-circle-getdistances-ablation/getdistances-repeated-evidence-manifest.json` | getDistances candidate median 0.6590x，doctor Error=1 / Warning=1。 |
| full-RVV diagnostic board evidence | `doc/phases/050-getdistances-vfsqrt-full-rvv/getdistances-full-rvv-repeated-evidence-manifest.json` | full-RVV candidate median 1.4745x，doctor 0/0/0。 |
| production getDistances board evidence | `doc/phases/060-getdistances-production-probe/getdistances-production-repeated-evidence-manifest.json` | 接入后 public getDistances median 1.4737x，doctor 0/0/0；Phase 070 已采纳。 |
| production select error-tail board evidence | `doc/phases/090-select-error-tail-production-probe/select-error-tail-production-repeated-evidence-manifest.json` | 接入后 public select median 2.5700x，min/max 2.5357x / 2.6770x，doctor 0/0/0；Phase 090 已采纳。 |
| strict A/B board evidence（严格 A/B 板卡证据） | `doc/phases/040-production-closeout-and-identity-frontier/identity-repeated-evidence-manifest.json` | identity candidate select median 0.9914x、count median 0.9698x，doctor Errors=2。 |
| registry freshness（登记新鲜度） | `log/evidence_registry.json`，`production_evidence_status`，`getdistances_evidence_status`，`getdistances_full_rvv_evidence_status`，`getdistances_production_evidence_status`，`select_error_tail_evidence_status`，`select_error_tail_production_evidence_status`，`identity_evidence_status` | Phase 000 / 020 / 040 / 050 / 060 / 080 / 090 summary evidence 已登记。 |

## 诊断证据链

Phase 020 的 negative（负向）结论只说明旧 test-only helper 在同一 RVV binary 的 public companion row 对比中不占优。它不能证明所有 `getDistancesToModel` RVV 设计都不可行，也不能替代 production direct evidence。Phase 050 已用 full-RVV diagnostic 关闭“RVV sqrt/store 是否值得试”的问题；Phase 060 已补 production direct 证据。

## 生产接入判断

Phase 000 已完成 select/count 第一版生产证据闭环。Phase 080/090 又证明 `selectWithinDistance` full-RVV error tail 接入后仍为 positive-stable，并按用户确认采纳为长期 production 行为。Phase 060 的 `getDistancesToModelRVV` 接入后证据为 positive-stable，Phase 070 已按用户确认采纳为长期 production 行为。identity-index strided load 已由 Phase 040 拒绝。

## 遗留风险和下一步

当前没有未阻塞的 topic-local 文档套件缺口。更多 PointXYZ-like 点型 dedicated board、`Scalar=double` 或其它 SAC 模型仍属于后续 scope，当前证据不能外推。`selectWithinDistance` 的可见标量误差尾段已经由 Phase 090 关闭；继续优化需要新的身份索引 / load 组织假设、不同硬件证据或点型扩展证据。
