# Phase 010 Production Depth Label Probe Result

## 实际执行范围

本阶段按 `plan.zh.md` 完成 PI1-PI5：为
`features/include/pcl/features/impl/organized_edge_detection.hpp` 接入 depth label RVV production path（生产路径），
并用真实 `OrganizedEdgeBase<PointXYZ, Label>::compute()` 的 correctness（正确性）、asm attribution
（反汇编归属）和 repeated board（重复板卡测试）证据判断是否采纳。

实际采纳范围：

- `PointT`：满足 `pcl::rvv::RVVXYZAoSFloatLayout<PointT>` 的 AoS 单 float xyz 布局；本阶段测试和板卡只证明 `PointXYZ`。
- `PointLT`：生产 RVV 分流收窄到 `pcl::Label`，其它 label 点类型走原标量 helper。
- 入口：`OrganizedEdgeBase::compute()` / `extractEdges()` 的 depth label path；派生 RGB / normal 入口仅间接复用 base depth path。
- 不覆盖：RGB Canny、normal Canny、`assignLabelIndices()` RVV 化、泛型 `PointLT` label field gate。

## 计划动作回填

| 动作 | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| RED asm gate | done | `make -C test-rvv/features/organized_edge_detection check_production_rvv_asm` 在 production patch 前失败 | 失败点为缺少 `organizedEdgeDepthLabelsRVV` 和 RVV 指令归属，证明验收能捕捉未接 production。 |
| PI2 production patch | done | `features/include/pcl/features/impl/organized_edge_detection.hpp` | 新增 `organizedEdgeDepthLabelsStandard()` 和 `organizedEdgeDepthLabelsRVV()`；`extractEdges()` 先尝试 RVV，失败回标量。 |
| PI3 production direct tests | done | `make -C test-rvv/features/organized_edge_detection run_test_compare` | Std/RVV 构建各 4/4 pass；新增 `ProductionComputeMatchesScalarReference` 覆盖真实 `compute()`。 |
| PI4 asm attribution | done | `make -C test-rvv/features/organized_edge_detection check_production_rvv_asm` | production bench 中有 `organizedEdgeDepthLabelsRVV`，filtered asm 含 `vle32.v`、`vfsub.vv`、`vfmin.vv`、`vfmax.vv`、`vse32.v`。 |
| PI4 board evidence | done | `make -C test-rvv/features/organized_edge_detection run_board_organized_edge_detection_production_repeated` 后重跑 `record_board_production_evidence_state` | 5-run production-public 证据 strong positive，checksum match。 |
| PI5 decision | done | `log/board/production-repeated-summary.md`、`log/board/production-evidence_doctor.md` | EvidenceDecision：`production-adopted`。 |

## Production Patch Scope

| 文件 / 符号 | 状态 | 说明 |
| --- | --- | --- |
| `organizedEdgeDepthLabelsStandard()` | adopted | 承载原 `extractEdges()` depth 标量主体，供非 RVV 和 fallback 复用。 |
| `organizedEdgeDepthLabelsRVV()` | adopted | `__RVV10__` 下对内部像素按 VL chunk 批量处理全有限 8 邻域；chunk 中有 invalid neighbor 时逐 lane 调标量 pixel helper 修正。 |
| `OrganizedEdgeBase::extractEdges()` | adopted | 短分流入口：RVV helper 返回 true 则结束，否则调用标准 helper。 |
| public API | unchanged | 未修改公开类接口、构造函数、setter/getter 或 label index 输出 API。 |

## Correctness / QEMU

`run_test_compare` 结果：

- Std build：4 个 gtest 全通过。
- RVV build：4 个 gtest 全通过。
- 新增 production direct 测试通过真实 `OrganizedEdgeBase<PointXYZ, Label>::compute()` 对比 test-local scalar reference，覆盖 finite depth step、NaN boundary search、edge type bit gate 和 label index 顺序。

QEMU 只作为 correctness 和日志形状证据，不参与性能结论。

## Board Performance

生产证据来自 `log/board/production-repeated-summary.md`，证据角色为 `production-public`。

| case | runs | mean B/A | median B/A | min B/A | max B/A | checksum |
| --- | --- | --- | --- | --- | --- | --- |
| `prod_depth_finite_320x240` | 5 | `6.194x` | `6.192x` | `6.164x` | `6.253x` | match |
| `prod_depth_finite_641x481_tail` | 5 | `5.521x` | `5.544x` | `5.420x` | `5.574x` | match |
| `prod_depth_nan_boundary_320x240` | 5 | `3.016x` | `3.013x` | `2.995x` | `3.038x` | match |

该结果使用接入 production 后的真实公开入口 bench，不沿用 Phase 000 test-helper timing。三项均远高于
`1.05x` production-adoption threshold，decision bucket 稳定为 `positive`。

## Evidence Doctor 和 Registry

| 项 | 结果 | 处理 |
| --- | --- | --- |
| Evidence Doctor | `log/board/production-evidence_doctor.md`：Errors=0，Warnings=0，Suggestions=3 | Suggestions 为缺少 taskset/governor/freq/temperature；不阻塞本阶段，但文档和 Handoff 保留环境 metadata 边界。 |
| evidence registry | `make -C test-rvv/features/organized_edge_detection evidence_status`：fresh | production summary、manifest、doctor 已登记。 |

## Diagnostic-to-Production Mismatch Audit 回填

| question | answer |
| --- | --- |
| evidence role | production-public。 |
| A/B boundary | public `OrganizedEdgeBase<PointXYZ, Label>::compute()`，bench wrapper 只负责合成输入和调用公开入口。 |
| 当前决策问题 | RVV-vs-scalar。 |
| diagnostic 是否可外推到 production | Phase 000 只作为候选价值信号；本阶段 production direct 证据已独立闭合。 |
| comparison-boundary / baseline mismatch 风险 | 已通过 production bench 消除主要 mismatch；剩余边界是 `PointXYZ` / `pcl::Label` 和合成 organized grid。 |
| clean adoption 是否需要 RVV-vs-RVV detail A/B | 不需要；当前 production 文件此前没有 adopted RVV family。 |

## Fallback Matrix 结果

| gate | 行为 | 证据 |
| --- | --- | --- |
| 非 `__RVV10__` 构建 | 编译不到 RVV helper，执行标准 helper。 | Std build 4/4 pass。 |
| `PointT` 不满足 `RVVXYZAoSFloatLayout` | `if constexpr` 返回 false，执行标准 helper。 | 编译期 gate；未单独上板。 |
| `PointLT != pcl::Label` | 返回 false，执行标准 helper。 | 编译期 gate；本阶段不外推。 |
| 小于 3 的宽 / 高 | RVV helper 直接 true，保持 labels 全 0。 | 与原无内部像素语义一致；现有 helper case 间接覆盖 tail，不单列 board。 |
| invalid neighbor | RVV chunk 写全有限结果后逐 lane 标量修正。 | QEMU NaN boundary test 和 production board NaN boundary case。 |

## Optimization Matrix 更新

- `depth_labels_rvv_same_chain`：从 `partial-production-candidate` 升级为 `production-adopted`。
- `label_indices_collection`：仍 `deferred`。当前 production bench 已 strong positive，暂不优化顺序 push。
- `rgb_canny_gray_prep` / `normal_canny_prep`：仍作为 separate follow-up；depth path 已采纳，不自动覆盖 Canny 前处理。
- `point_type_expansion_queue`：保留为下一阶段候选，优先补 PointXYZ-like smoke 和泛型 `PointLT` gate 审计。

## Continue / Stop Decision

本阶段 closeout 结论为 `production-adopted`。当前没有必须继续才能采纳 depth path 的未阻塞动作；仍可继续的方向是扩展 scope：

1. `020-point-type-expansion`：验证 `PointXYZI`、`PointXYZRGB`、`PointXYZRGBNormal` 等 traits-gated 点类型和泛型 `PointLT` label field gate。
2. `030-rgb-normal-derived-entries`：单独评估 RGB / normal Canny 前处理是否值得 RVV。
3. `assignLabelIndices` 消融：只有真实 profile 显示 label index 收集成为主成本时再做。

这些方向会扩大当前 adopted scope，不阻塞本阶段采纳。
