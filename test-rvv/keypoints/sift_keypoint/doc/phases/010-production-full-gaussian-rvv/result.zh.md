# Phase 010 Result: production full Gaussian RVV

## 阶段结论

`keypoints/include/pcl/keypoints/impl/sift_keypoint.hpp` 已保留窄范围 production patch（生产补丁）：
在 `computeScaleSpace()` 中，RVV 路径批量计算 Gaussian weight（高斯权重），随后按原邻域顺序用
scalar tail（标量尾段）累加 numerator / denominator。该形态替代了早期 full vector reduction
（完整向量规约）尝试；后者曾导致 public trace（公开入口输出跟踪）顺序不一致。

最终 `EvidenceDecision`（证据决策）为 `production-adopted-narrow-scope`。该结论只覆盖
`PointXYZI -> PointWithScale`、organized dense synthetic `public_sift_keypoint_320x240` case、
`Scalar=float` 和 Milkv-Jupiter 板卡上的真实 public `SIFTKeypoint::compute()` 入口，不外推到其它点型、
真实业务 workload（工作负载）、更大规模或 `findScaleSpaceExtrema()`。

## 执行动作回填

| 动作 | 状态 | 命令 / 路径 | 结论 |
| --- | --- | --- | --- |
| production patch | done | `keypoints/include/pcl/keypoints/impl/sift_keypoint.hpp` | public API 不变；`__RVV10__` 关闭时保留原标量循环 |
| public correctness | done | `make -C test-rvv/keypoints/sift_keypoint run_test_compare` | Std / RVV gtest 对拍通过，public smoke 可运行 |
| public output trace | done | `test-rvv/keypoints/sift_keypoint/log/board/public_trace_phase010_public_compute/public_trace_compare.md` | 116 / 116 keypoints，`in_order_mismatch_count=0`，`x/y/z/scale` 最大差异为 0 |
| asm attribution | done | `make -C test-rvv/keypoints/sift_keypoint check_sift_keypoint_rvv_asm` | RVV bench binary 命中 `expf_RVV_f32m2`、load 和 `vsetvl` 相关路径 |
| production board evidence | done | `make -C test-rvv/keypoints/sift_keypoint board_repeated_public record_evidence_state_public` | 5-run median speedup 1.309x，min 1.304x，max 1.310x，`B/A < 1 = 0/5` |
| Evidence Doctor | done | `test-rvv/keypoints/sift_keypoint/log/board/repeated_phase010_public_compute/evidence_doctor.md` | Errors=0，Warnings=0，Suggestions=2 |
| evidence registry | done | `test-rvv/keypoints/sift_keypoint/log/evidence_registry.json` | public summary / manifest / doctor 已登记，并引用 phase result、evaluation 和 `doc-rvv` |

## Production Evidence

| 证据 | 路径 | 结论 | 边界 |
| --- | --- | --- | --- |
| public repeated board summary | `test-rvv/keypoints/sift_keypoint/log/board/repeated_phase010_public_compute/summary.md` | `public_sift_keypoint_320x240` 5-run median speedup 1.309x，`B/A < 1 = 0/5` | 只证明当前 public RVV path 快于当前 public scalar path |
| public output trace compare | `test-rvv/keypoints/sift_keypoint/log/board/public_trace_phase010_public_compute/public_trace_compare.md` | Std / RVV 输出计数、顺序和字段完全一致 | 只覆盖该 synthetic organized public case |
| Evidence Doctor | `test-rvv/keypoints/sift_keypoint/log/board/repeated_phase010_public_compute/evidence_doctor.md` | 无 Error / Warning；2 条 Suggestion 不阻塞采纳 | 环境 metadata 缺失仍是复现边界 |
| manifest | `test-rvv/keypoints/sift_keypoint/log/board/repeated_phase010_public_compute/evidence_manifest.json` | `evidence_role=production_public`，`summary_role=production_public` | manifest 记录 public entry 边界，不是 RVV-vs-RVV family A/B |

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | Phase 000 是 `diagnostic`；Phase 010 当前决策使用 `production_public` |
| A/B boundary | Phase 000 为 `test_helper`；Phase 010 为 `public_entry` |
| 当前决策问题 | RVV-vs-scalar：当前 public RVV path 是否快于当前 public scalar path |
| diagnostic 是否可外推到 production | 诊断证据只解释为什么值得 probe；采纳依据是 production-public repeated board |
| comparison-boundary / baseline mismatch 风险 | 已通过 public entry 重跑降低；仍不能外推到其它点型、规模或真实 workload |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不适用；当前 production-public 证据是 positive |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前没有已采纳 SIFT RVV family，决策不是 RVV-family-selection；不要求 RVV-vs-RVV detail A/B |

## 实现取舍

| 维度 | 当前状态 | 证据 | 边界 / 恢复条件 |
| --- | --- | --- | --- |
| 保守 weight staging（权重暂存） | adopted | public trace 0 diff；board median 1.309x | 只暂存 weight，后续累加保持原邻域顺序 |
| production helper split（生产 helper 拆分） | accepted exception | `computeScaleSpace()` 是 header-only protected template helper，当前 RVV 只替换内层纯 weight 计算 | 不拆 `*_Std` / `*_RVV`，避免为窄范围补丁扩大生产头文件接口面；扩到 extrema 或更多点型时再新建 helper split phase |
| full vector reduction | rejected | 历史 public trace 曾出现 116 vs 116 但 `in_order_mismatch_count=51` | 若要重开，必须先设计 order-preserving reduction（保序规约）并重新做 public trace |
| `findScaleSpaceExtrema()` RVV | not_now | 当前 adopted patch 后无 profile 证明它是新瓶颈 | 后续需要独立 phase、正确性 trace、asm 和 board evidence |
| point type expansion（点类型扩展） | deferred | 当前 public case 只覆盖 `PointXYZI -> PointWithScale` | 扩到其它 `PointInT` / intensity layout 前必须补 fallback、bench、asm 和 board |

## Doc Suite Role Inventory

| area | current shape scan | quality bar / supplemental calibration | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| topic_navigation | `README.zh.md` 存在并列出阅读路径、命令和证据白名单 | 需要指向 production adopted 结论和 `doc-rvv` | adopted | 已刷新 | none |
| testing_overview | `doc/testing-overview.zh.md` 存在 | 需要区分 diagnostic、production-public、QEMU 和 board target | adopted | 已刷新 | none |
| correctness_tests | `doc/correctness-tests.zh.md` 存在 | 需要说明 public trace 与 gtest 的不同证据角色 | adopted | 已刷新 | none |
| benchmark_and_evidence | `doc/benchmark-and-evidence.zh.md` 存在 | 需要记录 public case、summary、manifest、doctor 和 registry | adopted | 已刷新 | none |
| optimization_evidence | `doc/optimization-evidence.zh.md` 存在 | 需要列 adopted / rejected / deferred candidate | adopted | 已刷新 | none |
| optimization_roadmap | `doc/optimization-roadmap.zh.md` 存在 | 需要把 next phase 默认动作写成停止条件或扩展条件 | adopted | 已刷新 | none |
| test_support_code_map | `doc/test-support-code-map.zh.md` 存在 | 需要加入 production patch、public trace 脚本和 public evidence | adopted | 已刷新 | none |
| phase suite | `doc/phases/README.zh.md`、plan、result、matrix 存在 | 需要 phase 000/010 result 和矩阵闭合 | adopted | 已刷新 | none |
| evaluation | `doc/sift_keypoint-evaluation.zh.md` 存在 | 需要 production integration 后最终证据更新 | adopted | 已刷新 | none |
| production_topic_doc | `doc-rvv/keypoints/sift_keypoint-RVV.zh.md` | 用户授权下 production-public positive 可采纳并创建长期文档 | adopted | 已创建 | none |

## Continue / Stop Decision

`continue_stop_decision`: stop_for_review。当前授权范围内的生产接入、证据登记、doc-suite closeout
和 screening 状态同步已经闭合；继续优化 `findScaleSpaceExtrema()`、其它点型 / 规模和真实 workload
需要新的 phase 计划与证据矩阵。由于当前没有 post-adoption profile（采纳后剖析）证明这些方向仍是收益最高的
未阻塞动作，本轮不继续扩大 production patch。
