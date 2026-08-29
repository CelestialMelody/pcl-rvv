# Phase 020 Result: eight corners production public dispatch

本阶段完成 TrajkovicKeypoint3D `EIGHT_CORNERS` response map 的窄范围 production public dispatch（生产公开入口分流）闭环。真实生产入口在满足 `method_=EIGHT_CORNERS`、`window_size_=3`、organized dense full cloud（有组织稠密完整点云）、`PointInT` 为 xyz float AoS layout（结构数组布局）且 `NormalT` 为 normal float layout（法线字段布局）时命中 RVV response helper；`FOUR_CORNERS` 仍作为同边界回归控制一起保留。

## 实际执行范围

| 项 | 结果 |
| --- | --- |
| production patch | `keypoints/include/pcl/keypoints/impl/trajkovic_3d.hpp` 新增 `trajkovic3DEightCornersResponseRVV`，并把 `detectKeypoints()` 的 public dispatch 扩展到 `EIGHT_CORNERS`。 |
| public entry | `pcl::TrajkovicKeypoint3D<pcl::PointXYZ, pcl::PointXYZI, pcl::Normal>::compute()`，测试和 bench 均通过真实公开入口。 |
| adopted scope | `FOUR_CORNERS` / `EIGHT_CORNERS`、`window_size_=3`、precomputed normals（预计算法线）、`input.is_dense && normals.is_dense`、organized full cloud。 |
| scalar fallback | 非 RVV 构建、traits/layout gate 不满足、非 3x3 window、non-dense 输入或法线、normal estimation 生成路径中的 non-dense 数据、tiny cloud 和其它未证明组合。 |

## 动作回填

| action | status | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| public bench mode | done | `src/bench_trajkovic_3d.cpp --mode public` | 输出 `public_four_corners_320x240` 和 `public_eight_corners_320x240` 主路径，以及 invalid/tail fallback controls。 |
| QEMU correctness | done | `make -C test-rvv/keypoints/trajkovic_3d run_test_compare` | Std/RVV 两个 gtest binary 均通过 10 个测试。 |
| asm gate | done | `make -C test-rvv/keypoints/trajkovic_3d check_trajkovic_3d_rvv_asm` | RVV 指令可归属到 Trajkovic 3D response helper；production manifest 记录 `asm_rvv_line_count=1223`。 |
| board repeated | done | `make -C test-rvv/keypoints/trajkovic_3d board_repeated_production record_evidence_state_production` | `public_four_corners_320x240` 和 `public_eight_corners_320x240` 在生产公开边界上均为 positive。 |
| Evidence Doctor | done | `test-rvv/keypoints/trajkovic_3d/log/board/repeated_phase020_eight_corners_production_public/evidence_doctor.md` | Errors=0，Warnings=0，Suggestions=4。 |
| docs | done in this closeout | 本 result、evaluation、README、doc suite、`doc-rvv/keypoints/trajkovic_3d-RVV.zh.md` | 生产证据主归属已更新为双 public path 的 production evidence。 |

## 板卡 Production Evidence

证据路径：

- summary：`test-rvv/keypoints/trajkovic_3d/log/board/repeated_phase020_eight_corners_production_public/summary.md`
- manifest：`test-rvv/keypoints/trajkovic_3d/log/board/repeated_phase020_eight_corners_production_public/evidence_manifest.json`
- doctor：`test-rvv/keypoints/trajkovic_3d/log/board/repeated_phase020_eight_corners_production_public/evidence_doctor.md`

| case | evidence role | A/B boundary | median speedup | min | max | B/A < 1 | decision bucket |
| --- | --- | --- | ---: | ---: | ---: | ---: | --- |
| `public_eight_corners_320x240` | production public | `public_compute` | 1.677x | 1.613x | 1.705x | 0/5 | positive |
| `public_four_corners_320x240` | production public | `public_compute` | 1.790x | 1.762x | 1.820x | 0/5 | positive |

两条 public case 的 checksum policy（校验和策略）相同：对 output intensity（输出强度）按 1e-5 量化后和 keypoint indices（关键点索引）一起计算 FNV-1a；manifest 中两条比较均为 `checksum_equal=true`。

invalid 和 tail public case 在当前 production gate 下回退标量。raw analyze log 中它们接近 1.0x，用于确认回退路径没有误命中 RVV 和没有明显成本异常；它们没有进入 production speedup manifest，因为它们不属于 adopted RVV 主路径。

## Evidence Doctor 回填

| 等级 | 数量 | 处理 |
| --- | ---: | --- |
| Errors | 0 | 无阻塞项。 |
| Warnings | 0 | 无需要降级的异常。 |
| Suggestions | 4 | 缺少 device/taskset/governor/freq/temperature 和 binary hash。当前 5-run 全部 positive 且 checksum 一致，因此不阻塞采纳；后续若出现方向反转或长尾，应优先补这些 metadata。 |

## Diagnostic 到 Production 回填

| question | result |
| --- | --- |
| evidence role | 当前采用 `production_public`，Phase 000 只作为历史前置证据。 |
| A/B boundary | `public_compute`，Std/RVV 两个二进制都调用真实 `Detector::compute(output)`。 |
| 当前决策问题 | `RVV-vs-scalar`：当前 public RVV path 是否快于当前 public scalar path。 |
| diagnostic 是否外推 | 没有直接外推；本阶段用 production public 证据重新证明。 |
| mismatch 风险 | NMS 与输出构造已包含在计时内；normal estimation 仍不在当前 adopted scope 内。 |
| clean adoption 是否需要 RVV-vs-RVV | 当前 topic 没有已有 Trajkovic 3D RVV family，不是 family-selection 问题；不需要 RVV-vs-RVV detail A/B。 |

## Doc Suite Parity Audit

| area | current shape scan | quality bar | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| README navigation | 已刷新到 Phase 020 result | 需要说明当前 adopted scope、命令和证据白名单 | adopted | `README.zh.md` 已更新 | none |
| testing_overview | 已说明 current adopted scope | 需要 target 分类和 QEMU / board 边界 | adopted | `doc/testing-overview.zh.md` 已更新 | none |
| correctness_tests | 已列出 eight-corners public tests | 需要 gtest 语义字典 | adopted | `doc/correctness-tests.zh.md` 已更新 | none |
| benchmark_and_evidence | 已同步 phase 020 board summary | 需要 case-filter、summary、doctor、registry 说明 | adopted | `doc/benchmark-and-evidence.zh.md` 已更新 | none |
| optimization_evidence | 已映射 four / eight corners adopted 状态 | 需要按 candidate family 归档 | adopted | `doc/optimization-evidence.zh.md` 已更新 | none |
| test_support_code_map | 已覆盖 four / eight corners helper | 需要生产、测试、脚本和输出定位 | adopted | `doc/test-support-code-map.zh.md` 已更新 | none |
| phase suite | phase 020 已完成 | result / matrix 必须反映当前证据 | adopted | 本 result、README、matrix、index 已刷新 | none |
| evaluation | 已包含 phase 020 adopted scope | production closeout 需要决策审计 | adopted | `doc/trajkovic_3d-evaluation.zh.md` 已更新 | none |
| production_topic_doc | 已刷新为双 public path adopted 文档 | PI5 positive 且本轮采纳后必须维护 | adopted | `doc-rvv/keypoints/trajkovic_3d-RVV.zh.md` 已更新 | none |
| artifact tracking | topic 新文档均为 tracked 或待 staging | closeout 需要提交边界说明 | adopted | Handoff / final response 列为 current topic artifacts | none |

## EvidenceDecision

`evidence_decision`: production-adopted narrow scope。生产 public bench 的 median speedup 为 1.790x / 1.677x，5-run 无低于 1.0x，Evidence Doctor 无 Error/Warning，QEMU correctness、fallback controls 和 asm gate 均闭合。当前采用范围覆盖 `FOUR_CORNERS` 与 `EIGHT_CORNERS` 的 3x3 dense public path；non-dense、其它 window、normal estimation 和更宽点型继续保持标量或另开阶段。

## Continue / Stop Decision

`continue_stop_decision`: stop with closeout. 当前 roadmap 中仍有更宽点型、normal estimation 和 NMS 等方向，但它们不属于当前已采纳 public response-map 的同边界继续优化：

- `point-type-expansion` 需要新的代表点型 / 布局审计和独立 board。
- normal estimation 属于 `IntegralImageNormalEstimation`，不是本文件 topic。
- NMS RVV 涉及排序、occupancy map 和输出顺序风险，需要 profile 证明它已经成为主成本。

默认下一轮动作：若用户继续本 topic，先从 `point-type-expansion` 新 phase 或相邻模块 topic 进入；若没有继续要求，当前窄范围 production closeout 可以交 reviewer。
