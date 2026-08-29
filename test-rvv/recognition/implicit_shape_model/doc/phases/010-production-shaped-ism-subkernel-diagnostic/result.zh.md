# Phase 010: production-shaped ISM subkernel diagnostic result
## 阶段结论

Phase 010 完成。`descriptor_batch_assignment` 在 production-shaped diagnostic（生产形态诊断）
边界下保持正向收益，5-run board median 为 `2.120x`，且 Evidence Doctor 无 Error、无 Warning。
这说明 Phase 000 的 descriptor 子核收益能承受一个更接近 `findObjects()` 的 batch / gate 形态。

本阶段仍未进入 production integration loop（生产接入闭环），也未修改
`recognition/include/pcl/recognition/impl/implicit_shape_model.hpp`。当前最佳下一步是把这条
descriptor batch 结果整理成 PI1 的生产接入候选，而不是直接落 production patch。

## 执行事实

| action | result | evidence |
| --- | --- | --- |
| RED test | failed as expected before helper landed | `src/test_ism.cpp` 新增 `DescriptorBatchAssignmentMatchesReference` |
| helper implementation | completed | `include/impl/ism_diagnostics.hpp` |
| bench implementation | completed | `src/bench_ism.cpp` |
| asm gate | passed | `make -C test-rvv/recognition/implicit_shape_model check_ism_rvv_asm` |
| board repeated | 5/5 runs collected | `log/board/repeated_phase010_production_shaped_descriptor_diagnostic/summary.md` |
| manifest | generated | `log/board/repeated_phase010_production_shaped_descriptor_diagnostic/evidence_manifest.json` |
| Evidence Doctor | `Errors=0，Warnings=0，Suggestions=2` | `log/board/repeated_phase010_production_shaped_descriptor_diagnostic/evidence_doctor.md`、`log/board/repeated_phase010_production_shaped_descriptor_diagnostic/evidence_doctor.json` |
| registry | fresh after doc refresh | `log/evidence_registry.json` |

## 板卡结果

| case | median speedup | min | max | decision |
| --- | ---: | ---: | ---: | --- |
| `descriptor_batch_assignment` | 2.120x | 2.040x | 2.120x | positive |

该 case 使用 `--case-filter descriptor_batch_assignment --descriptors 512 --clusters 184 --iterations 100 --warmup-iterations 5`。
checksum 采用整数 assignment 的 semantic fingerprint，Std/RVV 一致；这比 raw bit checksum 更适合
batch assignment 这种生产形态诊断。

## Evidence Doctor 解释

Doctor 无 Warning，因此本阶段没有新的 board 风险信号需要降级。剩余 Suggestions 只要求补环境字段
`device` / `taskset` / `governor` / `freq` / `temperature` 和 binary 身份字段；这不影响当前正向结论，
但在进入 production patch 前应补齐。

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | production-shaped helper |
| 当前决策问题 | `findObjects()` 形态下的 RVV-vs-scalar descriptor assignment 是否仍正向 |
| diagnostic 是否可外推到 production | partial；它已更接近 `findObjects()`，但仍不含 feature estimator、radiusSearch 和 vote tree |
| comparison-boundary / baseline mismatch 风险 | yes；batch 仍是 synthetic descriptor batch，不是真实 feature distribution |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no；本阶段是 positive，因此可继续做 PI1 接入前冻结 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes；未来 production patch 仍需同边界证据 |

## Phase scope 与扩展队列

- validated_scope：synthetic `pcl::Histogram<153>` batch、contiguous cluster centers、descriptor_sum gate。
- unvalidated_scope：真实 FPFH、真实 sampled cloud、KMeans labels、radiusSearch、vote tree state、double
  `std::exp`、完整 `findObjects()` / `trainISM()`。
- point_type_expansion_queue：当前 deferred；若未来接 production patch，再单独审计 `PointT` / `NormalT`。
- phase_closeout_boundary：关闭 production-shaped descriptor batch diagnostic，不关闭 production 接入。

## 下一步

进入 PI1：冻结生产 patch 方案，优先考虑把 `findObjects()` 中 descriptor assignment 这一段抽成
`*_Std` / `*_RVV` helper 并保持公开入口短分流。若 PI1 审计通过，再推进 PI2-PI5。
