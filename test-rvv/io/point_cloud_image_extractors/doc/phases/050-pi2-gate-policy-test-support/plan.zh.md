# Phase 050 Plan: PI2 gate policy test support

## 阶段意图和边界

本阶段把 Phase 040 冻结的 PI2 production probe gate（生产探针准入条件）转成 topic-local
correctness test（主题本地正确性测试）里的可失败验收。它只新增测试专用 helper，不修改
`io/include/pcl/io/impl/point_cloud_image_extractors.hpp`，也不声明 production dispatch
（生产分流）已经存在。

## 当前状态清单

| item | state | evidence |
| --- | --- | --- |
| RED test | observed | `make run_test_rvv` 已因缺少 `rgbProductionProbeGate` 和 `scalingProductionProbeGate` 编译失败。 |
| PI1 scope | frozen | `040-pi1-production-integration-plan/result.zh.md` 固定 RGB exact `PointXYZRGB/PointXYZRGBA` 与 scaling exact `PointXYZI intensity full-range`。 |
| production source | untouched | 本阶段不触碰 `io/include/pcl/io/impl/point_cloud_image_extractors.hpp`。 |

## 实现和测试动作

| action | artifact | expected evidence | completion |
| --- | --- | --- | --- |
| add test-only gate helper | `include/impl/pcie_support.hpp` | helpers compile in Std/RVV builds | `make run_test_compare` passes |
| keep test boundary explicit | `src/test_pcie.cpp` and docs | test remains about Phase 040 policy, not production dispatch | docs mention helper is test-only |
| update topic-local docs | correctness/testing/code-map/phase docs | readers can locate the policy test and boundary | markdown + diff checks pass |

## 优化矩阵

| candidate family | scope and entry | correctness / fallback target | board evidence | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- |
| PI2 gate policy test support | test-only helper for Phase 040 frozen gate | `ProductionProbeGatePolicyMatchesPi1Plan` | not_applicable; no benchmark in this phase | not_applicable; no EvidenceDecision change | planned |

## Continue / Stop 条件

本阶段完成后，如果 correctness 和文档检查通过，默认仍停在 `PI2-production-patch after explicit user confirmation`。
停止条件是继续会修改 production header，需要用户确认；这不是板卡或工具阻塞。
