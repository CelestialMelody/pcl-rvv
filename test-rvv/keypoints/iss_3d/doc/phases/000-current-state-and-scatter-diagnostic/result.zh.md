# Phase 000 Result: Current State And Scatter Diagnostic

## 执行范围

本阶段完成 f64 vector reduction scatter（双精度向量规约散布矩阵）诊断闭环。范围是 test-only `computeScatterMatrixCandidate`，row source（行来源）是 indexed neighbor list（索引邻域列表），点型是 `pcl::PointXYZ`，layout（布局）是 AoS xyz float，scatter accumulator（散布矩阵累加器）为 double。

## 计划动作回填

| action | status | evidence |
| --- | --- | --- |
| A1 QEMU correctness | done | `make -C test-rvv/keypoints/iss_3d run_test_compare` 通过 4 个测试。 |
| A2 asm attribution | done | `make -C test-rvv/keypoints/iss_3d check_iss_3d_rvv_asm` 通过。 |
| A3 board smoke | done | 单次 board compare checksum 匹配，indexed / tail / contiguous 均正向。 |
| A4 repeated board + doctor | done | `iss_3d_phase000_scatter_f64_repeated` historical unstable；`iss_3d_phase000_scatter_f64_rerun1` 当前 positive。 |
| A5 docs / Handoff | done | topic-local doc suite、matrix、roadmap 和 Handoff 已补齐。 |
| A6 production 判断 | done | diagnostic positive 支撑进入 Phase 010 bounded production probe。 |

## 证据结果

`iss_3d_phase000_scatter_f64_repeated` 首轮 5-run 中 indexed case 有一个 0.691x 离群值，Evidence Doctor 为 Errors=0、Warnings=3、Suggestions=6，因此降级为 historical unstable。

`iss_3d_phase000_scatter_f64_rerun1` 为当前 diagnostic truth：indexed 256 median 1.662x，tail 73 median 1.213x，contiguous 256 median 1.859x，checksum 一致，Evidence Doctor 为 Errors=0、Warnings=1、Suggestions=6。Warning 是 tail case 的 group outlier（组内偏离），不推翻局部 positive，但提醒 production 不能直接外推。

## Diagnostic-To-Production Mismatch Audit 回填

| question | answer |
| --- | --- |
| evidence role | diagnostic / component ablation。 |
| A/B boundary | test helper only。 |
| 当前决策问题 | 局部 scatter RVV 是否值得进入 bounded production probe。 |
| 是否可外推到 production | 只能支持 probe，不能支持 adopted。 |
| mismatch 风险 | public compute 还有 search、EVD、NMS 和 output 成本。 |
| 允许 production probe 条件 | 当前 diagnostic rerun1 positive，checksum 正确，用户本轮授权正向 production 证据可采纳。 |
| clean adoption 条件 | 必须由 Phase 010 production public evidence 决定。 |

## EvidenceDecision

Phase 000 decision 是 `diagnostic_positive_proceed_to_production_probe`。本阶段只关闭 diagnostic scatter 矩阵条目，不关闭 production adoption、泛型点型或 broader ISS pipeline。

## Continue / Stop Decision

本阶段没有命中停止条件，已继续进入 `010-production-integration`。Phase 010 的 production evidence 后续把本局部 positive 缩窄为 public compute neutral。
