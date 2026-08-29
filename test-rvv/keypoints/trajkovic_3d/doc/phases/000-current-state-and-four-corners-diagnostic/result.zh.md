# Phase 000 Result: FOUR_CORNERS response diagnostic

本阶段完成 TrajkovicKeypoint3D（3D Trajkovic 关键点）`FOUR_CORNERS` response map（四邻域响应图）的 response-only diagnostic（只测响应图的诊断）。它证明测试专用 Std helper（标量辅助函数）和 RVV helper（RISC-V 向量辅助函数）在 organized full cloud（有组织完整点云）上等价，并在板卡上给出进入 production integration loop（生产接入闭环）的正向信号；它本身不构成 adopted production behavior（已采纳生产行为）。

## 实际执行范围

| 项 | 结果 |
| --- | --- |
| 入口边界 | `test-rvv/keypoints/trajkovic_3d/include/impl/trajkovic_3d_response.hpp` 的 `computeFourCornersResponseStd/RVV`。 |
| 点类型和布局 | `pcl::PointXYZ` 输入、`pcl::Normal` 法线、`float` AoS（结构数组）跨步加载。 |
| 覆盖语义 | finite point / normal gate（有限值门控）、invalid neighbor null-normal（非法邻域法线置零）、threshold（阈值）、border zero（边界置零）和尾部 VL chunk（可变向量长度分块）。 |
| 不覆盖 | 真实 `Detector::compute()`、NMS（非极大值抑制）、normal estimation（法线估计）、`EIGHT_CORNERS`、泛型点类型和 production dispatch（生产分流）。 |

## 动作回填

| action | status | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| RED/GREEN response helper | done | `make -C test-rvv/keypoints/trajkovic_3d run_test_compare` | Std/RVV 两侧 gtest 通过；QEMU（仿真器）只作为 correctness（正确性）和日志形状证据。 |
| ASM | done | `make -C test-rvv/keypoints/trajkovic_3d check_trajkovic_3d_rvv_asm` | 反汇编中 `computeFourCornersResponseRVV` 命中 `vlse32.v`、`vfmul.vv` 和 `vfsqrt.v`。 |
| BOARD | done | `test-rvv/keypoints/trajkovic_3d/log/board/repeated_phase000_four_corners_response/summary.md` | 5-run repeated board（重复板卡测试）全部 positive。 |
| Evidence Doctor | done | `test-rvv/keypoints/trajkovic_3d/log/board/repeated_phase000_four_corners_response/evidence_doctor.md` | response-only 证据无阻塞 Error；环境 metadata 和 binary identity 建议只影响复现解释，不推翻阶段结论。 |
| Registry | done | `test-rvv/keypoints/trajkovic_3d/log/evidence_registry.json` | Phase 000 summary / manifest / doctor 已登记，并引用本 result 与 evaluation。 |

## 板卡结果

证据路径：`test-rvv/keypoints/trajkovic_3d/log/board/repeated_phase000_four_corners_response/summary.md`。

| case | evidence role | median speedup | min | max | B/A < 1 | 结论 |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| `four_corners_320x240` | production-shaped diagnostic（生产形态诊断） | 3.596x | 3.469x | 3.725x | 0/5 | response helper 有稳定收益。 |
| `four_corners_641x481_tail` | production-shaped diagnostic | 3.396x | 3.307x | 3.500x | 0/5 | 非整行尾部规模仍正向。 |
| `four_corners_invalid_320x240` | production-shaped diagnostic | 2.317x | 2.043x | 2.656x | 0/5 | invalid neighbor null-normal 语义下仍正向。 |

这些结果只说明 response map 的纯公式片段值得进入 production direct（真实生产入口直连）验证。它们不直接证明 `compute()` 公开入口可采纳，因为公开入口还包含 NMS 和输出构造。

## Diagnostic 到 Production 回填

| question | result |
| --- | --- |
| evidence role | Phase 000 是 `production_shaped_diagnostic`，不是 `production_public`。 |
| A/B boundary | `test_helper`，只测 response map helper。 |
| 当前决策问题 | 是否值得推进到 `public_compute` 生产边界。 |
| 是否可外推到 production | 只能外推为 bounded production probe（有界生产探针）可行；不能直接采纳。 |
| mismatch 风险 | NMS、输出构造和 production dispatch 可能稀释收益。 |
| clean adoption 需要什么 | 需要 Phase 010 的 production public repeated board 和 Evidence Doctor。 |

## EvidenceDecision

Phase 000 决策为 `partial-production-candidate`：`four-corners-response-rvv` 在 response-only 边界收益稳定，允许进入 Phase 010 / PI1-PI5。未覆盖的 `EIGHT_CORNERS`、normal estimation、NMS RVV 和更宽点类型留到 roadmap；它们不是当前 production 采纳的前置条件。

## Continue / Stop Decision

`continue_stop_decision`: continue。Phase 010 已作为默认下一阶段，目标是把同一 RVV response helper 接入真实 `detectKeypoints()` 并在 `Detector::compute()` public boundary（公开入口边界）重跑 correctness、asm、board 和 Evidence Doctor。
