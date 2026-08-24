# Phase 047 结果：correspondence sorted-copy production probe

## 本阶段做了什么

本阶段把 Phase 046 中已经证明正向的 correspondence shuffle 子边界接入真实 production probe：

- 真实公开入口仍是 `TransformationEstimationSVDScale::estimateRigidTransformation(source, target, correspondences, matrix)`。
- 只有 `correspondences.size() >= 65536` 且 query order 呈非规则 shuffle-like disorder 时，production helper 才先复制并按 query index 排序，再复用现有 RVV accumulation。
- 4K 或规则顺序仍回到原有 current path，不把 sorted-copy 扩成全局默认。

## 证据结论

| 项目 | 结果 | 说明 |
| --- | --- | --- |
| QEMU correctness | 完成 | `run_test_compare_recorded` 现为 14 tests；Std/RVV 都通过。 |
| QEMU production probe smoke | 完成 | `record_qemu_correspondence_sorted_copy_production_probe_state` 通过，Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`。 |
| board repeated probe | 完成 | 5 runs 的 correspondence probe 全部 positive；64K / 256K 继续显著快于 Std。 |
| board Evidence Doctor | 完成 | `Errors=0`、`Warnings=1`、`Suggestions=0`；唯一 warning 是 4K group outlier，需要按 size 分开报告。 |

### board 结果

| case | median B/A | bucket | 结论 |
| --- | ---: | --- | --- |
| 4K | `8.128x` | positive | 4K 仍然快，但与 64K / 256K 不应合并解释。 |
| 64K | `3.929x` | positive | 生产 probe 的核心正向子边界。 |
| 256K | `3.618x` | positive | 生产 probe 的核心正向子边界。 |

## 实现落点

| 文件 | 变化 |
| --- | --- |
| `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` | 新增 correspondence sorted-copy production probe helper 和非规则乱序判断。 |
| `src/test_tesvd_scale.cpp` | 新增 64K shuffled correspondence correctness gate。 |
| `src/bench_tesvd_scale.cpp` | 新增 production probe case-filter。 |
| `script/generate_tesvd_scale_qemu_evidence_manifest.py` | 新增 probe case 分类。 |
| `script/generate_tesvd_scale_board_repeated_summary.py` | 新增 probe evidence-role 分类。 |
| `Makefile` | 新增 QEMU / board / registry targets。 |

## 诊断到生产的错配审计

| question | answer |
| --- | --- |
| evidence role | `production_public_row_source_probe`。 |
| A/B boundary | 真实 public correspondence overload；不是 bench-only detail A/B。 |
| 当前决策问题 | correspondence sorted-copy 是否可以作为有界 production patch。 |
| diagnostic 是否可外推到 production | 可以，但必须保留 size gate 与 shuffle-like disorder gate。 |
| comparison-boundary / baseline mismatch 风险 | 有，4K 与规则顺序不能继承 64K / 256K 的结论。 |
| weak / negative 时是否允许 bounded probe | 已经允许，并且在 64K / 256K 上拿到 positive。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 仍需要用户确认前的最终判断，但当前已不再是纯诊断。 |

## 当前决策

`adopted_by_user / correspondence-sorted-copy-production-probe / PointXYZ -> PointXYZ / Scalar=float / dense xyz AoS / size >= 64K / shuffle-like disorder`。

理由很直接：production probe 已在 QEMU、board repeated 和 Evidence Doctor 上闭合，而且 64K / 256K 维持了清晰收益；用户已确认保留这条 production patch。当前采用范围仍然很窄，只覆盖 correspondence 且满足 size gate / disorder gate 的路径。

## 下一步

保留当前 correspondence sorted-copy production probe 的 dispatch / fallback 边界，并把对应长期 `doc-rvv`、README、evaluation 和 topic-local doc suite 同步到当前 truth。后续若再推进，必须另开新的 candidate family 或更窄的 row-source 诊断，不再把 sorted-copy 继续外推成全 row-source 方案。
