# Phase 070 Result: size-threshold-tuning

## 实际执行范围

本阶段只调整 production public（公开入口生产证据）路径的规模 gate（会回到标量的准入条件）。真实入口仍是 `ExtractPolygonalPrismData<PointT>::segment(PointIndices&)`，范围限定为 `PointXYZ`、dense ordered indices、single polygon、`--path production`。本阶段不修改 public API（公开接口），不改 `projectPoints`，不把结论外推到 `PointXYZINormal`、非 float xyz layout、非法 indices 或其它自定义点型。

生产源码中的 work item count 阈值已从 `indices_->size() < 64` 降到 `indices_->size() < 32`。对应 fallback 测试改为 16 点输入，确保小于 32 的输入仍回到 `segmentStd`。

## 动作结果

| action | 状态 | 证据 / 命令 | 结论 |
| --- | --- | --- | --- |
| 阈值候选生产补丁 | done | `segmentation/include/pcl/segmentation/impl/extract_polygonal_prism_data.hpp` 中阈值改为 32；`SegmentRvvDeclinesSmallInputs` 改为 16 点 | `indices_->size() >= 32` 可以进入 RVV 尝试，其它 gate 不变 |
| QEMU correctness（QEMU 正确性） | done | `make run_test_compare` | Std 2 tests、RVV 16 tests 通过；QEMU 不作为性能证据 |
| board size sweep（板卡规模扫描） | done | `log/board/repeated-production-size{32,48,64,96,128,256}-threshold32/summary.md` | 六个规模的 3-run 都为正向，但因 run 数低，Evidence Doctor 均给 `low_run_count` warning，只作为初筛 |
| 32 点确认复跑 | done | `log/board/repeated-production-size32-threshold32-confirm5/summary.md` | 5 runs，median 1.19x，min 1.18x，max 1.38x，p10 1.18x，p90 1.30x |
| Evidence Doctor（证据体检） | done | `log/board/repeated-production-size32-threshold32-confirm5/evidence_doctor.md` | Errors=0 / Warnings=0 / Suggestions=0，支持采纳 32 阈值 |
| evidence registry（证据登记表） | done | `log/evidence_registry.json` | Phase 070 sweep 和 confirm5 summary / manifest / doctor 已登记；confirm5 是采纳主证据 |

## Board Sweep 初筛

| size | run label | run count | median | min | max | Evidence Doctor | 证据角色 |
| ---: | --- | ---: | ---: | ---: | ---: | --- | --- |
| 32 | `eppd-full-scan-production-size32-threshold32` | 3 | 1.18x | 1.18x | 1.19x | 0 / 1 / 0 | 初筛，触发 confirm5 |
| 48 | `eppd-full-scan-production-size48-threshold32` | 3 | 1.27x | 1.27x | 1.28x | 0 / 1 / 0 | 初筛 |
| 64 | `eppd-full-scan-production-size64-threshold32` | 3 | 1.34x | 1.34x | 1.34x | 0 / 1 / 0 | 初筛 |
| 96 | `eppd-full-scan-production-size96-threshold32` | 3 | 1.48x | 1.48x | 1.49x | 0 / 1 / 0 | 初筛 |
| 128 | `eppd-full-scan-production-size128-threshold32` | 3 | 1.56x | 1.56x | 1.57x | 0 / 1 / 0 | 初筛 |
| 256 | `eppd-full-scan-production-size256-threshold32` | 3 | 1.90x | 1.73x | 1.91x | 0 / 1 / 0 | 初筛 |

这些 3-run 数据的方向一致，说明降低阈值值得继续验证；但 `low_run_count` warning 表示它们不能单独作为强 production performance（生产性能）结论。

## 采纳证据

| 候选 | post-change board summary | Evidence Doctor | decision bucket | 结论 |
| --- | --- | --- | --- | --- |
| `indices_->size() >= 32` | run label `eppd-full-scan-production-size32-threshold32-confirm5`；`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-size32-threshold32-confirm5/summary.md`：5 runs，median 1.19x，min 1.18x，max 1.38x | `test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-size32-threshold32-confirm5/evidence_doctor.md`：0 / 0 / 0 | positive | 采纳 32 作为当前 production RVV 规模阈值 |

本阶段使用的 repeated board（重复板卡测试）口径为 `--size 32 --iterations 32 --warmup 4 --path production`。`summary.md` 和 `evidence_manifest.json` 已手动刷新为 `iterations=32`、`warmup_iterations=4`；Makefile 也新增 `EPPD_EVIDENCE_ITERATIONS` 与 `EPPD_EVIDENCE_WARMUP_ITERATIONS`，避免后续用默认 target 覆盖成 8 / 2 的错误 metadata。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production-public` |
| A/B boundary | public overload；Std / RVV build 都调用真实 `segment` |
| 当前决策问题 | threshold gate 是否应从 64 降到 32 |
| diagnostic 是否可外推到 production | 不使用 diagnostic 外推；采用真实 production path sweep 和 confirm5 |
| comparison-boundary / baseline mismatch 风险 | 已固定 `PointXYZ`、dense ordered indices、single polygon 和 `--path production`；结论不外推到其它点型 / polygon 组合 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段没有用 diagnostic 弱结果拒绝 production；32 点 confirm5 为 positive |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | no；当前决策是规模 gate 调整，不是新 RVV 实现族替换 |

## Optimization Matrix 更新

| candidate family | row source | point type / layout | correctness / fallback | board evidence | doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- |
| threshold keep 64 | dense ordered | `PointXYZ` / single polygon | 旧 fallback test 曾覆盖 `<64` | 被 threshold 32 confirm5 取代 | not_applicable | superseded | none |
| threshold lower to 48 | dense ordered | `PointXYZ` / single polygon | 被 32 候选覆盖 | 3-run median 1.27x，doctor 0 / 1 / 0 | low_run_count | not selected because 32 passed confirm5 | none |
| threshold lower to 32 | dense ordered | `PointXYZ` / single polygon | `make run_test_compare` pass；`SegmentRvvDeclinesSmallInputs` 覆盖 `<32` fallback | confirm5 median 1.19x，min 1.18x | 0 / 0 / 0 | adopted | none inside threshold tuning |

## Continue / Stop Decision

Phase 070 完成并采纳 32 阈值。当前 topic 内还可以想到的方向有三类，但本阶段证据不支持继续自动推进：

- `PointXYZINormal` / wide-stride point type（宽 stride 点型）此前 20-run 有 5/20 低于 1 且 Evidence Doctor 出现 Error，当前已通过 `sizeof(PointT) > 32` 显式回退；继续需要 dedicated wide-stride phase（专项宽步长阶段）和新的实现族，不建议在当前自动 loop 中继续。
- `projectPoints` 自身仍是标量前置成本，但它属于 `SampleConsensusModelPlane` 组件，涉及更宽 topic 与上游 helper，不再是本文件扫描段的直接优化；若要做，应另开 component ablation（组件消融）topic。
- `Scalar=double`、非 float xyz layout 或用户自定义点型需要新的数值 / layout 证据和可能的公共 traits 扩展，会扩大当前 production boundary（生产边界）。

因此本阶段的 `continue_stop_decision` 为 `turn_stop_deferred with stop_condition_hit`：当前 topic 授权范围内没有更值得自动推进且未阻塞的高优先级生产优化方向。默认下一步是停在整理 / review 边界，等待 reviewer 或用户决定是否提交、是否另开 `projectPoints` / wide-stride / custom point type 专题。
