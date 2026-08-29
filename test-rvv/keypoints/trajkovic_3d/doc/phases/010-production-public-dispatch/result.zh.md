# Phase 010 Result: production public dispatch

本阶段完成 TrajkovicKeypoint3D `FOUR_CORNERS` response map 的窄范围 production public dispatch（生产公开入口分流）闭环。真实生产入口在满足 `method_=FOUR_CORNERS`、`window_size_=3`、organized dense full cloud（有组织稠密完整点云）、`PointInT` 为 xyz float AoS layout（结构数组布局）且 `NormalT` 为 normal float layout（法线字段布局）时命中 RVV response helper；其它路径自然回到原标量主体。

## 实际执行范围

| 项 | 结果 |
| --- | --- |
| production patch | `keypoints/include/pcl/keypoints/impl/trajkovic_3d.hpp` 新增 `trajkovic3DFourCornersResponseRVV` 和 `detectKeypoints()` 中的短路分流。 |
| public entry | `pcl::TrajkovicKeypoint3D<pcl::PointXYZ, pcl::PointXYZI, pcl::Normal>::compute()`，测试和 bench 均通过真实公开入口。 |
| adopted scope | `FOUR_CORNERS`、`window_size_=3`、precomputed normals（预计算法线）、`input.is_dense && normals.is_dense`、organized full cloud。 |
| scalar fallback | 非 RVV 构建、traits/layout gate 不满足、`EIGHT_CORNERS`、非 3x3 window、non-dense 输入或法线、normal estimation 生成路径中的 non-dense 数据、tiny cloud 和其它未证明组合。 |

## 动作回填

| action | status | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| public bench mode | done | `src/bench_trajkovic_3d.cpp --mode public` | 输出 `public_four_corners_320x240` 主路径和 invalid/tail fallback control（回退控制用例）。 |
| QEMU correctness | done | `make -C test-rvv/keypoints/trajkovic_3d run_test_compare` | Std/RVV 两个 gtest binary 均通过 7 个测试。 |
| asm gate | done | `make -C test-rvv/keypoints/trajkovic_3d check_trajkovic_3d_rvv_asm` | RVV 指令可归属到 Trajkovic 3D response helper；production manifest 记录 `asm_rvv_line_count=977`。 |
| board repeated | done | `make -C test-rvv/keypoints/trajkovic_3d board_repeated_production record_evidence_state_production` | `public_four_corners_320x240` production public repeated board 为 positive。 |
| Evidence Doctor | done | `test-rvv/keypoints/trajkovic_3d/log/board/repeated_phase010_production_public/evidence_doctor.md` | Errors=0，Warnings=0，Suggestions=2。 |
| docs | done in this closeout | 本 result、evaluation、README、doc suite、`doc-rvv/keypoints/trajkovic_3d-RVV.zh.md` | 生产证据主归属从 diagnostic 升级到 production direct。 |

## 板卡 Production Evidence

证据路径：

- summary：`test-rvv/keypoints/trajkovic_3d/log/board/repeated_phase010_production_public/summary.md`
- manifest：`test-rvv/keypoints/trajkovic_3d/log/board/repeated_phase010_production_public/evidence_manifest.json`
- doctor：`test-rvv/keypoints/trajkovic_3d/log/board/repeated_phase010_production_public/evidence_doctor.md`

| case | evidence role | A/B boundary | median speedup | min | max | B/A < 1 | decision bucket |
| --- | --- | --- | ---: | ---: | ---: | ---: | --- |
| `public_four_corners_320x240` | production public | `public_compute` | 1.769x | 1.736x | 1.810x | 0/5 | positive |

`public_four_corners_320x240` 的 checksum policy（校验和策略）是对 output intensity（输出强度）按 1e-5 量化后和 keypoint indices（关键点索引）一起计算 FNV-1a；manifest 中 `checksum_equal=true`。

invalid 和 tail public case 在当前 production gate 下回退标量。raw analyze log 中它们接近 1.0x，用于确认回退路径没有误命中 RVV 和没有明显成本异常；它们没有进入 production speedup manifest，因为它们不属于 adopted RVV 主路径。

## Evidence Doctor 回填

| 等级 | 数量 | 处理 |
| --- | ---: | --- |
| Errors | 0 | 无阻塞项。 |
| Warnings | 0 | 无需要降级的异常。 |
| Suggestions | 2 | 缺少 device/taskset/governor/freq/temperature 和 binary hash。当前 5-run 全部 positive 且 checksum 一致，因此不阻塞采纳；后续若出现方向反转或长尾，应优先补这些 metadata。 |

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
| README navigation | topic README 存在但旧结论仍写 Phase 000 in_progress | README 需要说明当前 adopted scope、命令和证据白名单 | adopted | 本 closeout 刷新 `README.zh.md` | none |
| testing_overview | 缺独立 role 文档 | 需要 target 分类和 QEMU / board 边界 | adopted | 新增 `doc/testing-overview.zh.md` | none |
| correctness_tests | 缺独立 role 文档 | 需要 gtest 语义字典 | adopted | 新增 `doc/correctness-tests.zh.md` | none |
| benchmark_and_evidence | 缺独立 role 文档 | 需要 case-filter、summary、doctor、registry 说明 | adopted | 新增 `doc/benchmark-and-evidence.zh.md` | none |
| optimization_evidence | roadmap 存在但缺 adopted/rejected/deferred 映射 | 需要按 candidate family 归档 | adopted | 新增 `doc/optimization-evidence.zh.md` | none |
| test_support_code_map | 缺独立 role 文档 | 需要生产、测试、脚本和输出定位 | adopted | 新增 `doc/test-support-code-map.zh.md` | none |
| phase suite | 只有 plan 和旧 index，没有 result | result / matrix 必须反映当前证据 | adopted | 新增 Phase 000/010 result，刷新 phase index 和 matrix | none |
| evaluation | 缺 `doc/trajkovic_3d-evaluation.zh.md` | production closeout 需要决策审计 | adopted | 新增 evaluation | none |
| production_topic_doc | 之前不存在 | PI5 positive 且本轮采纳后必须创建长期文档 | adopted | 新增 `doc-rvv/keypoints/trajkovic_3d-RVV.zh.md` | none |
| artifact tracking | topic 新文档均为未跟踪文件 | closeout 需要提交边界说明 | adopted | Handoff / final response 列为 current topic artifacts；raw logs 默认不提交 | none |

## EvidenceDecision

`evidence_decision`: production-adopted narrow scope。生产 public bench 的 median speedup 为 1.769x，5-run 无低于 1.0x，Evidence Doctor 无 Error/Warning，QEMU correctness、fallback controls 和 asm gate 均闭合。当前采用范围只覆盖 `FOUR_CORNERS` 3x3 dense public path；`EIGHT_CORNERS`、non-dense、其它 window、normal estimation 和更宽点型继续保持标量或另开阶段。

## Continue / Stop Decision

`continue_stop_decision`: stop with closeout. 当前 roadmap 中仍有可研究方向，但没有一个在本轮同一 production adopted scope 内既高收益又未阻塞：

- `EIGHT_CORNERS` 需要独立公式实现、correctness、asm 和 board，不应混入当前采纳补丁。
- normal estimation 属于 `IntegralImageNormalEstimation`，不是本文件 topic。
- NMS RVV 涉及排序、occupancy map 和输出顺序风险，需要 profile 证明它已经成为主成本。
- 更宽点类型虽然 traits gate 已支持 xyz / normal float layout，但当前 production evidence 只覆盖 `PointXYZ + Normal` 的公开入口实例；扩大需要 point-type expansion phase。

默认下一轮动作：若用户继续本 topic，优先选择 `EIGHT_CORNERS` 或 point-type expansion（点类型扩展）作为新 phase；若没有继续要求，当前窄范围 production closeout 可以交 reviewer。
